#include "route_finder/graph.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "external/json_single.hpp"
#include "route_finder/geometry.hpp"
#include "route_finder/kdtree.hpp"
#include "route_finder/routing.hpp"
#include "route_finder/state.hpp"

namespace route_finder
{
namespace
{

double get_default_speed(const std::string &highway_type)
{
    if (highway_type == "motorway")
    {
        return 100.0;
    }
    if (highway_type == "trunk")
    {
        return 90.0;
    }
    if (highway_type == "primary")
    {
        return 80.0;
    }
    if (highway_type == "secondary")
    {
        return 60.0;
    }
    if (highway_type == "tertiary")
    {
        return 50.0;
    }
    if (highway_type == "residential")
    {
        return 30.0;
    }
    if (highway_type == "living_street")
    {
        return 20.0;
    }
    if (highway_type == "service")
    {
        return 20.0;
    }
    if (highway_type == "unclassified")
    {
        return 40.0;
    }
    return 30.0;
}

} // namespace

void build_graph_from_overpass(const nlohmann::json &osm_data)
{
    std::cout << "Building graph from OpenStreetMap data..." << std::endl;

    nodes.clear();
    graph.clear();

    if (!osm_data.contains("elements") || osm_data["elements"].empty())
    {
        std::cerr << "No valid elements in OSM data." << std::endl;
        return;
    }

    for (const auto &element : osm_data["elements"])
    {
        if (element["type"] == "node")
        {
            long id = element["id"];
            double lat = element["lat"];
            double lon = element["lon"];
            nodes[id] = {id, lat, lon};
        }
    }
    std::cout << "Stored " << nodes.size() << " nodes from OSM data." << std::endl;

    int edge_count = 0;
    int oneway_count = 0;

    for (const auto &element : osm_data["elements"])
    {
        if (element["type"] == "way" && element.contains("nodes"))
        {
            std::string highway_type;
            bool is_oneway = false;
            double speed_kmh = 30.0;

            if (element.contains("tags"))
            {
                const auto &tags = element["tags"];

                if (tags.contains("highway"))
                {
                    highway_type = tags["highway"].get<std::string>();
                    speed_kmh = get_default_speed(highway_type);
                }

                if (tags.contains("oneway"))
                {
                    const std::string oneway_value = tags["oneway"].get<std::string>();
                    is_oneway = (oneway_value == "yes" || oneway_value == "true" || oneway_value == "1");
                }

                if (tags.contains("maxspeed"))
                {
                    const std::string maxspeed_str = tags["maxspeed"].get<std::string>();
                    try
                    {
                        speed_kmh = std::stod(maxspeed_str);
                    }
                    catch (...)
                    {
                    }
                }
            }

            const auto &way_node_ids = element["nodes"];

            for (size_t i = 0; i + 1 < way_node_ids.size(); i++)
            {
                const long node1_id = way_node_ids[i];
                const long node2_id = way_node_ids[i + 1];

                if (nodes.find(node1_id) == nodes.end() || nodes.find(node2_id) == nodes.end())
                {
                    continue;
                }

                const double dist_meters = haversine(
                    nodes[node1_id].lat, nodes[node1_id].lon,
                    nodes[node2_id].lat, nodes[node2_id].lon);
                const double dist_km = dist_meters / 1000.0;
                const double time_hours = dist_km / speed_kmh;
                const double time_seconds = time_hours * 3600.0;

                if (is_oneway)
                {
                    graph[node1_id].push_back({node2_id, time_seconds});
                    edge_count++;
                    oneway_count++;
                }
                else
                {
                    graph[node1_id].push_back({node2_id, time_seconds});
                    graph[node2_id].push_back({node1_id, time_seconds});
                    edge_count += 2;
                }
            }
        }
    }

    std::cout << "Graph built with " << nodes.size() << " nodes and " << edge_count << " directed edges." << std::endl;
    std::cout << "Identified " << oneway_count << " one-way segments." << std::endl;

    compute_connected_components();
}

void generate_simulated_graph_fallback(double min_lat, double min_lon, double max_lat, double max_lon)
{
    std::cout << "Generating simulated fallback graph..." << std::endl;

    nodes.clear();
    graph.clear();

    constexpr int grid_size = 80;
    const double lat_step = (max_lat - min_lat) / grid_size;
    const double lon_step = (max_lon - min_lon) / grid_size;

    long node_id = 1;
    std::vector<std::vector<long>> grid_nodes(grid_size, std::vector<long>(grid_size));

    for (int i = 0; i < grid_size; i++)
    {
        for (int j = 0; j < grid_size; j++)
        {
            const double lat = min_lat + i * lat_step;
            const double lon = min_lon + j * lon_step;
            nodes[node_id] = {node_id, lat, lon};
            grid_nodes[i][j] = node_id;
            node_id++;
        }
    }

    const std::vector<std::pair<int, int>> directions = {
        {0, 1}, {1, 0}, {1, 1}, {1, -1}, {0, -1}, {-1, 0}, {-1, -1}, {-1, 1}};

    for (int i = 0; i < grid_size; i++)
    {
        for (int j = 0; j < grid_size; j++)
        {
            const long current = grid_nodes[i][j];

            for (const auto &dir : directions)
            {
                const int ni = i + dir.first;
                const int nj = j + dir.second;

                if (ni < 0 || ni >= grid_size || nj < 0 || nj >= grid_size)
                {
                    continue;
                }

                const long neighbor = grid_nodes[ni][nj];
                const double dist = haversine(
                    nodes[current].lat, nodes[current].lon,
                    nodes[neighbor].lat, nodes[neighbor].lon);

                bool exists = false;
                for (const auto &edge : graph[current])
                {
                    if (edge.first == neighbor)
                    {
                        exists = true;
                        break;
                    }
                }

                if (!exists)
                {
                    graph[current].push_back({neighbor, dist});
                }
            }
        }
    }

    std::cout << "Simulated graph generated with " << nodes.size() << " nodes." << std::endl;

    compute_connected_components();
}

void build_allotment_lookup()
{
    std::cout << "Precomputing distance lookup for centres..." << std::endl;

    allotment_lookup_map.clear();

    for (const auto &centre : centres)
    {
        std::cout << "Running Dijkstra from centre " << centre.centre_id << "..." << std::endl;

        const auto distances = dijkstra(centre.snapped_node_id);

        for (const auto &[node_id, dist] : distances)
        {
            allotment_lookup_map[node_id][centre.centre_id] = dist;
        }
    }

    std::cout << "Allotment lookup table ready." << std::endl;
}

} // namespace route_finder


