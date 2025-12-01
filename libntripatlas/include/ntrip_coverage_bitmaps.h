/**
 * NTRIP Atlas - Hierarchical Service Coverage Bitmaps
 *
 * Implementation inspired by Brad Fitzpatrick's latlong library approach:
 * - Multi-level hierarchical tiles (5 levels: Continental → Local)
 * - Service coverage encoded as bitmaps per tile
 * - ~15KB total storage vs 1MB+ for polygon coordinates
 * - O(1) lookup with hierarchical fallback
 */

#ifndef NTRIP_COVERAGE_BITMAPS_H
#define NTRIP_COVERAGE_BITMAPS_H

#include <stdint.h>
#include <stdbool.h>

// Coverage tile hierarchy (following Brad Fitzpatrick's approach)
#define COVERAGE_MAX_LEVELS           5
#define COVERAGE_MAX_SERVICES_PER_TILE 32  // 32-bit bitmap limit

// Hierarchical coverage levels (similar to zoom levels)
typedef enum {
    COVERAGE_LEVEL_CONTINENTAL = 0,    // 2×4 tiles globally (8 tiles)
    COVERAGE_LEVEL_REGIONAL    = 1,    // 4×8 tiles globally (32 tiles)
    COVERAGE_LEVEL_NATIONAL    = 2,    // 8×16 tiles globally (128 tiles)
    COVERAGE_LEVEL_STATE       = 3,    // 16×32 tiles globally (512 tiles)
    COVERAGE_LEVEL_LOCAL       = 4     // 32×64 tiles globally (2048 tiles)
} ntrip_coverage_level_t;

/**
 * Service coverage tile (Brad Fitzpatrick bitmap approach)
 * Each tile contains a bitmap of which services cover that geographic area
 */
typedef struct __attribute__((packed)) {
    uint8_t level;              // Coverage level (0-4)
    uint16_t lat_tile;          // Tile latitude index
    uint16_t lon_tile;          // Tile longitude index
    uint32_t service_bitmap;    // Bitmap of services covering this tile (32 services max)
    uint8_t service_count;      // Number of services in this tile (population count)
    uint8_t reserved[2];        // Padding for 12-byte alignment
} ntrip_coverage_tile_t;        // 12 bytes per tile

/**
 * Hierarchical coverage index (stores all tiles compactly)
 */
typedef struct {
    ntrip_coverage_tile_t tiles[2560]; // Max tiles across all levels (2^11)
    uint16_t tile_count;               // Actual tiles used
    uint8_t max_service_index;         // Highest service ID in use
    bool initialized;
} ntrip_coverage_index_t;             // ~30KB total

/**
 * Service coverage area definition (for YAML schema)
 * Used during build-time generation of coverage bitmaps
 */
typedef struct {
    uint8_t service_index;      // Index in service array
    int16_t lat_min_deg100;     // Bounding box (for tile assignment)
    int16_t lat_max_deg100;
    int16_t lon_min_deg100;
    int16_t lon_max_deg100;
    uint8_t coverage_quality;   // 1-5: How well this service covers the area
} ntrip_service_coverage_t;

/**
 * Coverage bitmap API (runtime functions)
 */

/**
 * Initialize hierarchical coverage system
 */
typedef enum {
    NTRIP_COVERAGE_SUCCESS = 0,
    NTRIP_COVERAGE_ERROR_INVALID_LEVEL,
    NTRIP_COVERAGE_ERROR_INVALID_COORDS,
    NTRIP_COVERAGE_ERROR_BITMAP_FULL,
    NTRIP_COVERAGE_ERROR_NOT_INITIALIZED
} ntrip_coverage_error_t;

/**
 * Initialize the hierarchical coverage index
 */
ntrip_coverage_error_t ntrip_coverage_init(ntrip_coverage_index_t* index);

/**
 * Convert geographic coordinates to tile coordinates at specific level
 */
ntrip_coverage_error_t ntrip_coverage_coord_to_tile(
    double latitude,
    double longitude,
    uint8_t level,
    uint16_t* lat_tile,
    uint16_t* lon_tile
);

/**
 * Find services covering a location using hierarchical bitmap lookup
 * Implements Brad Fitzpatrick's hierarchical fallback strategy
 */
size_t ntrip_coverage_find_services(
    const ntrip_coverage_index_t* index,
    double latitude,
    double longitude,
    uint8_t* service_indices,
    size_t max_services
);

/**
 * Add service coverage to hierarchical tiles (build-time function)
 */
ntrip_coverage_error_t ntrip_coverage_add_service(
    ntrip_coverage_index_t* index,
    uint8_t service_index,
    const ntrip_service_coverage_t* coverage
);

/**
 * Get coverage statistics
 */
typedef struct {
    uint16_t tiles_per_level[COVERAGE_MAX_LEVELS];
    uint16_t total_tiles_populated;
    uint8_t services_in_index;
    size_t memory_usage_bytes;
    double coverage_efficiency; // % of tiles with services
} ntrip_coverage_stats_t;

void ntrip_coverage_get_stats(
    const ntrip_coverage_index_t* index,
    ntrip_coverage_stats_t* stats
);

/**
 * Tile coordinate calculations (Brad Fitzpatrick style)
 */

// Get tile dimensions for a given level
static inline void ntrip_coverage_get_level_dimensions(uint8_t level, uint16_t* lat_tiles, uint16_t* lon_tiles) {
    // Hierarchical doubling: Level 0=2×4, Level 1=4×8, Level 2=8×16, etc.
    *lat_tiles = 2 << level;  // 2, 4, 8, 16, 32
    *lon_tiles = 4 << level;  // 4, 8, 16, 32, 64
}

// Calculate total tiles for a level
static inline uint16_t ntrip_coverage_get_level_tile_count(uint8_t level) {
    uint16_t lat_tiles, lon_tiles;
    ntrip_coverage_get_level_dimensions(level, &lat_tiles, &lon_tiles);
    return lat_tiles * lon_tiles;
}

// Get geographic bounds of a tile
ntrip_coverage_error_t ntrip_coverage_tile_to_bounds(
    uint8_t level,
    uint16_t lat_tile,
    uint16_t lon_tile,
    double* lat_min,
    double* lat_max,
    double* lon_min,
    double* lon_max
);

#endif // NTRIP_COVERAGE_BITMAPS_H