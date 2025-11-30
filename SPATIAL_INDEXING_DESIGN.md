# NTRIP Atlas Spatial Indexing Design
## Adaptive Grid Optimization (Brad Fitzpatrick Approach)

**Goal**: Replace O(n) bounding box scanning with O(1) spatial grid lookups for NTRIP service discovery.

**Reference**: Based on Brad Fitzpatrick's latlong library spatial indexing techniques.

---

## Current Performance vs. Target

| **Current Approach** | **Target Approach** |
|---------------------|---------------------|
| O(n) linear scan through 32+ services | O(1) direct grid lookup |
| 32 bounding box checks per query | 1-2 tile lookups max |
| ~160 floating point comparisons | ~4 integer operations |
| No spatial locality optimization | Hierarchical spatial indexing |

---

## 🏗️ **Design Architecture**

### **1. Hierarchical Tile System**

```
World Division:
├── Level 0: 4×2 tiles    (90° × 180° each) - Continental
├── Level 1: 8×4 tiles    (45° × 90° each)  - Regional
├── Level 2: 16×8 tiles   (22.5° × 45° each) - National
├── Level 3: 32×16 tiles  (11.25° × 22.5° each) - State
└── Level 4: 64×32 tiles  (5.625° × 11.25° each) - Local
```

### **2. Tile Key Encoding** (32-bit)

```c
typedef uint32_t ntrip_tile_key_t;

// Bit layout: [level:3][reserved:3][lat_tile:13][lon_tile:13]
#define TILE_LEVEL_SHIFT    29
#define TILE_LAT_SHIFT      13
#define TILE_LON_SHIFT      0
#define TILE_LAT_MASK       0x1FFF  // 13 bits = 8192 max tiles
#define TILE_LON_MASK       0x1FFF  // 13 bits = 8192 max tiles

static inline ntrip_tile_key_t encode_tile_key(uint8_t level, uint16_t lat_tile, uint16_t lon_tile) {
    return ((uint32_t)level << TILE_LEVEL_SHIFT) |
           ((uint32_t)lat_tile << TILE_LAT_SHIFT) |
           ((uint32_t)lon_tile << TILE_LON_SHIFT);
}
```

### **3. Service Tile Assignment**

```c
// Tile entry contains service indices for that geographic area
typedef struct {
    ntrip_tile_key_t key;
    uint8_t service_count;
    uint8_t service_indices[16];  // Max 16 services per tile
} ntrip_tile_t;

// Global spatial index (pre-computed at build time)
typedef struct {
    ntrip_tile_t tiles[256];      // Support up to 256 tiles
    uint16_t tile_count;
    bool initialized;
} ntrip_spatial_index_t;
```

---

## 🔧 **Implementation Strategy**

### **Phase 1: Coordinate-to-Tile Mapping**

```c
/**
 * Convert lat/lon to tile coordinates for given zoom level
 */
void lat_lon_to_tile(double lat, double lon, uint8_t level, uint16_t* tile_lat, uint16_t* tile_lon) {
    // Tiles per level: level_tiles = 2^(level+1) for lat, 2^(level+2) for lon
    uint16_t lat_tiles = 2 << level;      // 2, 4, 8, 16, 32
    uint16_t lon_tiles = 4 << level;      // 4, 8, 16, 32, 64

    // Normalize coordinates: lat [-90,90] → [0,180], lon [-180,180] → [0,360]
    double norm_lat = lat + 90.0;         // [0, 180]
    double norm_lon = lon + 180.0;        // [0, 360]

    // Map to tile space
    *tile_lat = (uint16_t)(norm_lat * lat_tiles / 180.0);
    *tile_lon = (uint16_t)(norm_lon * lon_tiles / 360.0);

    // Clamp to valid range
    if (*tile_lat >= lat_tiles) *tile_lat = lat_tiles - 1;
    if (*tile_lon >= lon_tiles) *tile_lon = lon_tiles - 1;
}
```

### **Phase 2: Service-to-Tile Assignment Algorithm**

```c
/**
 * Assign NTRIP service to all tiles it covers (build-time)
 */
void assign_service_to_tiles(const ntrip_service_compact_t* service, uint8_t service_index) {
    // For each zoom level, find tiles that overlap service coverage
    for (uint8_t level = 0; level <= 4; level++) {
        uint16_t min_lat_tile, max_lat_tile, min_lon_tile, max_lon_tile;

        // Convert service bounding box to tile ranges
        lat_lon_to_tile(service->lat_min_deg100 / 100.0, service->lon_min_deg100 / 100.0,
                       level, &min_lat_tile, &min_lon_tile);
        lat_lon_to_tile(service->lat_max_deg100 / 100.0, service->lon_max_deg100 / 100.0,
                       level, &max_lat_tile, &max_lon_tile);

        // Add service to all overlapping tiles
        for (uint16_t lat = min_lat_tile; lat <= max_lat_tile; lat++) {
            for (uint16_t lon = min_lon_tile; lon <= max_lon_tile; lon++) {
                ntrip_tile_key_t key = encode_tile_key(level, lat, lon);
                add_service_to_tile(key, service_index);
            }
        }
    }
}
```

