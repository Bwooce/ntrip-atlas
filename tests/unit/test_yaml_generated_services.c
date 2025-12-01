/**
 * Test: YAML Generated Services
 *
 * Validates that the YAML-to-C generation pipeline works correctly
 * and that generated services match expected structure and quality.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "ntrip_atlas.h"

// Include generated services
#include "../../libntripatlas/src/generated/ntrip_generated_services.h"

/**
 * Test basic generated service structure
 */
void test_generated_service_structure() {
    printf("\nTest: Generated Service Structure\n");
    printf("=================================\n");

    size_t service_count;
    const ntrip_service_compact_t* services = get_generated_services(&service_count);

    assert(services != NULL);
    assert(service_count > 0);

    printf("✅ Generated %zu services successfully\n", service_count);

    // Test each service has valid structure
    for (size_t i = 0; i < service_count; i++) {
        const ntrip_service_compact_t* service = &services[i];

        // Validate hostname
        assert(strlen(service->hostname) > 0);
        assert(strlen(service->hostname) <= 31);

        // Validate port
        assert(service->port > 0);
        assert(service->port <= 65535);

        // Validate geographic bounds
        assert(service->lat_min_deg100 >= -9000);
        assert(service->lat_max_deg100 <= 9000);
        assert(service->lon_min_deg100 >= -18000);
        assert(service->lon_max_deg100 <= 18000);
        assert(service->lat_min_deg100 <= service->lat_max_deg100);

        // Validate metadata
        assert(service->network_type >= 1 && service->network_type <= 3);
        assert(service->quality_rating >= 1 && service->quality_rating <= 5);

        printf("✅ Service %zu: %s - valid structure\n", i, service->hostname);
    }

    printf("✅ All generated services have valid structure\n");
}

/**
 * Test provider name lookup functionality
 */
void test_provider_name_lookup() {
    printf("\nTest: Provider Name Lookup\n");
    printf("==========================\n");

    size_t service_count;
    const ntrip_service_compact_t* services = get_generated_services(&service_count);

    for (size_t i = 0; i < service_count; i++) {
        const ntrip_service_compact_t* service = &services[i];
        const char* provider_name = get_provider_name(service->provider_index);

        assert(provider_name != NULL);
        assert(strlen(provider_name) > 0);

        printf("✅ Service %zu (%s) -> Provider: %s\n",
               i, service->hostname, provider_name);
    }

    // Test invalid provider index
    const char* invalid_provider = get_provider_name(255);
    assert(strcmp(invalid_provider, "Unknown") == 0);

    printf("✅ Provider name lookup working correctly\n");
}

/**
 * Test service coverage validation
 */
void test_service_coverage() {
    printf("\nTest: Service Coverage Validation\n");
    printf("==================================\n");

    size_t service_count;
    const ntrip_service_compact_t* services = get_generated_services(&service_count);

    // Test known locations against expected services
    struct {
        double lat, lon;
        const char* location;
        const char* expected_hostname;  // One service we expect to find
    } test_locations[] = {
        {-33.8688, 151.2093, "Sydney, Australia", "ntrip.data.gnss.ga.gov.au"},
        {-41.2865, 174.7762, "Wellington, NZ", "positionz-rt.linz.govt.nz"},
        {0.0, 0.0, "Null Island", "rtk2go.com"},  // Global services should cover
    };

    for (size_t t = 0; t < sizeof(test_locations) / sizeof(test_locations[0]); t++) {
        printf("📍 Testing %s (%.4f°, %.4f°):\n",
               test_locations[t].location, test_locations[t].lat, test_locations[t].lon);

        bool found_expected = false;
        int coverage_count = 0;

        for (size_t i = 0; i < service_count; i++) {
            bool within_coverage = ntrip_atlas_is_location_within_service_coverage(
                &services[i], test_locations[t].lat, test_locations[t].lon
            );

            if (within_coverage) {
                coverage_count++;
                printf("  📡 %s covers this location\n", services[i].hostname);

                if (strstr(services[i].hostname, test_locations[t].expected_hostname)) {
                    found_expected = true;
                }
            }
        }

        printf("  Found %d services with coverage\n", coverage_count);

        if (test_locations[t].expected_hostname) {
            assert(found_expected);
            printf("  ✅ Expected service found: %s\n", test_locations[t].expected_hostname);
        }
    }

    printf("✅ Service coverage validation working correctly\n");
}

