#pragma once

#include <string>

#include "external/json_single.hpp"
#include "types.hpp"

namespace route_finder
{

void build_graph_from_overpass(const nlohmann::json &osm_data);
void generate_simulated_graph_fallback(double min_lat, double min_lon, double max_lat, double max_lon);
void build_allotment_lookup();

} // namespace route_finder


