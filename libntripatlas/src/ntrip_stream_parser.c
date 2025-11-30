/**
 * NTRIP Sourcetable Streaming Parser
 *
 * Memory-efficient line-by-line parser for NTRIP sourcetables.
 * Processes STR lines as they arrive via HTTP streaming callback.
 * Maintains minimal state between callback invocations.
 *
 * Copyright (c) 2024 NTRIP Atlas Contributors
 * Licensed under MIT License
 */

#include "ntrip_stream_parser.h"
#include "ntrip_atlas.h"
#include "ntrip_atlas_config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/**
 * Parser state for streaming sourcetable processing
 */
typedef struct {
    char line_buffer[NTRIP_LINE_BUFFER_SIZE];
    size_t line_pos;
    uint8_t in_sourcetable;
    uint8_t parsing_complete;

    // Current best mountpoint
    ntrip_mountpoint_t best;
    uint8_t has_result;

    // Search criteria
    double user_lat;
    double user_lon;
    const ntrip_service_config_t* service;
    const ntrip_selection_criteria_t* criteria;

    // Early termination thresholds
    uint8_t stop_threshold_score;   // Stop if score exceeds this
    double stop_threshold_distance; // Stop if distance under this
} ntrip_stream_parser_state_t;

/**
 * Initialize streaming parser state
 */
void ntrip_stream_parser_init(
    ntrip_stream_parser_state_t* state,
    double user_lat,
    double user_lon,
    const ntrip_service_config_t* service,
    const ntrip_selection_criteria_t* criteria
) {
    memset(state, 0, sizeof(ntrip_stream_parser_state_t));
    state->user_lat = user_lat;
    state->user_lon = user_lon;
    state->service = service;
    state->criteria = criteria;

    // Early termination: stop if we find excellent nearby service
    state->stop_threshold_score = 80;    // 80% score or better
    state->stop_threshold_distance = 5.0; // Within 5km
}

/**
 * Parse a single STR (station/stream) line from sourcetable
 *
 * Format: STR;mountpoint;identifier;format;format-details;carrier;nav-system;
 *         network;country;lat;lon;nmea;solution;generator;compression;auth;fee;bitrate
 */
static int parse_str_line(
    ntrip_stream_parser_state_t* state,
    const char* line
) {
    ntrip_mountpoint_t mp;
    memset(&mp, 0, sizeof(mp));

    // Parse CSV fields from STR line
    char* line_copy = strdup(line);
    if (!line_copy) return -1;

    char* saveptr = NULL;
    char* token;
    int field = 0;

    token = strtok_r(line_copy, ";", &saveptr);
    while (token != NULL && field < 18) {
        switch (field) {
            case 0: // Type (should be "STR")
                if (strcmp(token, "STR") != 0) {
                    free(line_copy);
                    return 0; // Not a STR line, skip
                }
                break;
            case 1: // Mountpoint
                strncpy(mp.mountpoint, token, NTRIP_ATLAS_MAX_MOUNTPOINT - 1);
                break;
            case 2: // Identifier (location name)
                strncpy(mp.identifier, token, sizeof(mp.identifier) - 1);
                break;
            case 3: // Format
                strncpy(mp.format, token, NTRIP_ATLAS_MAX_FORMAT - 1);
                break;
            case 4: // Format details
                strncpy(mp.format_details, token, NTRIP_ATLAS_MAX_DETAILS - 1);
                break;
            case 6: // Navigation system
                strncpy(mp.nav_system, token, sizeof(mp.nav_system) - 1);
                break;
            case 9: // Latitude
                mp.latitude = atof(token);
                break;
            case 10: // Longitude
                mp.longitude = atof(token);
                break;
            case 11: // NMEA required
                mp.nmea_required = atoi(token);
                break;
            case 13: // Receiver type
                strncpy(mp.receiver_type, token, sizeof(mp.receiver_type) - 1);
                break;
            case 15: // Authentication
                if (strcmp(token, "B") == 0) {
                    mp.authentication = NTRIP_AUTH_BASIC;
                } else if (strcmp(token, "D") == 0) {
                    mp.authentication = NTRIP_AUTH_DIGEST;
                } else {
                    mp.authentication = NTRIP_AUTH_NONE;
                }
                break;
            case 16: // Fee
                mp.fee_required = (strcmp(token, "Y") == 0);
                break;
            case 17: // Bitrate
                mp.bitrate = atoi(token);
                break;
        }
        token = strtok_r(NULL, ";", &saveptr);
        field++;
    }

    free(line_copy);

    // Validate required fields
    if (mp.mountpoint[0] == '\0' || mp.latitude == 0.0 || mp.longitude == 0.0) {
        return 0; // Incomplete data
    }

    // Calculate distance from user position
    mp.distance_km = ntrip_atlas_calculate_distance(
        state->user_lat, state->user_lon,
        mp.latitude, mp.longitude
    );

    // Apply selection criteria filters
    if (state->criteria) {
        // Distance filter
        if (state->criteria->max_distance_km > 0 &&
            mp.distance_km > state->criteria->max_distance_km) {
            return 0;
        }

        // Free only filter
        if (state->criteria->free_only && mp.fee_required) {
            return 0;
        }

        // Format filter
        if (state->criteria->required_formats[0] != '\0') {
            if (strstr(mp.format, state->criteria->required_formats) == NULL &&
                strstr(mp.format_details, state->criteria->required_formats) == NULL) {
                return 0;
            }
        }

        // Bitrate filter
        if (state->criteria->min_bitrate > 0 && mp.bitrate < state->criteria->min_bitrate) {
            return 0;
        }
    }

    // Calculate suitability score (0-100)
    uint8_t score = 0;

    // Distance component (40 points): closer is better
    if (mp.distance_km < 10.0) {
        score += 40;
    } else if (mp.distance_km < 50.0) {
        score += 30;
    } else if (mp.distance_km < 100.0) {
        score += 20;
    } else if (mp.distance_km < 200.0) {
        score += 10;
    }

    // Service quality component (30 points)
    if (state->service) {
        score += (state->service->quality_rating * 6); // 5 stars = 30 points
    }

    // Technical compatibility (20 points)
    if (strstr(mp.format, "RTCM3") != NULL) score += 15;
    if (strstr(mp.nav_system, "GPS") != NULL) score += 5;

    // Access ease (10 points)
    if (mp.authentication == NTRIP_AUTH_NONE) score += 5;
    if (!mp.fee_required) score += 5;

    mp.suitability_score = score;
    mp.service = state->service;

    // Keep if better than current best
    if (!state->has_result || score > state->best.suitability_score) {
        memcpy(&state->best, &mp, sizeof(ntrip_mountpoint_t));
        state->has_result = 1;

        // Check for early termination
        if (score >= state->stop_threshold_score &&
            mp.distance_km <= state->stop_threshold_distance) {
            return 1; // Signal to stop streaming
        }
    }

    return 0;
}

