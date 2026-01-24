<script>
  import { onMount, onDestroy } from 'svelte';
  import { env as publicEnv } from '$env/dynamic/public';
  
  // Configuration
  const API_URL = publicEnv.PUBLIC_API_URL || "http://localhost:8086/api/v2/query?org=docs"
  const TOKEN = "rCFxXenjRSU3W4CD727ZR0ec583mAb49fQARY1pEkAwed0f3Sk-sQGosiv86lItEuyWNcvpskOP51gahbgpLcg==";
  const REFRESH_INTERVAL = Number(publicEnv.PUBLIC_REFRESH_INTERVAL || 1000);
  const MAX_DATA_POINTS = Number(publicEnv.PUBLIC_MAX_DATA_POINTS || 50);

  
  // Port colors
  const PORT_COLORS = {
    '3000': { line: '#3b82f6', gradient: 'url(#gradient3000)', name: 'Port 3000' },
    '3001': { line: '#10b981', gradient: 'url(#gradient3001)', name: 'Port 3001' },
    '3002': { line: '#f59e0b', gradient: 'url(#gradient3002)', name: 'Port 3002' },
  };
  
  // State
  let serverData = {};
  let ports = [];
  let loading = true;
  let error = null;
  let lastUpdate = null;
  let refreshInterval;
  let hoveredRpsPoint = null;
  let hoveredCpuPoint = null;
  let mousePos = { x: 0, y: 0 };
  
  // Date filter state
  let selectedDate = new Date().toISOString().split('T')[0]; // Today's date in YYYY-MM-DD format
  let isRealtimeMode = true;
  let maxDataPointsForHistorical = 1440; // For 24 hours with minute-level data
  
  // Build Flux query based on mode
  function buildFluxQuery() {
    if (isRealtimeMode) {
      return 'from(bucket: "prescal") |> range(start: -1h) |> filter(fn: (r) => r._measurement == "server") |> pivot(rowKey:["_time"], columnKey:["_field"], valueColumn:"_value")';
    } else {
      // For historical data, query full 24 hours of selected date
      const startTime = `${selectedDate}T00:00:00Z`;
      const endTime = `${selectedDate}T23:59:59Z`;
      return `from(bucket: "prescal") |> range(start: ${startTime}, stop: ${endTime}) |> filter(fn: (r) => r._measurement == "server") |> pivot(rowKey:["_time"], columnKey:["_field"], valueColumn:"_value")`;
    }
  }
  
  // Parse CSV response from InfluxDB v2
  function parseCSV(csvText) {
    try {
      if (!csvText || typeof csvText !== 'string') {
        console.error('Invalid CSV text received');
        return [];
      }

      const lines = csvText.trim().split('\n');
      const data = [];
      
      // Skip the first line (header)
      for (let i = 1; i < lines.length; i++) {
        const line = lines[i].trim();
        if (!line) continue;
        
        const columns = line.split(',');
        
        // CSV format: ,result,table,_start,_stop,_time,_measurement,port,cpu,rps
        // Index:       0  1      2     3      4     5     6            7    8   9
        if (columns.length >= 10) {
          const time = columns[5];
          const port = columns[7];
          const cpu = columns[8];
          const rps = columns[9];
          
          // Check all required values exist
          if (time && port && cpu !== undefined && rps !== undefined) {
            const item = {
              time: time,
              port: port,
              cpu: cpu,
              rps: rps
            };
            data.push(item);
          }
        }
      }
      
      return data;
    } catch (err) {
      console.error('Error in parseCSV:', err);
      return [];
    }
  }
  
  // Fetch data from API
  async function fetchServerData() {
    try {
      const response = await fetch(API_URL, {
        method: 'POST',
        headers: {
          'Authorization': `Token ${TOKEN}`,
          'Content-Type': 'application/json',
          'Accept': 'application/csv'
        },
        body: JSON.stringify({
          query: buildFluxQuery()
        })
      });
      
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      
      const csvText = await response.text();
      const data = parseCSV(csvText);
      processData(data);
      error = null;
      lastUpdate = new Date();
    } catch (err) {
      error = err.message;
      console.error('Error fetching data:', err);
    } finally {
      loading = false;
    }
  }
  
  // Process and organize data by port
  function processData(data) {
    const newServerData = {};
    
    if (data && Array.isArray(data)) {
      data.forEach(item => {
        const port = item.port;
        const timestamp = new Date(item.time);
        const rps = parseFloat(item.rps) || 0;
        const cpu = parseFloat(item.cpu) || 0;
        
        if (!newServerData[port]) {
          newServerData[port] = {
            rps: [],
            cpu: [],
            timestamps: []
          };
        }
        
        newServerData[port].timestamps.push(timestamp);
        newServerData[port].rps.push(rps);
        newServerData[port].cpu.push(cpu);
      });
      
      // Sort by timestamp and limit data points
      Object.keys(newServerData).forEach(port => {
        const portData = newServerData[port];
        const indices = portData.timestamps
          .map((ts, idx) => ({ ts, idx }))
          .sort((a, b) => a.ts - b.ts);
        
        // Only limit data points in realtime mode
        const limitedIndices = isRealtimeMode 
          ? indices.slice(-MAX_DATA_POINTS).map(item => item.idx)
          : indices.map(item => item.idx);
        
        portData.timestamps = limitedIndices.map(i => portData.timestamps[i]);
        portData.rps = limitedIndices.map(i => portData.rps[i]);
        portData.cpu = limitedIndices.map(i => portData.cpu[i]);
      });
      
      serverData = newServerData;
      ports = Object.keys(newServerData).sort();
    }
  }
  
  // Format timestamp for display
  function formatTime(date) {
    return date.toLocaleTimeString('en-US', { 
      hour12: false, 
      hour: '2-digit', 
      minute: '2-digit', 
      second: '2-digit' 
    });
  }
  
  // Get current stats for a port
  function getCurrentStats(port) {
    const data = serverData[port];
    if (!data || data.rps.length === 0) {
      return { rps: 0, cpu: 0, avgRps: 0, avgCpu: 0 };
    }
    
    const latest = data.rps.length - 1;
    const avgRps = data.rps.reduce((a, b) => a + b, 0) / data.rps.length;
    const avgCpu = data.cpu.reduce((a, b) => a + b, 0) / data.cpu.length;
    
    return {
      rps: data.rps[latest],
      cpu: data.cpu[latest],
      avgRps: avgRps.toFixed(2),
      avgCpu: avgCpu.toFixed(2)
    };
  }
  
  // Get max value across all ports for a metric
  function getMaxValue(metric) {
    let max = 1;
    ports.forEach(port => {
      if (serverData[port] && serverData[port][metric]) {
        const portMax = Math.max(...serverData[port][metric]);
        if (portMax > max) max = portMax;
      }
    });
    return max;
  }
  
  // Handle mouse move on chart
  function handleMouseMove(event, chartType) {
    const rect = event.currentTarget.getBoundingClientRect();
    const x = event.clientX - rect.left;
    const y = event.clientY - rect.top;
    
    mousePos = { x: event.clientX, y: event.clientY };
    
    // Calculate which data point is being hovered
    const chartWidth = rect.width;
    const dataLength = Math.max(...ports.map(p => serverData[p]?.[chartType]?.length || 0));
    
    if (dataLength > 1) {
      const pointIndex = Math.round((x / chartWidth) * (dataLength - 1));
      
      if (pointIndex >= 0 && pointIndex < dataLength) {
        const hoverData = {};
        ports.forEach(port => {
          if (serverData[port] && serverData[port][chartType] && serverData[port][chartType][pointIndex] !== undefined) {
            hoverData[port] = {
              value: serverData[port][chartType][pointIndex],
              timestamp: serverData[port].timestamps[pointIndex]
            };
          }
        });
        
        if (chartType === 'rps') {
          hoveredRpsPoint = hoverData;
        } else {
          hoveredCpuPoint = hoverData;
        }
      }
    }
  }
  
  function handleMouseLeave(chartType) {
    if (chartType === 'rps') {
      hoveredRpsPoint = null;
    } else {
      hoveredCpuPoint = null;
    }
  }
  
  // Handle date change
  function handleDateChange(event) {
    selectedDate = event.target.value;
    isRealtimeMode = false;
    
    // Stop realtime updates when viewing historical data
    if (refreshInterval) {
      clearInterval(refreshInterval);
      refreshInterval = null;
    }
    
    // Fetch historical data
    loading = true;
    fetchServerData();
  }
  
  // Switch to realtime mode
  function switchToRealtime() {
    isRealtimeMode = true;
    selectedDate = new Date().toISOString().split('T')[0];
    
    // Restart realtime updates
    if (!refreshInterval) {
      fetchServerData();
      refreshInterval = setInterval(fetchServerData, REFRESH_INTERVAL);
    }
  }
  
  // Lifecycle
  onMount(() => {
    fetchServerData();
    refreshInterval = setInterval(fetchServerData, REFRESH_INTERVAL);
  });
  
  onDestroy(() => {
    if (refreshInterval) {
      clearInterval(refreshInterval);
    }
  });
