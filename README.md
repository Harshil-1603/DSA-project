# Test Centre Allotment Platform

High-performance student-to-test-centre assignment system built with C++17 backend and interactive Leaflet.js frontend. Achieves 100% assignment success rate with optimized graph algorithms, smart caching, and real-time path visualization.

## Core Features

- **Real-time OSM Integration:** Fetches live road network data from OpenStreetMap via Overpass API
- **Component-Aware Graph Construction:** Ensures all entities snap to the main connected component for guaranteed reachability
- **Smart Caching System:** Validates cached maps by bounds and detail level for instant offline testing (<100ms vs 6-7s fetch)
- **Pre-computed Dijkstra:** One-time shortest path computation with parent tracking for instant route reconstruction
- **Tiered Greedy Allotment:** Distance-first assignment with priority queues (Male → PwD → Female)
- **A\* Pathfinding:** Real-world route visualization with travel time estimation
- **Enhanced Diagnostics:** Comprehensive performance analytics with timing breakdown and quality metrics

## Performance Metrics

| Metric                 | Before         | After                 | Improvement |
| ---------------------- | -------------- | --------------------- | ----------- |
| **Assignment Rate**    | 50% (503/1000) | 100% (1000/1000)      | +100%       |
| **OSM Fetch Time**     | 25+ seconds    | 6-7s (cached: <100ms) | +75%        |
| **Allotment Time**     | ~1000ms        | ~87ms                 | +90%        |
| **Path Visualization** | 20s timeout    | <100ms                | +99.5%      |
| **Graph Build Total**  | ~35s           | ~7s (cached: instant) | +80%        |

## Project Structure

```
backend/
  external/                   # Third-party headers (cpp-httplib, nlohmann::json)
  include/route_finder/       # Public API headers
    types.hpp                 # Core domain objects
    overpass.hpp              # OSM data fetching
    graph.hpp                 # Graph construction
    geometry.hpp              # Haversine distance
    kdtree.hpp                # Spatial indexing
    routing.hpp               # Dijkstra & A* algorithms
    allotment.hpp             # Assignment logic
    state.hpp                 # Global state management
  src/
    part1_ingestion/          # Data acquisition & graph building
      overpass.cpp            # Overpass API integration with caching
      graph.cpp               # Weighted graph construction from OSM
    part2_spatial/            # Spatial algorithms & indexing
      geometry.cpp            # Geographic distance calculations
      kdtree.cpp              # KD-tree for O(log n) nearest neighbor search
    part3_allocation/         # Core allocation engine
      routing.cpp             # Dijkstra (shortest path) & A* (heuristic search)
      allotment.cpp           # Greedy tiered assignment with priority queues
      state.cpp               # In-memory graph and global state
    part4_api/                # REST API layer
      server.cpp              # HTTP endpoints with cpp-httplib
frontend/
  index.html                  # Dashboard UI (Leaflet map, controls, analytics)
  app.js                      # Client logic (AJAX, visualization, diagnostics)
  osm_cache.json              # Local cache with metadata validation
```

## Data Structures & Algorithms

### Core Data Structures

1. **Adjacency List Graph (`unordered_map<int, vector<Edge>>`)**

   - Weighted directed graph for road network representation
   - Edge weights = travel time (distance / speed)
   - Supports one-way streets and variable speed limits
   - O(1) access to neighbors, O(E) space complexity

2. **KD-Tree (2D Binary Space Partitioning)**

   - Spatial indexing for 2D geographic coordinates (lat/lon)
   - Enables O(log n) nearest neighbor search vs O(n) brute force
   - Used for snapping students/centres to nearest road nodes
   - Balanced tree with median-split construction

3. **Priority Queue (Min-Heap)**

   - Dijkstra: `priority_queue<pair<double, int>, vector, greater<>>` for O(log V) extract-min
   - Greedy Allotment: Distance-sorted queues per student category
   - A\*: Combined g(n) + h(n) cost ordering for optimal pathfinding

4. **Disjoint Set Union (Connected Components)**

   - Identifies isolated graph components via DFS/BFS
   - Ensures all entities snap to main connected component
   - Prevents unreachable assignments (critical for 100% success rate)

