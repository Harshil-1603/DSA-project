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

**Initial Contributions:**

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

**Files Created:**

- `backend/src/part4_api/server.cpp`
- `frontend/index.html`
- `frontend/app.js`
- `README.md`
- `backend/external/httplib.h`
- `backend/external/json_single.hpp`

---

### Atharva Ajmera (6 commits)

**Role:** Data Ingestion, Graph Construction & Performance Optimization

**Contributions:**

- **OpenStreetMap Integration**

  - Implemented Overpass API integration for fetching OSM road network data
  - Created `overpass.cpp` with optimized query structure
  - Added `overpass.hpp` header for OSM data retrieval
  - Optimized API queries with GET requests and bbox pre-filtering

- **Graph Construction**

  - Built graph construction pipeline from OSM data
  - Implemented speed-based edge weight calculation using highway types
  - Added support for one-way road handling
  - Created fallback simulated graph generation for demo purposes

- **Performance Optimizations**

  - Fixed component-aware snapping for 100% student assignment
  - Removed redundant Dijkstra computation from allotment phase
  - Made A\* suitable for instant path visualization with real-world paths instead of displacement-based pathfinding
  - Implemented smart caching system with metadata validation

- **Enhanced Diagnostics**
  - Added comprehensive stats endpoint with performance metrics
  - Implemented caching for fast OSM data fetch
  - Created UI tweaks and fixed x64 build configuration

**Files:**

- `backend/src/part1_ingestion/overpass.cpp`
- `backend/src/part1_ingestion/graph.cpp`
- `backend/src/part4_api/server.cpp`
- `frontend/app.js`
- `frontend/index.html`
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
  - Created A\* pathfinding algorithm with bidirectional search
  - Added path validation and cleaning functions
  - Implemented Dijkstra with parent tracking for route reconstruction

- **Allocation Engine**
  - Built greedy allotment algorithm with priority queues
  - Implemented tiered allotment logic with priority: Male > PwD > Female
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

The project was developed over a 3-week period (October 30 - November 14, 2025) with the following progression:

1. **Week 1 (Oct 30 - Nov 4):** Project setup, data ingestion, and spatial algorithms
2. **Week 2 (Nov 5 - Nov 12):** Routing algorithms, allocation logic, API, and frontend
3. **Week 3 (Nov 13 - Nov 14):** Major performance optimizations, bug fixes, and enhanced diagnostics

## Total Contributions

- **Total Commits:** 26
- **Harshil:** 8 commits (31%)
- **Atharva:** 7 commits (27%)
- **Fagun:** 6 commits (23%)
- **Harshit:** 5 commits (19%)

---

_Last Updated: November 14, 2025_
