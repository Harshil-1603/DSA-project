#include "external/httplib.h"
#include "external/json_single.hpp"

#include <chrono>
#include <cmath>
#include <ctime>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "route_finder/allotment.hpp"
#include "route_finder/geometry.hpp"
#include "route_finder/graph.hpp"
#include "route_finder/kdtree.hpp"
#include "route_finder/overpass.hpp"
#include "route_finder/routing.hpp"
#include "route_finder/state.hpp"
#include "route_finder/types.hpp"

namespace route_finder
{
namespace
{

using json = nlohmann::json;

void build_kdtree_for_graph()
{
    std::cout << "Building KD-tree for " << nodes.size() << " nodes..." << std::endl;

    std::vector<std::pair<long, std::pair<double, double>>> node_points;
    node_points.reserve(nodes.size());

    for (const auto &[node_id, node] : nodes)
    {
        if (graph.find(node_id) != graph.end() && !graph[node_id].empty())
        {
            node_points.push_back({node_id, {node.lat, node.lon}});
        }
    }

    std::cout << "KD-tree will be built from " << node_points.size() << " connected nodes." << std::endl;

    reset_kdtree();
    kdtree_root = build_kdtree(node_points, 0);
}

void snap_centres_to_graph()
{
    for (auto &centre : centres)
    {
        centre.snapped_node_id = find_nearest_node(centre.lat, centre.lon);
    }
}

void snap_students_to_graph(json const &students_json)
{
    students.clear();
    students.reserve(students_json.size());

    for (const auto &s : students_json)
    {
        Student student;
        student.student_id = s.value("student_id", "");
        student.lat = s.value("lat", 0.0);
        student.lon = s.value("lon", 0.0);
        student.category = s.value("category", "male");
        student.snapped_node_id = find_best_snap_node_fast(student.lat, student.lon);

        if (student.snapped_node_id != -1)
        {
            const int component = node_component.count(student.snapped_node_id) ? node_component[student.snapped_node_id] : -1;
            if (component <= 0)
            {
                const long fallback = find_nearest_in_main_component(student.lat, student.lon);
                if (fallback != -1)
                {
                    student.snapped_node_id = fallback;
                }
            }
        }

        students.push_back(student);
    }
}

json build_debug_distances_payload()
{
    json distances_json = json::object();
    for (const auto &student : students)
    {
        if (allotment_lookup_map.find(student.snapped_node_id) != allotment_lookup_map.end())
        {
            distances_json[student.student_id] = allotment_lookup_map[student.snapped_node_id];
        }
        else
        {
            distances_json[student.student_id] = json::object();
        }
    }
    return distances_json;
}

json collect_diagnostics()
{
    json diagnostic_report;

    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    char timestamp[64];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now_time));

    diagnostic_report["metadata"] = {
        {"run_id", std::string("run_") + timestamp},
        {"timestamp", timestamp},
        {"city", "Unnamed"},
        {"num_students", students.size()},
        {"num_centres", centres.size()},
        {"capacity_per_centre", centres.empty() ? 0 : centres.front().max_capacity},
        {"notes", "Detailed diagnostic export"}};

    std::unordered_map<std::string, int> centre_assignment_count;
    for (const auto &centre : centres)
    {
        centre_assignment_count[centre.centre_id] = 0;
    }
    for (const auto &[student_id, centre_id] : final_assignments)
    {
        centre_assignment_count[centre_id]++;
    }

    json centres_json = json::array();
    for (const auto &centre : centres)
    {
        centres_json.push_back({
            {"centre_id", centre.centre_id},
            {"lat", centre.lat},
            {"lon", centre.lon},
            {"graph_node_id", centre.snapped_node_id},
            {"assigned_students", centre_assignment_count[centre.centre_id]}});
    }
    diagnostic_report["centres"] = centres_json;

    json students_json = json::array();
    int unreachable_count = 0;
    int large_snap_count = 0;
    double snap_distance_sum = 0.0;
    int snap_count = 0;

