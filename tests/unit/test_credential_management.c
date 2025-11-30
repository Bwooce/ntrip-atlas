/**
 * Credential Management Unit Tests
 *
 * Tests the new credential store API for managing multiple service
 * credentials and providing access to authentication-required services.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Include the header
#include "../../libntripatlas/include/ntrip_atlas.h"

// Test credential store initialization
bool test_credential_store_init() {
    printf("Testing credential store initialization...\n");

    ntrip_credential_store_t store;
    ntrip_atlas_init_credential_store(&store);

    if (store.count != 0) {
        printf("  ❌ Store count should be 0 after initialization, got %u\n", store.count);
        return false;
    }

    printf("  ✅ Credential store initialized correctly\n");
    return true;
}

// Test adding credentials
bool test_add_credentials() {
    printf("Testing credential addition...\n");

    ntrip_credential_store_t store;
    ntrip_atlas_init_credential_store(&store);

    // Add Point One credentials
    ntrip_atlas_error_t result = ntrip_atlas_add_credential(&store, "Point One Navigation",
                                                           "user@company.com", "secret123");
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Failed to add Point One credentials\n");
        return false;
    }

    // Add Massachusetts MaCORS credentials
    result = ntrip_atlas_add_credential(&store, "Massachusetts DOT",
                                       "MA_username", "MA_password");
    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Failed to add MaCORS credentials\n");
        return false;
    }

    if (store.count != 2) {
        printf("  ❌ Store should have 2 credentials, got %u\n", store.count);
        return false;
    }

    printf("  ✅ Successfully added multiple credentials\n");
    return true;
}

// Test credential checking
bool test_has_credentials() {
    printf("Testing credential existence checking...\n");

    ntrip_credential_store_t store;
    ntrip_atlas_init_credential_store(&store);

    ntrip_atlas_add_credential(&store, "Point One Navigation", "user@company.com", "secret123");

    if (!ntrip_atlas_has_credentials(&store, "Point One Navigation")) {
        printf("  ❌ Should have Point One credentials\n");
        return false;
    }

    if (ntrip_atlas_has_credentials(&store, "EUREF-IP Network")) {
        printf("  ❌ Should not have EUREF-IP credentials\n");
        return false;
    }

    printf("  ✅ Credential existence checking works correctly\n");
    return true;
}

// Test credential retrieval
bool test_get_credentials() {
    printf("Testing credential retrieval...\n");

    ntrip_credential_store_t store;
    ntrip_atlas_init_credential_store(&store);

    ntrip_atlas_add_credential(&store, "Point One Navigation", "user@company.com", "secret123");

    char username[64], password[64];
    ntrip_atlas_error_t result = ntrip_atlas_get_credentials(&store, "Point One Navigation",
                                                            username, sizeof(username),
                                                            password, sizeof(password));

    if (result != NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Failed to retrieve credentials\n");
        return false;
    }

    if (strcmp(username, "user@company.com") != 0) {
        printf("  ❌ Wrong username: expected 'user@company.com', got '%s'\n", username);
        return false;
    }

    if (strcmp(password, "secret123") != 0) {
        printf("  ❌ Wrong password: expected 'secret123', got '%s'\n", password);
        return false;
    }

    printf("  ✅ Credential retrieval works correctly\n");
    return true;
}

// Test updating existing credentials
bool test_update_credentials() {
    printf("Testing credential updates...\n");

    ntrip_credential_store_t store;
    ntrip_atlas_init_credential_store(&store);

    // Add initial credentials
    ntrip_atlas_add_credential(&store, "Point One Navigation", "user@company.com", "old_password");

    if (store.count != 1) {
        printf("  ❌ Store should have 1 credential after first add\n");
        return false;
    }

    // Update with new password
    ntrip_atlas_add_credential(&store, "Point One Navigation", "user@company.com", "new_password");

    if (store.count != 1) {
        printf("  ❌ Store should still have 1 credential after update, got %u\n", store.count);
        return false;
    }

    // Verify updated password
    char username[64], password[64];
    ntrip_atlas_get_credentials(&store, "Point One Navigation", username, sizeof(username),
                               password, sizeof(password));

    if (strcmp(password, "new_password") != 0) {
        printf("  ❌ Password not updated: expected 'new_password', got '%s'\n", password);
        return false;
    }

    printf("  ✅ Credential updates work correctly\n");
    return true;
}

// Test store capacity
bool test_store_capacity() {
    printf("Testing credential store capacity...\n");

    ntrip_credential_store_t store;
    ntrip_atlas_init_credential_store(&store);

    // Fill the store to capacity (16 services)
    char service_id[32], username[32], password[32];
    for (int i = 0; i < 16; i++) {
        snprintf(service_id, sizeof(service_id), "service_%d", i);
        snprintf(username, sizeof(username), "user_%d", i);
        snprintf(password, sizeof(password), "pass_%d", i);

        ntrip_atlas_error_t result = ntrip_atlas_add_credential(&store, service_id, username, password);
        if (result != NTRIP_ATLAS_SUCCESS) {
            printf("  ❌ Failed to add credential %d\n", i);
            return false;
        }
    }

    // Try to add one more (should fail)
    ntrip_atlas_error_t result = ntrip_atlas_add_credential(&store, "overflow_service", "user", "pass");
    if (result != NTRIP_ATLAS_ERROR_NO_MEMORY) {
        printf("  ❌ Should have failed with NO_MEMORY error when store is full\n");
        return false;
    }

    printf("  ✅ Store capacity limits work correctly (16 service limit)\n");
    return true;
}

// Test error handling
bool test_error_handling() {
    printf("Testing error handling...\n");

    ntrip_credential_store_t store;
    ntrip_atlas_init_credential_store(&store);

    // Test invalid parameters
    if (ntrip_atlas_add_credential(NULL, "service", "user", "pass") == NTRIP_ATLAS_SUCCESS) {
        printf("  ❌ Should fail with NULL store\n");
        return false;
    }

    if (ntrip_atlas_has_credentials(NULL, "service")) {
        printf("  ❌ Should return false with NULL store\n");
        return false;
    }

    char username[64], password[64];
    if (ntrip_atlas_get_credentials(&store, "nonexistent", username, sizeof(username),
                                   password, sizeof(password)) != NTRIP_ATLAS_ERROR_NOT_FOUND) {
        printf("  ❌ Should return NOT_FOUND for nonexistent service\n");
        return false;
    }

    printf("  ✅ Error handling works correctly\n");
    return true;
}

int main() {
    printf("Credential Management Tests\n");
    printf("===========================\n\n");

    struct {
        const char* name;
        bool (*test_func)();
    } tests[] = {
        {"Credential store initialization", test_credential_store_init},
        {"Add credentials", test_add_credentials},
        {"Check credential existence", test_has_credentials},
        {"Retrieve credentials", test_get_credentials},
        {"Update credentials", test_update_credentials},
        {"Store capacity limits", test_store_capacity},
        {"Error handling", test_error_handling},
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
        printf("🎉 All credential management tests passed!\n");
        printf("Credential store API ready for real-world NTRIP service authentication\n");
        printf("Users can now provide their service credentials and get only accessible services\n");
        return 0;
    } else {
        printf("💥 Some credential management tests failed!\n");
        printf("API needs fixes before deployment\n");
        return 1;
    }
}