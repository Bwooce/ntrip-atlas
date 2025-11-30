/**
 * NTRIP Atlas - Compact Service Structures
 *
 * Replaces tiered loading with simple runtime field extraction.
 * Reduces memory usage from 248 bytes to 46 bytes per service (81.5% reduction).
 *
 * Licensed under MIT License
 */

#include "ntrip_atlas.h"
#include <string.h>
#include <stdlib.h>

// Global provider string table (shared across all services)
static struct {
    const char* providers[32];
    uint8_t count;
    bool initialized;
} g_provider_table = {0};

/**
 * Initialize provider string table
 */
static void init_provider_table(void) {
    if (g_provider_table.initialized) return;

    // Common NTRIP service providers (build-time known)
    g_provider_table.providers[0] = "RTK2go Community";
    g_provider_table.providers[1] = "Point One Navigation";
    g_provider_table.providers[2] = "Geoscience Australia";
    g_provider_table.providers[3] = "EUREF-IP Network";
    g_provider_table.providers[4] = "Massachusetts DOT";
    g_provider_table.providers[5] = "Finland NLS";
    g_provider_table.providers[6] = "BKG EUREF-IP";
    g_provider_table.providers[7] = "Leica SmartNet";
    g_provider_table.providers[8] = "Trimble VRS Now";
    g_provider_table.providers[9] = "IGS Network";
    g_provider_table.count = 10;
    g_provider_table.initialized = true;
}

/**
 * Get provider index from provider name
 */
static uint8_t get_provider_index(const char* provider) {
    init_provider_table();

    for (uint8_t i = 0; i < g_provider_table.count; i++) {
        if (strcmp(g_provider_table.providers[i], provider) == 0) {
            return i;
        }
    }

    // Add new provider if space available
    if (g_provider_table.count < 32) {
        // In production, this would allocate/copy string
        // For now, just return index for unknown providers
        return 255; // Unknown provider marker
    }

    return 255; // Unknown provider
}

/**
 * Get provider name from index
 */
static const char* get_provider_name(uint8_t index) {
    init_provider_table();

    if (index < g_provider_table.count) {
        return g_provider_table.providers[index];
    }
    return "Unknown Provider";
}

/**
 * Convert full service config to compact format
 */
ntrip_atlas_error_t ntrip_atlas_compress_service(
    const ntrip_service_config_t* full,
    ntrip_service_compact_t* compact
) {
    if (!full || !compact) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    memset(compact, 0, sizeof(*compact));

    // Copy hostname (truncate if needed)
    strncpy(compact->hostname, full->base_url, 31);
    compact->hostname[31] = '\0';

    // Connection info
    compact->port = full->port;

    // Pack flags
    compact->flags = 0;
    if (full->ssl) compact->flags |= NTRIP_FLAG_SSL;
    if (full->auth_method == NTRIP_AUTH_BASIC) compact->flags |= NTRIP_FLAG_AUTH_BASIC;
    if (full->auth_method == NTRIP_AUTH_DIGEST) compact->flags |= NTRIP_FLAG_AUTH_DIGEST;
    if (full->requires_registration) compact->flags |= NTRIP_FLAG_REQUIRES_REG;
    if (full->typical_free_access) compact->flags |= NTRIP_FLAG_FREE_ACCESS;

    // Geographic coverage (convert double to int16 with 0.01 precision)
    compact->lat_min_deg100 = (int16_t)(full->coverage_lat_min * 100.0);
    compact->lat_max_deg100 = (int16_t)(full->coverage_lat_max * 100.0);
    compact->lon_min_deg100 = (int16_t)(full->coverage_lon_min * 100.0);
    compact->lon_max_deg100 = (int16_t)(full->coverage_lon_max * 100.0);

    // Service metadata
    compact->provider_index = get_provider_index(full->provider);
    compact->network_type = (uint8_t)full->network_type;
    compact->quality_rating = full->quality_rating;

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Convert compact service back to full format (for compatibility)
 */
ntrip_atlas_error_t ntrip_atlas_expand_service(
    const ntrip_service_compact_t* compact,
    ntrip_service_config_t* full
) {
    if (!compact || !full) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    memset(full, 0, sizeof(*full));

    // Connection info
    strncpy(full->base_url, compact->hostname, NTRIP_ATLAS_MAX_URL_LEN - 1);
    full->port = compact->port;
    full->ssl = (compact->flags & NTRIP_FLAG_SSL) ? 1 : 0;

    // Authentication
    if (compact->flags & NTRIP_FLAG_AUTH_DIGEST) {
        full->auth_method = NTRIP_AUTH_DIGEST;
    } else if (compact->flags & NTRIP_FLAG_AUTH_BASIC) {
        full->auth_method = NTRIP_AUTH_BASIC;
    } else {
        full->auth_method = NTRIP_AUTH_NONE;
    }

    full->requires_registration = (compact->flags & NTRIP_FLAG_REQUIRES_REG) ? 1 : 0;
    full->typical_free_access = (compact->flags & NTRIP_FLAG_FREE_ACCESS) ? 1 : 0;

    // Geographic coverage (convert int16 back to double)
    full->coverage_lat_min = compact->lat_min_deg100 / 100.0;
    full->coverage_lat_max = compact->lat_max_deg100 / 100.0;
    full->coverage_lon_min = compact->lon_min_deg100 / 100.0;
    full->coverage_lon_max = compact->lon_max_deg100 / 100.0;

    // Service metadata
    const char* provider = get_provider_name(compact->provider_index);
    strncpy(full->provider, provider, NTRIP_ATLAS_MAX_PROVIDER - 1);
    full->network_type = (ntrip_network_type_t)compact->network_type;
    full->quality_rating = compact->quality_rating;

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Get memory usage statistics for compact vs full services
 */
ntrip_atlas_error_t ntrip_atlas_get_compact_memory_stats(
    size_t service_count,
    size_t* full_bytes,
    size_t* compact_bytes,
    size_t* savings_bytes
) {
    if (full_bytes) {
        *full_bytes = service_count * sizeof(ntrip_service_config_t);
    }

    if (compact_bytes) {
        // Compact services + shared provider table
        *compact_bytes = service_count * sizeof(ntrip_service_compact_t) +
                        (g_provider_table.count * 32); // Estimated provider table size
    }

    if (savings_bytes && full_bytes && compact_bytes) {
        *savings_bytes = *full_bytes - *compact_bytes;
    }

    return NTRIP_ATLAS_SUCCESS;
}