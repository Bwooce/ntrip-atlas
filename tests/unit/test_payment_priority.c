/**
 * Test: Payment Priority Configuration
 *
 * Validates configurable payment priority system:
 * - FREE_FIRST: Try free services first, paid services as fallback
 * - PAID_FIRST: Try paid services first, free services as fallback
 * - Credential checking and service skipping
 * - Service ordering validation for both modes
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "ntrip_atlas.h"

// Include generated services
#include "../../libntripatlas/src/generated/ntrip_generated_services.h"

/**
 * Test payment priority configuration API
 */
void test_payment_priority_configuration() {
    printf("\nTest: Payment Priority Configuration API\n");
    printf("=========================================\n");

    // Test default priority (should be FREE_FIRST)
    ntrip_payment_priority_t default_priority = ntrip_atlas_get_payment_priority();
    assert(default_priority == NTRIP_PAYMENT_PRIORITY_FREE_FIRST);
    printf("✅ Default payment priority is FREE_FIRST\n");

    // Test setting FREE_FIRST priority
    ntrip_atlas_error_t result = ntrip_atlas_set_payment_priority(NTRIP_PAYMENT_PRIORITY_FREE_FIRST);
    assert(result == NTRIP_ATLAS_SUCCESS);
    assert(ntrip_atlas_get_payment_priority() == NTRIP_PAYMENT_PRIORITY_FREE_FIRST);
    printf("✅ Successfully set FREE_FIRST priority\n");

    // Test setting PAID_FIRST priority
    result = ntrip_atlas_set_payment_priority(NTRIP_PAYMENT_PRIORITY_PAID_FIRST);
    assert(result == NTRIP_ATLAS_SUCCESS);
    assert(ntrip_atlas_get_payment_priority() == NTRIP_PAYMENT_PRIORITY_PAID_FIRST);
    printf("✅ Successfully set PAID_FIRST priority\n");

    // Test invalid priority
    result = ntrip_atlas_set_payment_priority(99);  // Invalid value
    assert(result == NTRIP_ATLAS_ERROR_INVALID_PARAM);
    assert(ntrip_atlas_get_payment_priority() == NTRIP_PAYMENT_PRIORITY_PAID_FIRST);  // Should remain unchanged
    printf("✅ Invalid priority value correctly rejected\n");

    // Reset to default for subsequent tests
    ntrip_atlas_set_payment_priority(NTRIP_PAYMENT_PRIORITY_FREE_FIRST);
    printf("✅ Payment priority configuration API working correctly\n");
}

/**
 * Test service usability checking (credential validation)
 */
void test_service_usability() {
    printf("\nTest: Service Usability Checking\n");
    printf("==================================\n");

    // Set up credential store
    ntrip_credential_store_t store;
    ntrip_atlas_init_credential_store(&store);

    // Add credentials for Point One Navigation, Inc. (paid service)
    ntrip_atlas_error_t result = ntrip_atlas_add_credential(&store, "Point One Navigation, Inc.", "testuser", "testpass");
    assert(result == NTRIP_ATLAS_SUCCESS);
    printf("✅ Added credentials for Point One Navigation, Inc.\n");

    size_t service_count;
    const ntrip_service_compact_t* services = get_generated_services(&service_count);

    // Find Point One Navigation, Inc. and RTK2go services
    const ntrip_service_compact_t* polaris_service = NULL;
    const ntrip_service_compact_t* rtk2go_service = NULL;

    for (size_t i = 0; i < service_count; i++) {
        if (strstr(services[i].hostname, "polaris.pointonenav.com")) {
            polaris_service = &services[i];
        }
        if (strstr(services[i].hostname, "rtk2go.com")) {
            rtk2go_service = &services[i];
        }
    }

    assert(polaris_service != NULL);
    assert(rtk2go_service != NULL);

    // Test free service usability (RTK2go - should always be usable)
    bool rtk2go_usable_with_store = ntrip_atlas_is_service_usable(rtk2go_service, &store);
    bool rtk2go_usable_without_store = ntrip_atlas_is_service_usable(rtk2go_service, NULL);
    assert(rtk2go_usable_with_store == true);
    assert(rtk2go_usable_without_store == true);
    printf("✅ Free service (RTK2go) is usable with and without credentials\n");

    // Test paid service usability with credentials (Point One Navigation, Inc.)
    bool polaris_usable_with_store = ntrip_atlas_is_service_usable(polaris_service, &store);
    assert(polaris_usable_with_store == true);
    printf("✅ Paid service (Polaris) is usable when credentials available\n");

    // Test paid service usability without credentials
    bool polaris_usable_without_store = ntrip_atlas_is_service_usable(polaris_service, NULL);
    assert(polaris_usable_without_store == false);
    printf("✅ Paid service (Polaris) is not usable without credentials\n");

    // Test paid service usability with store but missing credentials
    ntrip_credential_store_t empty_store;
    ntrip_atlas_init_credential_store(&empty_store);
    bool polaris_usable_empty_store = ntrip_atlas_is_service_usable(polaris_service, &empty_store);
    assert(polaris_usable_empty_store == false);
    printf("✅ Paid service (Polaris) is not usable with empty credential store\n");

    printf("✅ Service usability checking working correctly\n");
}

