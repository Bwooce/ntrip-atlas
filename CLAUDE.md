# CLAUDE.md - Development Guidance for NTRIP Atlas

Development context and guidelines for Claude when working on this project.

## Project Overview

NTRIP Atlas is a C library + community database for global NTRIP service discovery. The goal is to eliminate hardcoded NTRIP configurations and enable automatic service selection worldwide.

**Key Principle**: Separate static discoverable data (compiled) from dynamic runtime data (queried).

## Critical Design Constraints

### Memory Optimization (ESP32 Target)
- **Total RAM budget**: ~3KB for discovery process
- **Strategy**: Streaming parse, never store full service list
- **Early termination**: Stop when good result found (score >80, distance <5km)
- **Buffer reuse**: Single HTTP + parse buffer, reused per service

### Single Lookup Optimization
Most embedded applications only do discovery once per runtime:
```c
// NOT this (stores everything):
load_all_services() -> parse_all_mountpoints() -> score_all() -> select_best()

// THIS (stream processing):
for_each_service_by_distance() -> stream_parse() -> score_immediately() -> keep_only_best()
```

### Platform Abstraction Strategy
Never directly call platform APIs in core library:
- ESP32: WiFiClient, Preferences
- Linux: libcurl, keyring
- Windows: WinHTTP, Credential Store

All platform access goes through function pointers in `ntrip_platform_t`.

## Architecture Reminders

### Data Organization
```
data/
├── VERSION               # YYYYMMDD.sequence format
├── global/              # RTK2go, IGS network
├── emea/               # Europe, Middle East, Africa
├── apac/               # Asia Pacific
├── americas/           # North & South America
└── africa/             # African regional services
```

### Geographic Coverage Strategy
Services YAML should include geographic bounding boxes:
```yaml
coverage:
  bounding_box:
    lat_min: -45.0
    lat_max: -10.0
    lon_min: 110.0
    lon_max: 160.0
```

### Versioning Strategy
- **Database**: `20241129.01` format (date.sequence)
- **Schema**: Semantic versioning for YAML structure changes
- **Library**: Standard semver for API compatibility

## Build System Guidelines

### Code Generation Workflow
```bash
# YAML services -> C structures (build time)
tools/generators/yaml_to_c.py data/ -> libntripatlas/src/generated/

# Include generated data in compilation
#include "ntrip_services_data.h"
```

### Platform Profiles
```c
// Embedded profile (ESP32)
#define NTRIP_ATLAS_PROFILE_EMBEDDED
#define NTRIP_ATLAS_STATIC_ALLOCATION
#define NTRIP_ATLAS_SINGLE_LOOKUP

// Desktop profile
#define NTRIP_ATLAS_PROFILE_DESKTOP
#define NTRIP_ATLAS_DYNAMIC_ALLOCATION
#define NTRIP_ATLAS_CACHING
```

### Memory Layout (ESP32)
```c
// Compact service representation (64 bytes each)
typedef struct __attribute__((packed)) {
    uint8_t service_id;
    char hostname[48];
    uint16_t port;
    uint8_t flags;      // ssl, auth_required, etc.
    int16_t lat_min_deg100, lat_max_deg100;
    int16_t lon_min_deg100, lon_max_deg100;
} ntrip_service_compact_t;
```

## Service Database Guidelines

### Service Quality Ratings
Use community assessment for static ratings:
- **5 stars**: Government services (GA, NRCan, BKG)
- **4 stars**: Commercial with SLA (Trimble VRS, Leica SmartNet)
- **3 stars**: Community (RTK2go, reliable community stations)
- **2 stars**: Experimental or intermittent services
- **1 star**: Legacy or unreliable services

### Required Service Information
Every service YAML must include:
```yaml
service:
  id: "unique_identifier"           # For credential storage key
  provider: "Organization Name"
  country: "ISO-3166"
  endpoints: [...]                  # hostname, port, ssl
  coverage: {...}                   # geographic boundaries
  authentication: {...}             # method, registration_url
  quality: {...}                    # reliability_rating, accuracy_rating
```

### Geographic Boundaries
Be conservative with coverage areas - better to underestimate than overestimate service range.

## Implementation Guidelines

### Error Handling Strategy
Always provide graceful degradation:
```c
// Find best with fallback
ntrip_atlas_find_best_with_fallback(&primary, &fallback, lat, lon);

// If primary fails, fallback is available
// If both fail, return meaningful error code
```

### Testing Approach
- **Unit tests**: Core distance calculation, parsing logic
- **Integration tests**: Real NTRIP service connectivity
- **Memory tests**: Ensure ESP32 memory limits not exceeded
- **Performance tests**: Discovery time < 100ms target

### Service Validation
Before adding services to database:
1. Verify service is publicly accessible
2. Test authentication requirements
3. Confirm geographic coverage claims
4. Validate RTCM format support
5. Check service reliability over time

## Security Considerations

