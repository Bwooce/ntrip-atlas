# NTRIP Atlas Library

**Memory-efficient NTRIP service discovery library with streaming HTTP support**

## Overview

NTRIP Atlas is a C library for automatic discovery and intelligent selection of NTRIP (Networked Transport of RTCM via Internet Protocol) correction services worldwide. It enables embedded devices and desktop applications to automatically find the best nearby RTK base station without hardcoded configurations.

### Key Features

- **Streaming HTTP Interface**: Handles unlimited sourcetable sizes with minimal memory (1.1KB during discovery)
- **Memory Optimized**: 75% less RAM usage vs buffer-based approaches
- **Early Termination**: Stops searching when excellent service found (90-95% bandwidth savings)
- **Platform Agnostic**: Works on ESP32, Linux, Windows, and macOS
- **VRS Support**: NMEA GGA position formatting for Virtual Reference Stations
- **Smart Scoring**: Considers distance, quality, format compatibility, and access requirements
- **Failure Tracking**: Exponential backoff for unreliable services

### Memory Efficiency

| Approach | RAM Usage | Notes |
|----------|-----------|-------|
| Old (buffer) | 4.5 KB | Failed on large sourcetables |
| **New (streaming)** | **1.1 KB** | Handles unlimited sizes |
| Peak (headers) | 2.8 KB | Brief spike during HTTP headers |

## Quick Start

### ESP32/Arduino

```cpp
#include <WiFi.h>
#include "ntrip_atlas.h"

void setup() {
    // Connect WiFi
    WiFi.begin("your-ssid", "password");
    while (WiFi.status() != WL_CONNECTED) delay(100);

    // Initialize NTRIP Atlas
    ntrip_atlas_init(&ntrip_platform_esp32);

    // Find best service near you
    ntrip_best_service_t service;
    ntrip_atlas_find_best(&service, 37.7749, -122.4194);  // San Francisco

    // Connect to NTRIP mountpoint
    WiFiClient client;
    client.connect(service.server, service.port);
    client.printf("GET /%s HTTP/1.1\r\n...", service.mountpoint);

    // Send GGA if VRS requires position
    if (service.nmea_required) {
        char gga[128];
        ntrip_atlas_format_gga(gga, sizeof(gga), 37.7749, -122.4194, 10.0, 4, 12);
        client.write(gga);
    }

    // Receive RTCM corrections
    while (client.available()) {
        uint8_t rtcm_byte = client.read();
        // Send to GNSS receiver
    }
}
```

### Linux

```c
#include "ntrip_atlas.h"

int main() {
    // Initialize with Linux platform
    ntrip_atlas_init(&ntrip_platform_linux);

    // Set selection criteria
    ntrip_selection_criteria_t criteria = {0};
    strcpy(criteria.required_formats, "RTCM3");
    criteria.max_distance_km = 50.0;
    criteria.free_only = 1;

    // Find best service
    ntrip_best_service_t service;
    int err = ntrip_atlas_find_best_filtered(
        &service, 51.5074, -0.1278,  // London
        &criteria
    );

    if (err == NTRIP_ATLAS_SUCCESS) {
        printf("Best service: %s:%d/%s\n",
               service.server, service.port, service.mountpoint);
        printf("Distance: %.1f km\n", service.distance_km);
        printf("Score: %d/100\n", service.quality_score);
    }

    return 0;
}
```

## Building

### Linux/macOS (Make)

```bash
cd libntripatlas
make
sudo make install
```

Requires: `libcurl`, `gcc/clang`

### Linux/macOS/Windows (CMake)

```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

### ESP32 (PlatformIO)

```ini
[env:esp32dev]
platform = espressif32
framework = arduino
lib_deps = ntrip-atlas
```

### ESP32 (Arduino IDE)

1. Copy `libntripatlas/` to Arduino libraries folder
2. Include in sketch: `#include "ntrip_atlas.h"`
3. Link against WiFi library

