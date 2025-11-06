#include "route_finder/kdtree.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "route_finder/geometry.hpp"
#include "route_finder/state.hpp"

namespace route_finder
{

KDTreeNode *build_kdtree(std::vector<std::pair<long, std::pair<double, double>>> &points, int depth)
{
    if (points.empty())
    {
        return nullptr;
    }

    int axis = depth % 2;

    std::sort(points.begin(), points.end(),
              [axis](const auto &a, const auto &b)
              {
                  return (axis == 0) ? a.second.first < b.second.first : a.second.second < b.second.second;
              });

    size_t median_idx = points.size() / 2;
    long median_id = points[median_idx].first;
    double median_lat = points[median_idx].second.first;
    double median_lon = points[median_idx].second.second;

    KDTreeNode *node = new KDTreeNode(median_id, median_lat, median_lon, axis);

    std::vector<std::pair<long, std::pair<double, double>>> left_points(points.begin(), points.begin() + median_idx);
    std::vector<std::pair<long, std::pair<double, double>>> right_points(points.begin() + median_idx + 1, points.end());

    node->left = build_kdtree(left_points, depth + 1);
    node->right = build_kdtree(right_points, depth + 1);

    return node;
}

void kdtree_nearest_helper(KDTreeNode *node, double target_lat, double target_lon, long &best_id, double &best_dist)
{
    if (!node)
    {
        return;
    }

    double dist = haversine(target_lat, target_lon, node->lat, node->lon);

    if (dist < best_dist)
    {
        best_dist = dist;
        best_id = node->node_id;
    }

    double diff = (node->axis == 0) ? (target_lat - node->lat) : (target_lon - node->lon);
    KDTreeNode *near_side = (diff < 0) ? node->left : node->right;
    KDTreeNode *far_side = (diff < 0) ? node->right : node->left;

    kdtree_nearest_helper(near_side, target_lat, target_lon, best_id, best_dist);

    double axis_dist = std::abs(diff) * 111000.0;
    if (axis_dist < best_dist)
    {
        kdtree_nearest_helper(far_side, target_lat, target_lon, best_id, best_dist);
    }
}

long find_nearest_node(double lat, double lon)
{
    if (kdtree_root)
    {
        long best_id = -1;
        double best_dist = std::numeric_limits<double>::max();
        kdtree_nearest_helper(kdtree_root, lat, lon, best_id, best_dist);

        if (best_id != -1)
        {
            return best_id;
        }
    }

    std::vector<long> nearest = find_k_nearest_nodes(lat, lon, 1);

    if (nearest.empty())
    {
        return -1;
    }

    return nearest[0];
}

std::vector<long> find_k_nearest_nodes(double lat, double lon, int k)
{
    std::vector<std::pair<double, long>> distances;
    distances.reserve(nodes.size());

    for (const auto &[node_id, node] : nodes)
    {
        if (graph.find(node_id) == graph.end())
        {
            continue;
        }

        double dist = haversine(lat, lon, node.lat, node.lon);
        distances.push_back({dist, node_id});
    }

    int k_safe = std::min(k, static_cast<int>(distances.size()));
    if (k_safe == 0)
    {
        return {};
    }

    std::nth_element(distances.begin(), distances.begin() + k_safe - 1, distances.end());

    std::vector<long> result;
    result.reserve(k_safe);
    for (int i = 0; i < k_safe; i++)
    {
        result.push_back(distances[i].second);
    }

    return result;
}

long find_best_snap_node_fast(double lat, double lon)
{
    if (kdtree_root)
    {
        long best_id = -1;
        double best_dist = std::numeric_limits<double>::max();
        kdtree_nearest_helper(kdtree_root, lat, lon, best_id, best_dist);

        if (best_id != -1)
        {
            return best_id;
        }
    }

    long best_node = -1;
    double best_dist = std::numeric_limits<double>::max();

    for (const auto &[node_id, node] : nodes)
    {
        if (graph.find(node_id) == graph.end() || graph[node_id].empty())
        {
            continue;
        }

        double dist = haversine(lat, lon, node.lat, node.lon);
        if (dist < best_dist)
        {
            best_dist = dist;
            best_node = node_id;
        }
    }

    return best_node;
}

void compute_connected_components()
{
    node_component.clear();
    int comp_id = 0;
    std::vector<long> stack;
    for (const auto &p : nodes)
    {
        long nid = p.first;
        if (node_component.find(nid) != node_component.end())
        {
            continue;
        }
        if (graph.find(nid) == graph.end() || graph[nid].empty())
        {
            node_component[nid] = -1;
            continue;
        }
        comp_id++;
        stack.clear();
        stack.push_back(nid);
        node_component[nid] = comp_id;
        while (!stack.empty())
        {
            long cur = stack.back();
            stack.pop_back();
            if (graph.find(cur) == graph.end())
            {
                continue;
            }
            for (auto &e : graph[cur])
            {
                long nb = e.first;
                if (node_component.find(nb) == node_component.end())
                {
                    node_component[nb] = comp_id;
                    stack.push_back(nb);
                }
            }
        }
    }
    std::cerr << "Computed components, found " << comp_id << " components (isolated marked -1)\n";
}

long find_nearest_in_main_component(double lat, double lon)
{
    std::unordered_map<int, int> comp_count;
    for (auto &p : node_component)
    {
        if (p.second > 0)
        {
            comp_count[p.second]++;
        }
    }
    int main_comp = -1;
    int maxc = 0;
    for (auto &q : comp_count)
    {
        if (q.second > maxc)
        {
            maxc = q.second;
            main_comp = q.first;
        }
    }
    if (main_comp == -1)
    {
        return find_best_snap_node_fast(lat, lon);
    }

    long best = -1;
    double bd = std::numeric_limits<double>::max();
    for (auto &p : nodes)
    {
        long nid = p.first;
        if (node_component.find(nid) == node_component.end())
        {
            continue;
        }
        if (node_component[nid] != main_comp)
        {
            continue;
        }
        double d = haversine(lat, lon, p.second.lat, p.second.lon);
        if (d < bd)
        {
            bd = d;
            best = nid;
        }
    }
    return best;
}

void snap_all_students_fast()
{
    std::cout << "\n⚡ Snapping " << students.size() << " students to road network..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    int snapped = 0;
    int failed = 0;

    for (auto &student : students)
    {
        student.snapped_node_id = find_best_snap_node_fast(student.lat, student.lon);

        if (student.snapped_node_id != -1)
        {
            int comp = node_component.count(student.snapped_node_id) ? node_component[student.snapped_node_id] : -1;
            if (comp <= 0)
            {
                long alt = find_nearest_in_main_component(student.lat, student.lon);
                if (alt != -1)
                {
                    student.snapped_node_id = alt;
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

        if (snapped % 250 == 0)
        {
            std::cout << "  ✓ Snapped " << snapped << " students..." << std::endl;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "✅ Snapping complete: " << snapped << " snapped, " << failed << " failed in " << ms << "ms" << std::endl;
}

} // namespace route_finder


