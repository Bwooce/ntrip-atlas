/**
 * NTRIP Atlas - Geographic Filtering Implementation
 *
 * Filters services based on geographic coverage bounds to eliminate services
 * that don't cover the user's location. Provides distance-based ranking and
 * coverage validation for improved discovery performance.
 *
 * Licensed under MIT License
 */

#define _USE_MATH_DEFINES  // For M_PI on Windows/MSVC
#include "ntrip_atlas.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Ensure M_PI is defined on all platforms
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * Check if user location is within service coverage bounds
 */
bool ntrip_atlas_is_location_within_service_coverage(
    const ntrip_service_compact_t* service,
    double user_latitude,
    double user_longitude
) {
    if (!service) {
        return false;
    }

    // Convert user coordinates to same precision as service bounds (×100)
    // Use rounding instead of truncation for consistent precision
    int16_t user_lat_deg100 = (int16_t)round(user_latitude * 100.0);
    int16_t user_lon_deg100 = (int16_t)round(user_longitude * 100.0);

    // Check if user location is within service coverage bounding box
    bool lat_in_range = (user_lat_deg100 >= service->lat_min_deg100) &&
                        (user_lat_deg100 <= service->lat_max_deg100);

    bool lon_in_range = (user_lon_deg100 >= service->lon_min_deg100) &&
                        (user_lon_deg100 <= service->lon_max_deg100);

    return lat_in_range && lon_in_range;
}

/**
 * Calculate distance from user to service coverage center
 */
double ntrip_atlas_calculate_distance_to_service_center(
    const ntrip_service_compact_t* service,
    double user_latitude,
    double user_longitude
) {
    if (!service) {
        return INFINITY;
    }

    // Calculate service coverage center
    double service_lat_center = (service->lat_min_deg100 + service->lat_max_deg100) / 200.0;
    double service_lon_center = (service->lon_min_deg100 + service->lon_max_deg100) / 200.0;

    // Use existing distance calculation from ntrip_distance.c
    // Haversine formula for great circle distance
    double lat1 = user_latitude * M_PI / 180.0;
    double lon1 = user_longitude * M_PI / 180.0;
    double lat2 = service_lat_center * M_PI / 180.0;
    double lon2 = service_lon_center * M_PI / 180.0;

    double dlat = lat2 - lat1;
    double dlon = lon2 - lon1;

    double a = sin(dlat/2) * sin(dlat/2) +
               cos(lat1) * cos(lat2) * sin(dlon/2) * sin(dlon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));

    const double earth_radius_km = 6371.0;
    return earth_radius_km * c;
}

/**
 * Calculate distance to nearest edge of service coverage area
 */
double ntrip_atlas_calculate_distance_to_coverage_edge(
    const ntrip_service_compact_t* service,
    double user_latitude,
    double user_longitude
) {
    if (!service) {
        return INFINITY;
    }

    // Convert service bounds back to degrees
    double lat_min = service->lat_min_deg100 / 100.0;
    double lat_max = service->lat_max_deg100 / 100.0;
    double lon_min = service->lon_min_deg100 / 100.0;
    double lon_max = service->lon_max_deg100 / 100.0;

    // If user is within coverage, return 0
    if (ntrip_atlas_is_location_within_service_coverage(service, user_latitude, user_longitude)) {
        return 0.0;
    }

    // Find closest point on the coverage rectangle
    double closest_lat = user_latitude;
    double closest_lon = user_longitude;

    // Clamp to coverage bounds
    if (closest_lat < lat_min) closest_lat = lat_min;
    if (closest_lat > lat_max) closest_lat = lat_max;
    if (closest_lon < lon_min) closest_lon = lon_min;
    if (closest_lon > lon_max) closest_lon = lon_max;

    // Calculate distance to closest point
    return ntrip_atlas_calculate_distance_to_service_center(service, closest_lat, closest_lon);
}

/**
 * Filter services by geographic coverage
 */
