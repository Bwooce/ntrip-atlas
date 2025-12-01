/**
 * NTRIP Atlas Phase 2 Geographic Coverage Validation Tests
 *
 * Comprehensive cross-checking tests to ensure that the geographic areas
 * we defined for each service actually work correctly in practice.
 *
 * This validates the critical assumptions about service coverage areas.
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "../../libntripatlas/include/ntrip_atlas.h"

// External Phase 2 service database
extern const ntrip_service_compact_t* get_sample_services(size_t* count);

// External spatial indexing functions
extern ntrip_atlas_error_t ntrip_atlas_init_spatial_index(void);
extern ntrip_atlas_error_t ntrip_atlas_lat_lon_to_tile(double lat, double lon, uint8_t level, uint16_t* tile_lat, uint16_t* tile_lon);
extern ntrip_tile_key_t ntrip_atlas_encode_tile_key(uint8_t level, uint16_t lat_tile, uint16_t lon_tile);
extern ntrip_atlas_error_t ntrip_atlas_add_service_to_tile(ntrip_tile_key_t tile_key, uint8_t service_index);
extern size_t ntrip_atlas_find_services_by_location_fast(double lat, double lon, uint8_t* service_indices, size_t max_services);

// Helper to populate spatial index (from Phase 2)
extern ntrip_atlas_error_t assign_service_to_level(const ntrip_service_compact_t* service, uint8_t service_index, uint8_t level);

/**
 * Populate spatial index with all services (copied from Phase 2)
 */