/**
 * Test FREE_FIRST payment priority ordering
 */
void test_free_first_priority() {
    printf("\nTest: FREE_FIRST Payment Priority\n");
    printf("==================================\n");

    // Set payment priority to FREE_FIRST
    ntrip_atlas_set_payment_priority(NTRIP_PAYMENT_PRIORITY_FREE_FIRST);

    // Set up credential store with credentials for paid services
    ntrip_credential_store_t store;
    ntrip_atlas_init_credential_store(&store);
    ntrip_atlas_add_credential(&store, "Point One Navigation, Inc.", "user", "pass");

    size_t service_count;
    const ntrip_service_compact_t* services = get_generated_services(&service_count);

    // Filter services by payment priority
    ntrip_service_compact_t filtered_services[10];
    size_t filtered_count = ntrip_atlas_filter_services_by_payment_priority(
        services, service_count, &store, NTRIP_PAYMENT_PRIORITY_FREE_FIRST,
        filtered_services, 10
    );

    printf("Found %zu usable services in FREE_FIRST priority order:\n", filtered_count);

    // Validate ordering: free services should come first
    bool found_paid_service = false;
    size_t first_paid_index = SIZE_MAX;
    size_t last_free_index = 0;

    for (size_t i = 0; i < filtered_count; i++) {
        bool is_paid = (filtered_services[i].flags & NTRIP_FLAG_PAID_SERVICE) != 0;
        bool is_usable = ntrip_atlas_is_service_usable(&filtered_services[i], &store);

        assert(is_usable);  // All filtered services should be usable

        if (is_paid && !found_paid_service) {
            found_paid_service = true;
            first_paid_index = i;
        }
        if (!is_paid) {
            last_free_index = i;
        }

        printf("  %zu: %s - %s, Quality: %d\n",
               i+1, filtered_services[i].hostname,
               is_paid ? "PAID" : "FREE", filtered_services[i].quality_rating);
    }

    // Validate that free services come before paid services (if both exist)
    if (found_paid_service) {
        assert(last_free_index < first_paid_index);
        printf("✅ FREE_FIRST priority: free services (0-%zu) come before paid services (%zu-)\n",
               last_free_index, first_paid_index);
    } else {
        printf("✅ FREE_FIRST priority: only free services found (expected when no paid service credentials)\n");
    }

    printf("✅ FREE_FIRST payment priority ordering working correctly\n");
}

/**
 * Test PAID_FIRST payment priority ordering
 */