size_t ntrip_atlas_filter_services_by_coverage(
    const ntrip_service_compact_t* services,
    size_t service_count,
    double user_latitude,
    double user_longitude,
    double max_distance_km,
    ntrip_service_compact_t* filtered_services,
    size_t max_filtered
) {
    if (!services || !filtered_services) {
        return 0;
    }

    size_t filtered_count = 0;

    for (size_t i = 0; i < service_count && filtered_count < max_filtered; i++) {
        const ntrip_service_compact_t* service = &services[i];

        // Check if user location is within service coverage
        bool within_coverage = ntrip_atlas_is_location_within_service_coverage(
            service, user_latitude, user_longitude
        );

        // If user is within coverage, always include the service
        if (within_coverage) {
            filtered_services[filtered_count] = *service;
            filtered_count++;
            continue;
        }

        // If user is outside coverage, check distance to coverage edge
        double distance_to_edge = ntrip_atlas_calculate_distance_to_coverage_edge(
            service, user_latitude, user_longitude
        );

        // Include service if it's within the maximum distance threshold
        if (distance_to_edge <= max_distance_km) {
            filtered_services[filtered_count] = *service;
            filtered_count++;
        }
    }

    return filtered_count;
}

/**
 * Global state for sorting (avoid non-portable qsort_r)
 */
static double g_sort_user_latitude = 0.0;
static double g_sort_user_longitude = 0.0;

/**
 * Sort services by distance to user location
 */
static int compare_services_by_distance(const void* a, const void* b) {
    const ntrip_service_compact_t* service_a = (const ntrip_service_compact_t*)a;
    const ntrip_service_compact_t* service_b = (const ntrip_service_compact_t*)b;

    double dist_a = ntrip_atlas_calculate_distance_to_service_center(
        service_a, g_sort_user_latitude, g_sort_user_longitude
    );
    double dist_b = ntrip_atlas_calculate_distance_to_service_center(
        service_b, g_sort_user_latitude, g_sort_user_longitude
    );

    if (dist_a < dist_b) return -1;
    if (dist_a > dist_b) return 1;
    return 0;
}

/**
 * Filter and sort services by geographic proximity
 */
size_t ntrip_atlas_filter_and_sort_services_by_location(
    ntrip_service_compact_t* services,
    size_t service_count,
    double user_latitude,
    double user_longitude,
    double max_distance_km
) {
    if (!services || service_count == 0) {
        return 0;
    }

    // First pass: Filter by coverage and distance
    size_t filtered_count = 0;
    for (size_t i = 0; i < service_count; i++) {
        bool within_coverage = ntrip_atlas_is_location_within_service_coverage(
            &services[i], user_latitude, user_longitude
        );

        double distance = within_coverage ? 0.0 :
            ntrip_atlas_calculate_distance_to_coverage_edge(
                &services[i], user_latitude, user_longitude
            );

        // Include if within coverage or within distance threshold
        if (within_coverage || distance <= max_distance_km) {
            if (filtered_count != i) {
                services[filtered_count] = services[i];
            }
            filtered_count++;
        }
    }

    // Second pass: Sort filtered services by distance
    if (filtered_count > 1) {
        g_sort_user_latitude = user_latitude;
        g_sort_user_longitude = user_longitude;
        qsort(services, filtered_count, sizeof(ntrip_service_compact_t),
              compare_services_by_distance);
    }

    return filtered_count;
}

/**
 * Get geographic coverage statistics
 */
ntrip_atlas_error_t ntrip_atlas_get_geographic_filtering_stats(
    const ntrip_service_compact_t* services,
    size_t service_count,
    double user_latitude,
    double user_longitude,
    ntrip_geo_filtering_stats_t* stats
) {
    if (!services || !stats) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    memset(stats, 0, sizeof(*stats));
    stats->total_services = service_count;

    for (size_t i = 0; i < service_count; i++) {
        bool within_coverage = ntrip_atlas_is_location_within_service_coverage(
            &services[i], user_latitude, user_longitude
        );

        if (within_coverage) {
            stats->services_with_coverage++;
        }

        double distance = ntrip_atlas_calculate_distance_to_service_center(
            &services[i], user_latitude, user_longitude
        );

        if (i == 0 || distance < stats->nearest_service_distance_km) {
            stats->nearest_service_distance_km = distance;
        }
        if (i == 0 || distance > stats->farthest_service_distance_km) {
            stats->farthest_service_distance_km = distance;
        }
    }

    stats->coverage_percentage = service_count > 0 ?
        (100.0 * stats->services_with_coverage / stats->total_services) : 0.0;

    return NTRIP_ATLAS_SUCCESS;
}