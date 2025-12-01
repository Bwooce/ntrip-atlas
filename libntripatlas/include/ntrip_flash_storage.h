/**
 * NTRIP Atlas - ESP32 Flash Storage Strategy
 *
 * Implements memory-mapped flash storage for multi-MB service databases
 * with polygon coverage areas. Designed for ESP32-S3 with 8MB flash.
 *
 * Flash Layout:
 * ┌─────────────────────────────────┐
 * │ Service Database (2MB)          │ ← ntrip_service_compact_t array
 * ├─────────────────────────────────┤
 * │ Polygon Storage (1MB)           │ ← Variable-length coordinate arrays
 * ├─────────────────────────────────┤
 * │ Spatial Index (20KB)            │ ← Tile-based O(1) lookups
 * ├─────────────────────────────────┤
 * │ Provider Metadata (10KB)        │ ← Provider names and info
 * └─────────────────────────────────┘
 */

#ifndef NTRIP_FLASH_STORAGE_H
#define NTRIP_FLASH_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

// Flash partition addresses (ESP32-S3 specific)
#define NTRIP_FLASH_PARTITION_BASE    0x3F400000
#define NTRIP_FLASH_PARTITION_SIZE    (4 * 1024 * 1024)  // 4MB partition

// Flash storage layout offsets
#define NTRIP_SERVICES_OFFSET         0x0000000           // Base address
#define NTRIP_POLYGONS_OFFSET         0x200000            // +2MB
#define NTRIP_SPATIAL_OFFSET          0x300000            // +3MB
#define NTRIP_METADATA_OFFSET         0x305000            // +3MB + 20KB

// Memory-mapped flash addresses
#define NTRIP_SERVICES_FLASH_ADDR     (NTRIP_FLASH_PARTITION_BASE + NTRIP_SERVICES_OFFSET)
#define NTRIP_POLYGONS_FLASH_ADDR     (NTRIP_FLASH_PARTITION_BASE + NTRIP_POLYGONS_OFFSET)
#define NTRIP_SPATIAL_FLASH_ADDR      (NTRIP_FLASH_PARTITION_BASE + NTRIP_SPATIAL_OFFSET)
#define NTRIP_METADATA_FLASH_ADDR     (NTRIP_FLASH_PARTITION_BASE + NTRIP_METADATA_OFFSET)

/**
 * Geographic coordinate pair for polygon boundaries
 * Stored in flash memory as variable-length arrays
 */
typedef struct __attribute__((packed)) {
    int16_t lat_deg1000;        // Latitude in 0.001 degree precision
    int16_t lon_deg1000;        // Longitude in 0.001 degree precision
} ntrip_coord_pair_t;           // 4 bytes per coordinate pair

/**
 * Enhanced service structure with polygon support
 * Now 52 bytes instead of 46, but supports precise coverage areas
 */
typedef struct __attribute__((packed)) {
    // Connection info (35 bytes)
    char hostname[32];          // Most NTRIP hostnames are short
    uint16_t port;              // TCP port
    uint8_t flags;              // Packed boolean flags

    // Geographic coverage - bounding box (8 bytes - for fast O(1) filtering)
    int16_t lat_min_deg100;     // Coverage bounds: -90.00 to 90.00 × 100
    int16_t lat_max_deg100;
    int16_t lon_min_deg100;     // Coverage bounds: -180.00 to 180.00 × 100
    int16_t lon_max_deg100;

    // Polygon coverage (6 bytes - for precise boundaries)
    uint32_t polygon_offset;    // Byte offset in flash polygon storage
    uint8_t polygon_point_count; // Number of coordinate pairs (0 = no polygon)
    uint8_t reserved;           // Padding for alignment

    // Service metadata (3 bytes)
    uint8_t provider_index;     // Index into shared provider string table
    uint8_t network_type;       // government/commercial/community
    uint8_t quality_rating;     // 1-5 star rating
} ntrip_service_compact_t;      // Total: 52 bytes

/**
 * Flash storage statistics and management
 */
typedef struct {
    uint32_t total_services;
    uint32_t services_with_polygons;
    uint32_t polygon_storage_used;
    uint32_t polygon_storage_free;
    uint32_t spatial_tiles_populated;
    bool flash_initialized;
} ntrip_flash_stats_t;

/**
 * Initialize ESP32 flash storage for NTRIP service database
 */
typedef enum {
    NTRIP_FLASH_SUCCESS = 0,
    NTRIP_FLASH_ERROR_PARTITION_NOT_FOUND,
    NTRIP_FLASH_ERROR_MEMORY_MAP_FAILED,
    NTRIP_FLASH_ERROR_INVALID_DATA,
    NTRIP_FLASH_ERROR_WRITE_FAILED
} ntrip_flash_error_t;

/**
 * Initialize flash storage system
 */
ntrip_flash_error_t ntrip_flash_init(void);

/**
 * Get memory-mapped pointer to service database
 * @param service_count Output parameter for total number of services
 * @return Direct flash memory pointer (read-only)
 */
const ntrip_service_compact_t* ntrip_flash_get_services(uint32_t* service_count);

/**
 * Get polygon coordinates for a service
 * @param service Service with polygon data
 * @param coords Output buffer for coordinates
 * @param max_coords Maximum coordinates to read
 * @return Number of coordinates read, 0 if no polygon
 */
uint8_t ntrip_flash_get_polygon(
    const ntrip_service_compact_t* service,
    ntrip_coord_pair_t* coords,
    uint8_t max_coords
);

/**
 * Check if point is within service polygon (precise coverage test)
 * Uses flash-based polygon data directly without copying to RAM
 */
bool ntrip_flash_point_in_polygon(
    const ntrip_service_compact_t* service,
    int16_t lat_deg1000,
    int16_t lon_deg1000
);

/**
 * Get flash storage statistics
 */
void ntrip_flash_get_stats(ntrip_flash_stats_t* stats);

/**
 * ESP32 platform-specific flash operations
 */
#ifdef ESP32

#include "esp_partition.h"
#include "esp_flash.h"

/**
 * ESP32-specific flash partition operations
 */
const esp_partition_t* ntrip_flash_find_partition(void);
esp_err_t ntrip_flash_map_partition(const esp_partition_t* partition);

#endif // ESP32

#endif // NTRIP_FLASH_STORAGE_H