void test_paid_first_priority() {
    printf("\nTest: PAID_FIRST Payment Priority\n");
    printf("==================================\n");

    // Set payment priority to PAID_FIRST
    ntrip_atlas_set_payment_priority(NTRIP_PAYMENT_PRIORITY_PAID_FIRST);

    // Set up credential store with credentials for paid services
    ntrip_credential_store_t store;
    ntrip_atlas_init_credential_store(&store);
    ntrip_atlas_add_credential(&store, "Point One Navigation, Inc.", "user", "pass");

    size_t service_count;
    const ntrip_service_compact_t* services = get_generated_services(&service_count);

    // Filter services by payment priority
    ntrip_service_compact_t filtered_services[10];
    size_t filtered_count = ntrip_atlas_filter_services_by_payment_priority(
        services, service_count, &store, NTRIP_PAYMENT_PRIORITY_PAID_FIRST,
        filtered_services, 10
    );

    printf("Found %zu usable services in PAID_FIRST priority order:\n", filtered_count);

    // Validate ordering: paid services should come first
    bool found_free_service = false;
    size_t first_free_index = SIZE_MAX;
    size_t last_paid_index = 0;

    for (size_t i = 0; i < filtered_count; i++) {
        bool is_paid = (filtered_services[i].flags & NTRIP_FLAG_PAID_SERVICE) != 0;
        bool is_usable = ntrip_atlas_is_service_usable(&filtered_services[i], &store);

        assert(is_usable);  // All filtered services should be usable

        if (!is_paid && !found_free_service) {
            found_free_service = true;
            first_free_index = i;
        }
        if (is_paid) {
            last_paid_index = i;
        }

        printf("  %zu: %s - %s, Quality: %d\n",
               i+1, filtered_services[i].hostname,
               is_paid ? "PAID" : "FREE", filtered_services[i].quality_rating);
    }

    // Validate that paid services come before free services (if both exist)
    if (found_free_service && last_paid_index != 0) {
        assert(last_paid_index < first_free_index);
        printf("✅ PAID_FIRST priority: paid services (0-%zu) come before free services (%zu-)\n",
               last_paid_index, first_free_index);
    } else {
        printf("✅ PAID_FIRST priority: ordering validated (only one service type found)\n");
    }

    printf("✅ PAID_FIRST payment priority ordering working correctly\n");
}

/**
 * Test credential checking and service skipping
 */
void test_credential_checking_and_skipping() {
    printf("\nTest: Credential Checking and Service Skipping\n");
    printf("===============================================\n");

    size_t service_count;
    const ntrip_service_compact_t* services = get_generated_services(&service_count);

    // Test with empty credential store (no paid services should be returned)
    ntrip_credential_store_t empty_store;
    ntrip_atlas_init_credential_store(&empty_store);

    ntrip_service_compact_t filtered_services[40];
    size_t filtered_count_no_creds = ntrip_atlas_filter_services_by_payment_priority(
        services, service_count, &empty_store, NTRIP_PAYMENT_PRIORITY_FREE_FIRST,
        filtered_services, 40
    );

    printf("With no credentials: %zu usable services\n", filtered_count_no_creds);

    // Verify no paid services are in the results
    for (size_t i = 0; i < filtered_count_no_creds; i++) {
        bool is_paid = (filtered_services[i].flags & NTRIP_FLAG_PAID_SERVICE) != 0;
        assert(!is_paid);
    }
    printf("✅ No paid services returned when credentials not available\n");

    // Test with credentials for paid services
    ntrip_credential_store_t store_with_creds;
    ntrip_atlas_init_credential_store(&store_with_creds);
    ntrip_atlas_add_credential(&store_with_creds, "Point One Navigation, Inc.", "user", "pass");

    size_t filtered_count_with_creds = ntrip_atlas_filter_services_by_payment_priority(
        services, service_count, &store_with_creds, NTRIP_PAYMENT_PRIORITY_FREE_FIRST,
        filtered_services, 40
    );

    printf("With credentials: %zu usable services\n", filtered_count_with_creds);

    // Should have more services when credentials are available
    assert(filtered_count_with_creds > filtered_count_no_creds);
    printf("✅ More services available when credentials provided (%zu vs %zu)\n",
           filtered_count_with_creds, filtered_count_no_creds);

    // Verify paid services are now included
    bool found_paid_service = false;
    for (size_t i = 0; i < filtered_count_with_creds; i++) {
        bool is_paid = (filtered_services[i].flags & NTRIP_FLAG_PAID_SERVICE) != 0;
        if (is_paid) {
            found_paid_service = true;
            printf("  Found paid service: %s\n", filtered_services[i].hostname);
        }
    }
    assert(found_paid_service);
    printf("✅ Paid services included when credentials available\n");

    printf("✅ Credential checking and service skipping working correctly\n");
}

