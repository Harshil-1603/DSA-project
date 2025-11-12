// ==================== GLOBAL STATE ====================

const API_BASE_URL = "http://localhost:8080";

let map;
let centres = [];
let students = [];
let assignments = {};
let debugDistances = {}; // <-- NEW GLOBAL VARIABLE
let graphBuilt = false;

let centreMarkers = [];
let studentMarkers = [];
let pathLayers = [];

const centreColors = [
  "#ef4444",
  "#f59e0b",
  "#10b981",
  "#3b82f6",
  "#8b5cf6",
  "#ec4899",
  "#14b8a6",
  "#f97316",
];

// ==================== INITIALIZE MAP ====================

function initMap() {
  // Initialize map centered on a default location (can be changed)
  map = L.map("map").setView([26.27396219795028, 73.03599411582631], 14); // Jodhpur, India

  L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
    attribution: "© OpenStreetMap contributors",
    maxZoom: 19,
  }).addTo(map);

  // Add click event to add centres
  map.on("click", function (e) {
    addCentre(e.latlng.lat, e.latlng.lng);
  });

  updateLegend(); // Initialize empty legend on map load

  console.log("Map initialized");
}

// ==================== CENTRE MANAGEMENT ====================

function addCentre(lat, lon) {
  const centreId = `centre_${centres.length + 1}`;
  const capacity = parseInt(document.getElementById("centreCapacity").value);

  const centre = {
    centre_id: centreId,
    lat: lat,
    lon: lon,
    max_capacity: capacity,
  };

  centres.push(centre);

  // Add marker to map
  const color = centreColors[(centres.length - 1) % centreColors.length];
  const marker = L.circleMarker([lat, lon], {
    radius: 10,
    fillColor: color, // FIX: Use dynamic color instead of hardcoded red
    color: "#fff",
    weight: 3,
    opacity: 1,
    fillOpacity: 0.9,
  }).addTo(map);

  marker.bindPopup(`
        <strong>${centreId}</strong><br>
        Capacity: ${capacity}<br>
    `);

  centreMarkers.push(marker);

  updateStats();
  updateLegend(); // Update legend when centre is added
  console.log(`Added ${centreId} at [${lat}, ${lon}]`);
}

function clearCentres() {
  centres = [];
  centreMarkers.forEach((marker) => map.removeLayer(marker));
  centreMarkers = [];
  updateStats();
  updateLegend(); // Update legend when centres are cleared
  console.log("Cleared all centres");
}

// ==================== GRAPH BUILDING ====================

async function buildGraph() {
  if (centres.length === 0) {
    alert("Please add at least one test centre first!");
    return;
  }

  showLoader("Building graph from OpenStreetMap...");

  try {
    // Get map bounds and graph detail setting
    const bounds = map.getBounds();
    const graphDetail = document.getElementById("graphDetail").value;

    const payload = {
      min_lat: bounds.getSouth(),
      min_lon: bounds.getWest(),
      max_lat: bounds.getNorth(),
      max_lon: bounds.getEast(),
      centres: centres,
      graph_detail: graphDetail, // v4.0: User-configurable graph detail
    };

    console.log("Sending build-graph request:", payload);

    const response = await fetch(`${API_BASE_URL}/build-graph`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(payload),
    });

    const data = await response.json();

    if (data.status === "success") {
      graphBuilt = true;
      document.getElementById("runAllotmentBtn").disabled = false;
      document.getElementById("testParallelBtn").disabled = false;
      document.getElementById("statNodes").textContent = data.nodes_count;

      // v3.0: Update timing metrics in sidebar
      if (data.timing) {
        document.getElementById("statFetchTime").textContent =
          data.timing.fetch_overpass_ms + " ms";
        document.getElementById("statBuildGraphTime").textContent =
          data.timing.build_graph_ms + " ms";
        document.getElementById("statKDTreeTime").textContent =
          data.timing.build_kdtree_ms + " ms";
        document.getElementById("statDijkstraTime").textContent =
          data.timing.dijkstra_precompute_ms + " ms";

        console.log("📊 v3.0 Graph Build Timing:", data.timing);
      }

      hideLoader();
      alert(
        `Graph built successfully!\nNodes: ${data.nodes_count}\nEdges: ${data.edges_count}`
      );
      console.log("Graph built:", data);
    } else {
      hideLoader();
      alert("Error building graph: " + data.message);
    }
  } catch (error) {
    hideLoader();
    alert(
      "Failed to connect to backend server. Make sure it is running on port 8080."
    );
    console.error("Error:", error);
  }
}

