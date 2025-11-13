#include "route_finder/routing.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "json_single.hpp"
#include "route_finder/geometry.hpp"
#include "route_finder/state.hpp"

namespace route_finder
{
namespace
{

constexpr double kMaxSpeedMetresPerSecond = 27.8;

double heuristic(long node1, long node2)
{
    if (nodes.find(node1) == nodes.end() || nodes.find(node2) == nodes.end())
    {
        return 0.0;
    }

    const double distance_metres = haversine(
        nodes[node1].lat, nodes[node1].lon,
        nodes[node2].lat, nodes[node2].lon);

    return distance_metres / kMaxSpeedMetresPerSecond;
}

struct BestFirstNode
{
    long node_id{};
    double g_score{};
    double f_score{};

    bool operator>(const BestFirstNode &other) const
    {
        return f_score > other.f_score;
    }
};

} // namespace

std::vector<long> clean_and_validate_path(const std::vector<long> &path)
{
    if (path.empty())
    {
        return {};
    }

    std::vector<long> cleaned_path;
    cleaned_path.reserve(path.size());

    for (long node_id : path)
    {
        if (nodes.find(node_id) == nodes.end())
        {
            std::cerr << "Path contains missing node " << node_id << std::endl;
            continue;
        }

        if (graph.find(node_id) == graph.end() || graph[node_id].empty())
        {
            std::cerr << "Path contains disconnected node " << node_id << std::endl;
            continue;
        }

        cleaned_path.push_back(node_id);
    }

    return cleaned_path;
}

std::vector<long> a_star_bidirectional(long start_node, long goal_node)
{
    if (start_node == goal_node)
    {
        return {start_node};
    }

    if (graph.find(start_node) == graph.end() || graph.find(goal_node) == graph.end())
    {
        std::cerr << "Start or goal node not found in graph." << std::endl;
        return {};
    }

    std::unordered_map<long, double> g_score_forward;
    std::unordered_map<long, double> g_score_backward;
    std::unordered_map<long, long> came_from_forward;
    std::unordered_map<long, long> came_from_backward;
    std::priority_queue<BestFirstNode, std::vector<BestFirstNode>, std::greater<BestFirstNode>> open_forward;
    std::priority_queue<BestFirstNode, std::vector<BestFirstNode>, std::greater<BestFirstNode>> open_backward;
    std::set<long> closed_forward;
    std::set<long> closed_backward;

    g_score_forward[start_node] = 0.0;
    g_score_backward[goal_node] = 0.0;

    open_forward.push({start_node, 0.0, heuristic(start_node, goal_node)});
    open_backward.push({goal_node, 0.0, heuristic(goal_node, start_node)});

    long meeting_point = -1;
    int iterations = 0;
    constexpr int kMaxIterations = 100000;

    while (!open_forward.empty() && !open_backward.empty() && iterations < kMaxIterations)
    {
        iterations++;

        if (!open_forward.empty())
        {
            const auto current = open_forward.top();
            open_forward.pop();

            if (closed_forward.count(current.node_id))
            {
                continue;
            }
            closed_forward.insert(current.node_id);

            if (closed_backward.count(current.node_id))
            {
                meeting_point = current.node_id;
                break;
            }

            if (graph.find(current.node_id) != graph.end())
            {
                for (auto it = graph[current.node_id].begin(); it != graph[current.node_id].end(); ++it)
                {
                    const long neighbor = it->first;
                    const double edge_weight = it->second;
                    const double tentative_g = g_score_forward[current.node_id] + edge_weight;

                    if (!g_score_forward.count(neighbor) || tentative_g < g_score_forward[neighbor])
                    {
                        g_score_forward[neighbor] = tentative_g;
                        came_from_forward[neighbor] = current.node_id;

                        const double f = tentative_g + heuristic(neighbor, goal_node);
                        open_forward.push({neighbor, tentative_g, f});
                    }
                }
            }
        }

        if (!open_backward.empty())
        {
            const auto current = open_backward.top();
            open_backward.pop();

            if (closed_backward.count(current.node_id))
            {
                continue;
            }
            closed_backward.insert(current.node_id);

            if (closed_forward.count(current.node_id))
            {
                meeting_point = current.node_id;
                break;
            }

            if (graph.find(current.node_id) != graph.end())
            {
                for (auto it = graph[current.node_id].begin(); it != graph[current.node_id].end(); ++it)
                {
                    const long neighbor = it->first;
                    const double edge_weight = it->second;
                    const double tentative_g = g_score_backward[current.node_id] + edge_weight;

                    if (!g_score_backward.count(neighbor) || tentative_g < g_score_backward[neighbor])
                    {
                        g_score_backward[neighbor] = tentative_g;
                        came_from_backward[neighbor] = current.node_id;

                        const double f = tentative_g + heuristic(neighbor, start_node);
                        open_backward.push({neighbor, tentative_g, f});
                    }
                }
            }
        }
    }

    if (meeting_point == -1)
    {
        return {};
    }

    std::vector<long> path_forward;
    std::vector<long> path_backward;

    long node = meeting_point;
    while (came_from_forward.find(node) != came_from_forward.end())
    {
        path_forward.push_back(node);
        node = came_from_forward[node];
    }
    path_forward.push_back(start_node);
    std::reverse(path_forward.begin(), path_forward.end());

    node = meeting_point;
    while (came_from_backward.find(node) != came_from_backward.end())
    {
        path_backward.push_back(node);
        node = came_from_backward[node];
    }
    path_backward.push_back(goal_node);

    std::vector<long> full_path = path_forward;
    full_path.insert(full_path.end(), path_backward.begin(), path_backward.end());

    return full_path;
}

std::vector<long> a_star(long start_node, long goal_node)
{
    std::unordered_map<long, double> g_score;
    std::unordered_map<long, double> f_score;
    std::unordered_map<long, long> came_from;

    auto compare = [&f_score](long a, long b)
    { return f_score[a] > f_score[b]; };
    std::priority_queue<long, std::vector<long>, decltype(compare)> open_set(compare);
    std::set<long> open_tracker;

    for (auto it = nodes.begin(); it != nodes.end(); ++it)
    {
        g_score[it->first] = std::numeric_limits<double>::max();
        f_score[it->first] = std::numeric_limits<double>::max();
    }

    g_score[start_node] = 0.0;
    f_score[start_node] = heuristic(start_node, goal_node);
    open_set.push(start_node);
    open_tracker.insert(start_node);

    while (!open_set.empty())
    {
        const long current = open_set.top();
        open_set.pop();
        open_tracker.erase(current);

        if (current == goal_node)
        {
            std::vector<long> path;
            long node = goal_node;
            while (came_from.find(node) != came_from.end())
            {
                path.push_back(node);
                node = came_from[node];
            }
            path.push_back(start_node);
            std::reverse(path.begin(), path.end());
            return path;
        }

        if (graph.find(current) != graph.end())
        {
            for (auto it = graph[current].begin(); it != graph[current].end(); ++it)
            {
                const long neighbor = it->first;
                const double edge_weight = it->second;
                const double tentative_g = g_score[current] + edge_weight;

                if (tentative_g < g_score[neighbor])
                {
                    came_from[neighbor] = current;
                    g_score[neighbor] = tentative_g;
                    f_score[neighbor] = tentative_g + heuristic(neighbor, goal_node);

                    if (!open_tracker.count(neighbor))
                    {
                        open_set.push(neighbor);
                        open_tracker.insert(neighbor);
                    }
                }
            }
        }
    }

    return {};
}

std::unordered_map<long, double> dijkstra(long start_node)
{
    std::unordered_map<long, double> distances;
    std::priority_queue<std::pair<double, long>,
                        std::vector<std::pair<double, long>>,
                        std::greater<std::pair<double, long>>>
        pq;

    for (auto it = nodes.begin(); it != nodes.end(); ++it)
    {
        distances[it->first] = std::numeric_limits<double>::max();
    }
    distances[start_node] = 0.0;
    pq.push({0.0, start_node});

    while (!pq.empty())
    {
        const auto current_pair = pq.top();
        pq.pop();
        const double current_dist = current_pair.first;
        const long current_node = current_pair.second;

        if (current_dist > distances[current_node])
        {
            continue;
        }

        if (graph.find(current_node) != graph.end())
        {
            for (auto it = graph[current_node].begin(); it != graph[current_node].end(); ++it)
            {
                const long neighbor = it->first;
                const double edge_weight = it->second;
                const double new_dist = current_dist + edge_weight;
                if (new_dist < distances[neighbor])
                {
                    distances[neighbor] = new_dist;
                    pq.push({new_dist, neighbor});
                }
            }
        }
    }

    return distances;
}

std::pair<std::unordered_map<long, double>, std::unordered_map<long, long>> dijkstra_with_parents(long start_node)
{
    std::unordered_map<long, double> distances;
    std::unordered_map<long, long> parents;
    std::priority_queue<std::pair<double, long>,
                        std::vector<std::pair<double, long>>,
                        std::greater<std::pair<double, long>>>
        pq;

    for (auto it = nodes.begin(); it != nodes.end(); ++it)
    {
        distances[it->first] = std::numeric_limits<double>::max();
        parents[it->first] = -1;
    }
    distances[start_node] = 0.0;
    parents[start_node] = start_node;
    pq.push({0.0, start_node});

    while (!pq.empty())
    {
        const auto current_pair = pq.top();
        pq.pop();
        const double current_dist = current_pair.first;
        const long current_node = current_pair.second;

        if (current_dist > distances[current_node])
        {
            continue;
        }

        if (graph.find(current_node) != graph.end())
        {
            for (auto it = graph[current_node].begin(); it != graph[current_node].end(); ++it)
            {
                const long neighbor = it->first;
                const double edge_weight = it->second;
                const double new_dist = current_dist + edge_weight;

                if (new_dist < distances[neighbor])
                {
                    distances[neighbor] = new_dist;
                    parents[neighbor] = current_node;
                    pq.push({new_dist, neighbor});
                }
            }
        }
    }

    return {distances, parents};
}

DijkstraResult run_dijkstra_for_centre(const Centre &centre)
{
    DijkstraResult result;
    result.centre_id = centre.centre_id;
    result.start_node = centre.snapped_node_id;
    result.success = false;

    try
    {
        const auto start_time = std::chrono::high_resolution_clock::now();
        auto result_pair = dijkstra_with_parents(centre.snapped_node_id);
        const auto end_time = std::chrono::high_resolution_clock::now();

        result.computation_time_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        result.distances = std::move(result_pair.first);
        result.parents = std::move(result_pair.second);
        result.success = true;

        std::cout << "Dijkstra complete for " << centre.centre_id << " in "
                  << result.computation_time_ms << " ms." << std::endl;
    }
    catch (const std::exception &ex)
    {
        result.error_message = ex.what();
        std::cerr << "Error running Dijkstra for " << centre.centre_id << ": "
                  << ex.what() << std::endl;
    }

    return result;
}

bool save_dijkstra_results(const DijkstraResult &result, const std::string &distances_file, const std::string &parents_file)
{
    try
    {
        nlohmann::json distances_json = nlohmann::json::object();
        for (auto it = result.distances.begin(); it != result.distances.end(); ++it)
        {
            if (it->second != std::numeric_limits<double>::max())
            {
                distances_json[std::to_string(it->first)] = it->second;
            }
        }

        std::ofstream dist_out(distances_file);
        if (!dist_out.is_open())
        {
            std::cerr << "Unable to open " << distances_file << std::endl;
            return false;
        }
        dist_out << distances_json.dump(2);

        nlohmann::json parents_json = nlohmann::json::object();
        for (auto it = result.parents.begin(); it != result.parents.end(); ++it)
        {
            if (it->second != -1)
            {
                parents_json[std::to_string(it->first)] = it->second;
            }
        }

        std::ofstream parent_out(parents_file);
        if (!parent_out.is_open())
        {
            std::cerr << "Unable to open " << parents_file << std::endl;
            return false;
        }
        parent_out << parents_json.dump(2);

        std::cout << "Saved Dijkstra results for " << result.centre_id << " to disk." << std::endl;
        return true;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error saving Dijkstra results: " << ex.what() << std::endl;
        return false;
    }
}

} // namespace route_finder


