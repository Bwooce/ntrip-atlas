/**
 * Linux Platform Implementation for NTRIP Atlas
 *
 * Uses libcurl for HTTP streaming and libsecret/keyring for credentials.
 *
 * Copyright (c) 2024 NTRIP Atlas Contributors
 * Licensed under MIT License
 */

#include "ntrip_atlas.h"
#include "ntrip_atlas_config.h"

#ifdef __linux__

#include <curl/curl.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Context for curl write callback
 */
typedef struct {
    ntrip_stream_callback_t user_callback;
    void* user_context;
    int callback_result;
} curl_stream_context_t;

/**
 * libcurl write callback - forwards data to user callback
 */
static size_t curl_write_callback(
    char* ptr,
    size_t size,
    size_t nmemb,
    void* userdata
) {
    curl_stream_context_t* ctx = (curl_stream_context_t*)userdata;
    size_t total_size = size * nmemb;

    // Call user's streaming callback
    ctx->callback_result = ctx->user_callback(ptr, total_size, ctx->user_context);

    // If callback returns non-zero, signal curl to stop
    if (ctx->callback_result != 0) {
        return 0; // Return 0 to stop curl transfer
    }

    return total_size; // Return bytes processed
}

/**
 * HTTP streaming implementation using libcurl
 */
static int linux_http_stream(
    const char* host,
    uint16_t port,
    uint8_t ssl,
    const char* path,
    ntrip_stream_callback_t on_data,
    void* user_context,
    uint32_t timeout_ms
) {
    CURL* curl;
    CURLcode res;
    char url[512];

    // Build URL
    snprintf(url, sizeof(url), "%s://%s:%d%s",
             ssl ? "https" : "http", host, port, path);

    curl = curl_easy_init();
    if (!curl) {
        return NTRIP_ATLAS_ERROR_PLATFORM;
    }

    // Set up streaming context
    curl_stream_context_t ctx = {
        .user_callback = on_data,
        .user_context = user_context,
        .callback_result = 0
    };

    // Configure curl for streaming
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "NTRIP-Atlas/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // SSL options
    if (ssl) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    // Perform request
    res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);

    // Check for errors
    if (res != CURLE_OK && res != CURLE_WRITE_ERROR) {
        if (res == CURLE_OPERATION_TIMEDOUT) {
            return NTRIP_ATLAS_ERROR_TIMEOUT;
        }
        return NTRIP_ATLAS_ERROR_PLATFORM;
    }

    // CURLE_WRITE_ERROR is expected when callback stops streaming
    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Send NMEA sentence (for socket-based connections)
 */
static int linux_send_nmea(
    void* connection,
    const char* nmea_sentence
) {
    // For Linux, connection would be a socket file descriptor
    // Implementation depends on how NTRIP connection is managed
    int* sock_fd = (int*)connection;

    if (!sock_fd || *sock_fd < 0 || !nmea_sentence) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    size_t len = strlen(nmea_sentence);
    ssize_t sent = send(*sock_fd, nmea_sentence, len, 0);

    if (sent < 0 || (size_t)sent != len) {
        return NTRIP_ATLAS_ERROR_PLATFORM;
    }

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Store credential using file-based storage
 * TODO: Integrate with libsecret/keyring for production
 */
static int linux_store_credential(const char* key, const char* value) {
    if (!key || !value) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    // Simple file-based storage for now
    // Production should use libsecret or similar
    char path[512];
    snprintf(path, sizeof(path), "%s/.ntrip_atlas_creds", getenv("HOME"));

    FILE* f = fopen(path, "a");
    if (!f) {
        return NTRIP_ATLAS_ERROR_PLATFORM;
    }

    fprintf(f, "%s=%s\n", key, value);
    fclose(f);

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Load credential from file-based storage
 */
static int linux_load_credential(const char* key, char* value, size_t max_len) {
    if (!key || !value || max_len == 0) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/.ntrip_atlas_creds", getenv("HOME"));

    FILE* f = fopen(path, "r");
    if (!f) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    char line[512];
    int found = 0;

    while (fgets(line, sizeof(line), f)) {
        char* equals = strchr(line, '=');
        if (equals) {
            *equals = '\0';
            if (strcmp(line, key) == 0) {
                strncpy(value, equals + 1, max_len - 1);
                value[max_len - 1] = '\0';
                // Remove trailing newline
                char* newline = strchr(value, '\n');
                if (newline) *newline = '\0';
                found = 1;
                break;
            }
        }
    }

    fclose(f);

    return found ? NTRIP_ATLAS_SUCCESS : NTRIP_ATLAS_ERROR_INVALID_PARAM;
}

/**
 * Store failure data
 */
static int linux_store_failure_data(
    const char* service_id,
    const ntrip_service_failure_t* failure
) {
    if (!service_id || !failure) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/.ntrip_atlas_failures", getenv("HOME"));

    FILE* f = fopen(path, "wb");
    if (!f) {
        return NTRIP_ATLAS_ERROR_PLATFORM;
    }

    fwrite(failure, sizeof(ntrip_service_failure_t), 1, f);
    fclose(f);

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Load failure data
 */
static int linux_load_failure_data(
    const char* service_id,
    ntrip_service_failure_t* failure
) {
    if (!service_id || !failure) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/.ntrip_atlas_failures", getenv("HOME"));

    FILE* f = fopen(path, "rb");
    if (!f) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    size_t read = fread(failure, sizeof(ntrip_service_failure_t), 1, f);
    fclose(f);

    return (read == 1) ? NTRIP_ATLAS_SUCCESS : NTRIP_ATLAS_ERROR_INVALID_PARAM;
}

/**
 * Clear failure data
 */
static int linux_clear_failure_data(const char* service_id) {
    if (!service_id) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/.ntrip_atlas_failures", getenv("HOME"));

    remove(path);
    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Log message to stdout
 */
static void linux_log_message(int level, const char* message) {
    const char* level_str;
    switch (level) {
        case 0: level_str = "ERROR"; break;
        case 1: level_str = "WARN"; break;
        case 2: level_str = "INFO"; break;
        case 3: level_str = "DEBUG"; break;
        default: level_str = "UNKNOWN"; break;
    }

    printf("[NTRIP-%s] %s\n", level_str, message);
}

/**
 * Get milliseconds (monotonic)
 */
static uint32_t linux_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

/**
 * Get seconds since epoch
 */
static uint32_t linux_get_time_seconds(void) {
    return (uint32_t)time(NULL);
}

/**
 * Linux Platform Implementation
 */
const ntrip_platform_t ntrip_platform_linux = {
    .interface_version = 2,
    .http_stream = linux_http_stream,
    .send_nmea = linux_send_nmea,
    .store_credential = linux_store_credential,
    .load_credential = linux_load_credential,
    .store_failure_data = linux_store_failure_data,
    .load_failure_data = linux_load_failure_data,
    .clear_failure_data = linux_clear_failure_data,
    .log_message = linux_log_message,
    .get_time_ms = linux_get_time_ms,
    .get_time_seconds = linux_get_time_seconds
};

#endif // __linux__