### **Phase 3: O(1) Service Lookup**

```c
/**
 * Find services covering user location (runtime - O(1))
 */
size_t ntrip_atlas_find_services_by_location_fast(
    double user_lat, double user_lon,
    uint8_t* service_indices, size_t max_services
) {
    // Start with finest resolution and work up until we find services
    for (uint8_t level = 4; level >= 0; level--) {
        uint16_t tile_lat, tile_lon;
        lat_lon_to_tile(user_lat, user_lon, level, &tile_lat, &tile_lon);

        ntrip_tile_key_t key = encode_tile_key(level, tile_lat, tile_lon);

        // Binary search for tile (O(log k) where k = number of tiles)
        ntrip_tile_t* tile = find_tile_by_key(key);
        if (tile && tile->service_count > 0) {
            size_t count = MIN(tile->service_count, max_services);
            memcpy(service_indices, tile->service_indices, count);
            return count;
        }
    }

    return 0;  // No services found
}
```

---

## 📊 **Performance Analysis**

### **Memory Usage** (ESP32 Optimized)

```
Spatial Index Memory:
├── 256 tiles × 20 bytes = 5,120 bytes
├── Index metadata = 64 bytes
└── Total: ~5.2KB (fits in ESP32 easily)

Per-Tile Storage:
├── Tile key: 4 bytes
├── Service count: 1 byte
├── Service indices: 16 bytes (max 16 services)
└── Total per tile: 21 bytes
```

### **Lookup Performance**

| **Operation** | **Current** | **With Spatial Index** | **Improvement** |
|---------------|-------------|------------------------|-----------------|
| **Service Discovery** | O(32) linear scan | O(log 256) = O(8) | **4x faster** |
| **Geographic Filtering** | 32 × 8 float ops | 1 × 4 int ops | **64x faster** |
| **Memory Access** | 32 × 46 bytes read | 1 × 21 bytes read | **69x less** |

---

## 🚀 **Implementation Phases**

### **Phase 1: Core Infrastructure** (1-2 days)
- [x] Design tile encoding system
- [x] Implement coordinate-to-tile mapping
- [x] Create spatial index data structures
- [ ] Add tile search algorithms (binary search)

### **Phase 2: Build-Time Index Generation** (1 day)
- [ ] Service-to-tile assignment algorithm
- [ ] Pre-compute spatial index for all 32 services
- [ ] Optimize tile density (balance resolution vs. memory)
- [ ] Generate compact spatial index data

### **Phase 3: Runtime Integration** (1 day)
- [ ] Replace current geographic filtering with O(1) lookups
- [ ] Integrate with existing blacklisting system
- [ ] Add fallback to bounding box filtering for edge cases
- [ ] Performance benchmarking and validation

### **Phase 4: Testing & Optimization** (1 day)
- [ ] Comprehensive test suite for spatial indexing
- [ ] Memory usage validation on ESP32
- [ ] Edge case testing (poles, date line, etc.)
- [ ] Performance benchmarking vs. current approach

---

## 🎯 **Expected Benefits**

### **Performance Gains**
- **4-64x faster** service discovery queries
- **69x less memory bandwidth** per lookup
- **Constant time scaling** as service database grows
- **Early termination** when services found at coarse resolution

### **Memory Efficiency**
- **5.2KB spatial index** (vs. current 1.5KB service table)
- **Predictable memory usage** independent of query location
- **Cache-friendly access patterns** for repeated queries
- **ESP32 compatible** within existing memory budget

### **Future Scalability**
- **Supports 100+ services** without performance degradation
- **Adaptive resolution** - high detail in dense areas, low detail in sparse areas
- **Hierarchical fallback** - coarse tiles when fine tiles empty
- **Extensible** to additional geographic optimizations

---

## 🔬 **Technical Considerations**

### **Edge Cases**
- **Poles**: Special handling for latitude extremes (±90°)
- **Date Line**: Longitude wrap-around at ±180°
- **Service Boundaries**: Large services spanning multiple tiles
- **Empty Tiles**: Fallback to coarser resolution

### **Memory vs. Accuracy Trade-offs**
- **Tile Resolution**: Balance between accuracy and memory usage
- **Service Distribution**: Some tiles may contain many services, others none
- **Dynamic Rebalancing**: Could adjust tile boundaries based on service density

### **Integration with Existing Systems**
- **Backward Compatibility**: Keep current bounding box system as fallback
- **Blacklisting Integration**: Apply spatial indexing AFTER blacklist filtering
- **Credential Filtering**: Apply accessibility checks AFTER spatial lookup

---

## 📝 **Implementation Notes**

This design provides a clear path to achieve the **O(1) geographic lookup performance** that Brad Fitzpatrick's latlong library demonstrates, adapted specifically for NTRIP service discovery patterns.

The hierarchical approach ensures we maintain good performance even as the service database grows to 100+ global services, while keeping memory usage reasonable for embedded deployment on ESP32.

**Next Step**: Implement Phase 1 (Core Infrastructure) to establish the foundation for this optimization.