function simulateStudents() {
  const count = parseInt(document.getElementById("studentCount").value);

  if (count <= 0) {
    alert("Please enter a valid number of students!");
    return;
  }

  if (centres.length === 0) {
    alert("Please add at least one test centre first!");
    return;
  }

  // Clear existing students
  students = [];
  studentMarkers.forEach((marker) => map.removeLayer(marker));
  studentMarkers = [];

  // --- 1. Calculate Centroid (Average Centre Location) ---
  let sumLat = 0;
  let sumLon = 0;
  for (const centre of centres) {
    sumLat += centre.lat;
    sumLon += centre.lon;
  }
  const centerLat = sumLat / centres.length;
  const centerLon = sumLon / centres.length;
  const centroid = L.latLng(centerLat, centerLon);

  // --- 2. Calculate Radius based on Farthest Centre ---
  let maxDistanceFromCentroid = 0;
  for (const centre of centres) {
    const centrePoint = L.latLng(centre.lat, centre.lon);
    const distance = centroid.distanceTo(centrePoint);
    if (distance > maxDistanceFromCentroid) {
      maxDistanceFromCentroid = distance;
    }
  }
  // Use the farthest centre as the radius, plus 25% padding
  const simulationRadius = maxDistanceFromCentroid * 1.25;
  // Add a minimum radius in case all centres are at one spot
  const finalRadius = Math.max(simulationRadius, 2000); // 5km min radius

  // --- 3. Create a tight bounding box around the circle (for efficient rejection sampling) ---
  // Calculate the lat/lon offset for the radius (approximate)
  const latOffset = finalRadius / 111320; // 1 degree lat ≈ 111.32 km
  const lonOffset =
    finalRadius / (111320 * Math.cos((centerLat * Math.PI) / 180)); // Adjust for latitude

  const minLat = centerLat - latOffset;
  const maxLat = centerLat + latOffset;
  const minLon = centerLon - lonOffset;
  const maxLon = centerLon + lonOffset;

  // --- 4. Use Rejection Sampling ---
  let studentsGenerated = 0;
  let safetyBreak = 0;
  while (studentsGenerated < count && safetyBreak < count * 100) {
    const lat = minLat + Math.random() * (maxLat - minLat);
    const lon = minLon + Math.random() * (maxLon - minLon);
    const randomPoint = L.latLng(lat, lon);

    const distance = centroid.distanceTo(randomPoint);

    // REJECT if outside our calculated circle
    if (distance > finalRadius) {
      safetyBreak++;
      continue;
    }

    // ACCEPT if inside the circle
    // Distribution: 5% pwd, 15% female, 80% male
    const rand = Math.random();
    let category;
    if (rand < 0.05) {
      category = "pwd"; // 5%
    } else if (rand < 0.2) {
      category = "female"; // 15% (0.05 + 0.15 = 0.20)
    } else {
      category = "male"; // 80% (remaining)
    }

    const student = {
      student_id: `student_${studentsGenerated + 1}`,
      lat: lat,
      lon: lon,
      category: category,
    };
    students.push(student);

    // Add marker
    const marker = L.circleMarker([lat, lon], {
      radius: 4,
      fillColor: "#3b82f6",
      color: "#fff",
      weight: 1,
      opacity: 1,
      fillOpacity: 0.7,
    }).addTo(map);

    marker.bindPopup(`
            <strong>${student.student_id}</strong><br>
            Category: ${category}
        `);

    studentMarkers.push(marker);

    studentsGenerated++;
    safetyBreak++;
  }

  updateStats();
  console.log(
    `Simulated ${studentsGenerated} students within a ${finalRadius.toFixed(
      0
    )}m radius around centre centroid (${centerLat.toFixed(
      4
    )}, ${centerLon.toFixed(4)}).`
  );
}

// ==================== ALLOTMENT ====================