## Architecture

### Streaming Parser

The library uses a line-by-line streaming parser instead of buffering entire sourcetables:

```
HTTP Response (400KB)
    ↓
Chunked Reading (512 bytes)
    ↓
Line Assembly (256 bytes)
    ↓
STR Line Parsing
    ↓
Immediate Scoring
    ↓
Keep Only Best → Result (< 1KB)
```

### Platform Abstraction

All platform-specific operations go through function pointers:

- **HTTP Streaming**: WiFiClient (ESP32), libcurl (Linux), WinHTTP (Windows)
- **Credential Storage**: Preferences (ESP32), files (Linux), Credential Manager (Windows)
- **Time Functions**: Platform-specific monotonic/wall clocks

## API Reference

### Core Functions

#### Initialize Library
```c
ntrip_atlas_error_t ntrip_atlas_init(const ntrip_platform_t* platform);
```

#### Find Best Service
```c
ntrip_atlas_error_t ntrip_atlas_find_best(
    ntrip_best_service_t* result,
    double latitude,
    double longitude
);
```

#### Find with Criteria
```c
ntrip_atlas_error_t ntrip_atlas_find_best_filtered(
    ntrip_best_service_t* result,
    double latitude,
    double longitude,
    const ntrip_selection_criteria_t* criteria
);
```

#### Format GGA Sentence
```c
int ntrip_atlas_format_gga(
    char* buffer,
    size_t max_len,
    double latitude,      // Decimal degrees
    double longitude,     // Decimal degrees
    double altitude_m,    // Meters
    uint8_t fix_quality,  // 1=GPS, 2=DGPS, 4=RTK, 5=Float
    uint8_t satellites    // Number of satellites
);
```

### Selection Criteria

```c
typedef struct {
    char required_formats[64];      // "RTCM 3.2" or "RTCM 3.2,RTCM 3.1"
    char required_systems[32];      // "GPS" or "GPS+GLONASS"
    uint16_t min_bitrate;           // Minimum data rate
    ntrip_auth_method_t max_auth;   // Maximum auth complexity
    uint8_t free_only;              // Exclude paid services
    double max_distance_km;         // Maximum distance
    uint8_t min_quality_rating;     // Minimum 1-5 stars
    ntrip_network_type_t preferred_network;
} ntrip_selection_criteria_t;
```

### Result Structure

```c
typedef struct {
    char server[128];                // Hostname/IP
    uint16_t port;                   // TCP port
    uint8_t ssl;                     // Use HTTPS/TLS
    char mountpoint[32];             // Mountpoint name
    char username[64];               // Credentials (if required)
    char password[64];
    double distance_km;              // Distance from user
    uint8_t quality_score;           // 0-100 score

    // Mountpoint details (copied inline, safe after discovery)
    double mountpoint_latitude;
    double mountpoint_longitude;
    char format[32];                 // "RTCM 3.2"
    uint8_t nmea_required;           // VRS needs position updates

    const ntrip_service_config_t* service_info;
} ntrip_best_service_t;
```

## Platform Implementation

### Implementing a New Platform

1. Create platform file: `platforms/myplatform/ntrip_platform_myplatform.c`

2. Implement required functions:

```c
// Streaming HTTP request
int my_http_stream(
    const char* host, uint16_t port, uint8_t ssl,
    const char* path,
    ntrip_stream_callback_t on_data,
    void* user_context,
    uint32_t timeout_ms
) {
    // 1. Connect to host:port (TCP or TLS)
    // 2. Send HTTP GET request
    // 3. Skip HTTP headers
    // 4. Read body in chunks (512 bytes recommended)
    // 5. Call on_data(chunk, len, user_context) for each chunk
    // 6. Stop if callback returns non-zero
    // 7. Close connection
    return NTRIP_ATLAS_SUCCESS;
}

// Send NMEA to open connection
int my_send_nmea(void* connection, const char* nmea_sentence) {
    // Send NMEA string to connection
    // Return success/error
}

// Credential storage (secure if possible)
int my_store_credential(const char* key, const char* value);
int my_load_credential(const char* key, char* value, size_t max_len);

// Failure tracking (optional, can be NULL)
int my_store_failure_data(const char* service_id, const ntrip_service_failure_t* failure);
int my_load_failure_data(const char* service_id, ntrip_service_failure_t* failure);
int my_clear_failure_data(const char* service_id);

// Logging
void my_log_message(int level, const char* message);

// Time functions
uint32_t my_get_time_ms(void);        // Monotonic milliseconds
uint32_t my_get_time_seconds(void);   // Unix timestamp
```

