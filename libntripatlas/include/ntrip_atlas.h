/**
 * NTRIP Atlas - Global NTRIP Service Discovery Library
 *
 * Copyright (c) 2024 NTRIP Atlas Contributors
 * Licensed under MIT License
 *
 * Platform-agnostic C library for automatic NTRIP service discovery,
 * intelligent base station selection, and secure credential management.
 */

#ifndef NTRIP_ATLAS_H
#define NTRIP_ATLAS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Library version
#define NTRIP_ATLAS_VERSION_MAJOR 1
#define NTRIP_ATLAS_VERSION_MINOR 0
#define NTRIP_ATLAS_VERSION_PATCH 0

// Configuration limits
#define NTRIP_ATLAS_MAX_URL_LEN     128
#define NTRIP_ATLAS_MAX_MOUNTPOINT  32
#define NTRIP_ATLAS_MAX_USERNAME    64
#define NTRIP_ATLAS_MAX_PASSWORD    64
#define NTRIP_ATLAS_MAX_PROVIDER    64
#define NTRIP_ATLAS_MAX_COUNTRY     4
#define NTRIP_ATLAS_MAX_FORMAT      32
#define NTRIP_ATLAS_MAX_DETAILS     128

// Memory allocation strategy
#ifdef NTRIP_ATLAS_STATIC_ALLOCATION
    #define NTRIP_ATLAS_MAX_SERVICES    64
    #define NTRIP_ATLAS_MAX_MOUNTPOINTS 256
#endif

/**
 * Error codes
 */
typedef enum {
    NTRIP_ATLAS_SUCCESS = 0,
    NTRIP_ATLAS_ERROR_INVALID_PARAM = -1,
    NTRIP_ATLAS_ERROR_NO_SERVICES = -2,
    NTRIP_ATLAS_ERROR_NO_NETWORK = -3,
    NTRIP_ATLAS_ERROR_AUTH_FAILED = -4,
    NTRIP_ATLAS_ERROR_INVALID_RESPONSE = -5,
    NTRIP_ATLAS_ERROR_DISTANCE_LIMIT = -6,
    NTRIP_ATLAS_ERROR_NO_MEMORY = -7,
    NTRIP_ATLAS_ERROR_TIMEOUT = -8,
    NTRIP_ATLAS_ERROR_PLATFORM = -9,
    NTRIP_ATLAS_ERROR_SERVICE_FAILED = -10,
    NTRIP_ATLAS_ERROR_ALL_SERVICES_FAILED = -11
} ntrip_atlas_error_t;

/**
 * Service network types
 */
typedef enum {
    NTRIP_NETWORK_GOVERNMENT = 0,
    NTRIP_NETWORK_COMMERCIAL = 1,
    NTRIP_NETWORK_COMMUNITY = 2,
    NTRIP_NETWORK_RESEARCH = 3
} ntrip_network_type_t;

/**
 * Authentication methods
 */
typedef enum {
    NTRIP_AUTH_NONE = 0,
    NTRIP_AUTH_BASIC = 1,
    NTRIP_AUTH_DIGEST = 2
} ntrip_auth_method_t;

/**
 * Service configuration (compiled-in static data)
 */
typedef struct {
    char provider[NTRIP_ATLAS_MAX_PROVIDER];
    char country[NTRIP_ATLAS_MAX_COUNTRY];
    char base_url[NTRIP_ATLAS_MAX_URL_LEN];
    uint16_t port;
    uint8_t ssl;
    ntrip_network_type_t network_type;
    ntrip_auth_method_t auth_method;
    uint8_t requires_registration;
    uint8_t typical_free_access;
    uint8_t quality_rating;  // 1-5 stars

    // Geographic coverage
    double coverage_lat_min;
    double coverage_lat_max;
    double coverage_lon_min;
    double coverage_lon_max;
} ntrip_service_config_t;

/**
 * Mountpoint information (discovered at runtime)
 */
typedef struct {
    char mountpoint[NTRIP_ATLAS_MAX_MOUNTPOINT];
    char identifier[64];     // Human-readable location name
    double latitude;
    double longitude;
    char format[NTRIP_ATLAS_MAX_FORMAT];
    char format_details[NTRIP_ATLAS_MAX_DETAILS];
    char nav_system[32];     // GPS+GLONASS+Galileo
    char receiver_type[64];  // Hardware generating data
    uint16_t bitrate;
    uint8_t nmea_required;
    ntrip_auth_method_t authentication;
    uint8_t fee_required;

    // Runtime calculated fields
    double distance_km;
    uint8_t suitability_score;
    const ntrip_service_config_t* service;
} ntrip_mountpoint_t;

