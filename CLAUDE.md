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

### Repository Structure (CRITICAL)
**ALL SERVICE DATA belongs in the data-repo submodule which points to ntrip-atlas-data repository!**

```
ntrip-atlas/              # Main library repository
├── libntripatlas/        # Core C library
├── tools/                # Development tools
├── tests/                # Unit tests
└── data-repo/            # Git submodule → ntrip-atlas-data.git (AUTHORITATIVE)
    ├── schema.yaml       # YAML service definition schema
    ├── VERSION           # YYYYMMDD.sequence format
    ├── global/           # Global services (RTK2go, Point One Polaris)
    ├── EMEA/            # Europe, Middle East, Africa (27+ services)
    ├── APAC/            # Asia Pacific (11+ services)
    └── AMER/            # Americas (33+ services)

~/dev/ntrip-atlas-data/   # Standalone checkout for direct work (KEEP IN SYNC)
```

**✅ CURRENT IMPLEMENTATION STATUS**:
- **data-repo/**: Contains ALL 70+ services using unified schema format
- **Schema standardized**: All services use consistent YAML format with hierarchical coverage
- **Build process**: Uses data-repo/ for complete service database generation
- **Sync strategy**: Work in data-repo/, keep standalone checkout in sync when needed

**🚫 DEPRECATED/INCORRECT**:
- Any `data/` directory in main repo is temporary/test data only
- If found, copy valuable content to data-repo/ and remove it
- Never work in data/ - always use data-repo/ submodule

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

### Data Repository Workflow (CRITICAL)
```bash
# CORRECT: Always work in the submodule
cd data-repo/
# Add/edit YAML files
# git add . && git commit -m "Add new services"
# git push origin main
cd ..
# git add data-repo && git commit -m "Update data submodule"

# SYNC: Keep standalone checkout updated when needed
cd ~/dev/ntrip-atlas-data/
git pull origin main

# NEVER: Work directly in data/ directory
# If files found there: copy to data-repo/, then rm -rf data/
```

### Code Generation Workflow
```bash
# YAML services -> C structures (build time)
tools/generators/yaml_to_c.py data-repo/ -> libntripatlas/src/generated/

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

### Current YAML Schema Format (v1.0)
Every service YAML must follow this exact format (see data-repo/schema.yaml):
```yaml
service:
  id: "unique_identifier"           # For credential storage key (lowercase, underscores)
  provider: "Organization Name"     # Full organization name
  country: "US"                     # ISO 3166-1 alpha-2 country code

  endpoints:
    - hostname: "hostname.domain"   # Max 31 characters for embedded compatibility
      port: 2101                    # Standard NTRIP port
      ssl: false                    # Boolean for HTTPS support

  coverage:
    bounding_box:
      lat_min: -90.0                # Decimal degrees
      lat_max: 90.0
      lon_min: -180.0
      lon_max: 180.0

    hierarchical:                   # Quality ratings by geographic scale (1-5)
      regional: 4                   # Cross-country coverage quality
      national: 5                   # National coverage quality
      state: 4                      # State/province coverage quality
      local: 3                      # Local area coverage quality

  authentication:
    method: "basic"                 # "none", "basic", or "digest"
    required: true                  # Boolean
    registration_required: true     # Boolean
    registration_url: "https://..."# If registration required

  quality:
    reliability_rating: 5           # 1-5 stars (5=government, 4=commercial, 3=community)
    accuracy_rating: 5              # 1-5 technical accuracy rating
    network_type: "government"      # "government", "commercial", "community", "research"

  metadata:
    description: "Service description"
    stations: 100                   # Number of reference stations
    coverage_area: "Geographic description"
    last_verified: "2024-12-01"     # ISO date when verified
    ntrip_version: "2.0"            # NTRIP protocol version: "1.0", "2.0", "mixed"
    rtcm_formats: ["RTCM 3.3", "RTCM 3.2"]  # Supported RTCM message formats
    constellations: ["GPS", "GLONASS", "Galileo", "BeiDou"]  # GNSS constellations
    notes: "Additional technical details"
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

## GitHub Actions Workflow Monitoring

### Checking Workflow Status After Every Commit

**CRITICAL**: Always verify GitHub Actions pass after commits to prevent CI/CD failures.

#### Quick Status Check Commands
```bash
# Check recent workflow runs for both repositories
gh run list --repo Bwooce/ntrip-atlas --limit 3
gh run list --repo Bwooce/ntrip-atlas-data --limit 3

# Get detailed failure logs if needed
gh run view --repo Bwooce/ntrip-atlas [RUN_ID] --log-failed
gh run view --repo Bwooce/ntrip-atlas-data [RUN_ID] --log-failed
```

#### Automated Monitoring Setup
```bash
# Add to your shell profile for automatic checks
alias check-ntrip-ci='echo "NTRIP Atlas CI Status:" && \
    gh run list --repo Bwooce/ntrip-atlas --limit 1 --json conclusion,headBranch,status && \
    gh run list --repo Bwooce/ntrip-atlas-data --limit 1 --json conclusion,headBranch,status'
```

#### Common Workflow Failures and Fixes

1. **Code Quality Issues**:
   - **TODO/FIXME comments**: Convert to specific implementation tasks or remove
   - **Unused parameters**: Add `(void)param_name;` to suppress warnings
   - **Memory constraint violations**: Update test limits or optimize structures

2. **Repository URL Mismatches**:
   - **Wrong GitHub username in workflows**: Ensure Bwooce vs bruce consistency
   - **Clone failures**: Verify repository URLs match actual GitHub repository names

3. **Security Scanner False Positives**:
   - **"script" in "description"**: Use word boundaries in regex patterns `\bscript\b`
   - **Legitimate content flagged**: Review and refine security patterns

4. **Memory Test Failures**:
   - **ESP32 constraints**: Balance realistic limits vs embedded constraints
   - **String buffer limits**: Consider real-world hostname/mountpoint lengths
   - **Static memory budget**: Account for expanded service data and optimizations

### General CI/CD Health Monitoring

#### Weekly Health Check Routine
```bash
# Check overall repository health
gh repo view Bwooce/ntrip-atlas --json defaultBranch,hasIssuesEnabled,hasWikiEnabled
gh repo view Bwooce/ntrip-atlas-data --json defaultBranch,hasIssuesEnabled,hasWikiEnabled

# Review recent workflow trends
gh run list --repo Bwooce/ntrip-atlas --limit 10 --json conclusion,createdAt,headBranch
gh run list --repo Bwooce/ntrip-atlas-data --limit 10 --json conclusion,createdAt,headBranch
```

#### Proactive Monitoring Strategy
- **Before each coding session**: Check latest workflow status
- **After each commit**: Verify workflows pass within 5 minutes
- **Weekly review**: Analyze workflow trends and failure patterns
- **Monthly audit**: Review and update workflow definitions for efficiency

#### Integration with Development Workflow
```bash
# Add to git commit hook (optional)
#!/bin/bash
echo "Checking CI status..."
sleep 30  # Wait for GitHub to trigger workflow
gh run list --repo $(gh repo view --json owner,name --jq '.owner.login + "/" + .name') --limit 1 --json conclusion --jq '.[] | select(.conclusion != "success") | "❌ CI Failed"'
```

This monitoring approach prevents CI/CD failures from accumulating and ensures
continuous integration health across both repositories.

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
- remember to check the github actions 5 minutes after every commit using the command line tools, and fix problems that appear
- remember to run the code compilation and tests locally before any git commits are done