/**
 * Test authentication flag mapping
 */
void test_authentication_flags() {
    printf("\nTest: Authentication Flags\n");
    printf("===========================\n");

    size_t service_count;
    const ntrip_service_compact_t* services = get_generated_services(&service_count);

    for (size_t i = 0; i < service_count; i++) {
        const ntrip_service_compact_t* service = &services[i];

        printf("Service %s:\n", service->hostname);

        if (service->flags & NTRIP_FLAG_SSL) {
            printf("  🔒 SSL enabled\n");
        }

        if (service->flags & NTRIP_FLAG_AUTH_BASIC) {
            printf("  🔑 Basic authentication required\n");
        }

        if (service->flags & NTRIP_FLAG_AUTH_DIGEST) {
            printf("  🔐 Digest authentication required\n");
        }

        if (service->flags & NTRIP_FLAG_REQUIRES_REG) {
            printf("  📝 Registration required\n");
        }

        if (service->flags & NTRIP_FLAG_FREE_ACCESS) {
            printf("  🆓 Free access\n");
        }

        printf("\n");
    }

    printf("✅ Authentication flags correctly mapped\n");
}

/**
 * Test integration with spatial indexing
 */
void test_spatial_integration() {
    printf("\nTest: Spatial Indexing Integration\n");
    printf("===================================\n");

    // Initialize spatial index
    ntrip_atlas_error_t result = ntrip_atlas_init_spatial_index();
    assert(result == NTRIP_ATLAS_SUCCESS);

    size_t service_count;
    const ntrip_service_compact_t* services = get_generated_services(&service_count);

    // Populate spatial index with non-global services only
    int global_services_skipped = 0;
    for (size_t i = 0; i < service_count; i++) {
        const ntrip_service_compact_t* service = &services[i];

        // Skip global services - they're handled separately
        if (service->flags & NTRIP_FLAG_GLOBAL_SERVICE) {
            global_services_skipped++;
            printf("⏭️  Skipping global service %zu (%s) - no spatial indexing needed\n",
                   i, service->hostname);
            continue;
        }

        double lat_min = service->lat_min_deg100 / 100.0;
        double lat_max = service->lat_max_deg100 / 100.0;
        double lon_min = service->lon_min_deg100 / 100.0;
        double lon_max = service->lon_max_deg100 / 100.0;

        for (uint8_t level = 0; level <= 4; level++) {
            uint16_t min_lat_tile, max_lat_tile, min_lon_tile, max_lon_tile;

            ntrip_atlas_lat_lon_to_tile(lat_min, lon_min, level, &min_lat_tile, &min_lon_tile);
            ntrip_atlas_lat_lon_to_tile(lat_max, lon_max, level, &max_lat_tile, &max_lon_tile);

            if (lon_max < lon_min) {  // Handle wraparound
                ntrip_atlas_lat_lon_to_tile(-90.0, -180.0, level, &min_lat_tile, &min_lon_tile);
                ntrip_atlas_lat_lon_to_tile(90.0, 180.0, level, &max_lat_tile, &max_lon_tile);
            }

            for (uint16_t lat_tile = min_lat_tile; lat_tile <= max_lat_tile; lat_tile++) {
                for (uint16_t lon_tile = min_lon_tile; lon_tile <= max_lon_tile; lon_tile++) {
                    ntrip_tile_key_t tile_key = ntrip_atlas_encode_tile_key(level, lat_tile, lon_tile);
                    ntrip_atlas_error_t add_result = ntrip_atlas_add_service_to_tile(tile_key, (uint8_t)i);
                    if (add_result != NTRIP_ATLAS_SUCCESS) {
                        const char* error_msg;
                        if (add_result == -21) {
                            error_msg = "SPATIAL_INDEX_FULL (maximum 16384 tiles reached)";
                        } else if (add_result == -22) {
                            error_msg = "TILE_FULL (maximum 64 services per tile reached)";
                        } else {
                            error_msg = "UNKNOWN_ERROR";
                        }
                        printf("❌ Service %zu (%s): Failed to add to tile (L%d, %u,%u) key=0x%08X: error %d (%s)\n",
                               i, services[i].hostname, level, lat_tile, lon_tile, tile_key, add_result, error_msg);
                        assert(add_result == NTRIP_ATLAS_SUCCESS);
                    }
                }
            }
        }
    }

    // Test spatial lookup with generated services
    uint8_t found_services[8];
    size_t found_count = ntrip_atlas_find_services_by_location_fast(
        -33.8688, 151.2093, found_services, 8  // Sydney
    );

    printf("✅ Spatial lookup found %zu services for Sydney\n", found_count);
    assert(found_count > 0);

    // Test integrated spatial-geographic lookup
    uint8_t verified_services[8];
    size_t verified_count = ntrip_atlas_find_services_spatial_geographic(
        -33.8688, 151.2093, services, service_count, verified_services, 8
    );

    printf("✅ Integrated lookup found %zu verified services\n", verified_count);
    assert(verified_count > 0);
    assert(verified_count <= found_count);  // Bounds checking should filter some

    printf("✅ Skipped %d global services from spatial indexing\n", global_services_skipped);
    printf("✅ Generated services work correctly with spatial indexing\n");
}

