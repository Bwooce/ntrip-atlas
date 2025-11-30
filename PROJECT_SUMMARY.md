# NTRIP Atlas Project Summary

**Created**: 2024-11-29
**Location**: `~/dev/ntrip-atlas/`
**Mission**: Democratize global RTK/GNSS precision positioning

## Project Overview

**NTRIP Atlas** is a community-driven project that solves the global fragmentation in NTRIP (RTK correction service) discovery. It provides:

- 📚 **Comprehensive service database** covering government, commercial, and community providers worldwide
- 🎯 **Intelligent auto-discovery** with distance-based selection and quality scoring
- 🔐 **Secure credential management** using platform-native storage
- 🌐 **Universal C library** with ESP32/Arduino, Linux, Windows support
- ⚡ **Memory optimized** for single-lookup embedded scenarios (3KB RAM vs 33KB+ traditional)

## Complete Project Structure

**Main Library Repository** ([ntrip-atlas](https://github.com/bruce/ntrip-atlas)):
```
ntrip-atlas/                           # Root directory
├── README.md                          # Project overview and quick start
├── DESIGN.md                          # Comprehensive design decisions
├── PROJECT_SUMMARY.md                 # This document
├── LICENSING.md                       # Dual licensing explanation
├── LICENSE-CODE                       # MIT License for C library
│
├── libntripatlas/                     # 💻 Core C library (MIT License)
│   ├── include/                       # Public API headers
│   │   ├── ntrip_atlas.h             # Main API with platform abstraction
│   │   └── ntrip_atlas_config.h      # Memory optimization configuration
│   ├── src/                           # Platform-agnostic core implementation
│   ├── platforms/                     # Platform-specific implementations
│   │   ├── esp32/                     # ESP32/Arduino support
│   │   ├── linux/                     # Linux with libcurl/keyring
│   │   └── windows/                   # Windows with WinHTTP/CredStore
│   └── examples/                      # Usage examples
│       ├── arduino/                   # Arduino IDE examples
│       │   ├── library.properties     # Arduino library metadata
│       │   └── basic_discovery/       # Basic usage demonstration
│       │       └── basic_discovery.ino # ESP32 example sketch
│       ├── platformio/                # PlatformIO examples
│       └── linux/                     # Linux command-line examples
│
├── tools/                             # 🔧 Development and maintenance
│   ├── generators/                    # YAML to C code generators
│   ├── validators/                    # Service data validation tools
│   └── testing/                       # Integration testing framework
│
└── docs/                              # 📖 Documentation
    ├── api/                           # API reference documentation
    ├── tutorials/                     # Step-by-step guides
    └── specifications/                # Technical specifications
```

**Service Database Repository** ([ntrip-atlas-data](https://github.com/bruce/ntrip-atlas-data)):
```
ntrip-atlas-data/                      # Service database repository
├── README.md                          # Community contribution guidelines
├── LICENSE-DATA                       # CC0 Public Domain license
│
├── data/                              # 🗺️ Community service database (CC0)
│   ├── VERSION                        # Database version (YYYYMMDD.sequence format)
│   ├── global/                        # Worldwide services
│   │   ├── rtk2go.yaml               # RTK2go community network
│   │   └── pointone-polaris.yaml     # Point One Navigation commercial service
│   ├── emea/                          # Europe, Middle East, Africa
│   │   ├── euref-ip.yaml             # European government network
│   │   └── finland-finnref.yaml     # Finnish government service
│   ├── apac/                          # Asia Pacific
│   │   └── australia-ga.yaml         # Geoscience Australia government service
│   ├── americas/                      # North and South America
│   │   └── usa-massachusetts-macors.yaml # Massachusetts state network
│   └── africa/                        # African regional services (empty currently)
```

## Key Design Decisions

### 🎯 **Core Philosophy**
- **Separate static from dynamic data**: Compile-time service metadata + runtime sourcetable discovery
- **Optimize for single lookup**: ESP32 needs ~3KB RAM vs traditional 33KB+ approaches
- **Maximum freedom**: CC0 public domain data + MIT licensed code

### 🧠 **Memory Optimization Strategy**
```c
// Traditional approach (33KB+ RAM):
ntrip_service_t all_services[128];        // 8KB+
ntrip_mountpoint_t all_mountpoints[512];  // 25KB+

// NTRIP Atlas streaming approach (3KB RAM):
ntrip_candidate_t best_candidate;         // 32 bytes
char http_buffer[1024];                   // 1KB (reused)
char parse_buffer[2048];                  // 2KB (reused)
```

### 📅 **Versioning Strategy**
- **Database**: `YYYYMMDD.sequence` format (e.g., `20241129.01`)
- **Schema**: Semantic versioning for data structure changes
- **Library**: Standard semantic versioning for API compatibility

### 🌍 **Geographic Organization**
- **Global**: Worldwide services (RTK2go, IGS network)
- **EMEA**: Europe, Middle East, Africa
- **APAC**: Asia Pacific (including Australia, Japan, China)
- **Americas**: North and South America
- **Africa**: Dedicated African regional services

### 🔐 **Security Model**
- **ESP32**: Encrypted NVS via Preferences library
- **Linux**: libsecret (GNOME Keyring) or keyctl
- **Windows**: Windows Credential Store
- **Principle**: Platform-native secure storage, no hardcoded credentials

## Sample YAML Service Definition

```yaml
# data/apac/australia-ga.yaml
service:
  id: "geoscience_australia"
  name: "Geoscience Australia GNSS Service"
  country: "AUS"
  provider: "Geoscience Australia"
  organization_type: "government"

  endpoints:
    - protocol: "https"
      hostname: "ntrip.data.gnss.ga.gov.au"
      port: 443

  coverage:
    bounding_box:
      lat_min: -54.0  # Heard Island
      lat_max: -9.0   # Torres Strait
      lon_min: 72.0
      lon_max: 168.0  # Norfolk Island

  authentication:
    required: true
    method: "basic"
    registration_url: "https://gnss.ga.gov.au/user-registration"

  quality:
    reliability_rating: 5  # 1-5 scale
    accuracy_rating: 5     # Professional survey grade
```

## Sample C API Usage

```c
#include <ntrip_atlas.h>

// Initialize with platform support
ntrip_atlas_init(&ntrip_platform_esp32);

// Store credentials securely (one time)
ntrip_atlas_set_credentials("geoscience_australia", "user", "pass");

// Auto-discover best service
ntrip_best_service_t service;
if (ntrip_atlas_find_best(&service, -33.479, 151.109) == NTRIP_ATLAS_SUCCESS) {
    printf("Best: %s:%d/%s (%.1fkm away)\n",
           service.server, service.port, service.mountpoint, service.distance_km);

    // Use with your NTRIP client
    ntripClient.connect(service.server, service.port, service.mountpoint,
                       service.username, service.password);
}
```

## Licensing: Maximum Freedom

### 📚 **Service Data**: CC0 Public Domain
- **Zero restrictions**: Use freely without attribution
- **Commercial unlimited**: Embed in proprietary products
- **Compete freely**: Create alternative libraries using same data
- **Global public good**: Accessible to all governments, companies, researchers

### 💻 **C Library**: MIT License
- **Very permissive**: Modify and redistribute freely
- **Commercial use**: Allowed with minimal attribution requirement
- **Industry standard**: Well-understood and widely accepted

## Implementation Status

### ✅ **Completed**
- [x] Project structure and build system design
- [x] Memory optimization architecture for ESP32
- [x] Comprehensive API design with platform abstraction
- [x] Sample service database (Australia, RTK2go)
- [x] Arduino example implementation
- [x] Versioning and licensing framework

### 🚧 **Next Steps**
- [ ] C library implementation (core discovery engine)
- [ ] Platform implementations (ESP32, Linux, Windows)
- [ ] YAML to C code generation tools
- [ ] Service validation and testing framework
- [ ] Community contribution guidelines

## Success Metrics & Goals

### 📊 **Adoption Targets**
- **1000+ GitHub stars** within first year
- **100+ contributors** from global GNSS community
- **50+ countries** represented in service database
- **200+ NTRIP services** documented and validated

### 🎯 **Technical Goals**
- **<3KB RAM usage** on ESP32 for single lookup
- **<100ms discovery time** with network access
- **<1% false positives** in service selection
- **99%+ service validation accuracy**

## Why This Matters

**Current State**: Every GNSS/RTK developer manually researches and hardcodes NTRIP services for each deployment region. This creates:
- 🔴 **Fragmented ecosystem** with duplicated research effort
- 🔴 **Regional lock-in** preventing global deployment
- 🔴 **Security risks** from hardcoded credentials
- 🔴 **Maintenance burden** when services change

**NTRIP Atlas Future**: One-line service discovery that works globally:
- 🟢 **Universal deployment**: Same code works worldwide
- 🟢 **Automatic optimization**: Always finds best local service
- 🟢 **Security by default**: Encrypted credential management
- 🟢 **Community maintained**: Shared effort, shared benefit

**Vision**: Make precision GNSS positioning as simple as connecting to WiFi - just provide coordinates and get optimal RTK service automatically.

---

**Project Location**: `~/dev/ntrip-atlas/`
**Repository**: Ready for GitHub initialization
**License**: MIT (code) + CC0 (data) = Maximum Freedom
**Community**: Designed for global collaborative maintenance