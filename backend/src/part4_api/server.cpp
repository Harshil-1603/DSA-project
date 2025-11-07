// Enable Windows 10+ APIs for httplib (CreateFile2, etc.)
#ifdef _WIN32
#ifndef WINVER
#define WINVER 0x0A00 // Windows 10
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 // Windows 10
#endif
// Prevent Windows.h from defining min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Include winsock2.h before windows.h to avoid warnings
#include <winsock2.h>
#include <windows.h>
#include <fileapi.h>
#include <winbase.h>
#endif

#include "httplib.h"
#include "json_single.hpp"

#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
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

        constexpr const char *CACHE_FILE_NAME = "osm_cache.json";

        // Global diagnostic tracking variables
        struct DiagnosticTimings
        {
            long long fetch_overpass_ms = 0;
            long long build_graph_ms = 0;
            long long compute_components_ms = 0;
            long long build_kdtree_ms = 0;
            long long dijkstra_precompute_ms = 0;
            long long snap_students_ms = 0;
            long long allotment_ms = 0;
        } g_timings;

        struct GraphStats
        {
            std::string detail_setting;
            int nodes_total = 0;
            int edges_directed = 0;
            int oneway_edges = 0;
            int component_count = 0;
            int main_component_id = -1;
            int main_component_nodes = 0;
        } g_graph_stats;

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
                centre.snapped_node_id = find_nearest_in_main_component(centre.lat, centre.lon);
                std::cout << "Centre " << centre.centre_id << " snapped to node " << centre.snapped_node_id;
                if (node_component.count(centre.snapped_node_id))
                {
                    std::cout << " (component " << node_component[centre.snapped_node_id] << ")";
                }
                std::cout << std::endl;
            }
        }

        void snap_students_to_graph(json const &students_json)
        {
            std::cout << "\n⚡ Snapping " << students_json.size() << " students to road network..." << std::endl;
            auto start = std::chrono::high_resolution_clock::now();

            std::unordered_map<int, int> comp_count;
            for (auto &p : node_component)
            {
                if (p.second > 0)
                    comp_count[p.second]++;
            }
            int main_comp_id = -1;
            int max_comp_size = 0;
            for (auto &q : comp_count)
            {
                if (q.second > max_comp_size)
                {
                    max_comp_size = q.second;
                    main_comp_id = q.first;
                }
            }
            std::cout << "   Main component ID is " << main_comp_id << " with " << max_comp_size << " nodes." << std::endl;

            students.clear();
            students.reserve(students_json.size());
            int snapped = 0;
            int failed = 0;
            int rescued = 0;

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
                    int comp_id = node_component.count(student.snapped_node_id) ? node_component[student.snapped_node_id] : -1;

                    // --- 3. THE FIX: Check if not on the mainland ---
                    if (comp_id != main_comp_id)
                    {
                        long alt_node = find_nearest_in_main_component(student.lat, student.lon);
                        if (alt_node != -1)
                        {
                            student.snapped_node_id = alt_node;
                            rescued++;
                        }
                        else
                        {
                            student.snapped_node_id = -1;
                        }
                    }
                }

                if (student.snapped_node_id == -1)
                {
                    failed++;
                }
                else
                {
                    snapped++;
                }
                students.push_back(student);
            }

            auto end = std::chrono::high_resolution_clock::now();
            long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            std::cout << "✅ Snapping complete: " << snapped << " snapped, " << rescued << " rescued, " << failed << " failed in " << ms << "ms" << std::endl;
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
                centres_json.push_back({{"centre_id", centre.centre_id},
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

            // Performance Summary
            diagnostic_report["performance_summary"] = {
                {"time_fetch_overpass_ms", g_timings.fetch_overpass_ms},
                {"time_build_graph_ms", g_timings.build_graph_ms},
                {"time_compute_components_ms", g_timings.compute_components_ms},
                {"time_build_kdtree_ms", g_timings.build_kdtree_ms},
                {"time_dijkstra_precompute_ms", g_timings.dijkstra_precompute_ms},
                {"time_snap_students_ms", g_timings.snap_students_ms},
                {"time_allotment_ms", g_timings.allotment_ms},
                {"time_total_ms", g_timings.fetch_overpass_ms + g_timings.build_graph_ms +
                                      g_timings.compute_components_ms + g_timings.build_kdtree_ms +
                                      g_timings.dijkstra_precompute_ms + g_timings.snap_students_ms +
                                      g_timings.allotment_ms}};

            // Allotment Quality Report
            int total_assigned = final_assignments.size();
            int total_unassigned = students.size() - total_assigned;
            double total_travel_time_sec = 0.0;
            double max_travel_time_sec = 0.0;
            int first_choice_count = 0;

            // By-category stats
            std::map<std::string, int> cat_total, cat_assigned;
            std::map<std::string, double> cat_travel_sum;

            for (const auto &student : students)
            {
                cat_total[student.category]++;
            }

            for (const auto &student : students)
            {
                const auto it = final_assignments.find(student.student_id);
                if (it != final_assignments.end())
                {
                    const std::string &assigned_centre_id = it->second;
                    cat_assigned[student.category]++;

                    // Get travel time
                    double travel_time_sec = 0.0;
                    const auto lookup_it = allotment_lookup_map.find(student.snapped_node_id);
                    if (lookup_it != allotment_lookup_map.end())
                    {
                        const auto entry_it = lookup_it->second.find(assigned_centre_id);
                        if (entry_it != lookup_it->second.end())
                        {
                            travel_time_sec = entry_it->second;
                        }
                    }

                    total_travel_time_sec += travel_time_sec;
                    cat_travel_sum[student.category] += travel_time_sec;
                    if (travel_time_sec > max_travel_time_sec)
                    {
                        max_travel_time_sec = travel_time_sec;
                    }

                    // Check if first choice (minimum distance to any centre)
                    double min_distance = std::numeric_limits<double>::max();
                    for (const auto &centre : centres)
                    {
                        const auto lookup_it2 = allotment_lookup_map.find(student.snapped_node_id);
                        if (lookup_it2 != allotment_lookup_map.end())
                        {
                            const auto entry_it2 = lookup_it2->second.find(centre.centre_id);
                            if (entry_it2 != lookup_it2->second.end())
                            {
                                if (entry_it2->second < min_distance)
                                {
                                    min_distance = entry_it2->second;
                                }
                            }
                        }
                    }
                    if (travel_time_sec <= min_distance + 0.1) // tolerance for floating point
                    {
                        first_choice_count++;
                    }
                }
            }

            json by_category = json::array();
            for (const auto &[cat, total] : cat_total)
            {
                int assigned = cat_assigned[cat];
                double avg_travel = (assigned > 0) ? (cat_travel_sum[cat] / assigned) : 0.0;
                by_category.push_back({{"category", cat},
                                       {"total", total},
                                       {"assigned", assigned},
                                       {"unassigned", total - assigned},
                                       {"avg_travel_time_sec", avg_travel}});
            }

            diagnostic_report["allotment_quality_report"] = {
                {"total_students", (int)students.size()},
                {"total_assigned", total_assigned},
                {"total_unassigned_final", total_unassigned},
                {"total_travel_time_sec", total_travel_time_sec},
                {"avg_travel_time_sec", total_assigned > 0 ? total_travel_time_sec / total_assigned : 0.0},
                {"max_travel_time_sec", max_travel_time_sec},
                {"first_choice_assignments", first_choice_count},
                {"fallback_assignments", total_assigned - first_choice_count},
                {"by_category", by_category}};

            // Graph Summary
            diagnostic_report["graph_summary"] = {
                {"graph_detail_setting", g_graph_stats.detail_setting},
                {"nodes_count_total", g_graph_stats.nodes_total},
                {"edges_count_directed", g_graph_stats.edges_directed},
                {"oneway_edges_count", g_graph_stats.oneway_edges},
                {"component_count", g_graph_stats.component_count},
                {"main_component_id", g_graph_stats.main_component_id},
                {"main_component_nodes", g_graph_stats.main_component_nodes},
                {"isolated_nodes_count", g_graph_stats.nodes_total - g_graph_stats.main_component_nodes}};

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
            const bool use_cache = body.value("use_cache", false);

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

            std::string osm_payload;
            long long fetch_ms = 0;
            bool cache_valid = false;

            // --- CACHING LOGIC WITH VALIDATION ---
            std::ifstream cache_file(CACHE_FILE_NAME);
            if (use_cache && cache_file.good())
            {
                std::stringstream buffer;
                buffer << cache_file.rdbuf();
                cache_file.close();
                
                try
                {
                    json cached_data = json::parse(buffer.str());
                    
                    // Validate cache metadata
                    if (cached_data.contains("metadata"))
                    {
                        const auto &meta = cached_data["metadata"];
                        const double cached_min_lat = meta.value("min_lat", 0.0);
                        const double cached_min_lon = meta.value("min_lon", 0.0);
                        const double cached_max_lat = meta.value("max_lat", 0.0);
                        const double cached_max_lon = meta.value("max_lon", 0.0);
                        const std::string cached_detail = meta.value("graph_detail", "");
                        
                        // Check if bounds and detail match (with small tolerance for floating point)
                        const double tolerance = 0.0001;
                        if (std::abs(cached_min_lat - min_lat) < tolerance &&
                            std::abs(cached_min_lon - min_lon) < tolerance &&
                            std::abs(cached_max_lat - max_lat) < tolerance &&
                            std::abs(cached_max_lon - max_lon) < tolerance &&
                            cached_detail == detail)
                        {
                            cache_valid = true;
                            osm_payload = cached_data["osm_data"].dump();
                            std::cout << "🚀 CACHE HIT: Re-using data from '" << CACHE_FILE_NAME << "' (bounds and detail match)" << std::endl;
                        }
                        else
                        {
                            std::cout << "⚠️  CACHE INVALID: Bounds or detail mismatch. Fetching fresh data..." << std::endl;
                        }
                    }
                    else
                    {
                        std::cout << "⚠️  CACHE INVALID: No metadata found. Fetching fresh data..." << std::endl;
                    }
                }
                catch (const std::exception &e)
                {
                    std::cout << "⚠️  CACHE ERROR: Failed to parse cache file. Fetching fresh data..." << std::endl;
                }
            }
            
            if (!cache_valid)
            {
                if (use_cache && cache_file.good())
                {
                    std::cout << "📡 Fetching from Overpass API..." << std::endl;
                }
                else if (use_cache)
                {
                    std::cout << "⚠️  CACHE MISS: Cache file not found. Fetching from API..." << std::endl;
                }

                const auto fetch_start = std::chrono::high_resolution_clock::now();
                osm_payload = fetch_overpass_data(min_lat, min_lon, max_lat, max_lon, detail);
                const auto fetch_end = std::chrono::high_resolution_clock::now();
                fetch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(fetch_end - fetch_start).count();

                // Save to cache with metadata
                json cache_object;
                cache_object["metadata"] = {
                    {"min_lat", min_lat},
                    {"min_lon", min_lon},
                    {"max_lat", max_lat},
                    {"max_lon", max_lon},
                    {"graph_detail", detail},
                    {"timestamp", std::time(nullptr)}
                };
                cache_object["osm_data"] = json::parse(osm_payload);
                
                std::ofstream out_cache(CACHE_FILE_NAME);
                if (out_cache.good())
                {
                    out_cache << cache_object.dump();
                    out_cache.close();
                    std::cout << "💾 CACHE WRITE: Saved new data to '" << CACHE_FILE_NAME << "' with metadata" << std::endl;
                }
            }
            // --- END CACHING LOGIC ---

            json osm_data = json::parse(osm_payload);

            const auto build_start = std::chrono::high_resolution_clock::now();
            build_graph_from_overpass(osm_data);
            const auto build_end = std::chrono::high_resolution_clock::now();

            if (nodes.empty())
            {
                std::cout << "Overpass data empty, generating simulated graph fallback." << std::endl;
                generate_simulated_graph_fallback(min_lat, min_lon, max_lat, max_lon);
            }

            // Compute connected components to identify main component
            const auto comp_start = std::chrono::high_resolution_clock::now();
            compute_connected_components();
            const auto comp_end = std::chrono::high_resolution_clock::now();

            const auto kd_start = std::chrono::high_resolution_clock::now();
            build_kdtree_for_graph();
            snap_centres_to_graph();
            const auto kd_end = std::chrono::high_resolution_clock::now();

            const auto dijkstra_start = std::chrono::high_resolution_clock::now();
            build_allotment_lookup();
            const auto dijkstra_end = std::chrono::high_resolution_clock::now();

            const auto build_ms = std::chrono::duration_cast<std::chrono::milliseconds>(build_end - build_start).count();
            const auto comp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(comp_end - comp_start).count();
            const auto kd_ms = std::chrono::duration_cast<std::chrono::milliseconds>(kd_end - kd_start).count();
            const auto dijkstra_ms = std::chrono::duration_cast<std::chrono::milliseconds>(dijkstra_end - dijkstra_start).count();

            // Store timing for diagnostics
            g_timings.fetch_overpass_ms = fetch_ms;
            g_timings.build_graph_ms = build_ms;
            g_timings.compute_components_ms = comp_ms;
            g_timings.build_kdtree_ms = kd_ms;
            g_timings.dijkstra_precompute_ms = dijkstra_ms;

            // Store graph stats for diagnostics
            g_graph_stats.detail_setting = detail;
            g_graph_stats.nodes_total = static_cast<int>(nodes.size());
            
            size_t edge_total = 0;
            for (const auto &entry : graph)
            {
                edge_total += entry.second.size();
            }
            g_graph_stats.edges_directed = static_cast<int>(edge_total);
            
            // Calculate main component
            std::unordered_map<int, int> comp_counts;
            for (const auto &[node_id, comp_id] : node_component)
            {
                if (comp_id > 0)
                {
                    comp_counts[comp_id]++;
                }
            }
            g_graph_stats.component_count = static_cast<int>(comp_counts.size());
            
            int max_comp_size = 0;
            for (const auto &[comp_id, count] : comp_counts)
            {
                if (count > max_comp_size)
                {
                    max_comp_size = count;
                    g_graph_stats.main_component_id = comp_id;
                    g_graph_stats.main_component_nodes = count;
                }
            }

            json response;
            response["status"] = "success";
            response["nodes_count"] = nodes.size();
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

            // Dijkstra already computed in /build-graph - no need to re-run
            std::cout << "\n🎯 Using pre-computed Dijkstra distances from /build-graph..." << std::endl;

            const auto allot_start = std::chrono::high_resolution_clock::now();
            run_batch_greedy_allotment();
            const auto allot_end = std::chrono::high_resolution_clock::now();
            const auto total_end = std::chrono::high_resolution_clock::now();

            const auto snap_ms = std::chrono::duration_cast<std::chrono::milliseconds>(snap_end - snap_start).count();
            const auto allot_ms = std::chrono::duration_cast<std::chrono::milliseconds>(allot_end - allot_start).count();
            const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start).count();

            // Store timing for diagnostics
            g_timings.snap_students_ms = snap_ms;
            g_timings.allotment_ms = allot_ms;

            json response;
            response["status"] = "success";
            response["assignments"] = final_assignments;
            response["debug_distances"] = build_debug_distances_payload();
            response["timing"] = {
                {"snap_students_ms", snap_ms},
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
            double total_time_seconds = 0.0;
            
            for (size_t i = 0; i < best_path.size(); i++)
            {
                long node_id = best_path[i];
                if (nodes.find(node_id) != nodes.end())
                {
                    path_coords.push_back({nodes[node_id].lat, nodes[node_id].lon});
                    
                    // Calculate actual travel time by summing edge weights (which are in seconds)
                    if (i > 0)
                    {
                        long prev_node = best_path[i - 1];
                        if (graph.find(prev_node) != graph.end())
                        {
                            // Graph structure: std::pair<long, double> = (neighbor_id, time_seconds)
                            for (const auto &neighbor : graph[prev_node])
                            {
                                if (neighbor.first == node_id)
                                {
                                    total_time_seconds += neighbor.second;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            
            response["path"] = path_coords;
            response["travel_time_seconds"] = total_time_seconds;

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
