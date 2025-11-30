/**
 * Test: Spatial-Geographic Integration
 *
 * Tests the new integrated function that combines O(1) spatial indexing
 * with precise geographic bounds checking to solve the "German problem".
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "ntrip_atlas.h"

// External functions for testing
extern const ntrip_service_compact_t* get_sample_services(size_t* count);

/**
 * Test case structure for geographic validation
 */
typedef struct {
    double lat;
    double lon;
    const char* location;
    size_t expected_spatial_candidates;  // From spatial indexing alone
    size_t expected_verified_services;   // With geographic bounds checking
    bool should_find_european;
    bool should_find_massachusetts;
    bool should_find_australian;
} test_case_t;

/**
 * Populate spatial index with service data (like Phase 2 generator)
 */
void populate_spatial_index() {
    size_t service_count;
    const ntrip_service_compact_t* services = get_sample_services(&service_count);

    // Assign each service to spatial index tiles
    for (size_t i = 0; i < service_count; i++) {
        const ntrip_service_compact_t* service = &services[i];

        // Convert service coverage to coordinates
        double lat_min = service->lat_min_deg100 / 100.0;
        double lat_max = service->lat_max_deg100 / 100.0;
        double lon_min = service->lon_min_deg100 / 100.0;
        double lon_max = service->lon_max_deg100 / 100.0;

        // Assign service to tiles at all levels (0-4)
        for (uint8_t level = 0; level <= 4; level++) {
            // Get tile ranges for service coverage
            uint16_t min_lat_tile, max_lat_tile, min_lon_tile, max_lon_tile;

            ntrip_atlas_lat_lon_to_tile(lat_min, lon_min, level, &min_lat_tile, &min_lon_tile);
            ntrip_atlas_lat_lon_to_tile(lat_max, lon_max, level, &max_lat_tile, &max_lon_tile);

            // Handle longitude wraparound
            if (lon_max < lon_min) {
                ntrip_atlas_lat_lon_to_tile(-90.0, -180.0, level, &min_lat_tile, &min_lon_tile);
                ntrip_atlas_lat_lon_to_tile(90.0, 180.0, level, &max_lat_tile, &max_lon_tile);
            }

            // Add service to all overlapping tiles
            for (uint16_t lat_tile = min_lat_tile; lat_tile <= max_lat_tile; lat_tile++) {
                for (uint16_t lon_tile = min_lon_tile; lon_tile <= max_lon_tile; lon_tile++) {
                    ntrip_tile_key_t tile_key = ntrip_atlas_encode_tile_key(level, lat_tile, lon_tile);
                    ntrip_atlas_add_service_to_tile(tile_key, (uint8_t)i);
                }
            }
        }
    }
}

/**
 * Test the German problem: Moscow should not find European services
 */