3. Export platform structure:

```c
const ntrip_platform_t ntrip_platform_myplatform = {
    .interface_version = 2,
    .http_stream = my_http_stream,
    .send_nmea = my_send_nmea,
    .store_credential = my_store_credential,
    .load_credential = my_load_credential,
    .store_failure_data = my_store_failure_data,
    .load_failure_data = my_load_failure_data,
    .clear_failure_data = my_clear_failure_data,
    .log_message = my_log_message,
    .get_time_ms = my_get_time_ms,
    .get_time_seconds = my_get_time_seconds
};
```

## Configuration

Edit `include/ntrip_atlas_config.h` for platform-specific tuning:

```c
// Memory limits
#define NTRIP_LINE_BUFFER_SIZE     256   // Single sourcetable line
#define NTRIP_TCP_CHUNK_SIZE       512   // TCP read chunks

// Early termination thresholds (in parser code)
stop_threshold_score = 80;      // Stop if score ≥ 80/100
stop_threshold_distance = 5.0;  // Stop if distance ≤ 5km
```

## Testing

### Memory Test (ESP32)
```cpp
size_t before = ESP.getFreeHeap();
ntrip_best_service_t result;
ntrip_atlas_find_best(&result, lat, lon);
size_t used = before - ESP.getFreeHeap();
Serial.printf("Memory used: %d bytes\n", used);
// Should be < 3000 bytes peak
```

### Large Sourcetable Test
```c
// Test with RTK2go (400KB+ sourcetable)
ntrip_service_config_t rtk2go = {
    .provider = "RTK2go",
    .base_url = "rtk2go.com",
    .port = 2101,
    // ...
};
// Should handle without issues
```

## Performance

### Typical Discovery Times
- WiFi connection: 1-2 seconds
- HTTP request: 100-300 ms
- Sourcetable streaming: 0.5-3 seconds (network dependent)
- **Total: 2-6 seconds**

### Bandwidth Usage
- Full sourcetable download: 200-400 KB
- **With early termination: 5-20 KB (90-95% reduction)**

## Troubleshooting

### "Out of memory" on ESP32
- Check free heap before discovery: `ESP.getFreeHeap()`
- Reduce `NTRIP_TCP_CHUNK_SIZE` to 256 bytes
- Disable other memory-intensive features during discovery

### Sourcetable parsing fails
- Enable debug logging: `platform->log_message(3, "Debug message")`
- Check HTTP response headers are properly skipped
- Verify sourcetable format matches NTRIP 1.0 or 2.0 spec

### VRS connection fails
- Ensure GGA sentence is properly formatted
- Check NMEA checksum is correct
- Verify position is within VRS network coverage

## License

MIT License - see LICENSE file for details

## Contributing

See CONTRIBUTING.md for development guidelines

## Documentation

- [Streaming Interface Guide](../STREAMING_INTERFACE.md)
- [Changes Summary](../CHANGES_SUMMARY.md)
- [ESP32 Example](../examples/esp32_streaming_example.cpp)

## Support

- Issues: https://github.com/ntrip-atlas/ntrip-atlas-code/issues
- Discussions: https://github.com/ntrip-atlas/ntrip-atlas-code/discussions

## Credits

NTRIP Atlas Contributors - see AUTHORS file