/**
 * Test quality-based ordering within same payment type
 */
void test_quality_based_ordering() {
    printf("\nTest: Quality-Based Ordering Within Payment Types\n");
    printf("==================================================\n");

    // Create a mock set of services with different quality ratings
    ntrip_service_compact_t test_services[] = {
        {
            .hostname = "free_service_3star",
            .flags = 0,  // Free service
            .quality_rating = 3,
            .provider_index = 0  // Will map to a valid provider
        },
        {
            .hostname = "free_service_5star",
            .flags = 0,  // Free service
            .quality_rating = 5,
            .provider_index = 1  // Will map to a valid provider
        },
        {
            .hostname = "paid_service_2star",
            .flags = NTRIP_FLAG_PAID_SERVICE,  // Paid service
            .quality_rating = 2,
            .provider_index = 30  // Point One Navigation, Inc.
        },
        {
            .hostname = "paid_service_4star",
            .flags = NTRIP_FLAG_PAID_SERVICE,  // Paid service
            .quality_rating = 4,
            .provider_index = 30  // Point One Navigation, Inc.
        }
    };

    // Set up credentials for paid services using actual provider name
    ntrip_credential_store_t store;
    ntrip_atlas_init_credential_store(&store);
    ntrip_atlas_add_credential(&store, "Point One Navigation, Inc.", "user", "pass");

    // Test FREE_FIRST ordering
    ntrip_service_compact_t filtered_services[4];
    size_t filtered_count = ntrip_atlas_filter_services_by_payment_priority(
        test_services, 4, &store, NTRIP_PAYMENT_PRIORITY_FREE_FIRST,
        filtered_services, 4
    );

    printf("FREE_FIRST ordering with quality:\n");
    for (size_t i = 0; i < filtered_count; i++) {
        bool is_paid = (filtered_services[i].flags & NTRIP_FLAG_PAID_SERVICE) != 0;
        printf("  %zu: %s - %s, Quality: %d\n",
               i+1, filtered_services[i].hostname,
               is_paid ? "PAID" : "FREE", filtered_services[i].quality_rating);
    }

    // Validate: Free services should come first, ordered by quality (higher first)
    // Expected order: free_5star, free_3star, paid_4star, paid_2star
    assert(strcmp(filtered_services[0].hostname, "free_service_5star") == 0);
    assert(strcmp(filtered_services[1].hostname, "free_service_3star") == 0);
    assert(strcmp(filtered_services[2].hostname, "paid_service_4star") == 0);
    assert(strcmp(filtered_services[3].hostname, "paid_service_2star") == 0);

    printf("✅ FREE_FIRST ordering with quality validation successful\n");

    // Test PAID_FIRST ordering
    filtered_count = ntrip_atlas_filter_services_by_payment_priority(
        test_services, 4, &store, NTRIP_PAYMENT_PRIORITY_PAID_FIRST,
        filtered_services, 4
    );

    printf("PAID_FIRST ordering with quality:\n");
    for (size_t i = 0; i < filtered_count; i++) {
        bool is_paid = (filtered_services[i].flags & NTRIP_FLAG_PAID_SERVICE) != 0;
        printf("  %zu: %s - %s, Quality: %d\n",
               i+1, filtered_services[i].hostname,
               is_paid ? "PAID" : "FREE", filtered_services[i].quality_rating);
    }

    // Validate: Paid services should come first, ordered by quality (higher first)
    // Expected order: paid_4star, paid_2star, free_5star, free_3star
    assert(strcmp(filtered_services[0].hostname, "paid_service_4star") == 0);
    assert(strcmp(filtered_services[1].hostname, "paid_service_2star") == 0);
    assert(strcmp(filtered_services[2].hostname, "free_service_5star") == 0);
    assert(strcmp(filtered_services[3].hostname, "free_service_3star") == 0);

    printf("✅ PAID_FIRST ordering with quality validation successful\n");

    printf("✅ Quality-based ordering within payment types working correctly\n");
}