### Credential Storage
Never log or expose credentials:
```c
// GOOD: Store credentials securely
ntrip_atlas_set_credentials("service_id", username, password);

// BAD: Never log credentials
Serial.printf("Connecting with %s:%s", username, password); // NO!
```

### Service Validation
Validate all input from NTRIP sourcetables:
- Check coordinate ranges are reasonable
- Validate mountpoint name characters
- Sanitize format strings before parsing
- Limit response sizes to prevent DoS

## Performance Guidelines

### Network Optimization
- **Timeout values**: 5s connect, 10s total for sourcetable
- **Early termination**: Stop after 3-5 services if good result
- **Caching**: Cache sourcetables for desktop applications
- **Compression**: Support gzip sourcetables where available

### Memory Optimization
- **Stack usage**: Keep function stacks small (<1KB per level)
- **String handling**: Use fixed-size buffers, avoid dynamic allocation
- **Struct packing**: Use `__attribute__((packed))` for network structures
- **Buffer reuse**: Reuse HTTP and parse buffers across services

## Common Patterns

### Distance-Based Service Selection
```c
// Always sort services by distance first
sort_services_by_distance(user_lat, user_lon, services, count);

// Query nearest services first
for (int i = 0; i < count; i++) {
    if (services[i].distance_km > MAX_USEFUL_DISTANCE) break;
    // Query this service...
}
```

### Quality Scoring Formula
```c
uint8_t score = 0;
score += distance_score * 0.4;    // Closest is best
score += service_score * 0.3;     // Government > Commercial > Community
score += tech_score * 0.2;        // Format compatibility
score += access_score * 0.1;      // Easier access preferred
```

## Common Mistakes to Avoid

### Memory Management
- ❌ Don't allocate large arrays on ESP32
- ❌ Don't store full sourcetables in RAM
- ❌ Don't use recursive parsing (stack overflow risk)
- ✅ Use streaming parsers with fixed buffers
- ✅ Process one service at a time

### Service Database
- ❌ Don't include services without testing them
- ❌ Don't copy-paste coordinates without verification
- ❌ Don't include expired or discontinued services
- ✅ Verify every service before adding
- ✅ Include registration/auth requirements

### API Design
- ❌ Don't expose internal data structures
- ❌ Don't require dynamic allocation in public API
- ❌ Don't make blocking calls without timeouts
- ✅ Use opaque handles for internal state
- ✅ Provide both blocking and non-blocking variants

## Development Workflow

### Adding New Services
1. Research service (coverage, auth, reliability)
2. Create YAML file in appropriate region
3. Test connectivity and validate coordinates
4. Run validation tools
5. Submit PR with service documentation

### Code Changes
1. Maintain platform abstraction - no direct platform calls
2. Update memory estimates if changing data structures
3. Test on ESP32 if memory-related changes
4. Run full test suite before committing
5. Update API documentation for public interface changes

### Temporary Files and Working Documents
1. **True temporary files** should be created in `/tmp/` directory, not in project root
2. **Working documents** (design reviews, implementation notes) should be excluded from git:
   - Add `*_REVIEW.md`, `*_CONTEXT.md`, `NOTES.md` patterns to `.gitignore`
   - Internal working documents are for development context only, not public commit
3. **Use descriptive names** for temporary files with context for later recovery
4. **Clean up** temporary files when no longer needed

## Current Implementation Status

### Service Database (Completed)
Initial dataset of documented NTRIP services added:
- **RTK2GO**: Global community network (800+ stations)
- **EUREF-IP**: European government network (BKG/ROB casters)
- **Point One Polaris**: Commercial global network (1,400+ stations)
- **Finland FinnRef**: Finnish government network (47 stations)
- **Massachusetts MaCORS**: US state network (22 stations)
- **Geoscience Australia**: Australian government network

### Failure Tracking System (Completed)
Implemented exponential backoff with:
- 8-level progression: 1h → 4h → 12h → 1d → 3d → 1w → 2w → 1m
- Persistent failure state across device reboots
- Automatic service healing after backoff periods
- Platform abstraction for failure data storage

### Validator Infrastructure (Completed)
- Python YAML service validator with comprehensive schema checking
- C unit tests for distance calculations
- ESP32 memory constraint validation tests
- Continuous integration test framework

### API Design (Completed)
- Platform abstraction layer with function pointers
- Service failure tracking API
- Memory-optimized structures for embedded systems
- Dual licensing (MIT for code, CC0 for data)

### Next Steps
- Initialize separate GitHub repositories (ntrip-atlas-code, ntrip-atlas-data)
- Set up automated releases with YYYYMMDD.sequence versioning
- Create community contribution guidelines

## Future Considerations

### Mobile/Roaming Support
For applications that move between regions:
- Implement service caching to avoid repeated discoveries
- Add region change detection
- Support background service updates

### Real-Time Monitoring
Eventually could add optional live service monitoring:
- Service availability checking
- Performance metrics collection
- Community reliability reporting

**Remember**: The core library must work without any live monitoring dependencies.