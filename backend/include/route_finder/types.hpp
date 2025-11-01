#pragma once

#include <cmath>
#include <cstddef>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace route_finder
{

struct Student
{
    std::string student_id;
    double lat{};
    double lon{};
    long snapped_node_id{-1};
    std::string category;
};

struct Centre
{
    std::string centre_id;
    double lat{};
    double lon{};
    long snapped_node_id{-1};
    int max_capacity{0};
    int current_load{0};
    bool has_wheelchair_access{false};
    bool is_female_only{false};
};

struct AssignmentPair
{
    double distance{};
    std::string student_id;
    std::string centre_id;

    bool operator>(const AssignmentPair &other) const
    {
        return distance > other.distance;
    }
};

struct Node
{
    long id{};
    double lat{};
    double lon{};
};

struct KDTreeNode
{
    long node_id{};
    double lat{};
    double lon{};
    int axis{};
    KDTreeNode *left{nullptr};
    KDTreeNode *right{nullptr};

    KDTreeNode(long id, double lat_, double lon_, int axis_)
        : node_id(id), lat(lat_), lon(lon_), axis(axis_), left(nullptr), right(nullptr) {}
};

struct SearchNode
{
    long node_id{};
    double g_score{};
    double f_score{};

    bool operator>(const SearchNode &other) const
    {
        return f_score > other.f_score;
    }
};

struct DijkstraResult
{
    std::string centre_id;
    long start_node{};
    std::unordered_map<long, double> distances;
    std::unordered_map<long, long> parents;
    long long computation_time_ms{};
    bool success{false};
    std::string error_message;
};

using Graph = std::unordered_map<long, std::vector<std::pair<long, double>>>;

} // namespace route_finder


