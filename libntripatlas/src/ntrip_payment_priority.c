/**
 * NTRIP Atlas - Payment Priority Management
 *
 * Configurable service prioritization based on payment model:
 * - FREE_FIRST: Try free services first, paid services as fallback
 * - PAID_FIRST: Try paid services first, free services as fallback
 *
 * Includes credential checking to skip paid services without credentials.
 *
 * Licensed under MIT License
 */

#include "ntrip_atlas.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Global payment priority configuration
static ntrip_payment_priority_t g_payment_priority = NTRIP_PAYMENT_PRIORITY_FREE_FIRST;

// Forward declaration for generated provider lookup function
extern const char* get_provider_name(uint8_t provider_index);

/**
 * Helper function to get service provider name from compact service
 * Uses the generated provider index lookup table
 */
static const char* get_service_provider_name(const ntrip_service_compact_t* service) {
    return get_provider_name(service->provider_index);
}

/**
 * Set global payment priority configuration
 */
ntrip_atlas_error_t ntrip_atlas_set_payment_priority(ntrip_payment_priority_t priority) {
    if (priority != NTRIP_PAYMENT_PRIORITY_FREE_FIRST &&
        priority != NTRIP_PAYMENT_PRIORITY_PAID_FIRST) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    g_payment_priority = priority;
    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Get current payment priority configuration
 */
ntrip_payment_priority_t ntrip_atlas_get_payment_priority(void) {
    return g_payment_priority;
}

/**
 * Check if hostname is a placeholder that should be filtered out
 */
static bool is_placeholder_hostname(const char* hostname) {
    if (!hostname) return true;

    // Check for common placeholder domains
    if (strstr(hostname, "example.com") != NULL) return true;
    if (strstr(hostname, "register.example") != NULL) return true;
    if (strstr(hostname, "contact-sales.example") != NULL) return true;
    if (strstr(hostname, "academic.example") != NULL) return true;

    // Check for obviously invalid hostnames
    if (strlen(hostname) == 0) return true;
    if (strcmp(hostname, "localhost") == 0) return true;
    if (strcmp(hostname, "127.0.0.1") == 0) return true;

    return false;
}

/**
 * Check if a service requires credentials and if we have them
 */
bool ntrip_atlas_is_service_usable(const ntrip_service_compact_t* service,
                                  const ntrip_credential_store_t* store) {
    if (!service) return false;

    // Filter out services with placeholder hostnames
    if (is_placeholder_hostname(service->hostname)) {
        return false;
    }

    // If service doesn't require payment, it's always usable
    if (!(service->flags & NTRIP_FLAG_PAID_SERVICE)) {
        return true;
    }

    // If service is paid but we don't have a credential store, it's unusable
    if (!store) {
        return false;
    }

    // Check if we have credentials for this paid service
    const char* provider_name = get_service_provider_name(service);
    return ntrip_atlas_has_credentials(store, provider_name);
}

/**
 * Compare function for service sorting by payment priority
 */
static int compare_services_by_payment_priority(
    const void* a,
    const void* b,
    const ntrip_credential_store_t* store,
    ntrip_payment_priority_t priority
) {
    const ntrip_service_compact_t* service_a = (const ntrip_service_compact_t*)a;
    const ntrip_service_compact_t* service_b = (const ntrip_service_compact_t*)b;

    bool a_is_paid = (service_a->flags & NTRIP_FLAG_PAID_SERVICE) != 0;
    bool b_is_paid = (service_b->flags & NTRIP_FLAG_PAID_SERVICE) != 0;

    bool a_usable = ntrip_atlas_is_service_usable(service_a, store);
    bool b_usable = ntrip_atlas_is_service_usable(service_b, store);

    // Unusable services always go to the end
    if (!a_usable && b_usable) return 1;
    if (a_usable && !b_usable) return -1;
    if (!a_usable && !b_usable) return 0;

    // Both services are usable, apply payment priority
    if (priority == NTRIP_PAYMENT_PRIORITY_FREE_FIRST) {
        // Free services first, then paid
        if (!a_is_paid && b_is_paid) return -1;  // a (free) comes before b (paid)
        if (a_is_paid && !b_is_paid) return 1;   // b (free) comes before a (paid)
    } else {
        // Paid services first, then free
        if (a_is_paid && !b_is_paid) return -1;  // a (paid) comes before b (free)
        if (!a_is_paid && b_is_paid) return 1;   // b (paid) comes before a (free)
    }

    // Same payment type - maintain relative order based on quality
    if (service_a->quality_rating != service_b->quality_rating) {
        return service_b->quality_rating - service_a->quality_rating;  // Higher quality first
    }

    return 0;  // Equal
}

/**
 * Filter services by payment priority and credential availability
 */
size_t ntrip_atlas_filter_services_by_payment_priority(
    const ntrip_service_compact_t* services,
    size_t service_count,
    const ntrip_credential_store_t* store,
    ntrip_payment_priority_t priority,
    ntrip_service_compact_t* filtered_services,
    size_t max_filtered
) {
    if (!services || !filtered_services || service_count == 0 || max_filtered == 0) {
        return 0;
    }

    size_t filtered_count = 0;

    // Phase 1: Copy all usable services
    for (size_t i = 0; i < service_count && filtered_count < max_filtered; i++) {
        if (ntrip_atlas_is_service_usable(&services[i], store)) {
            memcpy(&filtered_services[filtered_count], &services[i], sizeof(ntrip_service_compact_t));
            filtered_count++;
        }
    }

    if (filtered_count == 0) {
        return 0;
    }

    // Phase 2: Sort by payment priority
    // We'll implement a simple bubble sort for now since service counts are small
    for (size_t i = 0; i < filtered_count - 1; i++) {
        for (size_t j = 0; j < filtered_count - 1 - i; j++) {
            if (compare_services_by_payment_priority(
                &filtered_services[j],
                &filtered_services[j + 1],
                store,
                priority
            ) > 0) {
                // Swap services
                ntrip_service_compact_t temp;
                memcpy(&temp, &filtered_services[j], sizeof(ntrip_service_compact_t));
                memcpy(&filtered_services[j], &filtered_services[j + 1], sizeof(ntrip_service_compact_t));
                memcpy(&filtered_services[j + 1], &temp, sizeof(ntrip_service_compact_t));
            }
        }
    }

    return filtered_count;
}