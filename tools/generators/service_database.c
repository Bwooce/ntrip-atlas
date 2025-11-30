/**
 * NTRIP Atlas Sample Service Database
 *
 * Real-world NTRIP services with geographic coverage for spatial indexing
 * This simulates what would be generated from YAML service definitions
 */

#include <stdint.h>
#include <stdbool.h>
#include "../../libntripatlas/include/ntrip_atlas.h"

// Sample NTRIP services with real-world geographic coverage
static const ntrip_service_compact_t sample_services[] = {
    // RTK2GO - Global community network (multiple regional coverage)
    {
        .provider_index = 0, // RTK2go Community
        .hostname = "rtk2go.com",
        .port = 2101,
        .flags = NTRIP_FLAG_FREE_ACCESS,
        .lat_min_deg100 = -9000,  // Global coverage: -90° to +90°
        .lat_max_deg100 = 9000,
        .lon_min_deg100 = -18000, // Global coverage: -180° to +180°
        .lon_max_deg100 = 18000,
        .network_type = 3,        // Community
        .quality_rating = 3       // Community rating
    },

    // Point One Polaris - Global commercial network
    {
        .provider_index = 1, // Point One Navigation
        .hostname = "polaris.pointonenav.com",
        .port = 2101,
        .flags = NTRIP_FLAG_AUTH_BASIC,
        .lat_min_deg100 = -9000,  // Global coverage
        .lat_max_deg100 = 9000,
        .lon_min_deg100 = -18000,
        .lon_max_deg100 = 18000,
        .network_type = 2,        // Commercial
        .quality_rating = 4       // Commercial rating
    },

    // Geoscience Australia - Australian continental coverage
    {
        .provider_index = 2, // Geoscience Australia
        .hostname = "auscors.ga.gov.au",
        .port = 2101,
        .flags = NTRIP_FLAG_FREE_ACCESS,
        .lat_min_deg100 = -4500,  // Australia: -45° to -10° latitude
        .lat_max_deg100 = -1000,
        .lon_min_deg100 = 11000,  // Australia: 110° to 160° longitude
        .lon_max_deg100 = 16000,
        .network_type = 1,        // Government
        .quality_rating = 5       // Government rating
    },

    // BKG EUREF-IP - European coverage
    {
        .provider_index = 6, // BKG EUREF-IP
        .hostname = "igs-ip.net",
        .port = 2101,
        .flags = NTRIP_FLAG_FREE_ACCESS,
        .lat_min_deg100 = 3500,   // Europe: 35° to 71° latitude
        .lat_max_deg100 = 7100,
        .lon_min_deg100 = -1000,  // Europe: -10° to 40° longitude
        .lon_max_deg100 = 4000,
        .network_type = 1,        // Government
        .quality_rating = 5       // Government rating
    },

    // Massachusetts MaCORS - Massachusetts state coverage
    {
        .provider_index = 4, // Massachusetts DOT
        .hostname = "radio-labs.com",
        .port = 2101,
        .flags = NTRIP_FLAG_FREE_ACCESS,
        .lat_min_deg100 = 4142,   // Massachusetts: 41.42° to 42.89° latitude
        .lat_max_deg100 = 4289,
        .lon_min_deg100 = -7330,  // Massachusetts: -73.3° to -69.9° longitude
        .lon_max_deg100 = -6990,
        .network_type = 1,        // Government
        .quality_rating = 5       // Government rating
    },

    // Finland FinnRef - Finland coverage
    {
        .provider_index = 5, // Finland NLS
        .hostname = "ntrip.nls.fi",
        .port = 2101,
        .flags = NTRIP_FLAG_FREE_ACCESS,
        .lat_min_deg100 = 5990,   // Finland: 59.9° to 70.1° latitude
        .lat_max_deg100 = 7010,
        .lon_min_deg100 = 1950,   // Finland: 19.5° to 31.6° longitude
        .lon_max_deg100 = 3160,
        .network_type = 1,        // Government
        .quality_rating = 5       // Government rating
    },

    // California CORS - California state coverage
    {
        .provider_index = 0, // RTK2go Community (California nodes)
        .hostname = "rtk2go.com",
        .port = 2101,
        .flags = NTRIP_FLAG_FREE_ACCESS,
        .lat_min_deg100 = 3250,   // California: 32.5° to 42.0° latitude
        .lat_max_deg100 = 4200,
        .lon_min_deg100 = -12440, // California: -124.4° to -114.1° longitude
        .lon_max_deg100 = -11410,
        .network_type = 3,        // Community
        .quality_rating = 3       // Community rating
    },

    // Japan GEONET - Japan coverage
    {
        .provider_index = 0, // RTK2go Community (Japan nodes)
        .hostname = "rtk2go.com",
        .port = 2101,
        .flags = NTRIP_FLAG_FREE_ACCESS,
        .lat_min_deg100 = 2400,   // Japan: 24° to 46° latitude
        .lat_max_deg100 = 4600,
        .lon_min_deg100 = 12900,  // Japan: 129° to 146° longitude
        .lon_max_deg100 = 14600,
        .network_type = 3,        // Community
        .quality_rating = 4       // High-quality community
    }
};

#define SAMPLE_SERVICE_COUNT (sizeof(sample_services) / sizeof(sample_services[0]))

const ntrip_service_compact_t* get_sample_services(size_t* count) {
    *count = SAMPLE_SERVICE_COUNT;
    return sample_services;
}