/**
 * Process incoming data chunk
 *
 * Called by HTTP streaming callback with each received chunk.
 * Assembles lines and parses them incrementally.
 */
int ntrip_stream_parser_process_chunk(
    ntrip_stream_parser_state_t* state,
    const char* chunk,
    size_t len
) {
    for (size_t i = 0; i < len; i++) {
        char c = chunk[i];

        // Line termination
        if (c == '\n' || c == '\r') {
            if (state->line_pos > 0) {
                state->line_buffer[state->line_pos] = '\0';

                // Check for sourcetable markers
                if (strncmp(state->line_buffer, "ENDSOURCETABLE", 14) == 0) {
                    state->parsing_complete = 1;
                    return 1; // Stop streaming
                }

                // Parse STR lines
                if (strncmp(state->line_buffer, "STR;", 4) == 0) {
                    state->in_sourcetable = 1;
                    if (parse_str_line(state, state->line_buffer) != 0) {
                        return 1; // Early termination triggered
                    }
                }

                state->line_pos = 0;
            }
            continue;
        }

        // Buffer character
        if (state->line_pos < NTRIP_LINE_BUFFER_SIZE - 1) {
            state->line_buffer[state->line_pos++] = c;
        }
        // Line too long - reset buffer to prevent overflow
        else {
            state->line_pos = 0;
        }
    }

    return 0; // Continue streaming
}

/**
 * Get best result after parsing complete
 */
int ntrip_stream_parser_get_result(
    const ntrip_stream_parser_state_t* state,
    ntrip_mountpoint_t* result
) {
    if (!state->has_result) {
        return -1;
    }

    memcpy(result, &state->best, sizeof(ntrip_mountpoint_t));
    return 0;
}

/**
 * Streaming callback wrapper
 * This is the actual callback passed to http_stream()
 */
static int streaming_callback_wrapper(
    const char* chunk,
    size_t len,
    void* context
) {
    ntrip_stream_parser_state_t* state = (ntrip_stream_parser_state_t*)context;
    return ntrip_stream_parser_process_chunk(state, chunk, len);
}

/**
 * High-level function to query a service using streaming
 */
int ntrip_query_service_streaming(
    const ntrip_platform_t* platform,
    const ntrip_service_config_t* service,
    double user_lat,
    double user_lon,
    const ntrip_selection_criteria_t* criteria,
    ntrip_mountpoint_t* result
) {
    // Initialize parser state
    ntrip_stream_parser_state_t state;
    ntrip_stream_parser_init(&state, user_lat, user_lon, service, criteria);

    // Start streaming HTTP request
    int ret = platform->http_stream(
        service->base_url,
        service->port,
        service->ssl,
        "/",  // Sourcetable path
        streaming_callback_wrapper,
        &state,
        10000  // 10 second timeout
    );

    if (ret < 0) {
        return ret;
    }

    // Extract result
    return ntrip_stream_parser_get_result(&state, result);
}
