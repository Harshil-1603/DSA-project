#pragma once

#include <string>

namespace route_finder
{

std::string fetch_overpass_data(double min_lat, double min_lon, double max_lat, double max_lon, const std::string &graph_detail = "medium");

} // namespace route_finder


