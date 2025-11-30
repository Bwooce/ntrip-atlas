/**
 * NTRIP Atlas - Credential Management Implementation
 *
 * Provides secure credential storage and accessible service discovery.
 * Enables practical real-world usage by filtering services based on
 * available user credentials.
 *
 * Licensed under MIT License
 */

#include "ntrip_atlas.h"
#include <string.h>
#include <stdio.h>

/**
 * Initialize credential store
 */
void ntrip_atlas_init_credential_store(ntrip_credential_store_t* store) {
    if (!store) return;

    memset(store, 0, sizeof(ntrip_credential_store_t));
    store->count = 0;
}

/**
 * Add credentials to store
 */
ntrip_atlas_error_t ntrip_atlas_add_credential(ntrip_credential_store_t* store,
                                              const char* service_id,
                                              const char* username,
                                              const char* password) {
    if (!store || !service_id || !username || !password) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    if (store->count >= 16) {
        return NTRIP_ATLAS_ERROR_NO_MEMORY;  // Store is full
    }

    // Check if service already exists (update existing)
    for (uint8_t i = 0; i < store->count; i++) {
        if (strcmp(store->credentials[i].service_id, service_id) == 0) {
            // Update existing credentials
            strncpy(store->credentials[i].username, username, NTRIP_ATLAS_MAX_USERNAME - 1);
            strncpy(store->credentials[i].password, password, NTRIP_ATLAS_MAX_PASSWORD - 1);
            store->credentials[i].username[NTRIP_ATLAS_MAX_USERNAME - 1] = '\0';
            store->credentials[i].password[NTRIP_ATLAS_MAX_PASSWORD - 1] = '\0';
            return NTRIP_ATLAS_SUCCESS;
        }
    }

    // Add new credential entry
    uint8_t index = store->count;
    strncpy(store->credentials[index].service_id, service_id, 31);
    strncpy(store->credentials[index].username, username, NTRIP_ATLAS_MAX_USERNAME - 1);
    strncpy(store->credentials[index].password, password, NTRIP_ATLAS_MAX_PASSWORD - 1);

    store->credentials[index].service_id[31] = '\0';
    store->credentials[index].username[NTRIP_ATLAS_MAX_USERNAME - 1] = '\0';
    store->credentials[index].password[NTRIP_ATLAS_MAX_PASSWORD - 1] = '\0';

    store->count++;
    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Check if credentials exist for a service
 */
bool ntrip_atlas_has_credentials(const ntrip_credential_store_t* store,
                                const char* service_id) {
    if (!store || !service_id) return false;

    for (uint8_t i = 0; i < store->count; i++) {
        if (strcmp(store->credentials[i].service_id, service_id) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * Get credentials for a service
 */
ntrip_atlas_error_t ntrip_atlas_get_credentials(const ntrip_credential_store_t* store,
                                               const char* service_id,
                                               char* username, size_t username_len,
                                               char* password, size_t password_len) {
    if (!store || !service_id || !username || !password) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    for (uint8_t i = 0; i < store->count; i++) {
        if (strcmp(store->credentials[i].service_id, service_id) == 0) {
            strncpy(username, store->credentials[i].username, username_len - 1);
            strncpy(password, store->credentials[i].password, password_len - 1);
            username[username_len - 1] = '\0';
            password[password_len - 1] = '\0';
            return NTRIP_ATLAS_SUCCESS;
        }
    }

    return NTRIP_ATLAS_ERROR_NOT_FOUND;
}

/**
 * Check if a service is accessible with available credentials
 * Public helper function - uses provider name as service identifier
 */
bool ntrip_atlas_is_service_accessible(const ntrip_service_config_t* service,
                                      const ntrip_credential_store_t* store) {
    if (!service) return false;

    // Free services (like RTK2GO) are always accessible
    if (!service->requires_registration || service->typical_free_access) {
        return true;
    }

    // Check if we have credentials for this service (using provider as ID)
    return ntrip_atlas_has_credentials(store, service->provider);
}

/**
 * Automatically populate credentials in service result
 * Internal helper to fill credentials from store
 */
ntrip_atlas_error_t ntrip_atlas_populate_credentials(ntrip_best_service_t* result,
                                                     const ntrip_credential_store_t* store,
                                                     const char* provider_name) {
    if (!result || !store || !provider_name) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    // Clear existing credentials
    memset(result->username, 0, sizeof(result->username));
    memset(result->password, 0, sizeof(result->password));

    // Special handling for known free services
    if (strcmp(provider_name, "RTK2go Community") == 0) {
        // RTK2GO uses email as username and literal "none" as password
        strncpy(result->username, "user@example.com", sizeof(result->username) - 1);
        strncpy(result->password, "none", sizeof(result->password) - 1);
        return NTRIP_ATLAS_SUCCESS;
    }

    // Look up credentials from store
    return ntrip_atlas_get_credentials(store, provider_name,
                                      result->username, sizeof(result->username),
                                      result->password, sizeof(result->password));
}