</script>

<div class="min-h-screen bg-gray-900 text-gray-100 p-6">
  <div class="max-w-7xl mx-auto">
    <!-- Header -->
    <div class="mb-8">
      <h1 class="text-4xl font-bold mb-2">Server Monitoring Dashboard</h1>
      <div class="flex items-center gap-4 text-sm text-gray-400 flex-wrap">
        <span>Last updated: {lastUpdate ? formatTime(lastUpdate) : 'Never'}</span>
        {#if loading && !lastUpdate}
          <span class="flex items-center gap-2">
            <div class="w-4 h-4 border-2 border-blue-500 border-t-transparent rounded-full animate-spin"></div>
            Loading...
          </span>
        {/if}
        {#if error}
          <span class="text-red-400">⚠ Error: {error}</span>
        {/if}
      </div>
      
      <!-- Date Filter -->
      <div class="mt-4 flex items-center gap-4 flex-wrap">
        <div class="flex items-center gap-2">
          <label for="date-filter" class="text-sm text-gray-400">Filter by date:</label>
          <input 
            id="date-filter"
            type="date" 
            value={selectedDate}
            max={new Date().toISOString().split('T')[0]}
            on:change={handleDateChange}
            class="bg-gray-800 border border-gray-600 rounded px-3 py-2 text-sm text-gray-100 focus:outline-none focus:border-blue-500"
          />
        </div>
        
        {#if !isRealtimeMode}
          <button 
            on:click={switchToRealtime}
            class="bg-blue-600 hover:bg-blue-700 text-white px-4 py-2 rounded text-sm transition-colors flex items-center gap-2"
          >
            <div class="w-2 h-2 bg-white rounded-full animate-pulse"></div>
            Switch to Realtime
          </button>
          <span class="text-sm text-yellow-400">Viewing historical data (24 hours)</span>
        {:else}
          <span class="text-sm text-green-400 flex items-center gap-2">
            <div class="w-2 h-2 bg-green-500 rounded-full animate-pulse"></div>
            Live Mode
          </span>
        {/if}
      </div>
    </div>
    
    {#if ports.length === 0 && !loading}
      <div class="bg-gray-800 rounded-lg p-8 text-center">
        <p class="text-gray-400">No server data available</p>
      </div>
    {:else}
      <!-- Stats Grid -->
      <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4 mb-8">
        {#each ports as port}
          {@const stats = getCurrentStats(port)}
          {@const color = PORT_COLORS[port] || PORT_COLORS['3000']}
          <div class="bg-gray-800 rounded-lg p-6 border border-gray-700">
            <div class="flex items-center justify-between mb-4">
              <div class="flex items-center gap-2">
                <div class="w-3 h-3 rounded-full" style="background-color: {color.line}"></div>
                <h3 class="text-lg font-semibold">Port {port}</h3>
              </div>
              <div class="w-3 h-3 bg-green-500 rounded-full animate-pulse"></div>
            </div>
            <div class="grid grid-cols-2 gap-4">
              <div>
                <p class="text-gray-400 text-sm">Current RPS</p>
                <p class="text-2xl font-bold" style="color: {color.line}">{stats.rps}</p>
                <p class="text-xs text-gray-500">Avg: {stats.avgRps}</p>
              </div>
              <div>
                <p class="text-gray-400 text-sm">Current CPU %</p>
                <p class="text-2xl font-bold" style="color: {color.line}">{stats.cpu.toFixed(2)}</p>
                <p class="text-xs text-gray-500">Avg: {stats.avgCpu}</p>
              </div>
            </div>
          </div>
        {/each}
      </div>
      
      <!-- Combined Charts -->
      <div class="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <!-- RPS Chart -->
        <div class="bg-gray-800 rounded-lg p-6 border border-gray-700">
          <div class="mb-4">
            <h2 class="text-xl font-bold mb-2">Requests Per Second (RPS)</h2>
            <div class="flex gap-3 flex-wrap">
              {#each ports as port}
                {@const color = PORT_COLORS[port] || PORT_COLORS['3000']}
                <div class="flex items-center gap-2">
                  <div class="w-3 h-3 rounded-full" style="background-color: {color.line}"></div>
                  <span class="text-xs text-gray-400">Port {port}</span>
                </div>
              {/each}
            </div>
          </div>
          
          <div class="relative h-80 bg-gray-900 rounded-lg p-4">
            {#if ports.length > 0}
              {@const maxRps = getMaxValue('rps')}
              {@const maxDataPoints = Math.max(...ports.map(p => serverData[p]?.rps?.length || 0))}
              
              <svg 
                class="w-full h-full cursor-crosshair" 
                viewBox="0 0 800 250" 
                preserveAspectRatio="none"
                on:mousemove={(e) => handleMouseMove(e, 'rps')}
                on:mouseleave={() => handleMouseLeave('rps')}
              >
                <!-- Grid lines -->
                {#each Array(6) as _, i}
                  <line 
                    x1="0" 
                    y1={i * 50} 
                    x2="800" 
                    y2={i * 50} 
                    stroke="#374151" 
                    stroke-width="1"
                    stroke-dasharray="5,5"
                  />
                {/each}
                
                <!-- Draw lines for each port -->
                {#each ports as port}
                  {@const color = PORT_COLORS[port] || PORT_COLORS['3000']}
                  {#if serverData[port] && serverData[port].rps.length > 1}
                    {@const points = serverData[port].rps.map((rps, i) => {
                      const x = (i / (serverData[port].rps.length - 1)) * 800;
                      const y = 250 - (rps / maxRps) * 230;
                      return `${x},${y}`;
                    }).join(' ')}
                    
                    <!-- Area fill -->
                    <polygon 
                      points={`0,250 ${points} 800,250`}
                      fill={color.gradient}
                      opacity="0.2"
                    />
                    
                    <!-- Line -->
                    <polyline 
                      points={points}
                      fill="none"
                      stroke={color.line}
                      stroke-width="3"
                      stroke-linecap="round"
                      stroke-linejoin="round"
                    />
                    
                    <!-- Data points -->
                    {#each serverData[port].rps as rps, i}
                      {@const x = (i / (serverData[port].rps.length - 1)) * 800}
                      {@const y = 250 - (rps / maxRps) * 230}
                      <circle cx={x} cy={y} r="4" fill={color.line} opacity="0.7" />
                    {/each}
                  {/if}
                {/each}
                
                <!-- Gradients -->
                <defs>
                  <linearGradient id="gradient3000" x1="0%" y1="0%" x2="0%" y2="100%">
                    <stop offset="0%" style="stop-color:#3b82f6;stop-opacity:0.8" />
                    <stop offset="100%" style="stop-color:#3b82f6;stop-opacity:0" />
                  </linearGradient>
                  <linearGradient id="gradient3001" x1="0%" y1="0%" x2="0%" y2="100%">
                    <stop offset="0%" style="stop-color:#10b981;stop-opacity:0.8" />
                    <stop offset="100%" style="stop-color:#10b981;stop-opacity:0" />
                  </linearGradient>
                  <linearGradient id="gradient3002" x1="0%" y1="0%" x2="0%" y2="100%">
                    <stop offset="0%" style="stop-color:#f59e0b;stop-opacity:0.8" />
                    <stop offset="100%" style="stop-color:#f59e0b;stop-opacity:0" />
                  </linearGradient>
                </defs>
              </svg>
              
              <!-- Y-axis labels -->
              <div class="absolute left-0 top-0 h-full flex flex-col justify-between text-xs text-gray-500 pr-2">
                {#each Array(6) as _, i}
                  <span>{(maxRps * (1 - i / 5)).toFixed(0)}</span>
                {/each}
              </div>
              
              <!-- Hover tooltip -->
              {#if hoveredRpsPoint && Object.keys(hoveredRpsPoint).length > 0}
                <div 
                  class="fixed bg-gray-800 border border-gray-600 rounded-lg p-3 shadow-xl z-50 pointer-events-none"
                  style="left: {mousePos.x + 15}px; top: {mousePos.y + 15}px;"
                >
                  {#each Object.entries(hoveredRpsPoint) as [port, data]}
                    {@const color = PORT_COLORS[port] || PORT_COLORS['3000']}
                    <div class="flex items-center gap-2 mb-1">
                      <div class="w-2 h-2 rounded-full" style="background-color: {color.line}"></div>
                      <span class="text-xs text-gray-300">Port {port}:</span>
                      <span class="text-xs font-bold" style="color: {color.line}">{data.value.toFixed(2)} RPS</span>
                    </div>
                  {/each}
                  <div class="text-xs text-gray-500 mt-2 pt-2 border-t border-gray-700">
                    {formatTime(Object.values(hoveredRpsPoint)[0].timestamp)}
                  </div>
                </div>
              {/if}
            {:else}
              <div class="flex items-center justify-center h-full text-gray-500">
                No data available
              </div>
            {/if}
          </div>
        </div>
        
        <!-- CPU Chart -->
        <div class="bg-gray-800 rounded-lg p-6 border border-gray-700">
          <div class="mb-4">
            <h2 class="text-xl font-bold mb-2">CPU Usage (%)</h2>
            <div class="flex gap-3 flex-wrap">
              {#each ports as port}
                {@const color = PORT_COLORS[port] || PORT_COLORS['3000']}
                <div class="flex items-center gap-2">
                  <div class="w-3 h-3 rounded-full" style="background-color: {color.line}"></div>
                  <span class="text-xs text-gray-400">Port {port}</span>
                </div>
              {/each}
            </div>
          </div>
          
          <div class="relative h-80 bg-gray-900 rounded-lg p-4">
            {#if ports.length > 0}
              {@const maxCpu = getMaxValue('cpu')}
              {@const maxDataPoints = Math.max(...ports.map(p => serverData[p]?.cpu?.length || 0))}
              
              <svg 
                class="w-full h-full cursor-crosshair" 
                viewBox="0 0 800 250" 
                preserveAspectRatio="none"
                on:mousemove={(e) => handleMouseMove(e, 'cpu')}
                on:mouseleave={() => handleMouseLeave('cpu')}
              >
                <!-- Grid lines -->
                {#each Array(6) as _, i}
                  <line 
                    x1="0" 
                    y1={i * 50} 
                    x2="800" 
                    y2={i * 50} 
                    stroke="#374151" 
                    stroke-width="1"
                    stroke-dasharray="5,5"
                  />
                {/each}
                
                <!-- Draw lines for each port -->
                {#each ports as port}
                  {@const color = PORT_COLORS[port] || PORT_COLORS['3000']}
                  {#if serverData[port] && serverData[port].cpu.length > 1}
                    {@const points = serverData[port].cpu.map((cpu, i) => {
                      const x = (i / (serverData[port].cpu.length - 1)) * 800;
                      const y = 250 - (cpu / maxCpu) * 230;
                      return `${x},${y}`;
                    }).join(' ')}
                    
                    <!-- Area fill -->
                    <polygon 
                      points={`0,250 ${points} 800,250`}
                      fill={color.gradient}
                      opacity="0.2"
                    />
                    
                    <!-- Line -->
                    <polyline 
                      points={points}
                      fill="none"
                      stroke={color.line}
                      stroke-width="3"
                      stroke-linecap="round"
                      stroke-linejoin="round"
                    />
                    
                    <!-- Data points -->
                    {#each serverData[port].cpu as cpu, i}
                      {@const x = (i / (serverData[port].cpu.length - 1)) * 800}
                      {@const y = 250 - (cpu / maxCpu) * 230}
                      <circle cx={x} cy={y} r="4" fill={color.line} opacity="0.7" />
                    {/each}
                  {/if}
                {/each}
                
                <!-- Gradients (reuse from RPS) -->
                <defs>
                  <linearGradient id="gradient3000" x1="0%" y1="0%" x2="0%" y2="100%">
                    <stop offset="0%" style="stop-color:#3b82f6;stop-opacity:0.8" />
                    <stop offset="100%" style="stop-color:#3b82f6;stop-opacity:0" />
                  </linearGradient>
                  <linearGradient id="gradient3001" x1="0%" y1="0%" x2="0%" y2="100%">
                    <stop offset="0%" style="stop-color:#10b981;stop-opacity:0.8" />
                    <stop offset="100%" style="stop-color:#10b981;stop-opacity:0" />
                  </linearGradient>
                  <linearGradient id="gradient3002" x1="0%" y1="0%" x2="0%" y2="100%">
                    <stop offset="0%" style="stop-color:#f59e0b;stop-opacity:0.8" />
                    <stop offset="100%" style="stop-color:#f59e0b;stop-opacity:0" />
                  </linearGradient>
                </defs>
              </svg>
              
              <!-- Y-axis labels -->
              <div class="absolute left-0 top-0 h-full flex flex-col justify-between text-xs text-gray-500 pr-2">
                {#each Array(6) as _, i}
                  <span>{(maxCpu * (1 - i / 5)).toFixed(1)}</span>
                {/each}
              </div>
              
              <!-- Hover tooltip -->
              {#if hoveredCpuPoint && Object.keys(hoveredCpuPoint).length > 0}
                <div 
                  class="fixed bg-gray-800 border border-gray-600 rounded-lg p-3 shadow-xl z-50 pointer-events-none"
                  style="left: {mousePos.x + 15}px; top: {mousePos.y + 15}px;"
                >
                  {#each Object.entries(hoveredCpuPoint) as [port, data]}
                    {@const color = PORT_COLORS[port] || PORT_COLORS['3000']}
                    <div class="flex items-center gap-2 mb-1">
                      <div class="w-2 h-2 rounded-full" style="background-color: {color.line}"></div>
                      <span class="text-xs text-gray-300">Port {port}:</span>
                      <span class="text-xs font-bold" style="color: {color.line}">{data.value.toFixed(2)}%</span>
                    </div>
                  {/each}
                  <div class="text-xs text-gray-500 mt-2 pt-2 border-t border-gray-700">
                    {formatTime(Object.values(hoveredCpuPoint)[0].timestamp)}
                  </div>
                </div>
              {/if}
            {:else}
              <div class="flex items-center justify-center h-full text-gray-500">
                No data available
              </div>
            {/if}
          </div>
        </div>
      </div>
    {/if}
  </div>
</div>

<style>
  :global(body) {
    margin: 0;
    padding: 0;
  }
</style>