5. **Hash Maps (`unordered_map`)**
   - Constant-time lookups for node/edge access
   - Centre distance caching: O(1) travel time queries after precomputation
   - Parent pointer storage for instant path reconstruction

### Key Algorithms

1. **Dijkstra's Algorithm (Single-Source Shortest Path)**

   - Precomputes travel times from all centres to all nodes
   - Time: O((V + E) log V) per centre with binary heap
   - Space: O(V) for distances + parent pointers
   - Enables O(1) allotment queries via lookup table

2. **A\* Pathfinding (Heuristic Search)**

   - Finds optimal routes between student-centre pairs
   - Heuristic: Haversine distance / average speed (admissible lower bound)
   - Time: O(E log V) in practice (much faster than Dijkstra for single paths)
   - Returns actual route geometry for visualization

3. **Greedy Tiered Assignment**

   - Priority-based allocation: Male students → PwD → Female
   - Per-tier: Sort by distance, assign to nearest centre with capacity
   - Time: O(S log S + S·C) where S = students, C = centres
   - Guarantees capacity constraints and fairness within tiers

4. **KD-Tree Construction & Search**
   - Build: O(n log n) with median-finding and recursive partitioning
   - Query: O(log n) average, O(√n) worst case for nearest neighbor
   - Component filtering ensures snapped nodes are reachable

## Backend Architecture

- `types.hpp` defines core domain objects (`Student`, `Centre`, `Node`, `Edge`, `KDTreeNode`, `DijkstraResult`)
- **Part 1 – Ingestion:**
  - `overpass.cpp`: Optimized GET requests with bbox pre-filtering and server prioritization
  - `graph.cpp`: Parses OSM JSON into adjacency list, calculates edge weights (time = distance/speed)
  - Smart caching: Validates bounds and detail level before using `osm_cache.json`
- **Part 2 – Spatial Core:**
  - `geometry.cpp`: Haversine distance formula for geographic calculations
  - `kdtree.cpp`: 2D binary space partitioning with component-aware snapping
  - Connected component analysis ensures reachability guarantees
- **Part 3 – Allocation Engine:**
  - `state.cpp`: Global in-memory graph and distance lookup tables
  - `routing.cpp`: Dijkstra with parent tracking, A\* with Haversine heuristic
  - `allotment.cpp`: Tiered greedy algorithm with priority queues and capacity tracking
- **Part 4 – API Gateway:**
  - `server.cpp`: REST endpoints with CORS, timing instrumentation, and diagnostic export
  - Parallel Dijkstra benchmark with `std::async` for performance testing

## Data Flow & Optimization Pipeline

1. **Graph Ingestion & Caching**

   - Check `osm_cache.json` for valid cached data (matches bounds + detail level)
   - If cache miss: `fetch_overpass_data()` pulls OSM ways/nodes via libcurl
   - Optimization: GET requests with URL encoding enable server-side caching
   - `build_graph_from_overpass()` constructs adjacency list with speed-based weights
   - Fallback: `generate_simulated_graph_fallback()` creates demo grid if API fails

2. **Component-Aware Snapping**

   - `compute_connected_components()` identifies isolated subgraphs via DFS
   - `build_kdtree()` indexes only main component nodes (largest connected subgraph)
   - `find_nearest_in_main_component()` guarantees centres/students snap to reachable nodes
   - Critical fix: Eliminated 497 unreachable assignments (50% → 100% success rate)

3. **Shortest Path Precomputation**

   - `build_allotment_lookup()` runs Dijkstra from each centre with parent tracking
   - Stores distances in hash map: `unordered_map<int, unordered_map<int, double>>`
   - Parent pointers enable instant path reconstruction: `centre_parent_maps`
   - Optimization: Moved computation from allotment phase → graph build phase
   - Result: 90% speedup in allotment (1000ms → 87ms)

4. **Tiered Greedy Allotment**

   - `run_batch_greedy_allotment()` processes students in priority order
   - Per tier: Build priority queue sorted by minimum centre distance
   - Assign to nearest centre with available capacity
   - Track loads per centre to enforce capacity constraints
   - O(1) distance lookups via precomputed table

5. **Real-time Path Visualization**
   - `/get-path` endpoint uses A\* for optimal route between student-centre pairs
   - Haversine heuristic: h(n) = straight-line distance / average speed
   - Reconstructs path geometry from parent pointers for map polyline
   - Optimization: Pre-computed Dijkstra paths eliminate timeouts
   - Result: 99.5% speedup (20s → <100ms)

