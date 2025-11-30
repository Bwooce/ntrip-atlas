# NTRIP Atlas Design Document

## Design Philosophy

**Core Principle**: Separate **discoverable static data** from **runtime dynamic behavior** to enable robust offline-capable discovery while maintaining simplicity and reliability.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        User Application                         │
├─────────────────────────────────────────────────────────────────┤
│                    NTRIP Atlas Public API                      │
├─────────────────────────────────────────────────────────────────┤
│  Discovery Engine │ Credential Manager │  Quality Scorer       │
├─────────────────────────────────────────────────────────────────┤
│               Platform Abstraction Layer                       │
├─────────────────────────────────────────────────────────────────┤
│    ESP32      │    Linux     │   Windows   │     Mobile        │
│  WiFiClient   │    libcurl   │   WinHTTP   │   Platform SDK    │
│  Preferences  │   Keyring    │   CredStore │   Secure Store    │
└─────────────────────────────────────────────────────────────────┘
```

## Key Design Decisions

### 1. **C Language Choice**

**Decision**: Pure C implementation with platform abstraction

**Rationale**:
- ✅ **Universal compatibility**: Works on microcontrollers to desktop systems
- ✅ **Minimal dependencies**: No runtime requirements beyond libc
- ✅ **Memory efficiency**: Critical for ESP32 with 320KB RAM
- ✅ **Stable ABI**: Binary compatibility across compiler versions
- ✅ **Language bindings**: Easy to wrap for Python, JavaScript, Rust

**Alternatives Considered**:
- C++: Rejected due to larger footprint and ABI complexity
- Rust: Rejected due to embedded ecosystem immaturity
- Python: Rejected due to performance and deployment complexity

### 2. **YAML Data Format**

**Decision**: YAML for human-editable service database

**Rationale**:
- ✅ **Human-readable**: Easy community contributions and reviews
- ✅ **Git-friendly**: Clear diffs and merge conflict resolution
- ✅ **Structured**: Schema validation and type safety
- ✅ **Comments**: In-line documentation and metadata
- ✅ **Hierarchical**: Natural geographic and organizational grouping

**Alternatives Considered**:
- JSON: Rejected due to lack of comments and poor readability
- XML: Rejected due to verbosity and complexity
- TOML: Considered but less familiar to contributors

### 3. **Geographic Organization**

**Decision**: Regional directories aligned with business/cultural boundaries

```
data/
├── global/        # Worldwide services (RTK2go, IGS network)
├── emea/          # Europe, Middle East, Africa
├── apac/          # Asia Pacific (includes Australia, Japan, China)
├── americas/      # North and South America
└── africa/        # African regional services (separate from EMEA)
```

**Rationale**:
- ✅ **Cultural alignment**: Matches how surveying/GNSS industry organizes
- ✅ **Maintenance scalability**: Regional experts can manage their areas
- ✅ **Service discovery**: Users typically want nearby services first
- ✅ **Political sensitivity**: Avoids geopolitical boundary disputes
- ✅ **Time zone alignment**: Contributors work in similar hours

### 4. **Static vs Dynamic Data Separation**

**Decision**: Compile-time static data with runtime discovery

**What's Compiled In** (Static):
```yaml
# Can be researched and documented by community
country: Australia
provider: Geoscience Australia
base_url: ntrip.data.gnss.ga.gov.au
port: 443
ssl: true
auth_method: registration_required
network_type: government
quality_rating: 5
coverage_area: [-45.0, -10.0, 110.0, 160.0]
```

**What's Discovered at Runtime** (Dynamic):
```c
// Auto-discovered from sourcetable queries
char mountpoint[32];      // "SYDN00AUS0"
char identifier[64];      // "Sydney Observatory"
double latitude;          // -33.8568
double longitude;         // 151.2153
char format[32];          // "RTCM 3.2"
char format_details[128]; // "1004(1),1012(1),1077(10)"
uint16_t bitrate;         // 2400
uint8_t authentication;   // 2 (digest)
```

**Rationale**:
- ✅ **Maintainability**: Static data changes rarely, can be curated
- ✅ **Accuracy**: Dynamic data always current via sourcetable
- ✅ **Offline capability**: Works without internet for known services
- ✅ **Performance**: No runtime web scraping or complex queries

### 5. **Platform Abstraction Strategy**

**Decision**: Callback-based platform abstraction layer

```c
typedef struct {
    int (*http_request)(const char* host, uint16_t port, uint8_t ssl,
                       const char* path, char* response, size_t max_len);
    int (*store_credential)(const char* key, const char* value);
    int (*load_credential)(const char* key, char* value, size_t max_len);
    void (*log_message)(int level, const char* message);
    uint32_t (*get_time_ms)(void);
} ntrip_platform_t;
```

**Rationale**:
- ✅ **Clean separation**: Core logic independent of platform
- ✅ **Testing**: Easy to mock platform functions
- ✅ **Flexibility**: Each platform can optimize implementation
- ✅ **Minimal interface**: Only essential operations required

### 6. **Credential Security Model**

**Decision**: Platform-native secure storage with fallback

**ESP32 Implementation**:
```c
// Uses ESP32 Preferences (encrypted NVS)
int esp32_store_credential(const char* key, const char* value) {
    return prefs.putString(key, value) ? 0 : -1;
}
```

**Linux Implementation**:
```c
// Uses libsecret (GNOME Keyring) or keyctl
int linux_store_credential(const char* key, const char* value) {
    return secret_store_password("ntrip-atlas", key, value);
}
```

**Rationale**:
- ✅ **Security**: Uses platform's secure credential storage
- ✅ **User experience**: Single credential entry, automatic reuse
- ✅ **No hardcoding**: Eliminates credentials in source code
- ✅ **Audit trail**: Platform logging for security compliance

### 7. **Service Quality Scoring**

**Decision**: Multi-factor static scoring with runtime adjustment

```c
uint8_t calculateQualityScore(service, mountpoint, user_location) {
    // Distance scoring (40% weight)
    uint8_t distance_score = scoreByDistance(mountpoint->distance_km);

    // Service type scoring (30% weight)
    uint8_t service_score = scoreServiceType(service->network_type);

    // Technical compatibility (20% weight)
    uint8_t tech_score = scoreTechnicalMatch(mountpoint->format, requirements);

    // Access simplicity (10% weight)
    uint8_t access_score = scoreAccessMethod(service->auth_method, service->fee);

    return (distance_score * 0.4 + service_score * 0.3 +
            tech_score * 0.2 + access_score * 0.1);
}
```

**Rationale**:
- ✅ **Deterministic**: Same inputs produce same results
- ✅ **Transparent**: Scoring logic is documentable and auditable
- ✅ **Tunable**: Weights can be adjusted based on community feedback
- ✅ **Offline-capable**: No dependency on live monitoring services

### 8. **Error Handling Strategy**

**Decision**: Graceful degradation with fallback options

```c
typedef enum {
    NTRIP_SUCCESS = 0,
    NTRIP_ERROR_NO_SERVICES = 1,
    NTRIP_ERROR_NO_NETWORK = 2,
    NTRIP_ERROR_AUTH_FAILED = 3,
    NTRIP_ERROR_INVALID_RESPONSE = 4,
    NTRIP_ERROR_DISTANCE_LIMIT = 5
} ntrip_error_t;

