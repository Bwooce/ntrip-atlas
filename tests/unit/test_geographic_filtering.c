/**
 * Geographic Filtering Unit Tests
 *
 * Tests the geographic filtering system that uses service coverage bounding
 * boxes to filter and rank services by distance and coverage suitability.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

// Include the header
#include "../../libntripatlas/include/ntrip_atlas.h"

// Helper function to create test service
ntrip_service_compact_t create_test_service(const char* hostname,
                                          double lat_min, double lat_max,
                                          double lon_min, double lon_max) {
    ntrip_service_compact_t service = {0};
    strncpy(service.hostname, hostname, sizeof(service.hostname) - 1);
    service.port = 2101;
    service.lat_min_deg100 = (int16_t)round(lat_min * 100);
    service.lat_max_deg100 = (int16_t)round(lat_max * 100);
    service.lon_min_deg100 = (int16_t)round(lon_min * 100);
    service.lon_max_deg100 = (int16_t)round(lon_max * 100);
    return service;
}

// Test coverage detection
bool test_coverage_detection() {
    printf("Testing coverage detection...\\n");

    // Create test service covering Australia
    ntrip_service_compact_t aus_service = create_test_service("australia.gov.au",
                                                            -45.0, -10.0, 110.0, 160.0);

    // Test locations
    struct {
        double lat, lon;
        bool should_be_covered;
        const char* description;
    } test_locations[] = {
        {-35.0, 149.0, true, "Canberra (within coverage)"},
        {-33.8, 151.2, true, "Sydney (within coverage)"},
        {-37.8, 144.9, true, "Melbourne (within coverage)"},
        {40.7, -74.0, false, "New York (outside coverage)"},
        {51.5, -0.1, false, "London (outside coverage)"},
        {-45.0, 110.0, true, "Southwest corner (boundary)"},
        {-10.0, 160.0, true, "Northeast corner (boundary)"},
        {-46.0, 110.0, false, "Just outside south boundary"},
        {-35.0, 109.0, false, "Just outside west boundary"}
    };

    for (size_t i = 0; i < sizeof(test_locations) / sizeof(test_locations[0]); i++) {
        bool is_covered = ntrip_atlas_is_location_within_service_coverage(
            &aus_service, test_locations[i].lat, test_locations[i].lon
        );

        if (is_covered != test_locations[i].should_be_covered) {
            printf("  ❌ %s: expected %s, got %s\\n",
                   test_locations[i].description,
                   test_locations[i].should_be_covered ? "covered" : "not covered",
                   is_covered ? "covered" : "not covered");
            return false;
        }
    }

    printf("  ✅ Coverage detection working correctly\\n");
    return true;
}

// Test distance calculations
bool test_distance_calculations() {
    printf("Testing distance calculations...\\n");

    // Create service covering New York area (small region for precise testing)
    ntrip_service_compact_t ny_service = create_test_service("ny.dot.gov",
                                                           40.0, 41.0, -75.0, -73.0);

    // Test distance calculations
    struct {
        double lat, lon;
        double expected_distance;  // Approximate expected distance in km
        double tolerance;          // Tolerance for test
        const char* description;
    } test_distances[] = {
        {40.5, -74.0, 0.0, 5.0, "Center of coverage (should be ~0)"},
        {42.0, -74.0, 166.0, 10.0, "North of coverage (1.5° ≈ 166km)"},
        {39.0, -74.0, 166.0, 10.0, "South of coverage (1.5° ≈ 166km)"},
        {40.5, -72.0, 155.0, 15.0, "East of coverage (2° longitude ≈ 155km)"},
        {40.5, -76.0, 155.0, 15.0, "West of coverage (2° longitude ≈ 155km)"}
    };

    for (size_t i = 0; i < sizeof(test_distances) / sizeof(test_distances[0]); i++) {
        double distance = ntrip_atlas_calculate_distance_to_service_center(
            &ny_service, test_distances[i].lat, test_distances[i].lon
        );

        if (fabs(distance - test_distances[i].expected_distance) > test_distances[i].tolerance) {
            printf("  ❌ %s: expected ~%.1f km, got %.1f km\\n",
                   test_distances[i].description,
                   test_distances[i].expected_distance, distance);
            return false;
        }
    }

    printf("  ✅ Distance calculations working correctly\\n");
    return true;
}

// Test service filtering by coverage
bool test_service_filtering() {
    printf("Testing service filtering by coverage...\\n");

    // Create multiple test services with different coverage areas
    ntrip_service_compact_t services[] = {
        create_test_service("australia.gov.au", -45.0, -10.0, 110.0, 160.0),    // Australia
        create_test_service("usa.noaa.gov", 25.0, 50.0, -125.0, -65.0),         // USA
        create_test_service("europe.euref.eu", 35.0, 70.0, -10.0, 40.0),        // Europe
        create_test_service("canada.nrcan.gc.ca", 45.0, 85.0, -140.0, -50.0),   // Canada
        create_test_service("rtk2go.com", -90.0, 90.0, -180.0, 180.0)           // Global
    };
    size_t service_count = sizeof(services) / sizeof(services[0]);

    // Test filtering for Sydney, Australia
    double sydney_lat = -33.8, sydney_lon = 151.2;
    ntrip_service_compact_t filtered[10];
    size_t filtered_count = ntrip_atlas_filter_services_by_coverage(
        services, service_count, sydney_lat, sydney_lon, 1000.0, filtered, 10
    );

    // Should find Australia and Global services
    if (filtered_count < 2) {
        printf("  ❌ Expected at least 2 services for Sydney, got %zu\\n", filtered_count);
        return false;
    }

    // Check that Australia service is included
    bool found_australia = false;
    bool found_global = false;
    for (size_t i = 0; i < filtered_count; i++) {
        if (strstr(filtered[i].hostname, "australia")) found_australia = true;
        if (strstr(filtered[i].hostname, "rtk2go")) found_global = true;
    }

    if (!found_australia) {
        printf("  ❌ Australia service should be included for Sydney location\\n");
        return false;
    }
    if (!found_global) {
        printf("  ❌ Global service should be included for Sydney location\\n");
        return false;
    }

    printf("  ✅ Service filtering working correctly\\n");
    return true;
}

// Test sorting by distance
bool test_distance_sorting() {
    printf("Testing distance-based sorting...\\n");

    // Create services at different distances from test location
    ntrip_service_compact_t services[] = {
        create_test_service("far.service.com", 60.0, 65.0, 10.0, 15.0),      // Far (Scandinavia)
        create_test_service("close.service.com", 40.0, 42.0, -75.0, -73.0),  // Close (New York)
        create_test_service("medium.service.com", 35.0, 40.0, -120.0, -115.0) // Medium (California)
    };
    size_t service_count = 3;

    // Test location: New York area
    double test_lat = 40.7, test_lon = -74.0;

    // Sort services by distance
    size_t sorted_count = ntrip_atlas_filter_and_sort_services_by_location(
        services, service_count, test_lat, test_lon, 10000.0  // Large distance to include all
    );

    if (sorted_count != service_count) {
        printf("  ❌ Expected %zu services, got %zu after sorting\\n", service_count, sorted_count);
        return false;
    }

    // Verify sorting: closest should be first
    if (strstr(services[0].hostname, "close") == NULL) {
        printf("  ❌ Closest service should be first, got '%s'\\n", services[0].hostname);
        return false;
    }

    printf("  ✅ Distance sorting working correctly\\n");
    return true;
}

// Test coverage statistics
bool test_coverage_statistics() {
    printf("Testing coverage statistics...\\n");

    // Create test services
    ntrip_service_compact_t services[] = {
        create_test_service("local1.service.com", 40.0, 41.0, -75.0, -74.0),   // Covers test location
        create_test_service("local2.service.com", 40.5, 41.5, -74.5, -73.5),  // Covers test location
        create_test_service("remote.service.com", 60.0, 61.0, 10.0, 11.0)     // Remote
    };
    size_t service_count = 3;

    // Test location within first two services
    double test_lat = 40.5, test_lon = -74.5;

    ntrip_geo_filtering_stats_t stats;
    ntrip_atlas_error_t result = ntrip_atlas_get_geographic_filtering_stats(
        services, service_count, test_lat, test_lon, &stats
    );

    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Failed to get coverage statistics\\n");
        return false;
    }

    if (stats.total_services != 3) {
        printf("  ❌ Expected 3 total services, got %u\\n", stats.total_services);
        return false;
    }

    if (stats.services_with_coverage != 2) {
        printf("  ❌ Expected 2 services with coverage, got %u\\n", stats.services_with_coverage);
        return false;
    }

    double expected_coverage = 2.0 / 3.0 * 100.0;  // 66.67%
    if (fabs(stats.coverage_percentage - expected_coverage) > 1.0) {
        printf("  ❌ Expected ~%.1f%% coverage, got %.1f%%\\n",
               expected_coverage, stats.coverage_percentage);
        return false;
    }

    printf("  ✅ Coverage statistics working correctly\\n");
    return true;
}

// Test edge cases and error handling
bool test_edge_cases() {
    printf("Testing edge cases and error handling...\\n");

    // Test with NULL parameters
    if (ntrip_atlas_is_location_within_service_coverage(NULL, 0.0, 0.0)) {
        printf("  ❌ Should return false with NULL service\\n");
        return false;
    }

    double distance = ntrip_atlas_calculate_distance_to_service_center(NULL, 0.0, 0.0);
    if (!isinf(distance)) {
        printf("  ❌ Should return infinity with NULL service\\n");
        return false;
    }

    // Test filtering with NULL arrays
    size_t count = ntrip_atlas_filter_services_by_coverage(
        NULL, 0, 0.0, 0.0, 100.0, NULL, 0
    );
    if (count != 0) {
        printf("  ❌ Should return 0 with NULL arrays\\n");
        return false;
    }

    // Test statistics with NULL pointer
    if (ntrip_atlas_get_geographic_filtering_stats(NULL, 0, 0.0, 0.0, NULL) == NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Should fail with NULL statistics pointer\\n");
        return false;
    }

    printf("  ✅ Edge cases handled correctly\\n");
    return true;
}

// Test coordinate precision
bool test_coordinate_precision() {
    printf("Testing coordinate precision with int16 storage...\\n");

    // Test service with precise boundaries (using .00 and .50 to avoid rounding issues)
    ntrip_service_compact_t service = create_test_service("precision.test.com",
                                                        40.10, 40.50, -74.80, -74.10);

    // Test coordinates near boundaries (with clear 0.01 degree separation)
    struct {
        double lat, lon;
        bool should_be_covered;
        const char* description;
    } precision_tests[] = {
        {40.15, -74.75, true, "Inside boundaries"},
        {40.49, -74.15, true, "Near max boundaries"},
        {40.09, -74.75, false, "Just outside min lat (40.09 vs 40.10)"},
        {40.51, -74.15, false, "Just outside max lat (40.51 vs 40.50)"},
        {40.30, -74.81, false, "Just outside min lon (-74.81 vs -74.80)"},
        {40.30, -74.09, false, "Just outside max lon (-74.09 vs -74.10)"}
    };

    for (size_t i = 0; i < sizeof(precision_tests) / sizeof(precision_tests[0]); i++) {
        bool is_covered = ntrip_atlas_is_location_within_service_coverage(
            &service, precision_tests[i].lat, precision_tests[i].lon
        );

        if (is_covered != precision_tests[i].should_be_covered) {
            printf("  ❌ %s: expected %s, got %s\\n",
                   precision_tests[i].description,
                   precision_tests[i].should_be_covered ? "covered" : "not covered",
                   is_covered ? "covered" : "not covered");
            return false;
        }
    }

    printf("  ✅ Coordinate precision working correctly\\n");
    return true;
}

int main() {
    printf("Geographic Filtering Tests\\n");
    printf("==========================\\n\\n");

    struct {
        const char* name;
        bool (*test_func)();
    } tests[] = {
        {"Coverage detection", test_coverage_detection},
        {"Distance calculations", test_distance_calculations},
        {"Service filtering by coverage", test_service_filtering},
        {"Distance-based sorting", test_distance_sorting},
        {"Coverage statistics", test_coverage_statistics},
        {"Edge cases and error handling", test_edge_cases},
        {"Coordinate precision", test_coordinate_precision},
    };

    int passed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < total; i++) {
        printf("Test %d: %s\\n", i + 1, tests[i].name);
        if (tests[i].test_func()) {
            printf("✅ PASS\\n\\n");
            passed++;
        } else {
            printf("❌ FAIL\\n\\n");
        }
    }

    printf("Results: %d/%d tests passed\\n", passed, total);

    if (passed == total) {
        printf("🎉 All geographic filtering tests passed!\\n");
        printf("Distance-based service filtering ready for deployment\\n");
        printf("Services can now be efficiently filtered and ranked by coverage and proximity\\n");
        printf("Performance optimized through bounding box pre-filtering\\n");
        return 0;
    } else {
        printf("💥 Some geographic filtering tests failed!\\n");
        printf("Geographic filtering system needs fixes before deployment\\n");
        return 1;
    }
}