## API Endpoints

| Endpoint              | Method | Description                                                             | Optimizations                                       |
| --------------------- | ------ | ----------------------------------------------------------------------- | --------------------------------------------------- |
| `/build-graph`        | POST   | Fetches OSM data, builds graph, snaps centres, precomputes Dijkstra     | Cache validation, GET requests, component filtering |
| `/run-allotment`      | POST   | Snaps students, runs tiered assignment, returns allocations             | O(1) lookups, removed redundant Dijkstra            |
| `/export-diagnostics` | GET    | Comprehensive JSON report with performance metrics and quality analysis | Timing breakdown, category stats, graph summary     |
| `/get-path`           | GET    | A\* route between student-centre with travel time estimation            | Parent pointer reconstruction, Haversine heuristic  |
| `/parallel-dijkstra`  | POST   | Concurrent Dijkstra benchmark with `std::async`                         | Performance stress testing                          |

### Request/Response Examples

**Build Graph:**

```json
POST /build-graph
{
  "bounds": {"south": 28.5, "west": 77.1, "north": 28.6, "east": 77.2},
  "centres": [{"lat": 28.55, "lon": 77.15, "capacity": 500}],
  "detail": "high",
  "use_cache": true
}

Response:
{
  "status": "success",
  "timing_ms": {"fetch": 6234, "build": 892, "snap": 43, "dijkstra": 1567},
  "graph": {"nodes": 12847, "edges": 28934, "components": 3, "main_size": 12801},
  "cache_used": true
}
```

**Export Diagnostics:**

```json
GET /export-diagnostics

Response:
{
  "performance_summary": {
    "time_fetch_ms": 6234, "time_build_ms": 892, "time_snap_students_ms": 43,
    "time_allotment_ms": 87, "total_ms": 8256
  },
  "allotment_quality_report": {
    "total_students": 1000, "assigned": 1000, "unassigned": 0,
    "avg_travel_time_sec": 324.5, "max_travel_time_sec": 1243.2,
    "by_category": {"male": {"assigned": 400}, "pwd": {"assigned": 100}, "female": {"assigned": 500}}
  },
  "graph_summary": {
    "total_nodes": 12847, "total_edges": 28934, "num_components": 3,
    "main_component_size": 12801, "detail_level": "high"
  }
}
```

## Building & Running

### Prerequisites

- **C++17 Compiler:** MSVC (Windows), GCC 15.2.0 (MSYS2), or Clang
- **CMake:** Version 3.10 or higher
- **vcpkg:** For dependency management (auto-installed by build script on Windows)
- **libcurl:** Automatically installed via vcpkg or system package manager

### Build Methods

#### Method 1: Windows with NMake (Recommended for MSVC)

Automated build script that sets up vcpkg, installs dependencies, and builds with NMake:

```batch
build_with_curl.bat
```

This script will:

1. Clone and bootstrap vcpkg if not present
2. Install libcurl via vcpkg for x64-windows
3. Configure CMake with NMake Makefiles generator
4. Build the project with `nmake`
5. Output: `build/route_finder.exe`

**Manual CMake + NMake:**

```powershell
# Install dependencies
vcpkg install curl:x64-windows

# Configure
cmake -B build -G "NMake Makefiles" -DCMAKE_TOOLCHAIN_FILE=vcpkg\scripts\buildsystems\vcpkg.cmake

# Build
cd build
nmake
```

#### Method 2: Windows with MSYS2 GCC 15.2.0

For GCC toolchain on Windows:

```powershell
.\build_with_msys2.ps1
```

Or from MSYS2 terminal:

```bash
./build_with_msys2.sh
```

#### Method 3: Linux/Mac with Make

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install libcurl4-openssl-dev cmake build-essential

# Configure and build
cmake -B build -G "Unix Makefiles"
cmake --build build --config Release

# Output: build/route_finder
```

### Running

**Start Backend:**

```bash
# Windows
cd build
route_finder.exe

# Linux/Mac
cd build
./route_finder