ntrip_error_t ntrip_atlas_find_best_with_fallback(
    ntrip_best_service_t* primary,
    ntrip_best_service_t* fallback,
    double lat, double lon);
```

**Rationale**:
- ✅ **Robustness**: Always provides backup options when possible
- ✅ **Diagnostics**: Clear error codes for debugging
- ✅ **User experience**: Graceful handling of network failures
- ✅ **Mission critical**: Essential for precision agriculture/surveying

### 9. **Memory Management**

**Decision**: Static allocation with optional dynamic optimization

```c
// Static allocation for embedded systems
#define MAX_SERVICES 128
#define MAX_MOUNTPOINTS 512
static ntrip_service_t services[MAX_SERVICES];
static ntrip_mountpoint_t mountpoints[MAX_MOUNTPOINTS];

// Dynamic allocation for desktop systems (compile-time option)
#ifdef NTRIP_ATLAS_DYNAMIC_ALLOCATION
static ntrip_service_t* services = NULL;
static ntrip_mountpoint_t* mountpoints = NULL;
#endif
```

**Rationale**:
- ✅ **Predictability**: No heap allocation surprises on ESP32
- ✅ **Real-time safe**: No malloc/free in discovery path
- ✅ **Scalability**: Desktop systems can use dynamic allocation
- ✅ **Memory debugging**: Static allocation simplifies leak detection

### 10. **Data Versioning Strategy**

**Decision**: Date-based versioning with daily sequence numbers

```
# Format: YYYYMMDD.sequence
DATABASE_VERSION=20241129.01

# Where:
# YYYYMMDD = Date of last service data update
# sequence = Incremental number for same-day updates (01, 02, 03...)
```

**Implementation**:
```c
// Compiled into library
#define NTRIP_ATLAS_DATABASE_VERSION "20241129.01"

// Runtime version check
typedef struct {
    uint32_t date;      // 20241129 (YYYYMMDD format)
    uint8_t sequence;   // 01, 02, 03...
    uint8_t schema_major;
    uint8_t schema_minor;
} ntrip_atlas_version_t;
```

**Rationale**:
- ✅ **Clear chronological order**: Easy to determine which version is newer
- ✅ **Human readable**: Date format immediately shows data freshness
- ✅ **Multiple updates per day**: Sequence number handles urgent fixes
- ✅ **Small storage**: Fits in 6 bytes total
- ✅ **CI/CD friendly**: Automatically generated by build system

### 11. **Memory Optimization for Single Lookup**

**Decision**: Streaming parse with immediate scoring for ESP32-class devices

**Problem**: Traditional approach loads all services and mountpoints, requiring significant RAM.

**Solution**: Process one service at a time with early termination.

```c
// Instead of storing everything:
ntrip_service_t all_services[128];        // 8KB+ RAM
ntrip_mountpoint_t all_mountpoints[512];  // 25KB+ RAM

