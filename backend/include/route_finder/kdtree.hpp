#pragma once

#include <vector>

#include "types.hpp"

namespace route_finder
{

KDTreeNode *build_kdtree(std::vector<std::pair<long, std::pair<double, double>>> &points, int depth);
void kdtree_nearest_helper(KDTreeNode *node, double target_lat, double target_lon, long &best_id, double &best_dist);
long find_nearest_node(double lat, double lon);
std::vector<long> find_k_nearest_nodes(double lat, double lon, int k = 5);
long find_best_snap_node_fast(double lat, double lon);
void compute_connected_components();
long find_nearest_in_main_component(double lat, double lon);
void snap_all_students_fast();

} // namespace route_finder


