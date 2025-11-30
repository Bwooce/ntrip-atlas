/**
 * NTRIP Atlas - Geographic Blacklisting Implementation
 *
 * Tracks services that report no coverage in specific geographic areas
 * to avoid repeated queries. Improves discovery performance by learning
 * from service coverage limitations.
 *
 * Licensed under MIT License
 */

#include "ntrip_atlas.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// Maximum geographic blacklist entries per service
#define MAX_BLACKLIST_ENTRIES_PER_SERVICE 8

// Geographic region size for blacklisting (degrees)
// Using 1-degree grid squares for reasonable granularity
#define BLACKLIST_GRID_SIZE_DEGREES 1.0

// Global geographic blacklist state
static struct {
    ntrip_geo_blacklist_entry_t entries[NTRIP_MAX_SERVICES][MAX_BLACKLIST_ENTRIES_PER_SERVICE];
    uint8_t entry_counts[NTRIP_MAX_SERVICES];
    bool initialized;
} g_geo_blacklist = {0};

/**
 * Convert lat/lon to grid coordinates for blacklisting
 */
static void lat_lon_to_grid(double latitude, double longitude, int16_t* grid_lat, int16_t* grid_lon) {
    // Convert to grid coordinates (1 degree resolution)
    // We want coordinates within the same 1-degree square to map to the same grid cell:
    // Examples:
    //   40.0 to 40.999 → grid 40
    //   -74.0 to -74.999 → grid -74
    //   -85.0 to -85.999 → grid -85
    //
    // For positive coordinates: floor() works correctly
    // For negative coordinates: we need ceil() since we want -85.2 to map to -85, not -86
    if (latitude >= 0) {
        *grid_lat = (int16_t)floor(latitude);
    } else {
        *grid_lat = (int16_t)ceil(latitude - 1.0);
    }

    if (longitude >= 0) {
        *grid_lon = (int16_t)floor(longitude);
    } else {
        *grid_lon = (int16_t)ceil(longitude - 1.0);
    }
}

/**
 * Get service index from provider name
 */
static uint8_t get_service_index_by_provider(const char* provider) {
    // This would integrate with the existing service index mapping
    // For now, use a simple hash of provider name
    if (!provider) return 255;

    uint8_t hash = 0;
    for (int i = 0; provider[i] && i < 32; i++) {
        hash = (hash * 33) + provider[i];
    }
    return hash % NTRIP_MAX_SERVICES;
}

/**
 * Initialize geographic blacklist system
 */