/**
 * Test hostname validation for placeholder domains
 */
void test_hostname_validation() {
    printf("\nTest: Hostname Validation\n");
    printf("=========================\n");

    // Test service with placeholder hostname - should be unusable
    ntrip_service_compact_t placeholder_service = {
        .hostname = "register.example.com",
        .port = 2101,
        .flags = 0  // Free service
    };

    bool usable = ntrip_atlas_is_service_usable(&placeholder_service, NULL);
    assert(!usable);
    printf("✅ Service with register.example.com hostname correctly filtered out\n");

    // Test service with contact-sales placeholder
    ntrip_service_compact_t contact_service = {
        .hostname = "contact-sales.example.com",
        .port = 2101,
        .flags = 0
    };

    usable = ntrip_atlas_is_service_usable(&contact_service, NULL);
    assert(!usable);
    printf("✅ Service with contact-sales.example.com hostname correctly filtered out\n");

    // Test service with academic placeholder
    ntrip_service_compact_t academic_service = {
        .hostname = "academic.example.com",
        .port = 2101,
        .flags = 0
    };

    usable = ntrip_atlas_is_service_usable(&academic_service, NULL);
    assert(!usable);
    printf("✅ Service with academic.example.com hostname correctly filtered out\n");

    // Test service with valid hostname - should be usable
    ntrip_service_compact_t valid_service = {
        .hostname = "ntrip.ign.gob.ar",
        .port = 2101,
        .flags = 0  // Free service
    };

    usable = ntrip_atlas_is_service_usable(&valid_service, NULL);
    assert(usable);
    printf("✅ Service with valid hostname correctly allowed\n");

    // Test service with localhost - should be filtered
    ntrip_service_compact_t localhost_service = {
        .hostname = "localhost",
        .port = 2101,
        .flags = 0
    };

    usable = ntrip_atlas_is_service_usable(&localhost_service, NULL);
    assert(!usable);
    printf("✅ Service with localhost hostname correctly filtered out\n");

    // Test service with empty hostname - should be filtered
    ntrip_service_compact_t empty_service = {
        .hostname = "",
        .port = 2101,
        .flags = 0
    };

    usable = ntrip_atlas_is_service_usable(&empty_service, NULL);
    assert(!usable);
    printf("✅ Service with empty hostname correctly filtered out\n");

    printf("✅ Hostname validation prevents placeholder domains from being used\n");
}

int main() {
    printf("NTRIP Atlas Payment Priority Tests\n");
    printf("===================================\n");

    test_payment_priority_configuration();
    test_service_usability();
    test_hostname_validation();
    test_free_first_priority();
    test_paid_first_priority();
    test_credential_checking_and_skipping();
    test_quality_based_ordering();

    printf("\n🎉 All payment priority tests passed!\n");
    printf("Payment priority configuration system working correctly:\n");
    printf("- ✅ FREE_FIRST: Free services prioritized, paid as fallback\n");
    printf("- ✅ PAID_FIRST: Paid services prioritized, free as fallback\n");
    printf("- ✅ Hostname validation: Placeholder domains filtered out to prevent connection failures\n");
    printf("- ✅ Credential checking: Paid services skipped without credentials\n");
    printf("- ✅ Quality ordering: Higher quality services within same payment type prioritized\n");
    printf("- ✅ Configuration API: Payment priority can be configured at runtime\n");

    return 0;
}