/**
 * Best service selection result
 * NOTE: All essential mountpoint data is copied inline to avoid pointer lifetime issues.
 *       Pointers to config/mountpoint structs are only valid during discovery.
 */
typedef struct {
    char server[NTRIP_ATLAS_MAX_URL_LEN];
    uint16_t port;
    uint8_t ssl;
    char mountpoint[NTRIP_ATLAS_MAX_MOUNTPOINT];
    char username[NTRIP_ATLAS_MAX_USERNAME];
    char password[NTRIP_ATLAS_MAX_PASSWORD];
    double distance_km;
    uint8_t quality_score;

    // Essential mountpoint information copied inline (fixes pointer lifetime bug)
    double mountpoint_latitude;
    double mountpoint_longitude;
    char format[NTRIP_ATLAS_MAX_FORMAT];
    uint8_t nmea_required;

    const ntrip_service_config_t* service_info;  // Still available for reference
} ntrip_best_service_t;

/**
 * Selection criteria for filtering services
 */
typedef struct {
    // Technical requirements
    char required_formats[64];   // "RTCM 3.2" or "RTCM 3.2,RTCM 3.1"
    char required_systems[32];   // "GPS" or "GPS+GLONASS"
    uint16_t min_bitrate;        // Minimum data rate
    ntrip_auth_method_t max_auth; // Maximum auth complexity
    uint8_t free_only;           // Exclude paid services

    // Geographic constraints
    double max_distance_km;      // Maximum distance from user

    // Quality requirements
    uint8_t min_quality_rating;  // Minimum 1-5 stars
    ntrip_network_type_t preferred_network; // Prefer govt/commercial/community
} ntrip_selection_criteria_t;

/**
 * Service failure tracking for exponential backoff
 */
typedef struct {
    char service_id[64];         // Unique service identifier
    uint32_t failure_count;      // Number of consecutive failures
    uint32_t first_failure_time; // Timestamp of first failure (seconds since epoch)
    uint32_t next_retry_time;    // When service can be retried again
    uint32_t backoff_seconds;    // Current backoff period
} ntrip_service_failure_t;

/**
 * Failure tracking configuration
 */
typedef struct {
    // Exponential backoff intervals (seconds): 1h, 4h, 12h, 1d, 3d, 1w, 2w, 1m
    uint32_t backoff_intervals[8];
    uint8_t max_backoff_level;   // Max index in backoff_intervals
    uint8_t failure_tracking_enabled; // Enable/disable failure tracking
} ntrip_failure_config_t;

/**
 * Streaming callback context
 */
typedef struct {
    void* user_data;           // User-provided context
    void* internal_state;      // Internal parser state
} ntrip_stream_context_t;

/**
 * HTTP streaming callback function
 *
 * Called repeatedly with chunks of HTTP response data as they arrive.
 * Enables processing of large sourcetables without buffering entire response.
 *
 * @param chunk      Pointer to data chunk (not null-terminated)
 * @param len        Length of data chunk in bytes
 * @param context    User context pointer passed to http_stream
 * @return          0 to continue receiving, non-zero to stop streaming
 */
typedef int (*ntrip_stream_callback_t)(
    const char* chunk,
    size_t len,
    void* context
);

/**
 * Platform abstraction interface (v2.0 - streaming support)
 */
typedef struct {
    uint16_t interface_version;  // Set to 2 for streaming support

    // Streaming HTTP/HTTPS for sourcetable discovery
    // Replaces old http_request buffer-based approach
    int (*http_stream)(
        const char* host,
        uint16_t port,
        uint8_t ssl,
        const char* path,
        ntrip_stream_callback_t on_data,
        void* user_context,
        uint32_t timeout_ms
    );

    // NMEA sentence sending for VRS networks
    // Called after successful mountpoint connection
    int (*send_nmea)(
        void* connection,
        const char* nmea_sentence
    );

    // Secure credential storage
    int (*store_credential)(const char* key, const char* value);
    int (*load_credential)(const char* key, char* value, size_t max_len);

    // Failure tracking persistence (optional, can be NULL)
    int (*store_failure_data)(const char* service_id, const ntrip_service_failure_t* failure);
    int (*load_failure_data)(const char* service_id, ntrip_service_failure_t* failure);
    int (*clear_failure_data)(const char* service_id);

    // Logging interface
    void (*log_message)(int level, const char* message);

    // Time functions
    uint32_t (*get_time_ms)(void);
    uint32_t (*get_time_seconds)(void);  // For failure tracking timestamps
} ntrip_platform_t;

/**
 * Core API Functions
 */

/**
 * Initialize NTRIP Atlas with platform-specific callbacks
 */
