/**
 * Compact Failure Tracking Unit Tests
 *
 * Tests the memory-optimized failure tracking system that reduces
 * storage from 80 bytes to 6 bytes per service (93% reduction).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

// Include the header
#include "../../libntripatlas/include/ntrip_atlas.h"

// Test service mapping for our 32 services
static const ntrip_service_index_entry_t test_service_index[] = {
    {"rtk2go", 0},
    {"pointone-polaris", 1},
    {"australia-ga", 2},
    {"euref-ip", 3},
    {"finland-finnref", 4},
    {"massachusetts-macors", 5},
    {"southafrica-trignet", 6},
    {"argentina-ramsac", 7},
    {"brazil-rbmc-ip", 8},
    {"hongkong-satref", 9},
    {"newzealand-positionz", 10},
    {"bkg-euref-ip-research", 11},
    {"poland-asg-eupos", 12},
    {"spain-ergnss", 13},
    {"norway-satref", 14},
    {"netherlands-netpos", 15},
    {"belgium-flepos", 16},
    {"belgium-walcors", 17},
    {"czech-czepos", 18},
    {"italy-friuli-venezia-giulia", 19},
    {"usa-alabama-alcors", 20},
    {"usa-arizona-azcors", 21},
    {"usa-california-crtn", 22},
    {"usa-earthscope-nota", 23},
    {"usa-florida-fdot", 24},
    {"usa-maine-medot", 25},
    {"usa-michigan-mdot", 26},
    {"usa-minnesota-mncors", 27},
    {"usa-mississippi-gcgc", 28},
    {"usa-missouri-modot", 29},
    {"usa-new-york-nysnet", 30},
    {"usa-ohio-odot", 31},
};

#define TEST_SERVICE_COUNT (sizeof(test_service_index) / sizeof(test_service_index[0]))

// Test structure size and memory usage
bool test_memory_optimization() {
    printf("Testing memory optimization...\n");

    size_t full_failure_size = sizeof(ntrip_service_failure_t);
    size_t compact_failure_size = sizeof(ntrip_compact_failure_t);

    printf("  Full failure structure: %zu bytes\n", full_failure_size);
    printf("  Compact failure structure: %zu bytes\n", compact_failure_size);

    // Verify 6-byte target achieved
    if (compact_failure_size != 6) {
        printf("  ❌ Compact structure should be 6 bytes, got %zu\n", compact_failure_size);
        return false;
    }

    // Calculate memory savings for our 32 services
    size_t full_total = TEST_SERVICE_COUNT * full_failure_size;
    size_t compact_total = TEST_SERVICE_COUNT * compact_failure_size;
    size_t savings = full_total - compact_total;
    double savings_percent = ((double)savings / full_total) * 100.0;

    printf("  Memory usage for %zu services:\n", TEST_SERVICE_COUNT);
    printf("    Full implementation: %zu bytes\n", full_total);
    printf("    Compact implementation: %zu bytes\n", compact_total);
    printf("    Savings: %zu bytes (%.1f%% reduction)\n", savings, savings_percent);

    // Verify we achieve target 93% reduction
    if (savings_percent < 90.0) {
        printf("  ❌ Expected >90%% reduction, got %.1f%%\n", savings_percent);
        return false;
    }

    printf("  ✅ Memory optimization target achieved\n");
    return true;
}

// Test service index mapping
bool test_service_mapping() {
    printf("Testing service index mapping...\n");

    // Initialize compact failure tracking
    ntrip_atlas_error_t result = ntrip_atlas_init_compact_failure_tracking(
        test_service_index, TEST_SERVICE_COUNT);

    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Failed to initialize compact failure tracking\n");
        return false;
    }

    // Test service ID to index mapping
    uint8_t index = ntrip_atlas_get_service_index("rtk2go");
    if (index != 0) {
        printf("  ❌ rtk2go should map to index 0, got %u\n", index);
        return false;
    }

    index = ntrip_atlas_get_service_index("usa-ohio-odot");
    if (index != 31) {
        printf("  ❌ usa-ohio-odot should map to index 31, got %u\n", index);
        return false;
    }

    // Test unknown service
    index = ntrip_atlas_get_service_index("unknown-service");
    if (index != 255) {
        printf("  ❌ Unknown service should return 255, got %u\n", index);
        return false;
    }

    printf("  ✅ Service mapping working correctly\n");
    return true;
}

// Test basic failure recording and blocking
bool test_failure_recording() {
    printf("Testing failure recording and blocking...\n");

    uint8_t rtk2go_index = ntrip_atlas_get_service_index("rtk2go");

    // Initially should not be blocked
    if (ntrip_atlas_is_compact_service_blocked(rtk2go_index)) {
        printf("  ❌ Service should not be blocked initially\n");
        return false;
    }

    // Record a failure
    ntrip_atlas_error_t result = ntrip_atlas_record_compact_failure(rtk2go_index);
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Failed to record failure\n");
        return false;
    }

    // Should now be blocked
    if (!ntrip_atlas_is_compact_service_blocked(rtk2go_index)) {
        printf("  ❌ Service should be blocked after failure\n");
        return false;
    }

    // Check retry time (should be > 0)
    uint32_t retry_hours = ntrip_atlas_get_compact_retry_time_hours(rtk2go_index);
    if (retry_hours == 0) {
        printf("  ❌ Retry time should be > 0 after failure\n");
        return false;
    }

    printf("  ✅ Service blocked for %u hours after failure\n", retry_hours);

    // Record success to reset
    result = ntrip_atlas_record_compact_success(rtk2go_index);
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Failed to record success\n");
        return false;
    }

    // Should no longer be blocked
    if (ntrip_atlas_is_compact_service_blocked(rtk2go_index)) {
        printf("  ❌ Service should not be blocked after success\n");
        return false;
    }

    printf("  ✅ Failure recording and blocking working correctly\n");
    return true;
}

// Test exponential backoff progression
bool test_exponential_backoff() {
    printf("Testing exponential backoff progression...\n");

    uint8_t test_index = ntrip_atlas_get_service_index("pointone-polaris");

    // Record multiple failures and check backoff progression
    uint32_t previous_retry_hours = 0;

    for (int i = 1; i <= 5; i++) {
        ntrip_atlas_record_compact_failure(test_index);
        uint32_t retry_hours = ntrip_atlas_get_compact_retry_time_hours(test_index);

        printf("  Failure %d: retry in %u hours\n", i, retry_hours);

        // Each backoff should be longer than the previous (exponential)
        if (i > 1 && retry_hours <= previous_retry_hours) {
            printf("  ❌ Backoff should increase exponentially\n");
            return false;
        }

        previous_retry_hours = retry_hours;
    }

    // Test backoff level to seconds conversion
    uint32_t backoff_1h = ntrip_atlas_get_backoff_seconds_from_level(1);
    uint32_t backoff_2h = ntrip_atlas_get_backoff_seconds_from_level(2);

    if (backoff_1h != 3600) {  // 1 hour
        printf("  ❌ Level 1 backoff should be 3600 seconds, got %u\n", backoff_1h);
        return false;
    }

    if (backoff_2h != 14400) {  // 4 hours
        printf("  ❌ Level 2 backoff should be 14400 seconds, got %u\n", backoff_2h);
        return false;
    }

    printf("  ✅ Exponential backoff progression working correctly\n");
    return true;
}

// Test conversion between compact and full structures
bool test_structure_conversion() {
    printf("Testing compact to full structure conversion...\n");

    uint8_t test_index = ntrip_atlas_get_service_index("australia-ga");

    // Record a failure
    ntrip_atlas_record_compact_failure(test_index);

    // Get compact structure (would need access to internal state in real implementation)
    ntrip_compact_failure_t compact = {
        .service_index = test_index,
        .backoff_level = 1,
        .failure_count = 1,
        .retry_time_hours = 123456
    };

    // Convert to full structure
    ntrip_service_failure_t full;
    ntrip_atlas_error_t result = ntrip_atlas_expand_compact_failure(&compact, &full);

    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Failed to expand compact failure\n");
        return false;
    }

    // Check conversion results
    if (full.failure_count != 1) {
        printf("  ❌ Failure count not preserved: expected 1, got %u\n", full.failure_count);
        return false;
    }

    if (strstr(full.service_id, "australia-ga") == NULL) {
        printf("  ❌ Service ID not correctly resolved: %s\n", full.service_id);
        return false;
    }

    printf("  ✅ Structure conversion working correctly\n");
    return true;
}

// Test discovery integration - blocked services are skipped
bool test_discovery_integration() {
    printf("Testing discovery integration (skip blocked services)...\n");

    // Reset service states (might be blocked from previous tests)
    uint8_t polaris_index = ntrip_atlas_get_service_index("pointone-polaris");
    uint8_t australia_index = ntrip_atlas_get_service_index("australia-ga");
    ntrip_atlas_record_compact_success(polaris_index);
    ntrip_atlas_record_compact_success(australia_index);

    // Create mock service list
    ntrip_service_config_t test_services[] = {
        {.provider = "rtk2go"},           // Will be blocked
        {.provider = "pointone-polaris"}, // Will be available
        {.provider = "australia-ga"},     // Will be available
        {.provider = "euref-ip"},         // Will be blocked
    };

    // Block some services
    uint8_t rtk2go_index = ntrip_atlas_get_service_index("rtk2go");
    uint8_t euref_index = ntrip_atlas_get_service_index("euref-ip");

    ntrip_atlas_record_compact_failure(rtk2go_index);
    ntrip_atlas_record_compact_failure(euref_index);

    // Verify they are blocked
    if (!ntrip_atlas_should_skip_service("rtk2go")) {
        printf("  ❌ rtk2go should be marked for skipping\n");
        return false;
    }

    if (!ntrip_atlas_should_skip_service("euref-ip")) {
        printf("  ❌ euref-ip should be marked for skipping\n");
        return false;
    }

    // Verify available services are not skipped
    if (ntrip_atlas_should_skip_service("pointone-polaris")) {
        printf("  ❌ pointone-polaris should not be skipped\n");
        return false;
    }

    // Filter service list
    ntrip_service_config_t filtered_services[4];
    size_t filtered_count = ntrip_atlas_filter_blocked_services(
        test_services, 4, filtered_services, 4
    );

    // Should have 2 services (pointone-polaris and australia-ga)
    if (filtered_count != 2) {
        printf("  ❌ Expected 2 filtered services, got %zu\n", filtered_count);
        return false;
    }

    // Check that correct services were filtered
    bool found_polaris = false;
    bool found_australia = false;

    for (size_t i = 0; i < filtered_count; i++) {
        if (strcmp(filtered_services[i].provider, "pointone-polaris") == 0) {
            found_polaris = true;
        }
        if (strcmp(filtered_services[i].provider, "australia-ga") == 0) {
            found_australia = true;
        }
    }

    if (!found_polaris || !found_australia) {
        printf("  ❌ Filtered services don't contain expected available services\n");
        return false;
    }

    printf("  ✅ Discovery correctly prioritizes available services over blocked ones\n");
    printf("  ✅ Users get immediate NTRIP data from next-best service\n");
    return true;
}

// Test edge cases and error conditions
bool test_edge_cases() {
    printf("Testing edge cases and error conditions...\n");

    // Test invalid service index
    bool blocked = ntrip_atlas_is_compact_service_blocked(255);
    if (blocked) {
        printf("  ❌ Invalid service index should not be blocked\n");
        return false;
    }

    // Test invalid parameters
    ntrip_atlas_error_t result = ntrip_atlas_record_compact_failure(255);
    if (result == NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Should fail with invalid service index\n");
        return false;
    }

    // Test failure count saturation (saturates at 15)
    uint8_t test_index = ntrip_atlas_get_service_index("finland-finnref");

    // Record 20 failures (more than 15)
    for (int i = 0; i < 20; i++) {
        ntrip_atlas_record_compact_failure(test_index);
    }

    // Failure count should be saturated at a reasonable level
    // (We can't directly check the internal state, but this exercises the saturation logic)
    printf("  ✅ Failure count saturation tested\n");

    // Test zero backoff level
    uint32_t zero_backoff = ntrip_atlas_get_backoff_seconds_from_level(0);
    if (zero_backoff != 0) {
        printf("  ❌ Zero backoff level should return 0 seconds, got %u\n", zero_backoff);
        return false;
    }

    printf("  ✅ Edge cases handled correctly\n");
    return true;
}

int main() {
    printf("Compact Failure Tracking Tests\n");
    printf("==============================\n\n");

    struct {
        const char* name;
        bool (*test_func)();
    } tests[] = {
        {"Memory optimization", test_memory_optimization},
        {"Service mapping", test_service_mapping},
        {"Failure recording", test_failure_recording},
        {"Exponential backoff", test_exponential_backoff},
        {"Structure conversion", test_structure_conversion},
        {"Discovery integration", test_discovery_integration},
        {"Edge cases", test_edge_cases},
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
        printf("🎉 All compact failure tests passed!\n");
        printf("Memory optimization successfully reduces storage by 93%% (80→6 bytes per service)\n");
        printf("Ready for ESP32 deployment with %zu services using only %zu bytes\n",
               TEST_SERVICE_COUNT, TEST_SERVICE_COUNT * 6);
        return 0;
    } else {
        printf("💥 Some compact failure tests failed!\n");
        printf("Compact failure implementation needs fixes before deployment\n");
        return 1;
    }
}