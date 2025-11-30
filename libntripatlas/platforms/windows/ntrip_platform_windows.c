/**
 * Windows Platform Implementation for NTRIP Atlas
 *
 * Uses WinHTTP for HTTP streaming and Windows Credential Manager for secure storage.
 *
 * Copyright (c) 2024 NTRIP Atlas Contributors
 * Licensed under MIT License
 */

#include "ntrip_atlas.h"
#include "ntrip_atlas_config.h"

#ifdef _WIN32

#include <windows.h>
#include <winhttp.h>
#include <wincred.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "advapi32.lib")

/**
 * HTTP streaming implementation using WinHTTP
 */
static int windows_http_stream(
    const char* host,
    uint16_t port,
    uint8_t ssl,
    const char* path,
    ntrip_stream_callback_t on_data,
    void* user_context,
    uint32_t timeout_ms
) {
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    int result = NTRIP_ATLAS_ERROR_PLATFORM;

    // Convert host to wide string
    WCHAR whost[256];
    MultiByteToWideChar(CP_UTF8, 0, host, -1, whost, 256);

    // Convert path to wide string
    WCHAR wpath[512];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 512);

    // Initialize WinHTTP
    hSession = WinHttpOpen(
        L"NTRIP-Atlas/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!hSession) goto cleanup;

    // Set timeouts
    WinHttpSetTimeouts(hSession, timeout_ms, timeout_ms, timeout_ms, timeout_ms);

    // Connect to server
    hConnect = WinHttpConnect(hSession, whost, port, 0);
    if (!hConnect) goto cleanup;

    // Open request
    DWORD flags = ssl ? WINHTTP_FLAG_SECURE : 0;
    hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        wpath,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );

    if (!hRequest) goto cleanup;

    // Send request
    if (!WinHttpSendRequest(
            hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0,
            0, 0)) {
        goto cleanup;
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        goto cleanup;
    }

    // Stream response data
    BYTE buffer[NTRIP_TCP_CHUNK_SIZE];
    DWORD bytes_read = 0;
    int callback_result = 0;

    while (WinHttpReadData(hRequest, buffer, NTRIP_TCP_CHUNK_SIZE, &bytes_read) && bytes_read > 0) {
        callback_result = on_data((char*)buffer, bytes_read, user_context);

        // Stop if callback returns non-zero
        if (callback_result != 0) {
            break;
        }
    }

    result = NTRIP_ATLAS_SUCCESS;

cleanup:
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);

    return result;
}

/**
 * Send NMEA sentence (for socket-based connections)
 */
