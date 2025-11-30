/**
 * NTRIP Atlas Configuration
 * Memory optimization and build-time configuration options
 */

#ifndef NTRIP_ATLAS_CONFIG_H
#define NTRIP_ATLAS_CONFIG_H

// Database version compiled into library
#define NTRIP_ATLAS_DATABASE_VERSION "20241130.02"

/**
 * Memory Optimization Profiles
 * Choose based on target platform and usage pattern
 */

#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO)
    // ESP32/Arduino: Minimal memory footprint for single lookup
    #define NTRIP_ATLAS_PROFILE_EMBEDDED
#elif defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
    // Desktop: Balanced memory usage for multiple lookups
    #define NTRIP_ATLAS_PROFILE_DESKTOP
#else
    // Default to embedded profile for unknown platforms
    #define NTRIP_ATLAS_PROFILE_EMBEDDED
#endif

/**
 * Embedded Profile: Optimized for ESP32-class devices
 * - Single lookup per runtime
 * - Minimal RAM usage
 * - Static allocation only
 * - Essential services only
 */
#ifdef NTRIP_ATLAS_PROFILE_EMBEDDED

// Memory limits - STREAMING APPROACH
#define NTRIP_ATLAS_MAX_SERVICES        16  // Reduced for essential services only
#define NTRIP_ATLAS_MAX_MOUNTPOINTS     64  // Limit concurrent discovery

// NEW: Streaming buffer sizes (replaces large SOURCETABLE_BUFFER)
#define NTRIP_LINE_BUFFER_SIZE          256  // Single STR line parsing
#define NTRIP_TCP_CHUNK_SIZE            512  // TCP read chunk size
#define NTRIP_HTTP_HEADER_BUFFER        512  // HTTP headers only

// DEPRECATED: Old buffer-based approach (kept for backward compat)
// DO NOT USE - sourcetable streaming processes data in chunks
#define NTRIP_ATLAS_SOURCETABLE_BUFFER  2048  // DEPRECATED - use streaming
#define NTRIP_ATLAS_HTTP_BUFFER         1024  // DEPRECATED - use streaming

// Feature flags
#define NTRIP_ATLAS_STATIC_ALLOCATION   1   // No dynamic allocation
#define NTRIP_ATLAS_SINGLE_LOOKUP       1   // Optimize for one-time use
#define NTRIP_ATLAS_MINIMAL_LOGGING     1   // Reduce log verbosity
#define NTRIP_ATLAS_ESSENTIAL_SERVICES  1   // Only high-quality services

// Disable expensive features
#undef NTRIP_ATLAS_CACHING
#undef NTRIP_ATLAS_FULL_SOURCETABLE_PARSE
#undef NTRIP_ATLAS_DETAILED_SCORING

#endif

/**
 * Desktop Profile: Balanced for desktop applications
 * - Multiple lookups supported
 * - Reasonable memory usage
 * - Dynamic allocation allowed
 * - Full service database
 */
#ifdef NTRIP_ATLAS_PROFILE_DESKTOP

// Memory limits
#define NTRIP_ATLAS_MAX_SERVICES        128
#define NTRIP_ATLAS_MAX_MOUNTPOINTS     512
#define NTRIP_ATLAS_SOURCETABLE_BUFFER  8192
#define NTRIP_ATLAS_HTTP_BUFFER         4096

// Feature flags
#define NTRIP_ATLAS_DYNAMIC_ALLOCATION  1   // Use malloc/free
#define NTRIP_ATLAS_MULTI_LOOKUP        1   // Cache for reuse
#define NTRIP_ATLAS_FULL_LOGGING        1   // Detailed logging
#define NTRIP_ATLAS_ALL_SERVICES        1   // Complete database

// Enable expensive features
#define NTRIP_ATLAS_CACHING             1   // Cache sourcetables
#define NTRIP_ATLAS_FULL_SOURCETABLE_PARSE 1
#define NTRIP_ATLAS_DETAILED_SCORING    1

#endif

/**
 * Single Lookup Optimization
 * For applications that only need one discovery per runtime
 */
#ifdef NTRIP_ATLAS_SINGLE_LOOKUP

