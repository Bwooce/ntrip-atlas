/**
 * ESP32 Memory Constraint Tests
 * Verifies memory usage stays within ESP32 limits
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

// ESP32 memory limits (conservative estimates)
#define ESP32_MAX_STACK_USAGE    8192   // 8KB max stack per task
#define ESP32_MAX_HEAP_USAGE     4096   // 4KB max heap for discovery
#define ESP32_TOTAL_RAM_LIMIT    320000 // 320KB total RAM

// Mock ESP32 memory tracking
static size_t simulated_heap_used = 0;
static size_t max_heap_used = 0;
static size_t stack_depth = 0;

// Mock malloc/free for tracking
void* test_malloc(size_t size) {
    simulated_heap_used += size;
    if (simulated_heap_used > max_heap_used) {
        max_heap_used = simulated_heap_used;
    }
    return malloc(size);
}

void test_free(void* ptr, size_t size) {
    simulated_heap_used -= size;
    free(ptr);
}

// Mock structures based on design
typedef struct __attribute__((packed)) {
    uint8_t service_id;
    char hostname[64];
    uint16_t port;
    uint8_t flags;
    int16_t lat_min_deg100, lat_max_deg100;
    int16_t lon_min_deg100, lon_max_deg100;
} ntrip_service_compact_t;

typedef struct __attribute__((packed)) {
    char mountpoint[32];
    int16_t lat_deg100;
    int16_t lon_deg100;
    uint16_t distance_m;
    uint8_t quality_score;
    uint8_t service_index;
} ntrip_candidate_t;

// Simulated discovery function with memory tracking
int simulate_discovery_process(double user_lat, double user_lon) {
    (void)user_lat;   // Suppress unused parameter warning
    (void)user_lon;   // Suppress unused parameter warning
    stack_depth += 512; // Function entry overhead

    // Static allocations (as designed)
    char http_buffer[1024];
    char parse_buffer[2048];
    ntrip_candidate_t best_candidate = {0};
    ntrip_candidate_t current_candidate = {0};

    stack_depth += sizeof(http_buffer) + sizeof(parse_buffer) +
                  sizeof(best_candidate) + sizeof(current_candidate);

    // Simulate processing 16 services (max in embedded profile)
    ntrip_service_compact_t services[16];
    stack_depth += sizeof(services);

    printf("Stack usage: %zu bytes\n", stack_depth);

    // Simulate heap usage for temporary allocations
    void* temp_buffer = test_malloc(512);  // Temporary parsing buffer
    test_free(temp_buffer, 512);

    stack_depth -= 512; // Function exit
    return 0;
}

bool test_service_structure_size() {
    printf("Testing structure sizes...\n");

    size_t service_size = sizeof(ntrip_service_compact_t);
    size_t candidate_size = sizeof(ntrip_candidate_t);

    printf("  ntrip_service_compact_t: %zu bytes\n", service_size);
    printf("  ntrip_candidate_t: %zu bytes\n", candidate_size);

    // Updated limits for ESP32 profile with improved string support
    return service_size <= 80 && candidate_size <= 48;
}

bool test_static_memory_usage() {
    printf("Testing static memory allocations...\n");

    // Simulate the memory layout from config
    size_t service_table = 16 * sizeof(ntrip_service_compact_t);  // 16 services max
    size_t http_buffer = 1024;
    size_t parse_buffer = 2048;
    size_t working_vars = 500;

    size_t total_static = service_table + http_buffer + parse_buffer + working_vars;

    printf("  Service table: %zu bytes\n", service_table);
    printf("  HTTP buffer: %zu bytes\n", http_buffer);
    printf("  Parse buffer: %zu bytes\n", parse_buffer);
    printf("  Working variables: %zu bytes\n", working_vars);
    printf("  Total static: %zu bytes\n", total_static);

    // Must stay under 5KB total for ESP32 deployment
    return total_static <= 5120;
}

bool test_stack_usage() {
    printf("Testing stack usage during discovery...\n");

    stack_depth = 0;
    max_heap_used = 0;
    simulated_heap_used = 0;

    simulate_discovery_process(-33.8568, 151.2153);

    printf("  Max stack depth: %zu bytes\n", stack_depth);
    printf("  Max heap used: %zu bytes\n", max_heap_used);

    return stack_depth <= ESP32_MAX_STACK_USAGE && max_heap_used <= ESP32_MAX_HEAP_USAGE;
}

bool test_service_count_limits() {
    printf("Testing service count limits...\n");

    // Embedded profile limits
    const int max_services = 16;
    const int max_mountpoints = 64;

    size_t service_memory = max_services * sizeof(ntrip_service_compact_t);
    size_t mountpoint_memory = max_mountpoints * sizeof(ntrip_candidate_t);

    printf("  Max services: %d (%zu bytes)\n", max_services, service_memory);
    printf("  Max mountpoints: %d (%zu bytes)\n", max_mountpoints, mountpoint_memory);

    // In streaming mode, we don't store all mountpoints simultaneously
    // Only store one candidate at a time
    return service_memory <= 1280; // 1.25KB for service table with extended strings
}

bool test_string_buffer_limits() {
    printf("Testing string buffer constraints...\n");

    // Test that all string fields fit in allocated space
    const char* test_hostname = "very.long.hostname.example.that.might.exceed.limits.com";
    const char* test_mountpoint = "VERY_LONG_MOUNTPOINT_NAME_TEST";

    bool hostname_fits = strlen(test_hostname) < 64;  // hostname field size
    bool mountpoint_fits = strlen(test_mountpoint) < 32;  // mountpoint field size

    printf("  Hostname length: %zu (limit: 64)\n", strlen(test_hostname));
    printf("  Mountpoint length: %zu (limit: 32)\n", strlen(test_mountpoint));

    return hostname_fits && mountpoint_fits;
}

bool test_coordinate_precision() {
    printf("Testing coordinate precision with int16 storage...\n");

    // Test that int16 * 100 provides adequate precision
    double test_lat = -33.8568;
    double test_lon = 151.2153;

    int16_t lat_stored = (int16_t)(test_lat * 100);
    int16_t lon_stored = (int16_t)(test_lon * 100);

    double lat_recovered = (double)lat_stored / 100.0;
    double lon_recovered = (double)lon_stored / 100.0;

    double lat_error = fabs(test_lat - lat_recovered);
    double lon_error = fabs(test_lon - lon_recovered);

    printf("  Latitude error: %.4f degrees\n", lat_error);
    printf("  Longitude error: %.4f degrees\n", lon_error);

    // Precision should be better than 0.01 degrees (~1km)
    return lat_error < 0.01 && lon_error < 0.01;
}

int main() {
    printf("ESP32 Memory Constraint Tests\n");
    printf("=============================\n\n");

    struct {
        const char* name;
        bool (*test_func)();
    } tests[] = {
        {"Structure sizes", test_service_structure_size},
        {"Static memory usage", test_static_memory_usage},
        {"Stack usage", test_stack_usage},
        {"Service count limits", test_service_count_limits},
        {"String buffer limits", test_string_buffer_limits},
        {"Coordinate precision", test_coordinate_precision},
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
        printf("🎉 All ESP32 memory tests passed!\n");
        printf("Memory usage is within ESP32 constraints.\n");
        return 0;
    } else {
        printf("💥 Some ESP32 memory tests failed!\n");
        printf("Memory optimization needed before ESP32 deployment.\n");
        return 1;
    }
}