    for (const auto &student : students)
    {
        json student_json;
        student_json["student_id"] = student.student_id;
        student_json["lat"] = student.lat;
        student_json["lon"] = student.lon;
        student_json["category"] = student.category;
        student_json["snap_node_id"] = student.snapped_node_id;

        double snap_distance = -1.0;
        if (nodes.find(student.snapped_node_id) != nodes.end())
        {
            const auto &snapped_node = nodes[student.snapped_node_id];
            snap_distance = haversine(student.lat, student.lon, snapped_node.lat, snapped_node.lon);
            snap_distance_sum += snap_distance;
            snap_count++;
            if (snap_distance > 100.0)
            {
                large_snap_count++;
            }
        }
        student_json["snap_distance_m"] = snap_distance;

        const bool assigned = final_assignments.find(student.student_id) != final_assignments.end();
        if (!assigned)
        {
            unreachable_count++;
        }
        student_json["assigned_centre_id"] = assigned ? json(final_assignments[student.student_id]) : json();

        std::map<std::string, double> alternative_costs;
        int reachable_centres = 0;
        double best_distance = std::numeric_limits<double>::max();
        double second_best = std::numeric_limits<double>::max();

        for (const auto &centre : centres)
        {
            double distance = std::numeric_limits<double>::max();
            const auto lookup_it = allotment_lookup_map.find(student.snapped_node_id);
            if (lookup_it != allotment_lookup_map.end())
            {
                const auto entry_it = lookup_it->second.find(centre.centre_id);
                if (entry_it != lookup_it->second.end())
                {
                    distance = entry_it->second;
                }
            }

            alternative_costs[centre.centre_id] = distance;
            if (distance < std::numeric_limits<double>::max())
            {
                reachable_centres++;
                if (distance < best_distance)
                {
                    second_best = best_distance;
                    best_distance = distance;
                }
                else if (distance < second_best)
                {
                    second_best = distance;
                }
            }
        }

        student_json["alt_distances_m"] = alternative_costs;
        student_json["component_id"] = node_component.count(student.snapped_node_id) ? node_component[student.snapped_node_id] : -1;
        student_json["reachable_count"] = reachable_centres;
        student_json["near_tie"] = (second_best < std::numeric_limits<double>::max() && std::abs(second_best - best_distance) < 20.0);

        students_json.push_back(student_json);
    }

    diagnostic_report["students"] = students_json;
    diagnostic_report["summary"] = {
        {"unreachable_count", unreachable_count},
        {"large_snap_count", large_snap_count},
        {"avg_snap_distance_m", snap_count > 0 ? snap_distance_sum / snap_count : 0.0}};

    return diagnostic_report;
}

void ensure_graph_ready(httplib::Response &res)
{
    if (graph.empty() || nodes.empty())
    {
        json error;
        error["status"] = "error";
        error["message"] = "Graph not built. Call /build-graph first.";
        res.set_content(error.dump(), "application/json");
    }
}

} // namespace
} // namespace route_finder