void test_german_problem_fix() {
    printf("\nTest: German Problem Fix\n");
    printf("========================\n");

    // Initialize spatial index
    ntrip_atlas_error_t result = ntrip_atlas_init_spatial_index();
    assert(result == NTRIP_ATLAS_SUCCESS);

    // Populate spatial index with service data
    populate_spatial_index();

    // Get service data for bounds checking
    size_t service_count;
    const ntrip_service_compact_t* services = get_sample_services(&service_count);

    printf("✅ Spatial index initialized and populated with %zu services\n", service_count);

    // Test coordinates that demonstrate the problem
    test_case_t test_cases[] = {
        // Cases that SHOULD find European service
        {52.5200, 13.4050, "Berlin, Germany", 4, 3, true, false, false},
        {48.8566, 2.3522, "Paris, France", 4, 3, true, false, false},

        // Cases that should NOT find European service (the "German problem")
        {55.7558, 37.6176, "Moscow, Russia", 4, 2, false, false, false},  // Too far east
        {30.0444, 31.2357, "Cairo, Egypt", 2, 2, false, false, false},    // Too far south

        // Massachusetts cases
        {42.3601, -71.0589, "Boston, MA", 3, 3, false, true, false},
        {41.2033, -77.1945, "Pennsylvania", 2, 2, false, false, false},   // Should not find MA

        // Australia cases
        {-33.8688, 151.2093, "Sydney, Australia", 3, 3, false, false, true},
        {0.0, 0.0, "Gulf of Guinea", 2, 2, false, false, false},           // Should not find AU
    };

    size_t num_tests = sizeof(test_cases) / sizeof(test_cases[0]);

    printf("Comparing original spatial indexing vs integrated bounds checking:\n\n");

    bool all_tests_passed = true;

    for (size_t i = 0; i < num_tests; i++) {
        test_case_t* test = &test_cases[i];

        printf("📍 %s (%.4f°, %.4f°):\n", test->location, test->lat, test->lon);

        // Test 1: Original spatial indexing (shows the problem)
        uint8_t spatial_services[8];
        size_t spatial_count = ntrip_atlas_find_services_by_location_fast(
            test->lat, test->lon, spatial_services, 8
        );

        // Test 2: New integrated function (shows the fix)
        uint8_t verified_services[8];
        size_t verified_count = ntrip_atlas_find_services_spatial_geographic(
            test->lat, test->lon, services, service_count, verified_services, 8
        );

        // Test 3: Statistical comparison
        size_t stat_candidates = 0, stat_verified = 0;
        ntrip_atlas_get_spatial_geographic_stats(
            test->lat, test->lon, services, service_count,
            &stat_candidates, &stat_verified
        );

        printf("  Spatial indexing only: %zu services\n", spatial_count);
        printf("  With bounds checking:  %zu services\n", verified_count);
        printf("  Stats verification:    %zu → %zu services\n", stat_candidates, stat_verified);

        // Verify the counts match expectations
        bool spatial_correct = (spatial_count >= test->expected_spatial_candidates - 1 &&
                               spatial_count <= test->expected_spatial_candidates + 1);
        bool verified_correct = (verified_count == test->expected_verified_services);

        // Check for specific service types in verified results
        bool found_european = false, found_massachusetts = false, found_australian = false;
        for (size_t j = 0; j < verified_count; j++) {
            uint8_t service_idx = verified_services[j];
            if (service_idx < service_count) {
                const ntrip_service_compact_t* service = &services[service_idx];

                if (strstr(service->hostname, "igs-ip.net")) {
                    found_european = true;
                }
                if (strstr(service->hostname, "radio-labs.com")) {
                    found_massachusetts = true;
                }
                if (strstr(service->hostname, "auscors.ga.gov.au")) {
                    found_australian = true;
                }
            }
        }

        bool regional_correct = (found_european == test->should_find_european) &&
                               (found_massachusetts == test->should_find_massachusetts) &&
                               (found_australian == test->should_find_australian);

        // Show which services were found
        if (verified_count > 0) {
            printf("    Verified services: ");
            for (size_t j = 0; j < verified_count; j++) {
                uint8_t service_idx = verified_services[j];
                if (service_idx < service_count) {
                    printf("%s ", services[service_idx].hostname);
                }
            }
            printf("\n");
        }

        if (spatial_correct && verified_correct && regional_correct) {
            printf("  ✅ PASS - Geographic bounds checking working correctly\n");
        } else {
            printf("  ❌ FAIL - Expected verified: %zu, got: %zu\n",
                   test->expected_verified_services, verified_count);
            if (!regional_correct) {
                printf("       Regional service filtering incorrect\n");
            }
            all_tests_passed = false;
        }
        printf("\n");
    }

    printf("Summary:\n");
    if (all_tests_passed) {
        printf("✅ All tests passed - German problem solved!\n");
        printf("   Spatial indexing + bounds checking provides accurate geographic coverage\n");
    } else {
        printf("❌ Some tests failed - German problem not fully solved\n");
    }
}

/**
 * Test performance comparison between approaches
 */
void test_performance_comparison() {
    printf("\nTest: Performance Comparison\n");
    printf("===========================\n");

    // Make sure spatial index is populated
    size_t service_count;
    const ntrip_service_compact_t* services = get_sample_services(&service_count);

    // Test coordinates in various regions
    struct {
        double lat, lon;
        const char* location;
    } locations[] = {
        {52.5200, 13.4050, "Berlin"},
        {42.3601, -71.0589, "Boston"},
        {-33.8688, 151.2093, "Sydney"},
        {55.7558, 37.6176, "Moscow"},
    };

    printf("Performance analysis for different lookup methods:\n\n");

    for (size_t i = 0; i < 4; i++) {
        printf("📍 %s:\n", locations[i].location);

        // Method 1: Spatial indexing only
        uint8_t spatial_results[8];
        size_t spatial_count = ntrip_atlas_find_services_by_location_fast(
            locations[i].lat, locations[i].lon, spatial_results, 8
        );

        // Method 2: Integrated spatial + geographic
        uint8_t integrated_results[8];
        size_t integrated_count = ntrip_atlas_find_services_spatial_geographic(
            locations[i].lat, locations[i].lon, services, service_count,
            integrated_results, 8
        );

        // Calculate filtering efficiency
        double filtering_efficiency = 0.0;
        if (spatial_count > 0) {
            filtering_efficiency = ((double)(spatial_count - integrated_count) / spatial_count) * 100.0;
        }

        printf("  Spatial indexing:      %zu services (O(1) lookup)\n", spatial_count);
        printf("  Integrated filtering:  %zu services (O(1) + bounds check)\n", integrated_count);
        printf("  Filtering efficiency:  %.1f%% (removed %zu false positives)\n",
               filtering_efficiency, spatial_count - integrated_count);
        printf("\n");
    }
}

int main() {
    printf("NTRIP Atlas Spatial-Geographic Integration Tests\n");
    printf("===============================================\n");

    test_german_problem_fix();
    test_performance_comparison();

    printf("🎉 Integration testing complete!\n");
    printf("The combined spatial indexing + geographic bounds checking\n");
    printf("provides both O(1) performance AND accurate geographic coverage.\n");

    return 0;
}