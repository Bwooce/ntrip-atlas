/**
 * NMEA GGA Sentence Formatter
 *
 * Generates NMEA GGA sentences for VRS position updates.
 *
 * Copyright (c) 2024 NTRIP Atlas Contributors
 * Licensed under MIT License
 */

#include "ntrip_atlas.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

/**
 * Convert decimal degrees to NMEA degrees-minutes format
 *
 * @param decimal    Latitude or longitude in decimal degrees
 * @param is_lat     1 for latitude, 0 for longitude
 * @param output     Output buffer for formatted degrees-minutes
 * @param direction  Output: 'N','S','E', or 'W'
 */
static void decimal_to_nmea(double decimal, int is_lat, char* output, char* direction) {
    double abs_decimal = fabs(decimal);
    int degrees = (int)abs_decimal;
    double minutes = (abs_decimal - degrees) * 60.0;

    if (is_lat) {
        snprintf(output, 16, "%02d%08.5f", degrees, minutes);
        *direction = (decimal >= 0) ? 'N' : 'S';
    } else {
        snprintf(output, 16, "%03d%08.5f", degrees, minutes);
        *direction = (decimal >= 0) ? 'E' : 'W';
    }
}

/**
 * Calculate NMEA checksum
 *
 * XOR of all characters between $ and *
 */
static uint8_t calculate_checksum(const char* sentence) {
    uint8_t checksum = 0;
    const char* p = sentence;

    // Skip leading $
    if (*p == '$') p++;

    // XOR until * or end of string
    while (*p && *p != '*') {
        checksum ^= *p;
        p++;
    }

    return checksum;
}

/**
 * Get current UTC time in HHMMSS.SS format
 */
static void get_utc_time(char* output, size_t max_len) {
    time_t now = time(NULL);
    struct tm* utc = gmtime(&now);

    snprintf(output, max_len, "%02d%02d%02d.00",
             utc->tm_hour, utc->tm_min, utc->tm_sec);
}

/**
 * Format NMEA GGA sentence for VRS position updates
 *
 * Example output:
 * $GPGGA,123519.00,4807.03810,N,01131.00000,E,1,08,0.9,545.4,M,46.9,M,,*47
 *
 * @param buffer        Output buffer for GGA sentence
 * @param max_len       Size of output buffer (recommend 128 bytes minimum)
 * @param latitude      Latitude in decimal degrees (-90 to +90)
 * @param longitude     Longitude in decimal degrees (-180 to +180)
 * @param altitude_m    Altitude in meters above WGS84 ellipsoid
 * @param fix_quality   Fix quality: 0=invalid, 1=GPS, 2=DGPS, 4=RTK fixed, 5=RTK float
 * @param satellites    Number of satellites in use (0-99)
 * @return             Length of formatted sentence, or negative on error
 */
int ntrip_atlas_format_gga(
    char* buffer,
    size_t max_len,
    double latitude,
    double longitude,
    double altitude_m,
    uint8_t fix_quality,
    uint8_t satellites
) {
    // Validate inputs
    if (!buffer || max_len < 128) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    if (latitude < -90.0 || latitude > 90.0 ||
        longitude < -180.0 || longitude > 180.0) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    if (fix_quality > 9 || satellites > 99) {
        return NTRIP_ATLAS_ERROR_INVALID_PARAM;
    }

    // Get UTC time
    char utc_time[16];
    get_utc_time(utc_time, sizeof(utc_time));

    // Convert coordinates to NMEA format
    char lat_nmea[16], lon_nmea[16];
    char lat_dir, lon_dir;
    decimal_to_nmea(latitude, 1, lat_nmea, &lat_dir);
    decimal_to_nmea(longitude, 0, lon_nmea, &lon_dir);

    // HDOP (horizontal dilution of precision) - use fixed value for now
    double hdop = 1.0;

    // Geoid separation - approximation, VRS doesn't usually care about precision here
    double geoid_sep = 0.0;

    // DGPS age and reference station - empty for initial position
    const char* dgps_age = "";
    const char* ref_station = "";

    // Build GGA sentence without checksum
    int len = snprintf(buffer, max_len,
        "$GPGGA,%s,%s,%c,%s,%c,%d,%02d,%.1f,%.1f,M,%.1f,M,%s,%s",
        utc_time,          // UTC time
        lat_nmea,          // Latitude DDMM.MMMMM
        lat_dir,           // N/S
        lon_nmea,          // Longitude DDDMM.MMMMM
        lon_dir,           // E/W
        fix_quality,       // Fix quality
        satellites,        // Number of satellites
        hdop,              // HDOP
        altitude_m,        // Altitude above ellipsoid
        geoid_sep,         // Geoid separation
        dgps_age,          // Age of DGPS data
        ref_station        // Reference station ID
    );

    if (len < 0 || (size_t)len >= max_len - 5) {
        return NTRIP_ATLAS_ERROR_NO_MEMORY;
    }

    // Calculate and append checksum
    uint8_t checksum = calculate_checksum(buffer);
    int total_len = snprintf(buffer, max_len, "%s*%02X\r\n", buffer, checksum);

    if (total_len < 0 || (size_t)total_len >= max_len) {
        return NTRIP_ATLAS_ERROR_NO_MEMORY;
    }

    return total_len;
}
