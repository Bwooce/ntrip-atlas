/**
 * Unit Tests for Distance Calculation
 * Tests the Haversine distance formula implementation
 */

#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <stdbool.h>

// Simplified implementation for testing
double haversine_distance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0; // Earth's radius in kilometers
    const double PI = 3.141592653589793;

    // Convert to radians
    double lat1_rad = lat1 * PI / 180.0;
    double lon1_rad = lon1 * PI / 180.0;
    double lat2_rad = lat2 * PI / 180.0;
    double lon2_rad = lon2 * PI / 180.0;

    // Haversine formula
    double dlat = lat2_rad - lat1_rad;
    double dlon = lon2_rad - lon1_rad;

    double a = sin(dlat/2) * sin(dlat/2) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(dlon/2) * sin(dlon/2);
    double c = 2 * asin(sqrt(a));

    return R * c;
}

bool test_distance_zero() {
    // Same point should have zero distance
    double dist = haversine_distance(-33.8568, 151.2153, -33.8568, 151.2153);
    return fabs(dist) < 0.001;
}

bool test_distance_sydney_melbourne() {
    // Sydney to Melbourne: approximately 714 km
    double sydney_lat = -33.8568, sydney_lon = 151.2153;
    double melbourne_lat = -37.8136, melbourne_lon = 144.9631;

    double dist = haversine_distance(sydney_lat, sydney_lon, melbourne_lat, melbourne_lon);

    // Allow 1% tolerance
    return fabs(dist - 714.0) < 7.0;
}

bool test_distance_sydney_perth() {
    // Sydney to Perth: approximately 3290 km
    double sydney_lat = -33.8568, sydney_lon = 151.2153;
    double perth_lat = -31.9505, perth_lon = 115.8605;

    double dist = haversine_distance(sydney_lat, sydney_lon, perth_lat, perth_lon);

    // Allow 1% tolerance
    return fabs(dist - 3290.0) < 33.0;
}

bool test_distance_across_dateline() {
    // Test across international dateline
    double tokyo_lat = 35.6762, tokyo_lon = 139.6503;
    double la_lat = 34.0522, la_lon = -118.2437;

    double dist = haversine_distance(tokyo_lat, tokyo_lon, la_lat, la_lon);

    // Approximately 8800 km
    return fabs(dist - 8800.0) < 100.0;
}

bool test_distance_north_south() {
    // Test north-south distance (same longitude)
    double north_lat = 60.0, lon = 0.0;
    double south_lat = 50.0;

    double dist = haversine_distance(north_lat, lon, south_lat, lon);

    // 10 degrees latitude ≈ 1111 km
    return fabs(dist - 1111.0) < 10.0;
}

bool test_distance_east_west() {
    // Test east-west distance at equator
    double lat = 0.0;
    double west_lon = 0.0, east_lon = 10.0;

    double dist = haversine_distance(lat, west_lon, lat, east_lon);

    // 10 degrees longitude at equator ≈ 1111 km
    return fabs(dist - 1111.0) < 10.0;
}

bool test_distance_edge_cases() {
    // Test extreme coordinates
    double north_pole_lat = 90.0, north_pole_lon = 0.0;
    double south_pole_lat = -90.0, south_pole_lon = 0.0;

    double dist = haversine_distance(north_pole_lat, north_pole_lon, south_pole_lat, south_pole_lon);

    // Half Earth circumference ≈ 20015 km
    return fabs(dist - 20015.0) < 50.0;
}

int main() {
    printf("Running distance calculation tests...\n");

    struct {
        const char* name;
        bool (*test_func)();
    } tests[] = {
        {"Zero distance", test_distance_zero},
        {"Sydney to Melbourne", test_distance_sydney_melbourne},
        {"Sydney to Perth", test_distance_sydney_perth},
        {"Across dateline", test_distance_across_dateline},
        {"North-South distance", test_distance_north_south},
        {"East-West distance", test_distance_east_west},
        {"Edge cases", test_distance_edge_cases},
    };

    int passed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < total; i++) {
        printf("Test %d: %s... ", i + 1, tests[i].name);
        if (tests[i].test_func()) {
            printf("PASS\n");
            passed++;
        } else {
            printf("FAIL\n");
        }
    }

    printf("\nResults: %d/%d tests passed\n", passed, total);

    if (passed == total) {
        printf("✅ All distance tests passed!\n");
        return 0;
    } else {
        printf("❌ Some distance tests failed!\n");
        return 1;
    }
}