int main()
{
    using namespace route_finder;

    httplib::Server server;

    server.set_pre_routing_handler([](const httplib::Request &req, httplib::Response &res)
                                   {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        if (req.method == "OPTIONS")
        {
            res.status = 200;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled; });

    server.Post("/build-graph", [](const httplib::Request &req, httplib::Response &res)
                {
        try
        {
            const auto body = json::parse(req.body);

            const double min_lat = body.value("min_lat", 26.0);
            const double min_lon = body.value("min_lon", 72.0);
            const double max_lat = body.value("max_lat", 27.0);
            const double max_lon = body.value("max_lon", 74.0);
            const std::string detail = body.value("graph_detail", "medium");

            centres.clear();
            if (body.contains("centres") && body["centres"].is_array())
            {
                for (const auto &centre_json : body["centres"])
                {
                    if (!centre_json.is_object())
                    {
                        continue;
                    }

                    Centre centre;
                    centre.centre_id = centre_json.value("centre_id", "centre");
                    centre.lat = centre_json.value("lat", 0.0);
                    centre.lon = centre_json.value("lon", 0.0);
                    centre.max_capacity = centre_json.value("max_capacity", 500);
                    centre.current_load = 0;
                    centre.has_wheelchair_access = centre_json.value("has_wheelchair_access", false);
                    centre.is_female_only = centre_json.value("is_female_only", false);

                    centres.push_back(centre);
                }
            }

            const auto fetch_start = std::chrono::high_resolution_clock::now();
            const std::string osm_payload = fetch_overpass_data(min_lat, min_lon, max_lat, max_lon, detail);
            const auto fetch_end = std::chrono::high_resolution_clock::now();

            json osm_data = json::parse(osm_payload);

            const auto build_start = std::chrono::high_resolution_clock::now();
            build_graph_from_overpass(osm_data);
            const auto build_end = std::chrono::high_resolution_clock::now();

            if (nodes.empty())
            {
                std::cout << "Overpass data empty, generating simulated graph fallback." << std::endl;
                generate_simulated_graph_fallback(min_lat, min_lon, max_lat, max_lon);
            }

            const auto kd_start = std::chrono::high_resolution_clock::now();
            build_kdtree_for_graph();
            snap_centres_to_graph();
            const auto kd_end = std::chrono::high_resolution_clock::now();

            const auto dijkstra_start = std::chrono::high_resolution_clock::now();
            build_allotment_lookup();
            const auto dijkstra_end = std::chrono::high_resolution_clock::now();

            const auto fetch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(fetch_end - fetch_start).count();
            const auto build_ms = std::chrono::duration_cast<std::chrono::milliseconds>(build_end - build_start).count();
            const auto kd_ms = std::chrono::duration_cast<std::chrono::milliseconds>(kd_end - kd_start).count();
            const auto dijkstra_ms = std::chrono::duration_cast<std::chrono::milliseconds>(dijkstra_end - dijkstra_start).count();

            json response;
            response["status"] = "success";
            response["nodes_count"] = nodes.size();
            size_t edge_total = 0;
            for (const auto &entry : graph)
            {
                edge_total += entry.second.size();
            }
            response["edges_count"] = edge_total;
            response["timing"] = {
                {"fetch_overpass_ms", fetch_ms},
                {"build_graph_ms", build_ms},
                {"build_kdtree_ms", kd_ms},
                {"dijkstra_precompute_ms", dijkstra_ms},
                {"total_ms", fetch_ms + build_ms + kd_ms + dijkstra_ms}};

            res.set_content(response.dump(), "application/json");
        }
        catch (const std::exception &ex)
        {
            json error;
            error["status"] = "error";
            error["message"] = ex.what();
            res.set_content(error.dump(), "application/json");
        } });

    server.Post("/run-allotment", [](const httplib::Request &req, httplib::Response &res)
                {
        if (graph.empty() || nodes.empty())
        {
            json error;
            error["status"] = "error";
            error["message"] = "Graph not built. Call /build-graph first.";
            res.set_content(error.dump(), "application/json");
            return;
        }

        try
        {
            const auto request_body = json::parse(req.body);

            const auto total_start = std::chrono::high_resolution_clock::now();
            const auto snap_start = std::chrono::high_resolution_clock::now();
            snap_students_to_graph(request_body["students"]);
            const auto snap_end = std::chrono::high_resolution_clock::now();

            const auto dijkstra_start = std::chrono::high_resolution_clock::now();
            std::unordered_map<std::string, std::unordered_map<long, double>> centre_distances_map;
            allotment_lookup_map.clear();

            for (const auto &centre : centres)
            {
                std::cout << "Running Dijkstra from centre " << centre.centre_id << "..." << std::endl;
                const auto distances = dijkstra(centre.snapped_node_id);
                centre_distances_map[centre.centre_id] = distances;

                for (const auto &[node_id, dist] : distances)
                {
                    allotment_lookup_map[node_id][centre.centre_id] = dist;
                }
            }
            const auto dijkstra_end = std::chrono::high_resolution_clock::now();

            const auto allot_start = std::chrono::high_resolution_clock::now();
            run_batch_greedy_allotment();
            const auto allot_end = std::chrono::high_resolution_clock::now();
            const auto total_end = std::chrono::high_resolution_clock::now();

            const auto snap_ms = std::chrono::duration_cast<std::chrono::milliseconds>(snap_end - snap_start).count();
            const auto dijkstra_ms = std::chrono::duration_cast<std::chrono::milliseconds>(dijkstra_end - dijkstra_start).count();
            const auto allot_ms = std::chrono::duration_cast<std::chrono::milliseconds>(allot_end - allot_start).count();
            const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start).count();

            json response;
            response["status"] = "success";
            response["assignments"] = final_assignments;
            response["debug_distances"] = build_debug_distances_payload();
            response["timing"] = {
                {"snap_students_ms", snap_ms},
                {"dijkstra_ms", dijkstra_ms},
                {"allotment_ms", allot_ms},
                {"total_ms", total_ms}};

            res.set_content(response.dump(), "application/json");
        }
        catch (const std::exception &ex)
        {
            json error;
            error["status"] = "error";
            error["message"] = ex.what();
            res.set_content(error.dump(), "application/json");
        } });

    server.Get("/export-diagnostics", [](const httplib::Request &, httplib::Response &res)
               {
        if (graph.empty() || nodes.empty())
        {
            json error;
            error["status"] = "error";
            error["message"] = "Graph not built. Call /build-graph first.";
            res.set_content(error.dump(), "application/json");
            return;
        }

        try
        {
            const auto diagnostics = collect_diagnostics();
            res.set_content(diagnostics.dump(2), "application/json");
        }
        catch (const std::exception &ex)
        {
            json error;
            error["status"] = "error";
            error["message"] = ex.what();
            res.set_content(error.dump(), "application/json");
        } });

    server.Get("/get-path", [](const httplib::Request &req, httplib::Response &res)
               {
        if (graph.empty() || nodes.empty())
        {
            json error;
            error["status"] = "error";
            error["message"] = "Graph not built. Call /build-graph first.";
            res.set_content(error.dump(), "application/json");
            return;
        }

        try
        {
            std::vector<long> student_candidates;
            std::vector<long> centre_candidates;

            if (req.has_param("student_node_id") && req.has_param("centre_node_id"))
            {
                student_candidates.push_back(std::stol(req.get_param_value("student_node_id")));
                centre_candidates.push_back(std::stol(req.get_param_value("centre_node_id")));
            }
            else if (req.has_param("student_lat") && req.has_param("student_lon") &&
                     req.has_param("centre_lat") && req.has_param("centre_lon"))
            {
                const double student_lat = std::stod(req.get_param_value("student_lat"));
                const double student_lon = std::stod(req.get_param_value("student_lon"));
                const double centre_lat = std::stod(req.get_param_value("centre_lat"));
                const double centre_lon = std::stod(req.get_param_value("centre_lon"));

                student_candidates = find_k_nearest_nodes(student_lat, student_lon, 5);
                centre_candidates = find_k_nearest_nodes(centre_lat, centre_lon, 5);
            }
            else
            {
                throw std::runtime_error("Missing required parameters.");
            }

            const auto astar_start = std::chrono::high_resolution_clock::now();
            std::vector<long> best_path;
            bool found = false;

            for (long student_node : student_candidates)
            {
                for (long centre_node : centre_candidates)
                {
                    auto path = a_star(student_node, centre_node);
                    if (!path.empty())
                    {
                        best_path = std::move(path);
                        found = true;
                        break;
                    }
                }
                if (found)
                {
                    break;
                }
            }
            const auto astar_end = std::chrono::high_resolution_clock::now();

            json response;
            response["status"] = "success";

            json path_coords = json::array();
            for (long node_id : best_path)
            {
                if (nodes.find(node_id) != nodes.end())
                {
                    path_coords.push_back({nodes[node_id].lat, nodes[node_id].lon});
                }
            }
            response["path"] = path_coords;

            const auto astar_ms = std::chrono::duration_cast<std::chrono::milliseconds>(astar_end - astar_start).count();
            response["timing"] = {
                {"astar_ms", astar_ms},
                {"total_ms", astar_ms}};

            res.set_content(response.dump(), "application/json");
        }
        catch (const std::exception &ex)
        {
            json error;
            error["status"] = "error";
            error["message"] = ex.what();
            res.set_content(error.dump(), "application/json");
        } });

    server.Post("/parallel-dijkstra", [](const httplib::Request &req, httplib::Response &res)
                {
        if (graph.empty() || nodes.empty())
        {
            json error;
            error["status"] = "error";
            error["message"] = "Graph not built. Call /build-graph first.";
            res.set_content(error.dump(), "application/json");
            return;
        }

        try
        {
            const auto body = json::parse(req.body);
            const std::string workflow_name = body.value("workflow_name", "Parallel_Dijkstra");
            const std::string workflow_type = body.value("workflow_type", "parallel");
            const bool save_to_files = body.value("save_to_files", false);
            const std::string output_dir = body.value("output_dir", "./");

            const auto start_time = std::chrono::high_resolution_clock::now();

            std::vector<std::future<DijkstraResult>> futures;
            futures.reserve(centres.size());

            const auto parallel_start = std::chrono::high_resolution_clock::now();
            for (const auto &centre : centres)
            {
                futures.push_back(std::async(std::launch::async, run_dijkstra_for_centre, std::ref(centre)));
            }

            std::vector<DijkstraResult> results;
            results.reserve(centres.size());
            for (auto &future : futures)
            {
                results.push_back(future.get());
            }
            const auto parallel_end = std::chrono::high_resolution_clock::now();

            int success_count = 0;
            int failure_count = 0;
            long long sequential_total = 0;
            json result_array = json::array();

            for (const auto &result : results)
            {
                json result_json;
                result_json["centre_id"] = result.centre_id;
                result_json["start_node"] = result.start_node;
                result_json["success"] = result.success;
                result_json["computation_time_ms"] = result.computation_time_ms;

                if (result.success)
                {
                    success_count++;
                    sequential_total += result.computation_time_ms;

                    int reachable_nodes = 0;
                    for (const auto &[node_id, dist] : result.distances)
                    {
                        if (dist != std::numeric_limits<double>::max())
                        {
                            reachable_nodes++;
                        }
                    }
                    result_json["reachable_nodes"] = reachable_nodes;

                    if (save_to_files)
                    {
                        const std::string dist_file = output_dir + result.centre_id + "_distances.json";
                        const std::string parent_file = output_dir + result.centre_id + "_parents.json";
                        const bool saved = save_dijkstra_results(result, dist_file, parent_file);
                        result_json["saved_to_files"] = saved;
                        if (saved)
                        {
                            result_json["distances_file"] = dist_file;
                            result_json["parents_file"] = parent_file;
                        }
                    }
                }
                else
                {
                    failure_count++;
                    result_json["error_message"] = result.error_message;
                }

                result_array.push_back(result_json);
            }

            const auto end_time = std::chrono::high_resolution_clock::now();
            const auto parallel_ms = std::chrono::duration_cast<std::chrono::milliseconds>(parallel_end - parallel_start).count();
            const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            const double average_sequential = success_count > 0 ? static_cast<double>(sequential_total) / success_count : 0.0;
            const double estimated_sequential = average_sequential * centres.size();
            const double speedup = (parallel_ms > 0 && estimated_sequential > 0.0) ? estimated_sequential / parallel_ms : 0.0;

            json response;
            response["status"] = "success";
            response["workflow_name"] = workflow_name;
            response["workflow_type"] = workflow_type;
            response["centres_processed"] = centres.size();
            response["successful"] = success_count;
            response["failed"] = failure_count;
            response["results"] = result_array;
            response["timing"] = {
                {"parallel_execution_ms", parallel_ms},
                {"total_time_ms", total_ms},
                {"avg_per_centre_ms", success_count > 0 ? sequential_total / success_count : 0},
                {"estimated_sequential_ms", static_cast<long long>(estimated_sequential)},
                {"speedup", speedup}};
            response["performance_metrics"] = {
                {"num_threads_used", centres.size()},
                {"nodes_in_graph", nodes.size()},
                {"edges_in_graph", graph.size()}};

            res.set_content(response.dump(2), "application/json");
        }
        catch (const std::exception &ex)
        {
            json error;
            error["status"] = "error";
            error["message"] = ex.what();
            res.set_content(error.dump(), "application/json");
        } });

    std::cout << "Server starting on http://localhost:8080" << std::endl;
    server.listen("0.0.0.0", 8080);
    return 0;
}