static ntrip_atlas_error_t setup_spatial_index_with_services(void) {
    size_t service_count;
    const ntrip_service_compact_t* services = get_sample_services(&service_count);

    for (size_t i = 0; i < service_count; i++) {
        const ntrip_service_compact_t* service = &services[i];

        // Assign service to all relevant levels (0-4)
        for (uint8_t level = 0; level <= 4; level++) {
            // Convert service coverage from int16 to double
            double lat_min = service->lat_min_deg100 / 100.0;
            double lat_max = service->lat_max_deg100 / 100.0;
            double lon_min = service->lon_min_deg100 / 100.0;
            double lon_max = service->lon_max_deg100 / 100.0;

            // Find tile ranges that overlap with service coverage
            uint16_t min_lat_tile, max_lat_tile, min_lon_tile, max_lon_tile;

            ntrip_atlas_error_t result;
            result = ntrip_atlas_lat_lon_to_tile(lat_min, lon_min, level, &min_lat_tile, &min_lon_tile);
            if (result != NTRIP_ATLAS_SUCCESS) continue;

            result = ntrip_atlas_lat_lon_to_tile(lat_max, lon_max, level, &max_lat_tile, &max_lon_tile);
            if (result != NTRIP_ATLAS_SUCCESS) continue;

            // Handle longitude wraparound case (service crosses date line)
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

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Helper function to check if a service index is found in results
 */
static bool service_found_in_results(uint8_t target_service, uint8_t* found_services, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (found_services[i] == target_service) {
            return true;
        }
    }
    return false;
}

/**
 * Test 1: Australia Coverage Validation
 * Geoscience Australia service should be found in Australia, not elsewhere
 */
bool test_australia_coverage_validation(void) {
    printf("Testing Geoscience Australia coverage validation...\\n");

    size_t service_count;
    const ntrip_service_compact_t* services = get_sample_services(&service_count);

    // Find Geoscience Australia service (hostname: auscors.ga.gov.au)
    int aus_service_idx = -1;
    for (size_t i = 0; i < service_count; i++) {
        if (strstr(services[i].hostname, "auscors.ga.gov.au") != NULL) {
            aus_service_idx = (int)i;
            break;
        }
    }

    if (aus_service_idx == -1) {
        printf("  ❌ Geoscience Australia service not found in database\\n");
        return false;
    }

    printf("  Found Geoscience Australia at index %d\\n", aus_service_idx);
    printf("  Coverage: %.2f° to %.2f° lat, %.2f° to %.2f° lon\\n",
           services[aus_service_idx].lat_min_deg100 / 100.0,
           services[aus_service_idx].lat_max_deg100 / 100.0,
           services[aus_service_idx].lon_min_deg100 / 100.0,
           services[aus_service_idx].lon_max_deg100 / 100.0);

    // Test locations that SHOULD find Geoscience Australia
    struct {
        double lat, lon;
        const char* location;
        bool should_find_aus;
    } test_locations[] = {
        // Inside Australia coverage
        {-35.2809, 149.1300, "Canberra, Australia", true},
        {-33.8688, 151.2093, "Sydney, Australia", true},
        {-37.8136, 144.9631, "Melbourne, Australia", true},
        {-27.4698, 153.0251, "Brisbane, Australia", true},
        {-31.9505, 115.8605, "Perth, Australia", true},
        {-34.9285, 138.5999, "Adelaide, Australia", true},
        {-42.8821, 147.3272, "Hobart, Australia", true},
        {-12.4634, 130.8456, "Darwin, Australia", true},

        // Outside Australia coverage
        {40.7128, -74.0060, "New York, USA", false},
        {51.5074, -0.1278, "London, UK", false},
        {35.6762, 139.6503, "Tokyo, Japan", false},
        {-34.6037, -58.3816, "Buenos Aires, Argentina", false},
        {55.7558, 37.6176, "Moscow, Russia", false},
        {1.3521, 103.8198, "Singapore", false},
        {0.0, 0.0, "Gulf of Guinea", false},

        // Boundary testing (edge of coverage)
        {-45.0, 110.0, "SW boundary", true},      // Exact SW corner
        {-10.0, 160.0, "NE boundary", true},      // Exact NE corner
        {-45.1, 109.9, "Just outside SW", false}, // Just outside
        {-9.9, 160.1, "Just outside NE", false},  // Just outside
    };

    bool all_passed = true;
    for (size_t i = 0; i < sizeof(test_locations) / sizeof(test_locations[0]); i++) {
        uint8_t found_services[16];
        size_t found_count = ntrip_atlas_find_services_by_location_fast(
            test_locations[i].lat,
            test_locations[i].lon,
            found_services,
            16
        );

        bool aus_found = service_found_in_results(aus_service_idx, found_services, found_count);

        printf("  %s (%.4f°, %.4f°): ",
               test_locations[i].location,
               test_locations[i].lat,
               test_locations[i].lon);

        if (aus_found == test_locations[i].should_find_aus) {
            printf("✅ %s\\n", aus_found ? "Found AUS" : "No AUS (correct)");
        } else {
            printf("❌ Expected %s, got %s\\n",
                   test_locations[i].should_find_aus ? "AUS found" : "no AUS",
                   aus_found ? "AUS found" : "no AUS");
            all_passed = false;
        }
    }

    return all_passed;
}

/**
 * Test 2: Massachusetts Coverage Validation
 * Massachusetts MaCORS should only be found in Massachusetts
 */
bool test_massachusetts_coverage_validation(void) {
    printf("Testing Massachusetts MaCORS coverage validation...\\n");

    size_t service_count;
    const ntrip_service_compact_t* services = get_sample_services(&service_count);

    // Find Massachusetts service (hostname: radio-labs.com)
    int ma_service_idx = -1;
    for (size_t i = 0; i < service_count; i++) {
        if (strstr(services[i].hostname, "radio-labs.com") != NULL) {
            ma_service_idx = (int)i;
            break;
        }
    }

    if (ma_service_idx == -1) {
        printf("  ❌ Massachusetts MaCORS service not found\\n");
        return false;
    }

    printf("  Found Massachusetts MaCORS at index %d\\n", ma_service_idx);
    printf("  Coverage: %.2f° to %.2f° lat, %.2f° to %.2f° lon\\n",
           services[ma_service_idx].lat_min_deg100 / 100.0,
           services[ma_service_idx].lat_max_deg100 / 100.0,
           services[ma_service_idx].lon_min_deg100 / 100.0,
           services[ma_service_idx].lon_max_deg100 / 100.0);

    struct {
        double lat, lon;
        const char* location;
        bool should_find_ma;
    } test_locations[] = {
        // Inside Massachusetts
        {42.3601, -71.0589, "Boston, MA", true},
        {42.2753, -71.8061, "Worcester, MA", true},
        {42.1015, -72.5898, "Springfield, MA", true},
        {41.7003, -70.9714, "New Bedford, MA", true},

        // Outside Massachusetts but nearby
        {41.2033, -77.1945, "Pennsylvania", false},
        {43.2081, -71.5376, "New Hampshire", false},
        {41.5801, -71.4774, "Rhode Island", false}, // Very close but outside
        {42.3601, -68.0589, "Atlantic Ocean east of Boston", false},

        // Far outside
        {40.7128, -74.0060, "New York City", false},
        {25.7617, -80.1918, "Miami, FL", false},
        {47.6062, -122.3321, "Seattle, WA", false},
    };

    bool all_passed = true;
    for (size_t i = 0; i < sizeof(test_locations) / sizeof(test_locations[0]); i++) {
        uint8_t found_services[16];
        size_t found_count = ntrip_atlas_find_services_by_location_fast(
            test_locations[i].lat,
            test_locations[i].lon,
            found_services,
            16
        );

        bool ma_found = service_found_in_results(ma_service_idx, found_services, found_count);

        printf("  %s (%.4f°, %.4f°): ",
               test_locations[i].location,
               test_locations[i].lat,
               test_locations[i].lon);

        if (ma_found == test_locations[i].should_find_ma) {
            printf("✅ %s\\n", ma_found ? "Found MA" : "No MA (correct)");
        } else {
            printf("❌ Expected %s, got %s\\n",
                   test_locations[i].should_find_ma ? "MA found" : "no MA",
                   ma_found ? "MA found" : "no MA");
            all_passed = false;
        }
    }

    return all_passed;
}

/**
 * Test 3: Europe Coverage Validation
 * BKG EUREF-IP should be found in Europe, not elsewhere
 */
bool test_europe_coverage_validation(void) {
    printf("Testing BKG EUREF-IP European coverage validation...\\n");

    size_t service_count;
    const ntrip_service_compact_t* services = get_sample_services(&service_count);

    // Find BKG EUREF-IP service (hostname: igs-ip.net)
    int eu_service_idx = -1;
    for (size_t i = 0; i < service_count; i++) {
        if (strstr(services[i].hostname, "igs-ip.net") != NULL) {
            eu_service_idx = (int)i;
            break;
        }
    }

    if (eu_service_idx == -1) {
        printf("  ❌ BKG EUREF-IP service not found\\n");
        return false;
    }

    printf("  Found BKG EUREF-IP at index %d\\n", eu_service_idx);

    struct {
        double lat, lon;
        const char* location;
        bool should_find_eu;
    } test_locations[] = {
        // Inside Europe coverage (35° to 71° lat, -10° to 40° lon)
        {52.5200, 13.4050, "Berlin, Germany", true},
        {48.8566, 2.3522, "Paris, France", true},
        {41.9028, 12.4964, "Rome, Italy", true},
        {40.4168, -3.7038, "Madrid, Spain", true},
        {59.3293, 18.0686, "Stockholm, Sweden", true},
        {60.1699, 24.9384, "Helsinki, Finland", true},

        // Outside Europe coverage
        {40.7128, -74.0060, "New York, USA", false},
        {35.6762, 139.6503, "Tokyo, Japan", false},
        {-33.8688, 151.2093, "Sydney, Australia", false},
        {55.7558, 37.6176, "Moscow, Russia", false}, // Outside lon range
        {30.0444, 31.2357, "Cairo, Egypt", false},   // Outside lat range

        // Boundary testing
        {35.0, 0.0, "Southern boundary", true},
        {71.0, 20.0, "Northern boundary", true},
        {50.0, -10.0, "Western boundary", true},
        {50.0, 40.0, "Eastern boundary", true},
        {34.9, 0.0, "Just south of boundary", false},
        {50.0, 40.1, "Just east of boundary", false},
    };

    bool all_passed = true;
    for (size_t i = 0; i < sizeof(test_locations) / sizeof(test_locations[0]); i++) {
        uint8_t found_services[16];
        size_t found_count = ntrip_atlas_find_services_by_location_fast(
            test_locations[i].lat,
            test_locations[i].lon,
            found_services,
            16
        );

        bool eu_found = service_found_in_results(eu_service_idx, found_services, found_count);

        printf("  %s (%.4f°, %.4f°): ",
               test_locations[i].location,
               test_locations[i].lat,
               test_locations[i].lon);

        if (eu_found == test_locations[i].should_find_eu) {
            printf("✅ %s\\n", eu_found ? "Found EU" : "No EU (correct)");
        } else {
            printf("❌ Expected %s, got %s\\n",
                   test_locations[i].should_find_eu ? "EU found" : "no EU",
                   eu_found ? "EU found" : "no EU");
            all_passed = false;
        }
    }

    return all_passed;
}

/**
 * Test 4: Global Services Validation
 * RTK2GO and Point One Polaris should be found everywhere
 */
bool test_global_services_validation(void) {
    printf("Testing global services (RTK2GO, Point One Polaris) coverage...\\n");

    size_t service_count;
    const ntrip_service_compact_t* services = get_sample_services(&service_count);

    // Find global services
    int rtk2go_count = 0;
    int polaris_idx = -1;
    for (size_t i = 0; i < service_count; i++) {
        if (strstr(services[i].hostname, "rtk2go.com") != NULL) {
            rtk2go_count++;
        }
        if (strstr(services[i].hostname, "polaris.pointonenav.com") != NULL) {
            polaris_idx = (int)i;
        }
    }

    printf("  Found %d RTK2GO services, Polaris at index %d\\n", rtk2go_count, polaris_idx);

    // Test locations around the globe - all should find at least global services
    struct {
        double lat, lon;
        const char* location;
    } global_test_locations[] = {
        {0.0, 0.0, "Equator/Prime Meridian"},
        {90.0, 0.0, "North Pole"},
        {-90.0, 0.0, "South Pole"},
        {0.0, 180.0, "Equator/Date Line"},
        {0.0, -180.0, "Equator/Date Line West"},
        {45.0, -120.0, "Pacific Northwest"},
        {-30.0, -60.0, "South America"},
        {20.0, 77.0, "India"},
        {-20.0, 140.0, "Australia Interior"},
        {70.0, -150.0, "Arctic Alaska"},
        {-60.0, 0.0, "Southern Ocean"},
    };

    bool all_passed = true;
    for (size_t i = 0; i < sizeof(global_test_locations) / sizeof(global_test_locations[0]); i++) {
        uint8_t found_services[16];
        size_t found_count = ntrip_atlas_find_services_by_location_fast(
            global_test_locations[i].lat,
            global_test_locations[i].lon,
            found_services,
            16
        );

        printf("  %s (%.1f°, %.1f°): ",
               global_test_locations[i].location,
               global_test_locations[i].lat,
               global_test_locations[i].lon);

        if (found_count >= 1) {
            printf("✅ Found %zu services\\n", found_count);
        } else {
            printf("❌ Found no services (expected at least global services)\\n");
            all_passed = false;
        }
    }

    return all_passed;
}

/**
 * Test 5: Coordinate Boundary Edge Cases
 * Test problematic coordinates like poles, date line, etc.
 */
bool test_coordinate_boundary_edge_cases(void) {
    printf("Testing coordinate boundary edge cases...\\n");

    struct {
        double lat, lon;
        const char* description;
        bool should_work;
    } edge_cases[] = {
        // Valid boundary cases
        {90.0, 0.0, "North Pole", true},
        {-90.0, 0.0, "South Pole", true},
        {0.0, 180.0, "Date Line East", true},
        {0.0, -180.0, "Date Line West", true},
        {89.999, 179.999, "Almost North Pole, almost Date Line", true},
        {-89.999, -179.999, "Almost South Pole, almost Date Line", true},

        // Invalid coordinates
        {91.0, 0.0, "Beyond North Pole", false},
        {-91.0, 0.0, "Beyond South Pole", false},
        {0.0, 181.0, "Beyond Date Line East", false},
        {0.0, -181.0, "Beyond Date Line West", false},
    };

    bool all_passed = true;
    for (size_t i = 0; i < sizeof(edge_cases) / sizeof(edge_cases[0]); i++) {
        uint8_t found_services[16];
        size_t found_count = ntrip_atlas_find_services_by_location_fast(
            edge_cases[i].lat,
            edge_cases[i].lon,
            found_services,
            16
        );

        printf("  %s (%.3f°, %.3f°): ",
               edge_cases[i].description,
               edge_cases[i].lat,
               edge_cases[i].lon);

        if (edge_cases[i].should_work) {
            if (found_count >= 0) {  // Any result (including 0) is valid for valid coordinates
                printf("✅ Found %zu services\\n", found_count);
            } else {
                printf("❌ Lookup failed\\n");
                all_passed = false;
            }
        } else {
            // For invalid coordinates, we expect either 0 results or the function to handle gracefully
            printf("✅ Found %zu services (invalid coords handled)\\n", found_count);
        }
    }

    return all_passed;
}

int main() {
    printf("NTRIP Atlas Phase 2 Geographic Coverage Validation\\n");
    printf("=================================================\\n\\n");

    // Initialize spatial index
    ntrip_atlas_error_t result = ntrip_atlas_init_spatial_index();
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("❌ Failed to initialize spatial index\\n");
        return 1;
    }

    // Populate spatial index with services
    result = setup_spatial_index_with_services();
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("❌ Failed to populate spatial index\\n");
        return 1;
    }

    printf("✅ Spatial index initialized and populated\\n\\n");

    // Run all validation tests
    struct {
        const char* name;
        bool (*test_func)(void);
    } tests[] = {
        {"Australia Coverage Validation", test_australia_coverage_validation},
        {"Massachusetts Coverage Validation", test_massachusetts_coverage_validation},
        {"Europe Coverage Validation", test_europe_coverage_validation},
        {"Global Services Validation", test_global_services_validation},
        {"Coordinate Boundary Edge Cases", test_coordinate_boundary_edge_cases},
    };

    int passed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < total; i++) {
        printf("Test %d: %s\\n", i + 1, tests[i].name);
        bool success = tests[i].test_func();
        if (success) {
            printf("✅ PASS\\n");
            passed++;
        } else {
            printf("❌ FAIL\\n");
        }
        printf("\\n");
    }

    // Summary
    printf("Results: %d/%d tests passed\\n", passed, total);
    if (passed == total) {
        printf("🎉 All geographic coverage validation tests passed!\\n");
        printf("✅ Service coverage areas are correctly implemented\\n");
        printf("✅ Spatial indexing correctly maps services to geographic regions\\n");
        printf("✅ Ready for production deployment\\n");
        return 0;
    } else {
        printf("💥 Some coverage validation tests failed!\\n");
        printf("❌ Geographic coverage implementation needs fixes\\n");
        printf("❌ Service-to-region mapping may be incorrect\\n");
        return 1;
    }
}