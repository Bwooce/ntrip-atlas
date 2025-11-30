/**
 * NTRIP Sourcetable Streaming Parser
 *
 * Internal header for streaming sourcetable processing.
 * Not part of public API.
 *
 * Copyright (c) 2024 NTRIP Atlas Contributors
 * Licensed under MIT License
 */

#ifndef NTRIP_STREAM_PARSER_H
#define NTRIP_STREAM_PARSER_H

#include "ntrip_atlas.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque parser state structure
 * Actual definition in .c file to hide implementation details
 */
typedef struct ntrip_stream_parser_state_t ntrip_stream_parser_state_t;

/**
 * Query a single NTRIP service using streaming HTTP
 *
 * This is the primary interface for memory-efficient service discovery.
 * Streams sourcetable data and processes it line-by-line without buffering
 * the entire response.
 *
 * @param platform   Platform abstraction with http_stream function
 * @param service    Service to query
 * @param user_lat   User latitude in decimal degrees
 * @param user_lon   User longitude in decimal degrees
 * @param criteria   Optional selection criteria (can be NULL)
 * @param result     Output: best mountpoint found
 * @return          0 on success, negative error code on failure
 */
int ntrip_query_service_streaming(
    const ntrip_platform_t* platform,
    const ntrip_service_config_t* service,
    double user_lat,
    double user_lon,
    const ntrip_selection_criteria_t* criteria,
    ntrip_mountpoint_t* result
);

#ifdef __cplusplus
}
#endif

#endif // NTRIP_STREAM_PARSER_H