// Memory optimization strategies
#define NTRIP_ATLAS_STREAMING_PARSE     1   // Parse as we read, don't store
#define NTRIP_ATLAS_IMMEDIATE_SCORING   1   // Score and discard, keep only best
#define NTRIP_ATLAS_EARLY_TERMINATION  1   // Stop when good enough result found
#define NTRIP_ATLAS_MINIMAL_METADATA    1   // Store only essential fields

// Single lookup workflow:
// 1. Query services in distance order (nearest first)
// 2. For each service: fetch sourcetable, parse streaming, score immediately
// 3. Keep only the best result, discard intermediate data
// 4. Stop when score threshold reached or max distance exceeded

#endif

/**
 * Database Compilation Options
 */

#ifdef NTRIP_ATLAS_ESSENTIAL_SERVICES
// Only include highest quality, most reliable services
// Reduces compiled database size by ~70%
#define NTRIP_ATLAS_MIN_QUALITY_RATING  4   // 4-5 star services only
#define NTRIP_ATLAS_GOVT_SERVICES_ONLY  0   // Include high-quality commercial
#define NTRIP_ATLAS_EXCLUDE_COMMUNITY   1   // Exclude RTK2go and similar
#endif

/**
 * Memory Layout for Embedded Systems
 */
typedef struct {
    // Only store essential service data for discovery
    uint8_t service_id;         // Index into compiled service table
    char hostname[48];          // Shortened hostname
    uint16_t port;
    uint8_t ssl : 1;
    uint8_t auth_required : 1;
    uint8_t quality_rating : 3; // 1-5 packed in 3 bits
    uint8_t reserved : 2;

    // Packed geographic coverage (reduced precision)
    int16_t lat_min_deg;        // Latitude * 100 (degree precision)
    int16_t lat_max_deg;
    int16_t lon_min_deg;        // Longitude * 100
    int16_t lon_max_deg;
} __attribute__((packed)) ntrip_service_compact_t;

/**
 * Streaming Parse Results
 * Minimal structure for single lookup optimization
 */
typedef struct {
    char mountpoint[24];        // Shortened mountpoint name
    int16_t lat_deg100;         // Latitude * 100
    int16_t lon_deg100;         // Longitude * 100
    uint16_t distance_m;        // Distance in meters (up to 65km)
    uint8_t quality_score;      // Combined score 0-100
    uint8_t service_index;      // Index to service in compact table
} __attribute__((packed)) ntrip_candidate_t;

/**
 * Version Management
 */
typedef struct {
    uint32_t database_version;  // YYYYMMDD format
    uint8_t sequence;           // Daily sequence number
    uint8_t schema_major;
    uint8_t schema_minor;
    uint8_t reserved;
} __attribute__((packed)) ntrip_atlas_version_t;

// Compile-time version check
#define NTRIP_ATLAS_VERSION_CHECK() \
    static_assert(sizeof(ntrip_service_compact_t) <= 64, \
                 "Service structure too large for embedded use")

/**
 * Memory Usage Estimates
 */

#ifdef NTRIP_ATLAS_PROFILE_EMBEDDED
// UPDATED: Estimated memory usage for ESP32 with STREAMING approach:
// - Service table: 16 services × 64 bytes = 1024 bytes (unchanged)
// - TCP chunk buffer: 512 bytes (was 1024)
// - Line parse buffer: 256 bytes (was 2048)
// - HTTP header buffer: 512 bytes (new, minimal)
// - Working variables: ~500 bytes (unchanged)
// - Total: ~2.8KB RAM (was ~4.5KB - 38% reduction!)

// With streaming:
// - OLD approach: 4.5KB RAM (buffered entire sourcetable)
// - NEW approach: ~1.1KB RAM during active streaming (75% reduction)
// - Peak usage: ~2.8KB RAM (only during HTTP header parsing)

// Flash usage (rough estimate):
// - Code: ~18-25KB (slightly larger due to streaming parser)
// - Service data: ~2-3KB
// - Total: ~20-28KB flash
#endif

#endif // NTRIP_ATLAS_CONFIG_H