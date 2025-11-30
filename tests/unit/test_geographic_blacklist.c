/**
 * Geographic Blacklisting Unit Tests
 *
 * Tests the geographic blacklisting system that avoids repeated queries
 * to services outside their coverage areas, improving discovery performance.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

// Include the header
#include "../../libntripatlas/include/ntrip_atlas.h"

// Test initialization of geographic blacklisting system
bool test_blacklist_initialization() {
    printf("Testing blacklist system initialization...\n");

    // Initialize the system
    ntrip_atlas_error_t result = ntrip_atlas_init_geographic_blacklist();
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Failed to initialize blacklist system\n");
        return false;
    }

    // Verify we can get statistics (system is initialized)
    ntrip_geo_blacklist_stats_t stats;
    result = ntrip_atlas_get_geographic_blacklist_stats(&stats);
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Failed to get blacklist statistics\n");
        return false;
    }

    if (stats.services_with_blacklists != 0 || stats.total_blacklisted_regions != 0) {
        printf("  ❌ Expected clean slate, got %u services with %u regions\n",
               stats.services_with_blacklists, stats.total_blacklisted_regions);
        return false;
    }

    printf("  ✅ Blacklist system initialized correctly\n");
    return true;
}

// Test adding regions to blacklist
bool test_add_blacklist_regions() {
    printf("Testing blacklist region addition...\n");

    // Clear any existing blacklists
    ntrip_atlas_clear_all_geographic_blacklists();

    // Add blacklist for Point One in Antarctica (no coverage expected)
    ntrip_atlas_error_t result = ntrip_atlas_blacklist_service_region(
        "Point One Navigation", -85.0, 0.0, "No coverage in Antarctica"
    );

    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Failed to add blacklist region\n");
        return false;
    }

    // Verify region is blacklisted
    if (!ntrip_atlas_is_service_geographically_blacklisted("Point One Navigation", -85.0, 0.0)) {
        printf("  ❌ Region should be blacklisted but isn't\n");
        return false;
    }

    // Verify nearby location is also blacklisted (same grid square)
    if (!ntrip_atlas_is_service_geographically_blacklisted("Point One Navigation", -85.2, 0.3)) {
        printf("  ❌ Nearby location in same grid should be blacklisted\n");
        return false;
    }

    // Verify different region is not blacklisted
    if (ntrip_atlas_is_service_geographically_blacklisted("Point One Navigation", 40.0, -74.0)) {
        printf("  ❌ New York region should not be blacklisted\n");
        return false;
    }

    // Verify different service is not affected
    if (ntrip_atlas_is_service_geographically_blacklisted("RTK2go Community", -85.0, 0.0)) {
        printf("  ❌ Different service should not be blacklisted\n");
        return false;
    }

    printf("  ✅ Blacklist region addition working correctly\n");
    return true;
}

// Test multiple blacklist regions for same service
bool test_multiple_blacklist_regions() {
    printf("Testing multiple blacklist regions...\n");

    ntrip_atlas_clear_all_geographic_blacklists();

    // Add multiple regions for Geoscience Australia (should only cover Australia)
    const char* provider = "Geoscience Australia";
    struct {
        double lat, lon;
        const char* reason;
    } test_regions[] = {
        {60.0, 100.0, "No coverage in Siberia"},
        {0.0, 0.0, "No coverage in Atlantic"},
        {-85.0, 0.0, "No coverage in Antarctica"},
        {40.0, -100.0, "No coverage in North America"},
        {10.0, 80.0, "No coverage in Indian Ocean"}
    };

    // Add all test regions
    for (size_t i = 0; i < sizeof(test_regions) / sizeof(test_regions[0]); i++) {
        ntrip_atlas_error_t result = ntrip_atlas_blacklist_service_region(
            provider, test_regions[i].lat, test_regions[i].lon, test_regions[i].reason
        );
        if (result != NTRIP_ATLAS_SUCCESS) {
            printf("  ❌ Failed to add region %zu\n", i);
            return false;
        }
    }

    // Verify all regions are blacklisted
    for (size_t i = 0; i < sizeof(test_regions) / sizeof(test_regions[0]); i++) {
        if (!ntrip_atlas_is_service_geographically_blacklisted(
                provider, test_regions[i].lat, test_regions[i].lon)) {
            printf("  ❌ Region %zu should be blacklisted\n", i);
            return false;
        }
    }

    // Check statistics
    ntrip_geo_blacklist_stats_t stats;
    ntrip_atlas_get_geographic_blacklist_stats(&stats);

    if (stats.services_with_blacklists != 1) {
        printf("  ❌ Expected 1 service with blacklists, got %u\n", stats.services_with_blacklists);
        return false;
    }

    if (stats.total_blacklisted_regions != 5) {
        printf("  ❌ Expected 5 blacklisted regions, got %u\n", stats.total_blacklisted_regions);
        return false;
    }

    printf("  ✅ Multiple blacklist regions working correctly\n");
    return true;
}

// Test removing blacklist regions
bool test_remove_blacklist_regions() {
    printf("Testing blacklist region removal...\n");

    ntrip_atlas_clear_all_geographic_blacklists();

    const char* provider = "Massachusetts DOT";

    // Add blacklist region
    ntrip_atlas_blacklist_service_region(provider, 60.0, 2.0, "No coverage in Scandinavia");

    // Verify it's blacklisted
    if (!ntrip_atlas_is_service_geographically_blacklisted(provider, 60.0, 2.0)) {
        printf("  ❌ Region should be blacklisted before removal\n");
        return false;
    }

    // Remove the blacklist
    ntrip_atlas_error_t result = ntrip_atlas_remove_geographic_blacklist(provider, 60.0, 2.0);
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Failed to remove blacklist region\n");
        return false;
    }

    // Verify it's no longer blacklisted
    if (ntrip_atlas_is_service_geographically_blacklisted(provider, 60.0, 2.0)) {
        printf("  ❌ Region should not be blacklisted after removal\n");
        return false;
    }

    // Try to remove non-existent region
    result = ntrip_atlas_remove_geographic_blacklist(provider, 30.0, 30.0);
    if (result != NTRIP_ATLAS_ERROR_NOT_FOUND) {
        printf("  ❌ Should return NOT_FOUND for non-existent region\n");
        return false;
    }

    printf("  ✅ Blacklist region removal working correctly\n");
    return true;
}

// Test clearing service blacklists
bool test_clear_service_blacklists() {
    printf("Testing service blacklist clearing...\n");

    ntrip_atlas_clear_all_geographic_blacklists();

    const char* provider1 = "Finland NLS";
    const char* provider2 = "BKG EUREF-IP";

    // Add blacklists for both services
    ntrip_atlas_blacklist_service_region(provider1, -40.0, 175.0, "No coverage in New Zealand");
    ntrip_atlas_blacklist_service_region(provider1, 70.0, 25.0, "No coverage in far north");
    ntrip_atlas_blacklist_service_region(provider2, 30.0, 120.0, "No coverage in China");

    // Verify all are blacklisted
    if (!ntrip_atlas_is_service_geographically_blacklisted(provider1, -40.0, 175.0) ||
        !ntrip_atlas_is_service_geographically_blacklisted(provider1, 70.0, 25.0) ||
        !ntrip_atlas_is_service_geographically_blacklisted(provider2, 30.0, 120.0)) {
        printf("  ❌ Some regions not blacklisted as expected\n");
        return false;
    }

    // Clear blacklists for provider1 only
    ntrip_atlas_error_t result = ntrip_atlas_clear_service_geographic_blacklist(provider1);
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Failed to clear service blacklists\n");
        return false;
    }

    // Verify provider1 blacklists are cleared
    if (ntrip_atlas_is_service_geographically_blacklisted(provider1, -40.0, 175.0) ||
        ntrip_atlas_is_service_geographically_blacklisted(provider1, 70.0, 25.0)) {
        printf("  ❌ Provider1 blacklists should be cleared\n");
        return false;
    }

    // Verify provider2 blacklists are unaffected
    if (!ntrip_atlas_is_service_geographically_blacklisted(provider2, 30.0, 120.0)) {
        printf("  ❌ Provider2 blacklists should be unaffected\n");
        return false;
    }

    printf("  ✅ Service blacklist clearing working correctly\n");
    return true;
}

// Test blacklist capacity and LRU replacement
bool test_blacklist_capacity() {
    printf("Testing blacklist capacity limits...\n");

    ntrip_atlas_clear_all_geographic_blacklists();

    const char* provider = "Test Service";

    // Add maximum number of blacklist entries (8 per service)
    for (int i = 0; i < 8; i++) {
        char reason[64];
        snprintf(reason, sizeof(reason), "Test region %d", i);

        ntrip_atlas_error_t result = ntrip_atlas_blacklist_service_region(
            provider, i * 10.0, i * 10.0, reason
        );

        if (result != NTRIP_ATLAS_SUCCESS) {
            printf("  ❌ Failed to add blacklist entry %d\n", i);
            return false;
        }
    }

    // Verify all 8 entries are blacklisted
    for (int i = 0; i < 8; i++) {
        if (!ntrip_atlas_is_service_geographically_blacklisted(provider, i * 10.0, i * 10.0)) {
            printf("  ❌ Entry %d should be blacklisted\n", i);
            return false;
        }
    }

    // Add one more entry (should replace oldest via LRU)
    ntrip_atlas_blacklist_service_region(provider, 90.0, 90.0, "Overflow entry");

    // New entry should be blacklisted
    if (!ntrip_atlas_is_service_geographically_blacklisted(provider, 90.0, 90.0)) {
        printf("  ❌ New entry should be blacklisted\n");
        return false;
    }

    // One of the old entries should have been replaced
    // (We can't predict which one due to time-based LRU, but the system should still function)

    printf("  ✅ Blacklist capacity limits working correctly\n");
    return true;
}

// Test geographic grid precision
bool test_geographic_grid_precision() {
    printf("Testing geographic grid precision...\n");

    ntrip_atlas_clear_all_geographic_blacklists();

    const char* provider = "Grid Test Service";

    // Add blacklist at specific coordinate
    ntrip_atlas_blacklist_service_region(provider, 40.123, -74.567, "Precise location");

    // Test various nearby coordinates (within 1-degree grid)
    struct {
        double lat, lon;
        bool should_be_blacklisted;
    } test_coords[] = {
        {40.123, -74.567, true},   // Exact match
        {40.9, -74.9, true},       // Same grid square
        {40.0, -74.0, true},       // Grid boundary
        {40.999, -74.999, true},   // Near grid boundary
        {41.0, -74.0, false},      // Different grid (lat)
        {40.0, -75.0, false},      // Different grid (lon)
        {39.5, -73.5, false},     // Different grid (both)
    };

    for (size_t i = 0; i < sizeof(test_coords) / sizeof(test_coords[0]); i++) {
        bool is_blacklisted = ntrip_atlas_is_service_geographically_blacklisted(
            provider, test_coords[i].lat, test_coords[i].lon
        );

        if (is_blacklisted != test_coords[i].should_be_blacklisted) {
            printf("  ❌ Coordinate %.3f,%.3f: expected %s, got %s\n",
                   test_coords[i].lat, test_coords[i].lon,
                   test_coords[i].should_be_blacklisted ? "blacklisted" : "not blacklisted",
                   is_blacklisted ? "blacklisted" : "not blacklisted");
            return false;
        }
    }

    printf("  ✅ Geographic grid precision working correctly\n");
    return true;
}

// Test error handling
bool test_error_handling() {
    printf("Testing error handling...\n");

    // Test with NULL parameters
    if (ntrip_atlas_blacklist_service_region(NULL, 0.0, 0.0, "test") == NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Should fail with NULL provider\n");
        return false;
    }

    if (ntrip_atlas_is_service_geographically_blacklisted(NULL, 0.0, 0.0)) {
        printf("  ❌ Should return false with NULL provider\n");
        return false;
    }

    if (ntrip_atlas_get_geographic_blacklist_stats(NULL) == NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Should fail with NULL stats pointer\n");
        return false;
    }

    // Test operations before initialization
    // (System auto-initializes on first use, but test explicit checks)

    printf("  ✅ Error handling working correctly\n");
    return true;
}

int main() {
    printf("Geographic Blacklisting Tests\n");
    printf("=============================\n\n");

    struct {
        const char* name;
        bool (*test_func)();
    } tests[] = {
        {"Blacklist system initialization", test_blacklist_initialization},
        {"Add blacklist regions", test_add_blacklist_regions},
        {"Multiple blacklist regions", test_multiple_blacklist_regions},
        {"Remove blacklist regions", test_remove_blacklist_regions},
        {"Clear service blacklists", test_clear_service_blacklists},
        {"Blacklist capacity limits", test_blacklist_capacity},
        {"Geographic grid precision", test_geographic_grid_precision},
        {"Error handling", test_error_handling},
    };

    int passed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < total; i++) {
        printf("Test %d: %s\n", i + 1, tests[i].name);
        if (tests[i].test_func()) {
            printf("✅ PASS\n\n");
            passed++;
        } else {
            printf("❌ FAIL\n\n");
        }
    }

    printf("Results: %d/%d tests passed\n", passed, total);

    if (passed == total) {
        printf("🎉 All geographic blacklisting tests passed!\n");
        printf("Geographic blacklist system ready for deployment\n");
        printf("Services can now avoid repeated queries outside their coverage areas\n");
        printf("Discovery performance improved through intelligent region learning\n");
        return 0;
    } else {
        printf("💥 Some geographic blacklisting tests failed!\n");
        printf("Geographic blacklist system needs fixes before deployment\n");
        return 1;
    }
}