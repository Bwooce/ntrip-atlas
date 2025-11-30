# NTRIP Atlas

**Global NTRIP Service Discovery and Auto-Selection Library**

A comprehensive, community-driven database and C library for automatic discovery and intelligent selection of NTRIP (Network Transport of RTCM via Internet Protocol) services worldwide.

## Problem Statement

Currently, GNSS/RTK developers must:
- ❌ **Manually research** NTRIP providers by country/region
- ❌ **Hardcode configurations** for each deployment location
- ❌ **Manage authentication** credentials insecurely
- ❌ **Handle service failures** without automatic failover
- ❌ **Navigate fragmented** service discovery across government, commercial, and community providers

**NTRIP Atlas solves this** by providing the missing infrastructure for global RTK deployment.

## What This Project Provides

### 🗺️ **Comprehensive Service Database**
- **Government services**: Geoscience Australia, EUREF-IP (Europe), Finland FinnRef, Massachusetts MaCORS
- **Commercial networks**: Point One Navigation Polaris (global)
- **Community services**: RTK2GO (800+ stations worldwide)
- **Geographic organization**: EMEA, APAC, Americas, Global regions
- **Initial dataset**: 6 documented services with full technical specifications

### 🎯 **Intelligent Auto-Discovery**
- **Automatic nearest base selection** using real-time sourcetable queries
- **Quality-based ranking** considering distance, reliability, and technical compatibility
- **Format filtering** (RTCM 2.0, RTCM 3.x, RAW) and constellation support (GPS+GLONASS+Galileo)
- **Smart failure tracking** with exponential backoff (1h → 4h → 12h → 1d → 3d → 1w → 2w → 1m)
- **Automatic failover** to next best service when primary fails

### 🔐 **Secure Credential Management**
- **Platform-native storage** (ESP32 Preferences, Linux Keyring, Windows Credential Store)
- **Per-service authentication** with automatic credential application
- **Multiple auth methods** (none, basic, digest, certificate-based)

### 🌐 **Universal Compatibility**
- **Pure C core** with platform abstraction layer
- **Arduino/ESP32** native support with minimal memory footprint
- **Linux/Windows** desktop applications
- **Python/JavaScript bindings** for web and script integration

## Quick Start

### Arduino/ESP32
```cpp
#include <ntrip_atlas.h>

void setup() {
    Serial.begin(115200);

    // Initialize with platform support
    ntrip_atlas_init(&ntrip_platform_esp32);

    // Store credentials once (saved automatically)
    ntrip_atlas_set_credentials("GeoscienceAustralia",
                               "your_username", "your_password");

    // Get current GPS position
    double lat = -33.479, lon = 151.109;

    // Auto-discover best available service (skips failed services)
    ntrip_best_service_t service;
    if (ntrip_atlas_find_best(&service, lat, lon) == 0) {
        Serial.printf("Selected: %s:%d/%s (%.1fkm, quality %d/5)\n",
                     service.server, service.port, service.mountpoint,
                     service.distance_km, service.quality_score);

        // Connect with your NTRIP client
        if (ntripClient.connect(service.server, service.port,
                              service.mountpoint, service.username, service.password)) {
            // Record success to reset failure count
            ntrip_atlas_record_service_success(service.service_info->id);
        } else {
            // Record failure for exponential backoff
            ntrip_atlas_record_service_failure(service.service_info->id);
        }
    }
}
```

### Linux/Desktop
```c
#include <ntrip_atlas.h>

int main() {
    ntrip_atlas_init(&ntrip_platform_linux);

    ntrip_best_service_t service;
    if (ntrip_atlas_find_best(&service, lat, lon) == 0) {
        printf("Connecting to %s:%d/%s\n",
               service.server, service.port, service.mountpoint);
        // Use service details with your NTRIP client...
    }
    return 0;
}
```

## Project Structure

**Main Library Repository** (this repository):
```
ntrip-atlas/
├── libntripatlas/            # Core C library
│   ├── src/                  # Platform-agnostic core
│   ├── include/              # Public API headers
│   ├── platforms/            # Platform-specific implementations
│   └── examples/             # Usage examples
├── tools/                    # Development and maintenance tools
├── tests/                    # Unit and integration tests
├── docs/                     # Documentation and specifications
└── README.md                 # This file
```

**Service Database Repository** (separate repository: [ntrip-atlas-data](https://github.com/bruce/ntrip-atlas-data)):
```
ntrip-atlas-data/
├── data/                     # Community-maintained service database
│   ├── global/              # Worldwide services (RTK2go, IGS)
│   ├── europe/              # European services
│   ├── asia-pacific/        # Asia Pacific region
│   ├── americas/            # North and South America
│   └── africa/              # African regional services
├── VERSION                   # Database version tracking
└── README.md                # Service contribution guidelines
```

## Contributing

### Adding New NTRIP Services
1. **Research service details** (coverage, authentication, reliability)
2. **Create YAML entry** in the [ntrip-atlas-data](https://github.com/bruce/ntrip-atlas-data) repository
3. **Validate YAML file** using the validator from this repository: `tools/validators/service_validator.py`
4. **Test service connectivity** and verify technical specifications
5. **Submit pull request** to the data repository with `source: "community"` or contact information

### Library Development
1. **Core improvements** to discovery algorithms and reliability
2. **Platform support** for new embedded systems or operating systems
3. **Performance optimizations** for memory-constrained environments
4. **Testing infrastructure** and validation tools

## Community

- **GitHub Discussions**: Design decisions and feature requests
- **Issue Tracker**: Bug reports and service updates
- **Wiki**: Regional service documentation and deployment guides
- **Security**: Responsible disclosure for authentication vulnerabilities

## License

- **Core Library**: MIT License (maximum compatibility)
- **Service Database**: CC0 1.0 (public domain, no attribution required)
- **Documentation**: Creative Commons Attribution 4.0

---

**Mission**: Democratize global RTK/GNSS precision positioning by eliminating the complexity and fragmentation in NTRIP service discovery and deployment.