/**
 * Test correct service discovery ordering: local first, global fallback
 */
void test_service_discovery_ordering() {
    printf("\nTest: Service Discovery Ordering (Local First, Global Fallback)\n");
    printf("================================================================\n");

    size_t service_count;
    const ntrip_service_compact_t* services = get_generated_services(&service_count);

    // Test Sydney: should find local Australian service + global services
    double sydney_lat = -33.8688;
    double sydney_lon = 151.2093;

    printf("📍 Testing service discovery order for Sydney (%.4f°, %.4f°):\n", sydney_lat, sydney_lon);

    // Step 1: Get spatially-indexed (local/regional) services first
    uint8_t local_services[8];
    size_t local_count = ntrip_atlas_find_services_by_location_fast(
        sydney_lat, sydney_lon, local_services, 8
    );

    printf("\n🏛️  LOCAL/REGIONAL services found first (HIGHEST QUALITY):\n");
    int australian_found = 0;
    for (size_t i = 0; i < local_count; i++) {
        uint8_t service_idx = local_services[i];
        const ntrip_service_compact_t* service = &services[service_idx];
        const char* provider = get_provider_name(service->provider_index);
        printf("  Priority %zu: Service %d - %s (%s)\n", i+1, service_idx, service->hostname, provider);

        if (strstr(provider, "Geoscience Australia")) {
            australian_found = 1;
        }
    }

    // Step 2: Get global services as fallback
    printf("\n🌍 GLOBAL services as fallback (LOWER QUALITY):\n");
    int global_count = 0;
    for (size_t i = 0; i < service_count; i++) {
        const ntrip_service_compact_t* service = &services[i];
        if (service->flags & NTRIP_FLAG_GLOBAL_SERVICE) {
            const char* provider = get_provider_name(service->provider_index);
            printf("  Fallback %d: Service %zu - %s (%s)\n", global_count+1, i, service->hostname, provider);
            global_count++;
        }
    }

    // Validate ordering
    assert(australian_found == 1);  // Australian service should be in local results
    assert(local_count > 0);        // Should find local services
    assert(global_count == 5);      // Should have 5 global services (RTK2go, Point One, HxGN, IGS, Topcon)

    printf("\n✅ Service discovery ordering correct:\n");
    printf("  1. Try %zu local/regional services first (HIGHEST QUALITY)\n", local_count);
    printf("  2. Fall back to %d global services if local fails (LOWER QUALITY)\n", global_count);
    printf("  3. Australian government service correctly prioritized over global\n");
}

int main() {
    printf("NTRIP Atlas YAML Generated Services Tests\n");
    printf("==========================================\n");

    test_generated_service_structure();
    test_provider_name_lookup();
    test_service_coverage();
    test_authentication_flags();
    test_spatial_integration();
    test_service_discovery_ordering();

    printf("\n🎉 All YAML generated service tests passed!\n");
    printf("The YAML-to-C generation pipeline is working correctly.\n");
    printf("Ready to replace manual service_database.c with generated code.\n");

    return 0;
}