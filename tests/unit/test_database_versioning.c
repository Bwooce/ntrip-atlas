/**
 * Database Versioning Unit Tests
 *
 * Tests the database versioning system for forward compatibility,
 * schema evolution, and graceful degradation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

// Include the header
#include "../../libntripatlas/include/ntrip_atlas.h"

// Test magic number verification
bool test_magic_number_verification() {
    printf("Testing magic number verification...\n");

    // Create valid header
    ntrip_db_header_t valid_header = {
        .magic_number = NTRIP_ATLAS_DB_MAGIC_V1,
        .schema_major = 1,
        .schema_minor = 0,
        .database_version = 20241130,
        .sequence_number = 1,
        .feature_flags = 0,
        .service_count = 32
    };

    // Test valid magic number
    ntrip_atlas_error_t result = ntrip_atlas_validate_database_header(&valid_header);
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Valid header should pass validation\n");
        return false;
    }

    // Test invalid magic number
    ntrip_db_header_t invalid_header = valid_header;
    invalid_header.magic_number = 0x12345678;

    result = ntrip_atlas_validate_database_header(&invalid_header);
    if (result != NTRIP_ATLAS_ERROR_INVALID_MAGIC) {
        printf("  ❌ Invalid magic number should be rejected\n");
        return false;
    }

    printf("  ✅ Magic number verification working correctly\n");
    return true;
}

// Test version compatibility checking
bool test_version_compatibility() {
    printf("Testing version compatibility checking...\n");

    ntrip_compatibility_t compatibility;
    ntrip_atlas_error_t result;

    // Test fully compatible version (same version)
    ntrip_db_header_t compatible_header = {
        .magic_number = NTRIP_ATLAS_DB_MAGIC_V1,
        .schema_major = NTRIP_ATLAS_SCHEMA_MAJOR,
        .schema_minor = NTRIP_ATLAS_SCHEMA_MINOR,
        .database_version = 20241130,
        .sequence_number = 1,
        .feature_flags = 0,
        .service_count = 32
    };

    result = ntrip_atlas_check_database_compatibility(&compatible_header, &compatibility);
    if (result != NTRIP_ATLAS_SUCCESS || compatibility != NTRIP_COMPAT_COMPATIBLE) {
        printf("  ❌ Same version should be fully compatible\n");
        return false;
    }

    // Test backward compatibility (newer minor version)
    ntrip_db_header_t newer_minor = compatible_header;
    newer_minor.schema_minor = NTRIP_ATLAS_SCHEMA_MINOR + 1;

    result = ntrip_atlas_check_database_compatibility(&newer_minor, &compatibility);
    if (result != NTRIP_ATLAS_SUCCESS || compatibility != NTRIP_COMPAT_BACKWARD_ONLY) {
        printf("  ❌ Newer minor version should be backward compatible\n");
        return false;
    }

    // Test upgrade needed (newer major version)
    ntrip_db_header_t newer_major = compatible_header;
    newer_major.schema_major = NTRIP_ATLAS_SCHEMA_MAJOR + 1;

    result = ntrip_atlas_check_database_compatibility(&newer_major, &compatibility);
    if (result != NTRIP_ATLAS_ERROR_VERSION_TOO_OLD || compatibility != NTRIP_COMPAT_UPGRADE_NEEDED) {
        printf("  ❌ Newer major version should require upgrade\n");
        return false;
    }

    // Test older version compatibility
    ntrip_db_header_t older_version = compatible_header;
    older_version.schema_major = NTRIP_ATLAS_SCHEMA_MAJOR > 1 ? NTRIP_ATLAS_SCHEMA_MAJOR - 1 : 1;

    result = ntrip_atlas_check_database_compatibility(&older_version, &compatibility);
    if (result != NTRIP_ATLAS_SUCCESS || compatibility != NTRIP_COMPAT_COMPATIBLE) {
        printf("  ❌ Older version should be fully compatible\n");
        return false;
    }

    printf("  ✅ Version compatibility checking working correctly\n");
    return true;
}

// Test feature flag compatibility
bool test_feature_compatibility() {
    printf("Testing feature flag compatibility...\n");

    // Test supported features
    bool supports_compact = ntrip_atlas_supports_feature(NTRIP_DB_FEATURE_COMPACT_FAILURES);
    if (!supports_compact) {
        printf("  ❌ Should support compact failures feature\n");
        return false;
    }

    // Test supported features (v1.1.0 now includes geographic index)
    bool supports_geo = ntrip_atlas_supports_feature(NTRIP_DB_FEATURE_GEOGRAPHIC_INDEX);
    if (!supports_geo) {
        printf("  ❌ Should support geographic index in v1.1.0\n");
        return false;
    }

    // Test database with unsupported features
    ntrip_db_header_t advanced_header = {
        .magic_number = NTRIP_ATLAS_DB_MAGIC_V1,
        .schema_major = NTRIP_ATLAS_SCHEMA_MAJOR,
        .schema_minor = NTRIP_ATLAS_SCHEMA_MINOR,
        .database_version = 20241130,
        .sequence_number = 1,
        .feature_flags = NTRIP_DB_FEATURE_GEOGRAPHIC_INDEX | NTRIP_DB_FEATURE_TIERED_LOADING,
        .service_count = 32
    };

    ntrip_compatibility_t compatibility;
    ntrip_atlas_error_t result = ntrip_atlas_check_database_compatibility(
        &advanced_header, &compatibility
    );

    if (result != NTRIP_ATLAS_SUCCESS || compatibility != NTRIP_COMPAT_BACKWARD_ONLY) {
        printf("  ❌ Database with unsupported features should be backward compatible\n");
        return false;
    }

    printf("  ✅ Feature compatibility checking working correctly\n");
    return true;
}

// Test version info retrieval
bool test_version_info() {
    printf("Testing version information retrieval...\n");

    ntrip_version_info_t info;
    ntrip_atlas_error_t result = ntrip_atlas_get_version_info(&info);

    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Failed to get version info\n");
        return false;
    }

    // Check library version matches defines
    if (info.library_schema_major != NTRIP_ATLAS_SCHEMA_MAJOR ||
        info.library_schema_minor != NTRIP_ATLAS_SCHEMA_MINOR) {
        printf("  ❌ Version info doesn't match library defines\n");
        return false;
    }

    // Check feature flags
    if (!info.compact_failure_support) {
        printf("  ❌ Should report compact failure support\n");
        return false;
    }

    if (info.geographic_index_support || info.tiered_loading_support) {
        printf("  ❌ Should not report unimplemented features as supported\n");
        return false;
    }

    printf("  ✅ Version info retrieval working correctly\n");
    printf("    Library: v%d.%d\n", info.library_schema_major, info.library_schema_minor);
    printf("    Database: %d\n", info.database_version);
    printf("    Features: 0x%02X\n", info.supported_features);

    return true;
}

// Test compatibility messages
bool test_compatibility_messages() {
    printf("Testing compatibility messages...\n");

    // Test all compatibility levels have messages
    const char* msg;

    msg = ntrip_atlas_get_compatibility_message(NTRIP_COMPAT_COMPATIBLE);
    if (!msg || strlen(msg) == 0) {
        printf("  ❌ Missing message for compatible\n");
        return false;
    }

    msg = ntrip_atlas_get_compatibility_message(NTRIP_COMPAT_BACKWARD_ONLY);
    if (!msg || strlen(msg) == 0) {
        printf("  ❌ Missing message for backward only\n");
        return false;
    }

    msg = ntrip_atlas_get_compatibility_message(NTRIP_COMPAT_UPGRADE_NEEDED);
    if (!msg || strlen(msg) == 0) {
        printf("  ❌ Missing message for upgrade needed\n");
        return false;
    }

    msg = ntrip_atlas_get_compatibility_message(NTRIP_COMPAT_INCOMPATIBLE);
    if (!msg || strlen(msg) == 0) {
        printf("  ❌ Missing message for incompatible\n");
        return false;
    }

    printf("  ✅ Compatibility messages working correctly\n");
    return true;
}

// Test database header creation and validation
bool test_header_creation() {
    printf("Testing database header creation and validation...\n");

    ntrip_db_header_t header;
    ntrip_atlas_error_t result = ntrip_atlas_create_database_header(
        &header, 20241130, 2, 32
    );

    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Failed to create database header\n");
        return false;
    }

    // Validate created header
    result = ntrip_atlas_validate_database_header(&header);
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Created header failed validation\n");
        return false;
    }

    // Check header contents
    if (header.magic_number != NTRIP_ATLAS_DB_MAGIC_V1) {
        printf("  ❌ Wrong magic number in created header\n");
        return false;
    }

    if (header.database_version != 20241130 || header.sequence_number != 2 || header.service_count != 32) {
        printf("  ❌ Wrong values in created header\n");
        return false;
    }

    printf("  ✅ Database header creation working correctly\n");
    return true;
}

// Test initialization with version checking
bool test_versioned_initialization() {
    printf("Testing versioned initialization...\n");

    // Test initialization without header (default mode)
    ntrip_atlas_error_t result = ntrip_atlas_init_with_version_check(NULL);
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Default initialization should succeed\n");
        return false;
    }

    // Test initialization with compatible header
    ntrip_db_header_t compatible_header;
    ntrip_atlas_create_database_header(&compatible_header, 20241130, 1, 32);

    result = ntrip_atlas_init_with_version_check(&compatible_header);
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Compatible header initialization should succeed\n");
        return false;
    }

    // Test initialization with incompatible header
    ntrip_db_header_t incompatible_header = compatible_header;
    incompatible_header.magic_number = 0x12345678;

    result = ntrip_atlas_init_with_version_check(&incompatible_header);
    if (result == NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Incompatible header should cause initialization failure\n");
        return false;
    }

    printf("  ✅ Versioned initialization working correctly\n");
    return true;
}

// Test edge cases
bool test_edge_cases() {
    printf("Testing edge cases...\n");

    // Test NULL parameter handling
    ntrip_atlas_error_t result = ntrip_atlas_validate_database_header(NULL);
    if (result != NTRIP_ATLAS_ERROR_INVALID_PARAM) {
        printf("  ❌ Should reject NULL header\n");
        return false;
    }

    ntrip_compatibility_t compatibility;
    result = ntrip_atlas_check_database_compatibility(NULL, &compatibility);
    if (result != NTRIP_ATLAS_ERROR_INVALID_PARAM) {
        printf("  ❌ Should reject NULL header in compatibility check\n");
        return false;
    }

    // Test invalid sequence numbers
    ntrip_db_header_t header;
    ntrip_atlas_create_database_header(&header, 20241130, 100, 32); // Invalid sequence > 99

    result = ntrip_atlas_validate_database_header(&header);
    if (result != NTRIP_ATLAS_ERROR_INVALID_PARAM) {
        printf("  ❌ Should reject invalid sequence number\n");
        return false;
    }

    printf("  ✅ Edge cases handled correctly\n");
    return true;
}

int main() {
    printf("Database Versioning Tests\n");
    printf("=========================\n\n");

    struct {
        const char* name;
        bool (*test_func)();
    } tests[] = {
        {"Magic number verification", test_magic_number_verification},
        {"Version compatibility", test_version_compatibility},
        {"Feature compatibility", test_feature_compatibility},
        {"Version info retrieval", test_version_info},
        {"Compatibility messages", test_compatibility_messages},
        {"Header creation", test_header_creation},
        {"Versioned initialization", test_versioned_initialization},
        {"Edge cases", test_edge_cases},
    };

    int passed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < total; i++) {
        printf("Test %d: %s\n", i + 1, tests[i].name);
        if (tests[i].test_func()) {
            printf("✅ PASS\n\n");
            passed++;
        } else {
            printf("❌ FAIL\n\n");
        }
    }

    printf("Results: %d/%d tests passed\n", passed, total);

    if (passed == total) {
        printf("🎉 All database versioning tests passed!\n");
        printf("Forward compatibility system ready for schema evolution\n");
        printf("Magic number: 0x%08X\n", NTRIP_ATLAS_DB_MAGIC_V1);
        printf("Schema version: %d.%d\n", NTRIP_ATLAS_SCHEMA_MAJOR, NTRIP_ATLAS_SCHEMA_MINOR);

        // Print version info as demo
        printf("\nLibrary Version Info:\n");
        ntrip_atlas_print_version_info();

        return 0;
    } else {
        printf("💥 Some database versioning tests failed!\n");
        printf("Version compatibility system needs fixes\n");
        return 1;
    }
}