async function runAllotment() {
  if (!graphBuilt) {
    alert("Please build the graph first!");
    return;
  }

  if (students.length === 0) {
    alert("Please simulate students first!");
    return;
  }

  showLoader("Running batch greedy allotment...");

  try {
    const payload = {
      students: students,
    };

    console.log("Sending run-allotment request");

    const response = await fetch(`${API_BASE_URL}/run-allotment`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(payload),
    });

    const data = await response.json();

    if (data.status === "success") {
      assignments = data.assignments;
      debugDistances = data.debug_distances; // <-- STORE THE NEW DATA
      visualizeAssignments();

      const assignedCount = Object.keys(assignments).length;

      // v3.0: Update timing metrics in sidebar
      if (data.timing) {
        document.getElementById("statAllotmentTime").textContent =
          data.timing.total_ms +
          " ms (snap: " +
          data.timing.snap_students_ms +
          "ms, allot: " +
          data.timing.allotment_ms +
          "ms)";

        console.log("📊 v3.0 Allotment Timing:", data.timing);
      }

      // Enable export diagnostics button
      document.getElementById("exportDiagnosticsBtn").disabled = false;

      hideLoader();
      alert(
        `Allotment complete!\n${assignedCount} students assigned to centres.`
      );
      console.log("Allotment complete:", assignments);
    } else {
      hideLoader();
      alert("Error running allotment: " + data.message);
    }
  } catch (error) {
    hideLoader();
    alert("Failed to run allotment. Check backend server.");
    console.error("Error:", error);
  }
}

// ==================== VISUALIZATION ====================

function visualizeAssignments() {
  // Create a map of centre_id to color
  const centreColorMap = {};
  centres.forEach((centre, index) => {
    centreColorMap[centre.centre_id] =
      centreColors[index % centreColors.length];
  });

  // Update student markers based on assignments
  students.forEach((student, index) => {
    const assignedCentreId = assignments[student.student_id];
    const studentDistances = debugDistances[student.student_id] || {}; // Get this student's debug data

    let markerColor = "#6b7280"; // Default: grey (unassigned)
    let popupContent = `
      <strong>${student.student_id}</strong><br>
      Category: ${student.category}<br>
    `;

    if (assignedCentreId) {
      // This student was assigned
      markerColor = centreColorMap[assignedCentreId];
      popupContent += `
        <strong>Status: <span style="color:${markerColor};">Assigned to ${assignedCentreId}</span></strong><br>
        <button onclick="showPath('${student.student_id}', '${assignedCentreId}')">Show Path</button>
      `;
    } else {
      popupContent += "<strong>Status: Unassigned</strong>";
    }

    // --- NEW DEBUG TABLE ---
    let debugTable = `
      <hr style="margin: 5px 0;">
      <strong>Debug Info (Travel Time):</strong>
      <table style="width: 100%; font-size: 0.8em;">
    `;

    centres.forEach((centre, i) => {
      const time = studentDistances[centre.centre_id];
      const color = centreColors[i % centreColors.length];

      let timeText = "N/A"; // Default if not found
      if (time === Infinity || (time && time > 9000000)) {
        // Check for 'infinity'
        timeText = "<strong>Unreachable</strong>";
      } else if (time != null) {
        timeText = `${(time / 60).toFixed(1)} min (${time.toFixed(0)}s)`; // Show minutes and seconds
      }

      debugTable += `
        <tr>
          <td><span class="legend-color" style="background-color:${color}"></span> ${centre.centre_id}</td>
          <td style="text-align: right;">${timeText}</td>
        </tr>
      `;
    });
    debugTable += "</table>";
    popupContent += debugTable;
    // --- END DEBUG TABLE ---

    // Update marker style
    studentMarkers[index].setStyle({
      fillColor: markerColor,
      fillOpacity: 0.9,
      radius: 5,
    });

    // Update popup content
    studentMarkers[index].bindPopup(popupContent);
  });

  updateStats();
}

