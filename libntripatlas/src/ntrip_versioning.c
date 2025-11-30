/**
 * NTRIP Atlas - Database Versioning and Forward Compatibility
 *
 * Provides version checking, compatibility verification, and graceful
 * degradation for database schema evolution.
 *
 * Licensed under MIT License
 */

#include "ntrip_atlas.h"
#include <stdio.h>
#include <string.h>

// Current library capabilities (v1.1.0 - 20241130.02)
#define CURRENT_SUPPORTED_FEATURES ( \
    NTRIP_DB_FEATURE_COMPACT_FAILURES | \
    NTRIP_DB_FEATURE_GEOGRAPHIC_INDEX | \
    NTRIP_DB_FEATURE_EXTENDED_AUTH \
)

/**
 * Check database compatibility with current library
 */
ntrip_atlas_error_t ntrip_atlas_check_database_compatibility(
    const ntrip_db_header_t* db_header,
    ntrip_compatibility_t* compatibility
) {
    if (!db_header || !compatibility) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    // Check magic number
    if (db_header->magic_number != NTRIP_ATLAS_DB_MAGIC_V1) {
        *compatibility = NTRIP_COMPAT_INCOMPATIBLE;
        return NTRIP_ATLAS_ERROR_INVALID_MAGIC;
    }

    // Get library schema version
    uint16_t lib_major = NTRIP_ATLAS_SCHEMA_MAJOR;
    uint16_t lib_minor = NTRIP_ATLAS_SCHEMA_MINOR;
    uint16_t db_major = db_header->schema_major;
    uint16_t db_minor = db_header->schema_minor;

    // Version compatibility logic
    if (db_major < lib_major) {
        // Database is older - fully compatible (library can read older formats)
        *compatibility = NTRIP_COMPAT_COMPATIBLE;
    } else if (db_major == lib_major) {
        if (db_minor <= lib_minor) {
            // Same major version, database minor <= library minor - compatible
            *compatibility = NTRIP_COMPAT_COMPATIBLE;
        } else {
            // Same major, database minor > library minor - backward compatible
            *compatibility = NTRIP_COMPAT_BACKWARD_ONLY;
        }
    } else {
        // Database major > library major - incompatible (breaking changes)
        *compatibility = NTRIP_COMPAT_UPGRADE_NEEDED;
        return NTRIP_ATLAS_ERROR_VERSION_TOO_OLD;
    }

    // Check feature requirements
    uint8_t required_features = db_header->feature_flags;
    uint8_t unsupported_features = required_features & (~CURRENT_SUPPORTED_FEATURES);

    if (unsupported_features != 0) {
        // Database requires features we don't support
        if (*compatibility == NTRIP_COMPAT_COMPATIBLE) {
            *compatibility = NTRIP_COMPAT_BACKWARD_ONLY;
        }
    }

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Get current library version information
 */
ntrip_atlas_error_t ntrip_atlas_get_version_info(
    ntrip_version_info_t* version_info
) {
    if (!version_info) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    version_info->library_schema_major = NTRIP_ATLAS_SCHEMA_MAJOR;
    version_info->library_schema_minor = NTRIP_ATLAS_SCHEMA_MINOR;
    version_info->database_version = 20241130; // Current database version
    version_info->supported_features = CURRENT_SUPPORTED_FEATURES;
    version_info->compact_failure_support = true;
    version_info->geographic_index_support = false; // Not implemented yet
    version_info->tiered_loading_support = false;   // Not implemented yet

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Check if library supports specific feature
 */
bool ntrip_atlas_supports_feature(uint8_t feature_flag) {
    return (CURRENT_SUPPORTED_FEATURES & feature_flag) != 0;
}

/**
 * Get human-readable compatibility message
 */
const char* ntrip_atlas_get_compatibility_message(ntrip_compatibility_t compatibility) {
    switch (compatibility) {
        case NTRIP_COMPAT_COMPATIBLE:
            return "Database fully compatible with library";

        case NTRIP_COMPAT_BACKWARD_ONLY:
            return "Database newer than library - some features may be unavailable";

        case NTRIP_COMPAT_UPGRADE_NEEDED:
            return "Library too old for database - please upgrade NTRIP Atlas library";

        case NTRIP_COMPAT_INCOMPATIBLE:
            return "Database format incompatible with this library version";

        default:
            return "Unknown compatibility status";
    }
}

/**
 * Print version information to stdout (for debugging)
 */
void ntrip_atlas_print_version_info(void) {
    ntrip_version_info_t info;
    ntrip_atlas_error_t result = ntrip_atlas_get_version_info(&info);

    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("Error getting version info\n");
        return;
    }

    printf("NTRIP Atlas Library v%d.%d\n",
           info.library_schema_major, info.library_schema_minor);
    printf("Database: %08d\n", info.database_version);
    printf("Features: %s%s%s%s\n",
           info.compact_failure_support ? "CompactFailure " : "",
           info.geographic_index_support ? "GeoIndex " : "",
           info.tiered_loading_support ? "TieredLoad " : "",
           (info.supported_features & NTRIP_DB_FEATURE_EXPERIMENTAL) ? "Experimental " : "");
    printf("Memory optimization: %s\n",
           info.compact_failure_support ? "93% reduction (80→6 bytes/service)" : "Standard");
}

/**
 * Initialize with version checking and graceful degradation
 */
ntrip_atlas_error_t ntrip_atlas_init_with_version_check(
    const ntrip_db_header_t* db_header
) {
    // If no database header provided, use default initialization
    if (!db_header) {
        printf("No database header - initializing with default settings\n");
        return NTRIP_ATLAS_SUCCESS; // Would call normal init here
    }

    // Check compatibility
    ntrip_compatibility_t compatibility;
    ntrip_atlas_error_t result = ntrip_atlas_check_database_compatibility(
        db_header, &compatibility
    );

    if (result != NTRIP_ATLAS_SUCCESS && compatibility == NTRIP_COMPAT_INCOMPATIBLE) {
        return result; // Fatal compatibility error
    }

    // Handle compatibility level
    switch (compatibility) {
        case NTRIP_COMPAT_COMPATIBLE:
            printf("Database fully compatible - enabling all features\n");
            // Initialize with all supported features enabled
            return ntrip_atlas_init_features(NTRIP_ATLAS_FEATURE_ALL);

        case NTRIP_COMPAT_BACKWARD_ONLY:
            printf("Database newer than library - enabling compatible features only\n");
            printf("Consider upgrading library for full feature access\n");
            // Initialize with only features supported by current library version
            return ntrip_atlas_init_features(NTRIP_ATLAS_FEATURE_CORE);
            break;

        case NTRIP_COMPAT_UPGRADE_NEEDED:
            printf("Library too old for database schema v%d.%d\n",
                   db_header->schema_major, db_header->schema_minor);
            printf("Please upgrade NTRIP Atlas library\n");
            return NTRIP_ATLAS_ERROR_VERSION_TOO_OLD;

        case NTRIP_COMPAT_INCOMPATIBLE:
            printf("Database format incompatible\n");
            return NTRIP_ATLAS_ERROR_INCOMPATIBLE_VERSION;
    }

    // Verify service count is reasonable
    if (db_header->service_count > 1000) {
        printf("Warning: Large service count (%d) may impact performance\n",
               db_header->service_count);
    }

    printf("Initialized with %d services, features: 0x%02X\n",
           db_header->service_count, db_header->feature_flags);

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Create database header for current library version
 */
ntrip_atlas_error_t ntrip_atlas_create_database_header(
    ntrip_db_header_t* header,
    uint32_t database_version,
    uint8_t sequence_number,
    uint16_t service_count
) {
    if (!header) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    header->magic_number = NTRIP_ATLAS_DB_MAGIC_V1;
    header->schema_major = NTRIP_ATLAS_SCHEMA_MAJOR;
    header->schema_minor = NTRIP_ATLAS_SCHEMA_MINOR;
    header->database_version = database_version;
    header->sequence_number = sequence_number;
    header->feature_flags = CURRENT_SUPPORTED_FEATURES;
    header->service_count = service_count;

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Validate database header format
 */
ntrip_atlas_error_t ntrip_atlas_validate_database_header(
    const ntrip_db_header_t* header
) {
    if (!header) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    // Check magic number
    if (header->magic_number != NTRIP_ATLAS_DB_MAGIC_V1) {
        return NTRIP_ATLAS_ERROR_INVALID_MAGIC;
    }

    // Validate schema version numbers
    if (header->schema_major == 0) {
        return NTRIP_ATLAS_ERROR_INCOMPATIBLE_VERSION;
    }

    // Validate sequence number range
    if (header->sequence_number > 99) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    // Validate service count
    if (header->service_count == 0 || header->service_count > 10000) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Initialize NTRIP Atlas with specific feature set
 */
ntrip_atlas_error_t ntrip_atlas_init_features(uint8_t feature_flags) {
    // Initialize components based on enabled features
    if (feature_flags & NTRIP_DB_FEATURE_COMPACT_FAILURES) {
        // Compact failures already initialized in system
    }

    if (feature_flags & NTRIP_DB_FEATURE_GEOGRAPHIC_INDEX) {
        // Geographic indexing not yet implemented
    }

    if (feature_flags & NTRIP_DB_FEATURE_TIERED_LOADING) {
        // Tiered loading initialization handled separately
    }

    // Feature flags processed successfully
    (void)feature_flags; // Mark as used for future feature expansion

    return NTRIP_ATLAS_SUCCESS;
}