ntrip_atlas_error_t ntrip_atlas_init_geographic_blacklist(void) {
    if (g_geo_blacklist.initialized) {
        return NTRIP_ATLAS_SUCCESS;
    }

    // Clear all blacklist entries
    memset(&g_geo_blacklist, 0, sizeof(g_geo_blacklist));
    g_geo_blacklist.initialized = true;

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Add a geographic region to service blacklist
 */
ntrip_atlas_error_t ntrip_atlas_blacklist_service_region(
    const char* provider,
    double latitude,
    double longitude,
    const char* error_reason
) {
    if (!provider || !g_geo_blacklist.initialized) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    uint8_t service_index = get_service_index_by_provider(provider);
    if (service_index >= NTRIP_MAX_SERVICES) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    // Convert to grid coordinates
    int16_t grid_lat, grid_lon;
    lat_lon_to_grid(latitude, longitude, &grid_lat, &grid_lon);

    // Check if already blacklisted
    for (uint8_t i = 0; i < g_geo_blacklist.entry_counts[service_index]; i++) {
        ntrip_geo_blacklist_entry_t* entry = &g_geo_blacklist.entries[service_index][i];
        if (entry->grid_lat == grid_lat && entry->grid_lon == grid_lon) {
            // Already blacklisted, update reason and timestamp
            strncpy(entry->reason, error_reason ? error_reason : "No coverage",
                   sizeof(entry->reason) - 1);
            entry->reason[sizeof(entry->reason) - 1] = '\0';
            entry->blacklisted_time = time(NULL);
            return NTRIP_ATLAS_SUCCESS;
        }
    }

    // Add new blacklist entry if space available
    if (g_geo_blacklist.entry_counts[service_index] < MAX_BLACKLIST_ENTRIES_PER_SERVICE) {
        uint8_t index = g_geo_blacklist.entry_counts[service_index];
        ntrip_geo_blacklist_entry_t* entry = &g_geo_blacklist.entries[service_index][index];

        entry->grid_lat = grid_lat;
        entry->grid_lon = grid_lon;
        strncpy(entry->reason, error_reason ? error_reason : "No coverage",
               sizeof(entry->reason) - 1);
        entry->reason[sizeof(entry->reason) - 1] = '\0';
        entry->blacklisted_time = time(NULL);

        g_geo_blacklist.entry_counts[service_index]++;
        return NTRIP_ATLAS_SUCCESS;
    }

    // Blacklist is full - replace oldest entry (LRU)
    uint8_t oldest_index = 0;
    time_t oldest_time = g_geo_blacklist.entries[service_index][0].blacklisted_time;

    for (uint8_t i = 1; i < MAX_BLACKLIST_ENTRIES_PER_SERVICE; i++) {
        if (g_geo_blacklist.entries[service_index][i].blacklisted_time < oldest_time) {
            oldest_time = g_geo_blacklist.entries[service_index][i].blacklisted_time;
            oldest_index = i;
        }
    }

    // Replace oldest entry
    ntrip_geo_blacklist_entry_t* entry = &g_geo_blacklist.entries[service_index][oldest_index];
    entry->grid_lat = grid_lat;
    entry->grid_lon = grid_lon;
    strncpy(entry->reason, error_reason ? error_reason : "No coverage",
           sizeof(entry->reason) - 1);
    entry->reason[sizeof(entry->reason) - 1] = '\0';
    entry->blacklisted_time = time(NULL);

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Check if a service is blacklisted for a geographic region
 */
bool ntrip_atlas_is_service_geographically_blacklisted(
    const char* provider,
    double latitude,
    double longitude
) {
    if (!provider || !g_geo_blacklist.initialized) {
        return false;
    }

    uint8_t service_index = get_service_index_by_provider(provider);
    if (service_index >= NTRIP_MAX_SERVICES) {
        return false;
    }

    // Convert to grid coordinates
    int16_t grid_lat, grid_lon;
    lat_lon_to_grid(latitude, longitude, &grid_lat, &grid_lon);

    // Check if blacklisted
    for (uint8_t i = 0; i < g_geo_blacklist.entry_counts[service_index]; i++) {
        ntrip_geo_blacklist_entry_t* entry = &g_geo_blacklist.entries[service_index][i];
        if (entry->grid_lat == grid_lat && entry->grid_lon == grid_lon) {
            return true;
        }
    }

    return false;
}

/**
 * Remove geographic blacklist entry for a service
 */
ntrip_atlas_error_t ntrip_atlas_remove_geographic_blacklist(
    const char* provider,
    double latitude,
    double longitude
) {
    if (!provider || !g_geo_blacklist.initialized) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    uint8_t service_index = get_service_index_by_provider(provider);
    if (service_index >= NTRIP_MAX_SERVICES) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    // Convert to grid coordinates
    int16_t grid_lat, grid_lon;
    lat_lon_to_grid(latitude, longitude, &grid_lat, &grid_lon);

    // Find and remove entry
    for (uint8_t i = 0; i < g_geo_blacklist.entry_counts[service_index]; i++) {
        ntrip_geo_blacklist_entry_t* entry = &g_geo_blacklist.entries[service_index][i];
        if (entry->grid_lat == grid_lat && entry->grid_lon == grid_lon) {
            // Remove by shifting remaining entries
            for (uint8_t j = i; j < g_geo_blacklist.entry_counts[service_index] - 1; j++) {
                g_geo_blacklist.entries[service_index][j] =
                    g_geo_blacklist.entries[service_index][j + 1];
            }
            g_geo_blacklist.entry_counts[service_index]--;
            return NTRIP_ATLAS_SUCCESS;
        }
    }

    return NTRIP_ATLAS_ERROR_NOT_FOUND;
}

/**
 * Clear all geographic blacklist entries for a service
 */
ntrip_atlas_error_t ntrip_atlas_clear_service_geographic_blacklist(const char* provider) {
    if (!provider || !g_geo_blacklist.initialized) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    uint8_t service_index = get_service_index_by_provider(provider);
    if (service_index >= NTRIP_MAX_SERVICES) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    g_geo_blacklist.entry_counts[service_index] = 0;
    memset(g_geo_blacklist.entries[service_index], 0,
           sizeof(g_geo_blacklist.entries[service_index]));

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Clear all geographic blacklist entries (for testing/reset)
 */
ntrip_atlas_error_t ntrip_atlas_clear_all_geographic_blacklists(void) {
    if (!g_geo_blacklist.initialized) {
        return NTRIP_ATLAS_ERROR_PLATFORM;
    }

    memset(g_geo_blacklist.entry_counts, 0, sizeof(g_geo_blacklist.entry_counts));
    memset(g_geo_blacklist.entries, 0, sizeof(g_geo_blacklist.entries));

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Get blacklist statistics for monitoring
 */
ntrip_atlas_error_t ntrip_atlas_get_geographic_blacklist_stats(
    ntrip_geo_blacklist_stats_t* stats
) {
    if (!stats || !g_geo_blacklist.initialized) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    memset(stats, 0, sizeof(*stats));

    // Count total entries and services with blacklists
    for (uint8_t i = 0; i < NTRIP_MAX_SERVICES; i++) {
        if (g_geo_blacklist.entry_counts[i] > 0) {
            stats->services_with_blacklists++;
            stats->total_blacklisted_regions += g_geo_blacklist.entry_counts[i];
        }
    }

    stats->max_entries_per_service = MAX_BLACKLIST_ENTRIES_PER_SERVICE;
    stats->grid_size_degrees = BLACKLIST_GRID_SIZE_DEGREES;

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Filter service list to remove geographically blacklisted services
 */
size_t ntrip_atlas_filter_geographically_blacklisted_services(
    const ntrip_service_compact_t* services,
    size_t service_count,
    double latitude,
    double longitude,
    ntrip_service_compact_t* filtered_services,
    size_t max_filtered
) {
    if (!services || !filtered_services || !g_geo_blacklist.initialized) {
        return 0;
    }

    size_t filtered_count = 0;

    for (size_t i = 0; i < service_count && filtered_count < max_filtered; i++) {
        // Use service index as provider identifier for geographic blacklist lookup
        char provider_id[16];
        snprintf(provider_id, sizeof(provider_id), "service_%zu", i);

        // Skip if geographically blacklisted
        if (!ntrip_atlas_is_service_geographically_blacklisted(provider_id, latitude, longitude)) {
            filtered_services[filtered_count] = services[i];
            filtered_count++;
        }
    }

    return filtered_count;
}