async function showPath(studentId, centreId) {
  // Find student and centre
  const student = students.find((s) => s.student_id === studentId);
  const centre = centres.find((c) => c.centre_id === centreId);

  if (!student || !centre) {
    alert("Student or centre not found!");
    return;
  }

  showLoader("Finding optimal path...");

  try {
    // We need to pass the snapped node IDs - for simplicity, we'll use a workaround
    // In production, you'd store these after the allotment response
    const response = await fetch(
      `${API_BASE_URL}/get-path?student_lat=${student.lat}&student_lon=${student.lon}&centre_lat=${centre.lat}&centre_lon=${centre.lon}`
    );

    const data = await response.json();

    console.log("Path response:", data);
    console.log("Path length:", data.path ? data.path.length : 0);
    console.log("Path data:", data.path);

    if (data.status === "success" && data.path && data.path.length > 0) {
      // Clear previous paths
      pathLayers.forEach((layer) => map.removeLayer(layer));
      pathLayers = [];

      console.log("Drawing path with", data.path.length, "points");

      // Draw path
      const pathLine = L.polyline(data.path, {
        color: "#f59e0b",
        weight: 4,
        opacity: 0.8,
        dashArray: "10, 10",
      }).addTo(map);

      pathLayers.push(pathLine);

      // Fit map to path
      map.fitBounds(pathLine.getBounds());

      // v3.0: Update timing metrics in sidebar
      if (data.timing) {
        document.getElementById("statAStarTime").textContent =
          data.timing.astar_ms + " ms";

        console.log("📊 v3.0 A* Timing:", data.timing);
      }

      hideLoader();
      alert(`Path found with ${data.path.length} points!`);
      console.log("Path drawn successfully");
    } else {
      hideLoader();
      console.error("No path found. Response:", data);
      alert(
        "Could not find path between student and centre. Check console for details."
      );
    }
  } catch (error) {
    hideLoader();
    alert("Failed to get path. Check backend server.");
    console.error("Error:", error);
  }
}

// ==================== UI HELPERS ====================

function updateStats() {
  document.getElementById("statCentres").textContent = centres.length;
  document.getElementById("statStudents").textContent = students.length;
  document.getElementById("statAssigned").textContent =
    Object.keys(assignments).length;

  // Count students by category
  let pwdCount = 0;
  let femaleCount = 0;
  let maleCount = 0;

  students.forEach((student) => {
    if (student.category === "pwd") {
      pwdCount++;
    } else if (student.category === "female") {
      femaleCount++;
    } else if (student.category === "male") {
      maleCount++;
    }
  });

  document.getElementById("statPwD").textContent = pwdCount;
  document.getElementById("statFemale").textContent = femaleCount;
  document.getElementById("statGeneral").textContent = maleCount;
}

// ==================== LEGEND ====================

function updateLegend() {
  const legendList = document.getElementById("legend-list");
  if (!legendList) return; // Skip if legend element doesn't exist yet

  legendList.innerHTML = ""; // Clear old legend

  centres.forEach((centre, index) => {
    const color = centreColors[index % centreColors.length];

    const div = document.createElement("div");
    div.className = "legend-item";
    div.innerHTML = `
            <div class="legend-color" style="background-color: ${color}; border-color: #333;"></div>
            <span>${centre.centre_id}</span>
        `;
    legendList.appendChild(div);
  });
}

// ==================== LOADER ====================

function showLoader(message) {
  const loader = document.getElementById("loader");
  const loaderText = loader.querySelector(".loader-text");
  loaderText.textContent = message;
  loader.classList.add("active");
}

function hideLoader() {
  const loader = document.getElementById("loader");
  loader.classList.remove("active");
}

// ==================== EXPORT DIAGNOSTICS ====================

async function exportDiagnostics() {
  try {
    showLoader("Generating diagnostic report...");

    const response = await fetch(`${API_BASE_URL}/export-diagnostics`);
    const data = await response.json();

    if (data.status === "error") {
      alert(`Error: ${data.message}`);
      hideLoader();
      return;
    }

    // Create a downloadable JSON file
    const timestamp = new Date().toISOString().replace(/[:.]/g, "-");
    const filename = `allotment_diagnostics_${timestamp}.json`;

    const blob = new Blob([JSON.stringify(data, null, 2)], {
      type: "application/json",
    });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    window.URL.revokeObjectURL(url);

    hideLoader();
    console.log(`✅ Diagnostic report exported: ${filename}`);
  } catch (error) {
    alert(`Export failed: ${error.message}`);
    hideLoader();
    console.error("Export error:", error);
  }
}

// ==================== PARALLEL DIJKSTRA TEST ====================

