
# Test Centre Allotment Platform

High-performance tool that ingests OpenStreetMap data, pre-computes travel-time metrics, and assigns students to examination centres with a distance-first heuristic. The project now has a clear separation between backend services (C++) and the interactive frontend (vanilla JS + Leaflet).

## Project Layout

```
backend/
  external/                # vendored third-party headers (cpp-httplib, nlohmann::json)
  include/route_finder/    # shared headers
  src/
    part1_ingestion/       # Overpass integration & graph construction
      graph.cpp
      overpass.cpp
    part2_spatial/         # geometry helpers & KD-tree snapping
      geometry.cpp
      kdtree.cpp
    part3_allocation/      # routing engines, allotment workflow, shared state
      allotment.cpp
      routing.cpp
      state.cpp
    part4_api/             # HTTP server surface
      server.cpp
frontend/
  index.html               # dashboard UI (Leaflet map, controls, analytics)
  app.js                   # client logic (graph build, simulation, diagnostics)
```

## Backend Architecture

- `types.hpp` defines the core domain objects (`Student`, `Centre`, `Node`, `KDTreeNode`, `DijkstraResult`, etc.).
- **Part 1 – Ingestion:** `overpass.cpp` fetches road-network data from Overpass using libcurl, while `graph.cpp` parses the payload into a weighted adjacency list and synthesises a fallback grid when needed.
- **Part 2 – Spatial Core:** `geometry.cpp` contains Haversine helpers and `kdtree.cpp` provides nearest-node search plus connected-component tagging to keep snapping robust.
- **Part 3 – Allocation Engine:** `state.cpp` owns the shared in-memory graph, `routing.cpp` provides Dijkstra/A* implementations, and `allotment.cpp` runs the tiered, distance-first assignment pipeline.
- **Part 4 – API Gateway:** `server.cpp` exposes REST endpoints for graph build, allotment, diagnostics, ad-hoc routing, and a parallel Dijkstra benchmark.

## Algorithms & Data Flow

- **Graph ingestion**: `fetch_overpass_data` pulls OSM ways and nodes inside the chosen bounding box. `build_graph_from_overpass` converts them to a directed graph where every edge weight approximates travel time (distance plus inferred speed limits). If the fetch fails, `generate_simulated_graph_fallback` synthesises a dense grid so the UI remains demo-able.
- **KD-tree snapping**: `build_kdtree` indexes connected nodes; snapping functions (`find_best_snap_node_fast`, `find_nearest_in_main_component`) guarantee both centres and students land on viable parts of the network.
- **Shortest-path precomputation**: `build_allotment_lookup` runs Dijkstra from every centre so the allotment step can query travel times in O(1). The `/parallel-dijkstra` endpoint can re-run these computations concurrently to stress-test scaling.
- **Tiered greedy allotment**: `run_batch_greedy_allotment` groups students by category (male, PwD, female) and processes distance-priority queues per tier. Centre loads are tracked to enforce capacities.
- **Pathfinding for diagnostics**: `/get-path` uses A* to emit actual route geometry between a student and a centre, helping validate assignments in the UI.

## Building & Running

1. **Prerequisites**
   - C++17 compiler
   - `libcurl` development headers (required by `overpass.cpp`)
   - `cmake` or your preferred build system (example below uses `g++`)

2. **Compile the backend**

   ```
   cd backend
   g++ -std=c++17 $(find src -name '*.cpp') -Iinclude -Iexternal -lcurl -pthread -o route_finder
   ```

   The command links `libcurl` and enables POSIX threads (needed for the parallel Dijkstra endpoint).

3. **Run the HTTP service**

   ```
    ./route_finder
   ```

   By default the server listens on `http://0.0.0.0:8080`.

4. **Serve the frontend**

   Any static file server works, for example:

   ```
   cd frontend
   python -m http.server 3000
   ```

   Then open `http://localhost:3000` in a browser.

## API Overview

- `POST /build-graph` – Fetch OSM data, build the graph, snap centres, and pre-compute distance tables. Returns timing metrics plus node/edge counts.
- `POST /run-allotment` – Snap supplied students, rebuilds the centre distance cache, runs the tiered allotment, and returns assignments plus debug distances.
- `GET /export-diagnostics` – Dumps a detailed JSON report (snap quality, reachable counts, per-student metrics).
- `GET /get-path` – Produces an A* route between a student and centre (parameters support direct node IDs or raw coordinates).
- `POST /parallel-dijkstra` – Launches Dijkstra from each centre with `std::async`, returning timing stats and optional persisted results.

## Development Notes

- All third-party headers live in `backend/external`, simplifying offline builds.
- Global state lives in `route_finder::state` to avoid repeated deserialisation of large graphs; if you need persistence, wrap the structures before exposing them to multiple threads.
- Keep files ASCII-encoded unless a dependency already ships Unicode text (all source files follow the current convention).
- When extending algorithms, prefer adding new headers under `backend/include/route_finder/` so modules stay loosely coupled, and add matching unit tests or diagnostics under a separate folder if required.

## Next Steps

- Add a `CMakeLists.txt` or Meson build definition for reproducible builds.
- Consider serialising pre-computed graphs/distances to disk to avoid repeated API calls.
- Extend `is_valid_assignment` with real eligibility rules (gender, accessibility, etc.) when that data becomes available.


