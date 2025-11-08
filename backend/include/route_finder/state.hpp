#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "types.hpp"

namespace route_finder
{

extern Graph graph;
extern std::unordered_map<long, Node> nodes;
extern KDTreeNode *kdtree_root;
extern std::unordered_map<long, std::unordered_map<std::string, double>> allotment_lookup_map;
extern std::vector<Centre> centres;
extern std::vector<Student> students;
extern std::unordered_map<std::string, std::string> final_assignments;
extern std::unordered_map<long, int> node_component;

void reset_kdtree();

} // namespace route_finder


