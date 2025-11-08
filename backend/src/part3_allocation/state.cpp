#include "route_finder/state.hpp"

namespace route_finder
{

Graph graph;
std::unordered_map<long, Node> nodes;
KDTreeNode *kdtree_root = nullptr;
std::unordered_map<long, std::unordered_map<std::string, double>> allotment_lookup_map;
std::vector<Centre> centres;
std::vector<Student> students;
std::unordered_map<std::string, std::string> final_assignments;
std::unordered_map<long, int> node_component;

void reset_kdtree()
{
    kdtree_root = nullptr;
}

} // namespace route_finder