ntrip_atlas_error_t ntrip_atlas_init(const ntrip_platform_t* platform);

/**
 * Find best NTRIP service for given coordinates
 */
ntrip_atlas_error_t ntrip_atlas_find_best(ntrip_best_service_t* result,
                                         double latitude, double longitude);

/**
 * Find best service with custom selection criteria
 */
ntrip_atlas_error_t ntrip_atlas_find_best_filtered(ntrip_best_service_t* result,
                                                  double latitude, double longitude,
                                                  const ntrip_selection_criteria_t* criteria);

/**
 * Find best service with fallback option
 */
ntrip_atlas_error_t ntrip_atlas_find_best_with_fallback(ntrip_best_service_t* primary,
                                                       ntrip_best_service_t* fallback,
                                                       double latitude, double longitude);

/**
 * Store credentials for a specific service provider
 */
ntrip_atlas_error_t ntrip_atlas_set_credentials(const char* service_id,
                                               const char* username,
                                               const char* password);

/**
 * Test connectivity to a specific service
 */
ntrip_atlas_error_t ntrip_atlas_test_service(const ntrip_best_service_t* service);

/**
 * Get all available services in a region
 */
ntrip_atlas_error_t ntrip_atlas_list_services_in_region(double lat_min, double lat_max,
                                                       double lon_min, double lon_max,
                                                       ntrip_service_config_t* services,
                                                       size_t* count, size_t max_count);

/**
 * Get detailed information about a specific service
 */
ntrip_atlas_error_t ntrip_atlas_get_service_info(const char* service_id,
                                                ntrip_service_config_t* info);

/**
 * Failure Tracking Functions
 */

/**
 * Initialize failure tracking with custom backoff configuration
 */
ntrip_atlas_error_t ntrip_atlas_init_failure_tracking(const ntrip_failure_config_t* config);

/**
 * Record a service failure and update backoff state
 */
ntrip_atlas_error_t ntrip_atlas_record_service_failure(const char* service_id);

/**
 * Record a successful service connection (resets failure count)
 */
ntrip_atlas_error_t ntrip_atlas_record_service_success(const char* service_id);

/**
 * Check if a service is currently blocked due to failures
 */
bool ntrip_atlas_is_service_blocked(const char* service_id);

/**
 * Get time until service can be retried (0 if available now)
 */
uint32_t ntrip_atlas_get_service_retry_time(const char* service_id);

/**
 * Clear all failure history (for testing or reset purposes)
 */
ntrip_atlas_error_t ntrip_atlas_clear_failure_history(void);

/**
 * Get default exponential backoff configuration
 * Returns: 1h, 4h, 12h, 1d, 3d, 1w, 2w, 1month progression
 */
ntrip_failure_config_t ntrip_atlas_get_default_failure_config(void);

/**
 * Utility Functions
 */

/**
 * Calculate distance between two coordinates (Haversine formula)
 */
double ntrip_atlas_calculate_distance(double lat1, double lon1, double lat2, double lon2);

/**
 * Format NMEA GGA sentence for VRS position updates
 *
 * Creates a properly formatted NMEA GGA sentence with checksum.
 * Required by many VRS networks to select appropriate virtual reference station.
 *
 * @param buffer        Output buffer for GGA sentence
 * @param max_len       Size of output buffer (recommend 128 bytes minimum)
 * @param latitude      Latitude in decimal degrees (-90 to +90)
 * @param longitude     Longitude in decimal degrees (-180 to +180)
 * @param altitude_m    Altitude in meters above WGS84 ellipsoid
 * @param fix_quality   Fix quality: 0=invalid, 1=GPS, 2=DGPS, 4=RTK fixed, 5=RTK float
 * @param satellites    Number of satellites in use (0-99)
 * @return             Length of formatted sentence, or negative on error
 */
int ntrip_atlas_format_gga(
    char* buffer,
    size_t max_len,
    double latitude,
    double longitude,
    double altitude_m,
    uint8_t fix_quality,
    uint8_t satellites
);

/**
 * Get library version string
 */
const char* ntrip_atlas_get_version(void);

/**
 * Get error description string
 */
const char* ntrip_atlas_error_string(ntrip_atlas_error_t error);

/**
 * Platform Implementations (declared in separate headers)
 */

// ESP32/Arduino platform
extern const ntrip_platform_t ntrip_platform_esp32;

// Linux platform
extern const ntrip_platform_t ntrip_platform_linux;

// Windows platform
extern const ntrip_platform_t ntrip_platform_windows;

#ifdef __cplusplus
}
#endif

#endif // NTRIP_ATLAS_H