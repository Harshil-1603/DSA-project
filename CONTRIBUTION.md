# Contribution Guide

This document outlines the contributions made by each team member to the Test Centre Allotment Platform project.

## Team Members

1. **Harshil** (harshil.agrawal098@gmail.com) - GitHub: [Harshil-1603](https://github.com/Harshil-1603)
2. **Atharva Ajmera** (atharvaajmera06@gmail.com) - GitHub: [atharvaajmera](https://github.com/atharvaajmera)
3. **Harshit** (harshit775@gmail.com) - GitHub: [harshit1081](https://github.com/harshit1081)
4. **Fagun Kaliha** (fagunkaliha1@gmail.com) - GitHub: [Fagun1](https://github.com/Fagun1)

---

## Contribution Summary

### Harshil (8 commits)
**Role:** Backend API & Frontend Development

**Contributions:**
- **Project Setup & Infrastructure**
  - Initial project structure and directory setup
  - Added project README with architecture overview
  - Integrated external dependencies (httplib.h, nlohmann/json)

- **Backend API Development**
  - Implemented HTTP server with REST API endpoints for graph building and allotment
  - Created endpoints: `/build-graph`, `/run-allotment`, `/export-diagnostics`, `/get-path`, `/parallel-dijkstra`
  - Integrated CORS handling and error management

- **Frontend Development**
  - Built complete frontend HTML structure with Leaflet map integration
  - Implemented frontend JavaScript logic for graph building and visualization
  - Added UI styling and performance analytics dashboard
  - Created interactive map interface with centre/student management
  - Implemented real-time visualization of assignments and paths

**Files:**
- `backend/src/part4_api/server.cpp`
- `frontend/index.html`
- `frontend/app.js`
- `README.md`
- `backend/external/httplib.h`
- `backend/external/json_single.hpp`

---

### Atharva Ajmera (4 commits)
**Role:** Data Ingestion & Graph Construction

**Contributions:**
- **OpenStreetMap Integration**
  - Implemented Overpass API integration for fetching OSM road network data
  - Created `overpass.cpp` with data fetching functionality
  - Added `overpass.hpp` header for OSM data retrieval

- **Graph Construction**
  - Built graph construction pipeline from OSM data
  - Implemented speed-based edge weight calculation using highway types
  - Added support for one-way road handling
  - Created fallback simulated graph generation for demo purposes

- **Precomputation & Optimization**
  - Implemented precomputation lookup table for efficient centre distance queries
  - Added `build_allotment_lookup()` function for distance caching

**Files:**
- `backend/src/part1_ingestion/overpass.cpp`
- `backend/src/part1_ingestion/graph.cpp`
- `backend/include/route_finder/overpass.hpp`
- `backend/include/route_finder/graph.hpp`

---

### Harshit (5 commits)
**Role:** Spatial Algorithms & Data Structures

**Contributions:**
- **Geometry Utilities**
  - Implemented Haversine distance calculation for geographic coordinates
  - Created `geometry.cpp` with distance computation functions

- **KD-Tree Implementation**
  - Built KD-tree data structure for efficient spatial indexing
  - Implemented nearest node search algorithms
  - Created fast snapping functions for students and centres to road network
  - Added support for k-nearest neighbor queries

- **Graph Analysis**
  - Implemented connected components computation for graph analysis
  - Added functions to find nodes in main connected component
  - Created robust snapping that ensures nodes are in reachable components

**Files:**
- `backend/src/part2_spatial/geometry.cpp`
- `backend/src/part2_spatial/kdtree.cpp`
- `backend/include/route_finder/geometry.hpp`
- `backend/include/route_finder/kdtree.hpp`

---

### Fagun Kaliha (6 commits)
**Role:** Routing Algorithms & Allocation Logic

**Contributions:**
- **Core Data Structures**
  - Defined core domain objects (Student, Centre, Node, KDTreeNode, DijkstraResult)
  - Created type definitions in `types.hpp`
  - Established data model for the entire system

- **State Management**
  - Implemented global state management module
  - Created shared in-memory graph and data structures
  - Added state reset and management functions

- **Pathfinding Algorithms**
  - Implemented Dijkstra algorithm for shortest path computation
  - Created A* pathfinding algorithm with bidirectional search
  - Added path validation and cleaning functions
  - Implemented Dijkstra with parent tracking for route reconstruction

- **Allocation Engine**
  - Built greedy allotment algorithm with priority queues
  - Implemented tiered allotment logic with priority: Female > PwD > Male
  - Created distance-first assignment pipeline
  - Added capacity management and load balancing

**Files:**
- `backend/include/route_finder/types.hpp`
- `backend/src/part3_allocation/state.cpp`
- `backend/src/state.cpp`
- `backend/src/part3_allocation/routing.cpp`
- `backend/src/part3_allocation/allotment.cpp`
- `backend/include/route_finder/state.hpp`
- `backend/include/route_finder/routing.hpp`
- `backend/include/route_finder/allotment.hpp`

---

## Project Timeline

The project was developed over a 2-week period (October 30 - November 12, 2025) with the following progression:

1. **Week 1 (Oct 30 - Nov 4):** Project setup, data ingestion, and spatial algorithms
2. **Week 2 (Nov 5 - Nov 12):** Routing algorithms, allocation logic, API, and frontend

## Total Contributions

- **Total Commits:** 23
- **Harshil:** 8 commits (35%)
- **Fagun:** 6 commits (26%)
- **Harshit:** 5 commits (22%)
- **Atharva:** 4 commits (17%)

---

## Module Breakdown

### Part 1: Ingestion (Atharva)
- OSM data fetching
- Graph construction from road networks
- Fallback graph generation

### Part 2: Spatial (Harshit)
- Geographic distance calculations
- KD-tree spatial indexing
- Node snapping and component analysis

### Part 3: Allocation (Fagun)
- Core data structures
- State management
- Pathfinding algorithms (Dijkstra, A*)
- Greedy allotment with tiered priorities

### Part 4: API & Frontend (Harshil)
- REST API server
- Frontend UI with Leaflet maps
- Real-time visualization
- Performance analytics

---

## Technologies Used

- **Backend:** C++17, cpp-httplib, nlohmann/json, libcurl
- **Frontend:** Vanilla JavaScript, Leaflet.js, HTML5/CSS3
- **Algorithms:** Dijkstra, A*, KD-Tree, Greedy Algorithms
- **Data Sources:** OpenStreetMap (OSM) via Overpass API

---

*Last Updated: November 13, 2025*