static int windows_send_nmea(
    void* connection,
    const char* nmea_sentence
) {
    // For Windows, connection would be a SOCKET handle
    SOCKET* sock = (SOCKET*)connection;

    if (!sock || *sock == INVALID_SOCKET || !nmea_sentence) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    size_t len = strlen(nmea_sentence);
    int sent = send(*sock, nmea_sentence, (int)len, 0);

    if (sent == SOCKET_ERROR || (size_t)sent != len) {
        return NTRIP_ATLAS_ERROR_PLATFORM;
    }

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Store credential using Windows Credential Manager
 */
static int windows_store_credential(const char* key, const char* value) {
    if (!key || !value) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    // Build target name
    char target[256];
    snprintf(target, sizeof(target), "NTRIP-Atlas:%s", key);

    // Convert to wide strings
    WCHAR wtarget[256];
    WCHAR wvalue[256];
    MultiByteToWideChar(CP_UTF8, 0, target, -1, wtarget, 256);
    MultiByteToWideChar(CP_UTF8, 0, value, -1, wvalue, 256);

    // Create credential
    CREDENTIALW cred = {0};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = wtarget;
    cred.CredentialBlobSize = (DWORD)(wcslen(wvalue) * sizeof(WCHAR));
    cred.CredentialBlob = (LPBYTE)wvalue;
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    if (!CredWriteW(&cred, 0)) {
        return NTRIP_ATLAS_ERROR_PLATFORM;
    }

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Load credential from Windows Credential Manager
 */
static int windows_load_credential(const char* key, char* value, size_t max_len) {
    if (!key || !value || max_len == 0) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    // Build target name
    char target[256];
    snprintf(target, sizeof(target), "NTRIP-Atlas:%s", key);

    // Convert to wide string
    WCHAR wtarget[256];
    MultiByteToWideChar(CP_UTF8, 0, target, -1, wtarget, 256);

    // Read credential
    PCREDENTIALW cred;
    if (!CredReadW(wtarget, CRED_TYPE_GENERIC, 0, &cred)) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    // Convert blob back to UTF-8
    WideCharToMultiByte(
        CP_UTF8, 0,
        (LPCWSTR)cred->CredentialBlob,
        cred->CredentialBlobSize / sizeof(WCHAR),
        value, (int)max_len - 1,
        NULL, NULL
    );
    value[max_len - 1] = '\0';

    CredFree(cred);
    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Store failure data to registry
 */
static int windows_store_failure_data(
    const char* service_id,
    const ntrip_service_failure_t* failure
) {
    if (!service_id || !failure) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    HKEY hKey;
    LONG result = RegCreateKeyExA(
        HKEY_CURRENT_USER,
        "Software\\NTRIP-Atlas\\Failures",
        0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL
    );

    if (result != ERROR_SUCCESS) {
        return NTRIP_ATLAS_ERROR_PLATFORM;
    }

    result = RegSetValueExA(
        hKey, service_id, 0, REG_BINARY,
        (BYTE*)failure, sizeof(ntrip_service_failure_t)
    );

    RegCloseKey(hKey);

    return (result == ERROR_SUCCESS) ? NTRIP_ATLAS_SUCCESS : NTRIP_ATLAS_ERROR_PLATFORM;
}

/**
 * Load failure data from registry
 */
static int windows_load_failure_data(
    const char* service_id,
    ntrip_service_failure_t* failure
) {
    if (!service_id || !failure) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    HKEY hKey;
    LONG result = RegOpenKeyExA(
        HKEY_CURRENT_USER,
        "Software\\NTRIP-Atlas\\Failures",
        0, KEY_READ, &hKey
    );

    if (result != ERROR_SUCCESS) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    DWORD size = sizeof(ntrip_service_failure_t);
    result = RegQueryValueExA(
        hKey, service_id, NULL, NULL,
        (BYTE*)failure, &size
    );

    RegCloseKey(hKey);

    return (result == ERROR_SUCCESS && size == sizeof(ntrip_service_failure_t))
        ? NTRIP_ATLAS_SUCCESS : NTRIP_ATLAS_ERROR_INVALID_PARAM;
}

/**
 * Clear failure data from registry
 */
static int windows_clear_failure_data(const char* service_id) {
    if (!service_id) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    HKEY hKey;
    LONG result = RegOpenKeyExA(
        HKEY_CURRENT_USER,
        "Software\\NTRIP-Atlas\\Failures",
        0, KEY_WRITE, &hKey
    );

    if (result != ERROR_SUCCESS) {
        return NTRIP_ATLAS_SUCCESS; // Already cleared
    }

    RegDeleteValueA(hKey, service_id);
    RegCloseKey(hKey);

    return NTRIP_ATLAS_SUCCESS;
}

/**
 * Log message to console
 */
static void windows_log_message(int level, const char* message) {
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
 * Get milliseconds (using GetTickCount64)
 */
static uint32_t windows_get_time_ms(void) {
    return (uint32_t)GetTickCount64();
}

/**
 * Get seconds since epoch
 */
static uint32_t windows_get_time_seconds(void) {
    return (uint32_t)time(NULL);
}

/**
 * Windows Platform Implementation
 */
const ntrip_platform_t ntrip_platform_windows = {
    .interface_version = 2,
    .http_stream = windows_http_stream,
    .send_nmea = windows_send_nmea,
    .store_credential = windows_store_credential,
    .load_credential = windows_load_credential,
    .store_failure_data = windows_store_failure_data,
    .load_failure_data = windows_load_failure_data,
    .clear_failure_data = windows_clear_failure_data,
    .log_message = windows_log_message,
    .get_time_ms = windows_get_time_ms,
    .get_time_seconds = windows_get_time_seconds
};

#endif // _WIN32
