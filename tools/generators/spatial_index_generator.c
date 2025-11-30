/**
 * NTRIP Atlas Spatial Index Generator
 *
 * Build-time generator that assigns services to spatial index tiles
 * based on their geographic coverage areas.
 *
 * This implements Phase 2 of the spatial indexing design.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "../../libntripatlas/include/ntrip_atlas.h"

// External service database
extern const ntrip_service_compact_t* get_sample_services(size_t* count);

// External spatial indexing functions
extern ntrip_atlas_error_t ntrip_atlas_init_spatial_index(void);
extern ntrip_atlas_error_t ntrip_atlas_lat_lon_to_tile(double lat, double lon, uint8_t level, uint16_t* tile_lat, uint16_t* tile_lon);
extern ntrip_tile_key_t ntrip_atlas_encode_tile_key(uint8_t level, uint16_t lat_tile, uint16_t lon_tile);
extern ntrip_atlas_error_t ntrip_atlas_add_service_to_tile(ntrip_tile_key_t tile_key, uint8_t service_index);
extern ntrip_atlas_error_t ntrip_atlas_get_spatial_index_stats(ntrip_spatial_index_stats_t* stats);

/**
 * Assign a single service to all tiles it overlaps at a given level
 */
static ntrip_atlas_error_t assign_service_to_level(
    const ntrip_service_compact_t* service,
    uint8_t service_index,
    uint8_t level
) {
    // Convert service coverage from int16 to double
    double lat_min = service->lat_min_deg100 / 100.0;
    double lat_max = service->lat_max_deg100 / 100.0;
    double lon_min = service->lon_min_deg100 / 100.0;
    double lon_max = service->lon_max_deg100 / 100.0;

    // Find tile ranges that overlap with service coverage
    uint16_t min_lat_tile, max_lat_tile, min_lon_tile, max_lon_tile;

    // Get tile coordinates for service bounds
    ntrip_atlas_error_t result;
    result = ntrip_atlas_lat_lon_to_tile(lat_min, lon_min, level, &min_lat_tile, &min_lon_tile);
    if (result != NTRIP_ATLAS_SUCCESS) return result;

    result = ntrip_atlas_lat_lon_to_tile(lat_max, lon_max, level, &max_lat_tile, &max_lon_tile);
    if (result != NTRIP_ATLAS_SUCCESS) return result;

    // Handle longitude wraparound case (service crosses date line)
    if (lon_max < lon_min) {
        // Service wraps around the date line
        // For now, treat it as global coverage at this level
        ntrip_atlas_lat_lon_to_tile(-90.0, -180.0, level, &min_lat_tile, &min_lon_tile);
        ntrip_atlas_lat_lon_to_tile(90.0, 180.0, level, &max_lat_tile, &max_lon_tile);
        printf("  Service crosses date line - treating as global at level %d\n", level);
    }

    printf("  Level %d: tiles [%d-%d, %d-%d] (coverage: %.2f°,%.2f° to %.2f°,%.2f°)\n",
           level, min_lat_tile, max_lat_tile, min_lon_tile, max_lon_tile,
           lat_min, lon_min, lat_max, lon_max);

    // Add service to all overlapping tiles
    int tiles_assigned = 0;
    for (uint16_t lat_tile = min_lat_tile; lat_tile <= max_lat_tile; lat_tile++) {
        for (uint16_t lon_tile = min_lon_tile; lon_tile <= max_lon_tile; lon_tile++) {
            ntrip_tile_key_t tile_key = ntrip_atlas_encode_tile_key(level, lat_tile, lon_tile);

            result = ntrip_atlas_add_service_to_tile(tile_key, service_index);
            if (result == NTRIP_ATLAS_SUCCESS) {
                tiles_assigned++;
            }
        }
    }

    printf("    → Assigned to %d tiles\n", tiles_assigned);
    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Assign all services to spatial index tiles based on their coverage areas
 */
static ntrip_atlas_error_t populate_spatial_index(void) {
    size_t service_count;
    const ntrip_service_compact_t* services = get_sample_services(&service_count);

    printf("Populating spatial index with %zu services...\n\n", service_count);

    for (size_t i = 0; i < service_count; i++) {
        const ntrip_service_compact_t* service = &services[i];

        printf("Service %zu: %s (provider %d, quality %d)\n",
               i, service->hostname, service->provider_index, service->quality_rating);

        // Assign service to all relevant levels (0-4)
        for (uint8_t level = 0; level <= 4; level++) {
            ntrip_atlas_error_t result = assign_service_to_level(service, (uint8_t)i, level);
            if (result != NTRIP_ATLAS_SUCCESS) {
                printf("  ❌ Failed to assign service %zu to level %d (error %d)\n",
                       i, level, result);
                return result;
            }
        }
        printf("\n");
    }

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Test service lookup at various locations
 */
static void test_service_lookup_at_locations(void) {
    printf("Testing service lookup at various global locations:\n");
    printf("==================================================\n\n");

    struct {
        double lat;
        double lon;
        const char* location;
        int expected_min_services;
    } test_locations[] = {
        {37.7749, -122.4194, "San Francisco, CA", 2},      // Should find RTK2GO + California CORS
        {42.3601, -71.0589, "Boston, MA", 2},              // Should find RTK2GO + Massachusetts MaCORS
        {52.5200, 13.4050, "Berlin, Germany", 2},          // Should find RTK2GO + BKG EUREF-IP
        {-33.8688, 151.2093, "Sydney, Australia", 2},      // Should find RTK2GO + Geoscience Australia
        {60.1699, 24.9384, "Helsinki, Finland", 2},        // Should find RTK2GO + Finland FinnRef
        {35.6762, 139.6503, "Tokyo, Japan", 2},            // Should find RTK2GO + Japan nodes
        {0.0, 0.0, "Gulf of Guinea (0°,0°)", 1},           // Should find only global services
        {-80.0, 0.0, "Antarctica", 1},                     // Should find only global services
    };

    for (size_t i = 0; i < sizeof(test_locations) / sizeof(test_locations[0]); i++) {
        printf("📍 %s (%.4f°, %.4f°):\n",
               test_locations[i].location,
               test_locations[i].lat,
               test_locations[i].lon);

        uint8_t found_services[16];
        size_t found_count = ntrip_atlas_find_services_by_location_fast(
            test_locations[i].lat,
            test_locations[i].lon,
            found_services,
            16
        );

        printf("  Found %zu services", found_count);
        if (found_count >= test_locations[i].expected_min_services) {
            printf(" ✅\n");
        } else {
            printf(" ❌ (expected at least %d)\n", test_locations[i].expected_min_services);
        }

        // Show which services were found
        if (found_count > 0) {
            size_t service_count;
            const ntrip_service_compact_t* services = get_sample_services(&service_count);

            for (size_t j = 0; j < found_count && j < 5; j++) {  // Limit to first 5 services
                uint8_t service_idx = found_services[j];
                if (service_idx < service_count) {
                    printf("    %s (quality %d)\n",
                           services[service_idx].hostname,
                           services[service_idx].quality_rating);
                }
            }
            if (found_count > 5) {
                printf("    ... and %zu more\n", found_count - 5);
            }
        }
        printf("\n");
    }
}

/**
 * Print spatial index statistics
 */
static void print_spatial_index_stats(void) {
    ntrip_spatial_index_stats_t stats;
    ntrip_atlas_error_t result = ntrip_atlas_get_spatial_index_stats(&stats);
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("❌ Failed to get spatial index stats (error %d)\n", result);
        return;
    }

    printf("Spatial Index Statistics:\n");
    printf("========================\n");
    printf("📊 Total tiles: %d\n", stats.total_tiles);
    printf("📊 Populated tiles: %d\n", stats.populated_tiles);
    printf("📊 Total service assignments: %d\n", stats.total_service_assignments);
    printf("📊 Average services per tile: %.1f\n", stats.average_services_per_tile);
    printf("📊 Max services per tile: %d\n", stats.max_services_per_tile);
    printf("📊 Memory usage: %zu bytes\n", stats.memory_used_bytes);
    printf("\n");

    double efficiency = (double)stats.populated_tiles / stats.total_tiles * 100.0;
    printf("📈 Tile utilization: %.1f%% (%d/%d tiles used)\n",
           efficiency, stats.populated_tiles, stats.total_tiles);

    if (efficiency < 50.0) {
        printf("💡 Suggestion: Consider optimizing tile boundaries for better utilization\n");
    }
    printf("\n");
}

int main() {
    printf("NTRIP Atlas Spatial Index Generator\n");
    printf("===================================\n\n");

    // Initialize spatial index
    ntrip_atlas_error_t result = ntrip_atlas_init_spatial_index();
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("❌ Failed to initialize spatial index (error %d)\n", result);
        return 1;
    }
    printf("✅ Spatial index initialized\n\n");

    // Populate spatial index with service data
    result = populate_spatial_index();
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("❌ Failed to populate spatial index (error %d)\n", result);
        return 1;
    }
    printf("✅ Spatial index populated with real service data\n\n");

    // Print statistics about the generated index
    print_spatial_index_stats();

    // Test service lookup at various locations
    test_service_lookup_at_locations();

    printf("🎉 Spatial index generation and testing complete!\n");
    printf("Phase 2 of spatial indexing implementation ready for integration.\n");

    return 0;
}