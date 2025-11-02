#include "route_finder/overpass.hpp"

#include <iostream>
#include <string>

namespace route_finder
{
std::string fetch_overpass_data(double min_lat, double min_lon, double max_lat, double max_lon, const std::string &graph_detail)
{
    std::cout << "Skipping live Overpass fetch (detail=" << graph_detail
              << ") in this environment. Falling back to simulated graph data." << std::endl;
    (void)min_lat;
    (void)min_lon;
    (void)max_lat;
    (void)max_lon;
    return "{}";
}

} // namespace route_finder


