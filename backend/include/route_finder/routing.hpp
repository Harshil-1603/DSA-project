#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "types.hpp"

namespace route_finder
{

std::vector<long> clean_and_validate_path(const std::vector<long> &path);
std::vector<long> a_star_bidirectional(long start_node, long goal_node);
std::vector<long> a_star(long start_node, long goal_node);
std::unordered_map<long, double> dijkstra(long start_node);
std::pair<std::unordered_map<long, double>, std::unordered_map<long, long>> dijkstra_with_parents(long start_node);
DijkstraResult run_dijkstra_for_centre(const Centre &centre);
bool save_dijkstra_results(const DijkstraResult &result, const std::string &distances_file, const std::string &parents_file);

} // namespace route_finder