# Server listens on http://0.0.0.0:8080
```

**Serve Frontend:**

```bash
cd frontend
python -m http.server 3000
# Open http://localhost:3000 in browser
```

### Project Structure on Disk

```
DSA-project/
├── CMakeLists.txt              # CMake build configuration
├── build_with_curl.bat         # Automated Windows build with vcpkg + NMake
├── build_with_msys2.ps1        # PowerShell script for MSYS2 GCC build
├── build_with_msys2.sh         # Bash script for MSYS2 GCC build
├── vcpkg/                      # vcpkg package manager (auto-installed)
├── build/                      # Build output directory
│   ├── route_finder.exe        # Compiled backend server
│   ├── libcurl-d.dll           # CURL dependency (Windows)
│   └── osm_cache.json          # Runtime cache file
├── backend/
│   ├── external/               # cpp-httplib, nlohmann::json
│   ├── include/route_finder/   # Public headers
│   └── src/                    # Implementation files
└── frontend/
    ├── index.html              # Dashboard UI
    └── app.js                  # Client logic
```

## Key Technical Decisions

1. **Graph Weights = Travel Time**

   - Edges store time (seconds) based on distance/speed, not raw distance
   - Enables accurate travel time optimization vs simple distance minimization

2. **Component-Aware Snapping**

   - All entities forced to main connected component for guaranteed reachability
   - Prevents assignments to isolated road segments

3. **Pre-computed Dijkstra**

   - One-time computation with parent tracking for instant path reconstruction
   - Moves expensive computation from allotment phase → graph build phase

4. **Smart Caching Strategy**

   - Validates bounds and detail level before using cached OSM data
   - Auto-invalidates stale cache when map view changes
   - Enables instant offline testing (<100ms vs 6-7s fetch)

5. **GET over POST for API**
   - URL-encoded GET requests enable server-side caching
   - Overpass API prioritization: overpass-api.de (fastest) → kumi.systems → openstreetmap.fr

## Frontend Features

- **Interactive Leaflet Map:** Pan, zoom, and select bounds for OSM fetch
- **Real-time Visualization:** Student markers, centre icons, assignment polylines
- **Performance Dashboard:** Live timing metrics and diagnostics
- **Cache Control:** Toggle smart caching for offline testing
- **Path Explorer:** Click student-centre pairs to view A\* routes with travel time

## Technologies

- **Backend:** C++17, cpp-httplib (HTTP server), nlohmann/json (JSON parsing), libcurl (API requests)
- **Frontend:** Vanilla JavaScript, Leaflet.js (maps), HTML5/CSS3
- **Algorithms:** Dijkstra, A\*, KD-Tree, Greedy Algorithms, DFS (connected components)
- **Data Source:** OpenStreetMap via Overpass API

## Performance Optimizations Applied

1. **Component-aware snapping** → 100% assignment success
2. **Redundant Dijkstra elimination** → 90% allotment speedup
3. **Overpass query optimization** → 75% fetch speedup
4. **A\* with real-world paths** → 99.5% path visualization speedup
5. **Smart caching system** → Instant offline testing
6. **Enhanced diagnostics** → Comprehensive performance insights
7. **UI tweaks & x64 build fix** → Improved developer experience

## Development Guidelines

- **Dependency Management:** vcpkg handles all external dependencies (libcurl, etc.)
- **Build System:** CMake with support for NMake (MSVC), MinGW Makefiles (GCC), and Unix Makefiles
- **Output Directory:** All binaries and runtime files in `build/` directory
- **Cache Location:** `build/osm_cache.json` stores validated OSM data
- **Thread Safety:** Global state in `route_finder::state` - wrap structures if exposing to multiple threads
- **Compiler Support:** MSVC 2022, GCC 15.2.0 (MSYS2), or any C++17-compliant compiler
- **Platform Support:** Windows (native MSVC or MSYS2), Linux, macOS

## Future Enhancements

- **CMake Presets:** Add CMakePresets.json for easier configuration switching
- **Graph Serialization:** Persist precomputed graphs/distances to disk
- **Advanced Eligibility:** Gender constraints, accessibility requirements, exam board rules
- **Multi-threading:** Parallelize Dijkstra precomputation across all centres
- **Real-time Updates:** WebSocket support for live assignment tracking
- **Docker Support:** Containerized deployment for consistent environments

---

**Contributors:** Harshil, Atharva Ajmera, Harshit, Fagun Kaliha
