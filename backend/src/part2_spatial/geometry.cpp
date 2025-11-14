#include "route_finder/geometry.hpp"

#include <cmath>

//for building with x64 mingw
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace route_finder
{

double haversine(double lat1, double lon1, double lat2, double lon2)
{
    const double R = 6371000.0;
    double phi1 = lat1 * M_PI / 180.0;
    double phi2 = lat2 * M_PI / 180.0;
    double delta_phi = (lat2 - lat1) * M_PI / 180.0;
    double delta_lambda = (lon2 - lon1) * M_PI / 180.0;

    double a = std::sin(delta_phi / 2.0) * std::sin(delta_phi / 2.0) +
               std::cos(phi1) * std::cos(phi2) * std::sin(delta_lambda / 2.0) * std::sin(delta_lambda / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

    return R * c;
}

} // namespace route_finder


