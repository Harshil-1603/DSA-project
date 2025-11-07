#include "route_finder/overpass.hpp"

#include <curl/curl.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace route_finder
{
    namespace
    {
        size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
        {
            ((std::string *)userp)->append((char *)contents, size * nmemb);
            return size * nmemb;
        }
    } 

    std::string fetch_overpass_data(double min_lat, double min_lon, double max_lat, double max_lon, const std::string &graph_detail)
    {
        std::cout << "Fetching OSM data from Overpass API (detail=" << graph_detail << ")..." << std::endl;

        // highway types
        std::string highway_types;
        if (graph_detail == "low")
        {
            highway_types = "primary|secondary|tertiary";
            std::cout << "📉 Low detail: Major roads only (fastest)" << std::endl;
        }
        else if (graph_detail == "high")
        {
            highway_types = "motorway|trunk|primary|secondary|tertiary|residential|living_street|service|unclassified";
            std::cout << "📈 High detail: All roads (most accurate)" << std::endl;
        }
        else 
        {
            highway_types = "primary|secondary|tertiary|residential|living_street|service|unclassified";
            std::cout << "📊 Medium detail: Most roads (balanced)" << std::endl;
        }

        //overpass query
        std::ostringstream query;
        query << std::fixed << std::setprecision(6); 
        query << "[out:json][timeout:60][bbox:"
              << min_lat << "," << min_lon << "," << max_lat << "," << max_lon << "];";
        query << "way[highway~\"^(" << highway_types << ")$\"];";
        query << "(._;>;);"; 
        query << "out body;";

        std::string query_str = query.str();
        std::cout << "Query: " << query_str << std::endl;

        //curl
        CURL *curl = curl_easy_init();
        if (!curl)
        {
            std::cerr << "Failed to initialize CURL" << std::endl;
            return "{}";
        }

        std::string response_data;

        const char *base_urls[] = {
            "https://overpass-api.de/api/interpreter",
            "https://overpass.kumi.systems/api/interpreter"};

        bool success = false;
        for (const char *base_url : base_urls)
        {
            response_data.clear();

            char *encoded_query = curl_easy_escape(curl, query_str.c_str(), query_str.length());
            std::string url = std::string(base_url) + "?data=" + std::string(encoded_query);
            curl_free(encoded_query);

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "RouteFinderApp/1.0");
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            CURLcode res = curl_easy_perform(curl);

            if (res == CURLE_OK)
            {
                long http_code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

                if (http_code == 200)
                {
                    success = true;
                    std::cout << "Fetched " << response_data.size() << " bytes from " << base_url << std::endl;
                    break;
                }
                else
                {
                    std::cout << "HTTP " << http_code << ", trying next server..." << std::endl;
                }
            }
            else
            {
                std::cout << "Connection failed: " << curl_easy_strerror(res) << ", trying next server..." << std::endl;
            }
        }

        curl_easy_cleanup(curl);

        if (!success)
        {
            std::cerr << "All Overpass servers failed" << std::endl;
            return "{}";
        }

        return response_data;
    }

} 