async function testParallelDijkstra() {
  const btn = document.getElementById("testParallelBtn");
  const resultsDiv = document.getElementById("parallelResults");

  if (!graphBuilt) {
    alert("Please build the graph first (Step 2)");
    return;
  }

  if (centres.length === 0) {
    alert("Please add at least one centre first (Step 1)");
    return;
  }

  btn.disabled = true;
  btn.textContent = "⏳ Running Parallel Dijkstra...";
  resultsDiv.innerHTML =
    '<div style="color: #666;">Computing shortest paths from all centres...</div>';

  try {
    const startTime = performance.now();

    const response = await fetch(`${API_BASE_URL}/parallel-dijkstra`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        workflow_name: "Parallel_Center_Dijkstra_Precomputation",
        workflow_type: "parallel",
        save_to_files: false,
      }),
    });

    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }

    const data = await response.json();
    const endTime = performance.now();
    const clientTime = Math.round(endTime - startTime);

    if (data.status === "success") {
      const timing = data.timing;
      const speedup = timing.speedup || 0;
      const parallelTime = timing.parallel_execution_ms || 0;
      const estimatedSeqTime = timing.estimated_sequential_ms || 0;

      let resultsHTML = `
        <div style="background: #d4edda; border: 1px solid #c3e6cb; border-radius: 8px; padding: 15px; margin-top: 10px;">
          <div style="font-weight: bold; color: #155724; margin-bottom: 10px;">
            ✅ Parallel Dijkstra Completed Successfully!
          </div>
          <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px; font-size: 12px;">
            <div>
              <strong>Centres Processed:</strong> ${data.centres_processed}
            </div>
            <div>
              <strong>Successful:</strong> ${data.successful}/${
        data.centres_processed
      }
            </div>
            <div>
              <strong>⚡ Parallel Time:</strong> <span style="color: #28a745; font-weight: bold;">${parallelTime}ms</span>
            </div>
            <div>
              <strong>Sequential Est.:</strong> ${estimatedSeqTime}ms
            </div>
            <div>
              <strong>🚀 Speedup:</strong> <span style="color: #dc3545; font-weight: bold;">${speedup.toFixed(
                2
              )}x faster</span>
            </div>
            <div>
              <strong>Client Roundtrip:</strong> ${clientTime}ms
            </div>
          </div>
          <div style="margin-top: 10px; padding-top: 10px; border-top: 1px solid #c3e6cb;">
            <strong>Per-Centre Results:</strong>
            <div style="max-height: 150px; overflow-y: auto; margin-top: 5px;">
      `;

      data.results.forEach((result, index) => {
        if (result.success) {
          resultsHTML += `
            <div style="padding: 5px; background: #f8f9fa; margin: 3px 0; border-radius: 4px;">
              <strong>${result.centre_id}:</strong> 
              ${result.computation_time_ms}ms, 
              ${result.reachable_nodes} nodes reachable
            </div>
          `;
        }
      });

      resultsHTML += `
            </div>
          </div>
          <div style="margin-top: 10px; font-size: 11px; color: #666;">
            💡 Tip: Speedup shows how much faster parallel execution is compared to running each centre sequentially.
          </div>
        </div>
      `;

      resultsDiv.innerHTML = resultsHTML;

      console.log("✅ Parallel Dijkstra Test Results:", data);
    } else {
      resultsDiv.innerHTML = `
        <div style="background: #f8d7da; border: 1px solid #f5c6cb; border-radius: 8px; padding: 15px; color: #721c24;">
          ❌ Error: ${data.message || "Unknown error"}
        </div>
      `;
    }
  } catch (error) {
    console.error("Parallel Dijkstra test error:", error);
    resultsDiv.innerHTML = `
      <div style="background: #f8d7da; border: 1px solid #f5c6cb; border-radius: 8px; padding: 15px; color: #721c24;">
        ❌ Network Error: ${error.message}
        <br><small>Make sure the backend server is running on port 8080</small>
      </div>
    `;
  } finally {
    btn.disabled = false;
    btn.textContent = "🚀 Test Parallel Dijkstra";
  }
}

// ==================== INITIALIZE ====================

window.addEventListener("DOMContentLoaded", () => {
  initMap();
  updateStats();
  console.log("Application initialized");
});

// Make functions globally accessible
window.addCentre = addCentre;
window.clearCentres = clearCentres;
window.buildGraph = buildGraph;
window.simulateStudents = simulateStudents;
window.runAllotment = runAllotment;
window.showPath = showPath;
window.exportDiagnostics = exportDiagnostics;
window.testParallelDijkstra = testParallelDijkstra;