// Stream processing approach:
ntrip_candidate_t best_candidate;         // 32 bytes
ntrip_candidate_t current_candidate;      // 32 bytes
char http_buffer[1024];                   // 1KB (reused)
char parse_buffer[2048];                  // 2KB (reused)
// Total: ~3KB vs 33KB+
```

**Streaming Algorithm**:
```c
ntrip_atlas_error_t find_best_streaming(double lat, double lon) {
    ntrip_candidate_t best = {0};

    // Sort services by distance to user (nearest first)
    sort_services_by_distance(lat, lon);

    for (each service in distance order) {
        // Early termination if we have a good nearby result
        if (best.distance_m < 5000 && best.quality_score > 80) break;

        // Stream parse sourcetable without storing
        parse_sourcetable_streaming(service, lat, lon, &current_candidate);

        // Keep only the best result
        if (current_candidate.quality_score > best.quality_score) {
            best = current_candidate;
        }

        // Stop if we exceed maximum useful distance
        if (service.distance_km > 100) break;
    }

    return convert_candidate_to_result(&best);
}
```

**Benefits**:
- ✅ **3KB total RAM** vs 33KB+ traditional approach
- ✅ **Early termination**: Stop when good result found
- ✅ **Distance optimization**: Check nearest services first
- ✅ **Cache friendly**: Reuse buffers for each service
- ✅ **Real-time suitable**: Deterministic memory usage

### 12. **Service Failure Tracking and Exponential Backoff**

**Decision**: Intelligent failure tracking with exponential backoff to improve service reliability

**Implementation**:
```c
// Failure tracking structure
typedef struct {
    char service_id[64];         // Unique service identifier
    uint32_t failure_count;      // Number of consecutive failures
    uint32_t first_failure_time; // Timestamp of first failure (seconds since epoch)
    uint32_t next_retry_time;    // When service can be retried again
    uint32_t backoff_seconds;    // Current backoff period
} ntrip_service_failure_t;

// Default exponential backoff intervals (seconds)
// 1h, 4h, 12h, 1d, 3d, 1w, 2w, 1month
uint32_t default_backoff_intervals[8] = {
    3600,     // 1 hour
    14400,    // 4 hours
    43200,    // 12 hours
    86400,    // 1 day
    259200,   // 3 days
    604800,   // 1 week
    1209600,  // 2 weeks
    2629746   // 1 month (30.44 days average)
};
```

**API Functions**:
```c
// Record service failure and calculate next retry time
ntrip_atlas_error_t ntrip_atlas_record_service_failure(const char* service_id);

// Record service success (resets failure count)
ntrip_atlas_error_t ntrip_atlas_record_service_success(const char* service_id);

// Check if service is currently blocked due to failures
bool ntrip_atlas_is_service_blocked(const char* service_id);

// Get time until service can be retried (0 if available now)
uint32_t ntrip_atlas_get_service_retry_time(const char* service_id);
```

**Rationale**:
- ✅ **Network resilience**: Avoids repeatedly connecting to failed services
- ✅ **Intelligent recovery**: Gradual backoff prevents overwhelming failing services
- ✅ **Battery optimization**: Reduces unnecessary connection attempts on mobile devices
- ✅ **Service protection**: Prevents DoS-style repeated failed connections
- ✅ **Automatic healing**: Failed services automatically become available again after backoff
- ✅ **Platform persistence**: Failure state survives across device reboots

**Exponential Backoff Schedule**:
1. **1st failure**: Retry after 1 hour
2. **2nd failure**: Retry after 4 hours
3. **3rd failure**: Retry after 12 hours
4. **4th failure**: Retry after 1 day
5. **5th failure**: Retry after 3 days
6. **6th failure**: Retry after 1 week
7. **7th failure**: Retry after 2 weeks
8. **8th+ failures**: Retry after 1 month

**Integration with Discovery**:
- `ntrip_atlas_find_best()` automatically skips services currently in backoff
- Failed services are excluded from candidate selection until retry time expires
- Successful connections reset the failure count to zero for that service
- Failure tracking persists across application restarts via platform storage

## Implementation Phases

### Phase 1: Core Infrastructure
- [x] Project structure and build system
- [ ] Basic YAML service database with 10-15 major providers
- [ ] Core C library with platform abstraction
- [ ] ESP32/Arduino integration

### Phase 2: Enhanced Discovery
- [ ] Sourcetable parsing and caching
- [ ] Quality scoring and selection algorithms
- [ ] Linux/Windows platform implementations
- [ ] Comprehensive testing framework

### Phase 3: Community & Ecosystem
- [ ] Contribution guidelines and validation tools
- [ ] Python/JavaScript language bindings
- [ ] Documentation and tutorial content
- [ ] Community feedback integration

## Success Metrics

- **Adoption**: 1000+ GitHub stars, 100+ contributors
- **Coverage**: 50+ countries, 200+ NTRIP services documented
- **Reliability**: <1% false positives in service discovery
- **Performance**: <2MB compiled size, <100ms discovery time
- **Community**: Self-sustaining with 5+ regional maintainers