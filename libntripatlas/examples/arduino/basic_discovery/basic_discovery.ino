/**
 * NTRIP Atlas Basic Discovery Example
 *
 * Demonstrates automatic NTRIP service discovery and selection
 * for ESP32-based GNSS/RTK applications.
 *
 * Hardware: ESP32-S3 with WiFi capability
 * GPS: Any UART-connected GNSS receiver (optional for demo)
 */

#include <WiFi.h>
#include <ntrip_atlas.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Example coordinates (Sydney, Australia)
// In real application, get these from your GPS receiver
double current_latitude = -33.8568;
double current_longitude = 151.2153;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("NTRIP Atlas Basic Discovery Example");
    Serial.println("===================================");

    // Initialize WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.printf("Connected to WiFi. IP: %s\n", WiFi.localIP().toString().c_str());

    // Initialize NTRIP Atlas with ESP32 platform support
    ntrip_atlas_error_t result = ntrip_atlas_init(&ntrip_platform_esp32);
    if (result != NTRIP_ATLAS_SUCCESS) {
        Serial.printf("Failed to initialize NTRIP Atlas: %s\n",
                     ntrip_atlas_error_string(result));
        return;
    }

    Serial.printf("NTRIP Atlas initialized. Database version: %s\n",
                 NTRIP_ATLAS_DATABASE_VERSION);

    // One-time credential setup (stored automatically in ESP32 Preferences)
    // For Geoscience Australia - requires free registration at https://gnss.ga.gov.au/
    Serial.println("\nSetting up credentials...");
    result = ntrip_atlas_set_credentials("geoscience_australia",
                                        "your_username",    // Replace with your username
                                        "your_password");   // Replace with your password

    if (result == NTRIP_ATLAS_SUCCESS) {
        Serial.println("Credentials stored securely");
    } else {
        Serial.printf("Failed to store credentials: %s\n",
                     ntrip_atlas_error_string(result));
    }

    // Auto-discover best NTRIP service for current location
    Serial.printf("\nDiscovering NTRIP services for location: %.4f, %.4f\n",
                 current_latitude, current_longitude);

    ntrip_best_service_t service;
    result = ntrip_atlas_find_best(&service, current_latitude, current_longitude);

    if (result == NTRIP_ATLAS_SUCCESS) {
        Serial.println("\n✅ Best NTRIP service found:");
        Serial.printf("  Server: %s:%d%s\n",
                     service.server, service.port, service.ssl ? " (SSL)" : "");
        Serial.printf("  Mountpoint: %s\n", service.mountpoint);
        Serial.printf("  Distance: %.1f km\n", service.distance_km);
        Serial.printf("  Quality Score: %d/100\n", service.quality_score);
        Serial.printf("  Provider: %s (%s)\n",
                     service.service_info->provider,
                     service.service_info->network_type == NTRIP_NETWORK_GOVERNMENT ? "Government" :
                     service.service_info->network_type == NTRIP_NETWORK_COMMERCIAL ? "Commercial" : "Community");

        // Test connectivity
        Serial.println("\n🔗 Testing connectivity...");
        result = ntrip_atlas_test_service(&service);
        if (result == NTRIP_ATLAS_SUCCESS) {
            Serial.println("✅ Service is accessible and responding");
        } else {
            Serial.printf("❌ Service test failed: %s\n", ntrip_atlas_error_string(result));
        }

        // Show how to use with your NTRIP client
        Serial.println("\n📡 Ready to connect your NTRIP client:");
        Serial.printf("  ntripClient.connect(\"%s\", %d, \"%s\", \"%s\", \"%s\");\n",
                     service.server, service.port, service.mountpoint,
                     service.username, service.password);

    } else if (result == NTRIP_ATLAS_ERROR_NO_SERVICES) {
        Serial.println("❌ No suitable NTRIP services found in this region");
        Serial.println("   Try moving closer to populated areas or checking credentials");
    } else {
        Serial.printf("❌ Discovery failed: %s\n", ntrip_atlas_error_string(result));
    }

    // Demonstrate fallback discovery
    Serial.println("\n🔄 Demonstrating fallback discovery...");
    ntrip_best_service_t primary, fallback;
    result = ntrip_atlas_find_best_with_fallback(&primary, &fallback,
                                               current_latitude, current_longitude);

    if (result == NTRIP_ATLAS_SUCCESS) {
        Serial.printf("Primary: %s:%d/%s (%.1fkm, score %d)\n",
                     primary.server, primary.port, primary.mountpoint,
                     primary.distance_km, primary.quality_score);
        Serial.printf("Fallback: %s:%d/%s (%.1fkm, score %d)\n",
                     fallback.server, fallback.port, fallback.mountpoint,
                     fallback.distance_km, fallback.quality_score);
    }

    // Show memory usage
    Serial.println("\n💾 Memory usage:");
    Serial.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("  PSRAM free: %d bytes\n", ESP.getFreePsram());

    Serial.println("\nExample complete!");
}

void loop() {
    // In a real application, you might:
    // 1. Read GPS coordinates from your receiver
    // 2. Periodically rediscover if moving long distances
    // 3. Use the service details to maintain NTRIP connection

    delay(10000);
}

/**
 * Advanced Usage Examples
 */

void demonstrateFilteredDiscovery() {
    // Custom selection criteria
    ntrip_selection_criteria_t criteria = {0};
    strncpy(criteria.required_formats, "RTCM 3.2", sizeof(criteria.required_formats));
    strncpy(criteria.required_systems, "GPS+GLONASS", sizeof(criteria.required_systems));
    criteria.max_distance_km = 50.0;
    criteria.min_quality_rating = 4;
    criteria.free_only = 1;  // Only free services
    criteria.preferred_network = NTRIP_NETWORK_GOVERNMENT;

    ntrip_best_service_t service;
    ntrip_atlas_error_t result = ntrip_atlas_find_best_filtered(&service,
                                                              current_latitude,
                                                              current_longitude,
                                                              &criteria);

    if (result == NTRIP_ATLAS_SUCCESS) {
        Serial.println("Found service matching strict criteria");
    }
}

void demonstrateRegionalServices() {
    // Get all services in a region (e.g., Australia)
    ntrip_service_config_t services[10];
    size_t count = 10;

    ntrip_atlas_error_t result = ntrip_atlas_list_services_in_region(
        -45.0, -10.0,    // Australia latitude range
        110.0, 160.0,    // Australia longitude range
        services, &count, 10);

    if (result == NTRIP_ATLAS_SUCCESS) {
        Serial.printf("Found %zu services in region:\n", count);
        for (size_t i = 0; i < count; i++) {
            Serial.printf("  %s (%s)\n", services[i].provider, services[i].country);
        }
    }
}