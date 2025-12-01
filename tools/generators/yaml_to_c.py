#!/usr/bin/env python3
"""
NTRIP Atlas YAML to C Generator

Converts YAML service definitions to C code for build-time inclusion.
Eliminates the need for manual service_database.c maintenance.

Usage:
    python3 yaml_to_c.py data/ libntripatlas/src/generated/

This replaces the architectural violation of hardcoded service_database.c
"""

import os
import sys
import yaml
import glob
from typing import List, Dict, Any
from datetime import datetime

def load_yaml_services(data_dir: str) -> List[Dict[str, Any]]:
    """Load all YAML service definitions from data directory tree."""
    services = []
    yaml_files = glob.glob(os.path.join(data_dir, "**/*.yaml"), recursive=True)

    # Skip schema.yaml
    yaml_files = [f for f in yaml_files if not f.endswith("schema.yaml")]

    for yaml_file in sorted(yaml_files):
        try:
            with open(yaml_file, 'r') as f:
                data = yaml.safe_load(f)
                if 'service' in data:
                    service = data['service']
                    service['_source_file'] = os.path.relpath(yaml_file, data_dir)
                    services.append(service)
                    print(f"Loaded: {service['id']} from {yaml_file}")
        except Exception as e:
            print(f"ERROR: Failed to load {yaml_file}: {e}")
            sys.exit(1)

    return services

def validate_service(service: Dict[str, Any]) -> bool:
    """Basic validation of service structure."""
    required_fields = ['id', 'provider', 'endpoints', 'coverage', 'authentication', 'quality']

    for field in required_fields:
        if field not in service:
            print(f"ERROR: Service {service.get('id', 'UNKNOWN')} missing required field: {field}")
            return False

    # Validate hostname length (ntrip_service_compact_t limit)
    for endpoint in service['endpoints']:
        if len(endpoint['hostname']) > 31:
            print(f"ERROR: Service {service['id']} hostname too long: {endpoint['hostname']}")
            return False

    # Validate bounding box
    bbox = service['coverage']['bounding_box']
    if bbox['lat_min'] >= bbox['lat_max']:
        print(f"ERROR: Service {service['id']} invalid latitude range")
        return False

    return True

def generate_coverage_bitmaps(services: List[Dict[str, Any]]) -> tuple[str, dict]:
    """Generate hierarchical coverage bitmaps using Brad Fitzpatrick approach."""

    c_code = []
    coverage_data = {}

    c_code.append("// Hierarchical Coverage Bitmaps (Brad Fitzpatrick approach)")
    c_code.append("// Services are assigned to coverage levels based on quality ratings")
    c_code.append("")

    has_hierarchical = False

    for service in services:
        service_id = service['id']
        coverage_levels = 0  # Bitmask of levels this service covers

        if 'hierarchical' in service['coverage']:
            hierarchical = service['coverage']['hierarchical']
            has_hierarchical = True

            # Build bitmask of coverage levels
            level_names = ['continental', 'regional', 'national', 'state', 'local']
            for level, name in enumerate(level_names):
                if name in hierarchical and hierarchical[name] >= 1:
                    coverage_levels |= (1 << level)  # Set bit for this level

            coverage_data[service_id] = {
                'levels': coverage_levels,
                'qualities': {name: hierarchical.get(name, 0) for name in level_names}
            }

            c_code.append(f"// {service_id}: Coverage levels 0b{coverage_levels:05b}")
            for level, name in enumerate(level_names):
                quality = hierarchical.get(name, 0)
                if quality > 0:
                    c_code.append(f"//   Level {level} ({name}): Quality {quality}")

        else:
            # Default: use bounding box to determine coverage levels
            coverage_levels = 0b11111  # All levels by default for backward compatibility
            coverage_data[service_id] = {
                'levels': coverage_levels,
                'qualities': {name: 3 for name in ['continental', 'regional', 'national', 'state', 'local']}
            }
            c_code.append(f"// {service_id}: Default coverage (all levels)")

        c_code.append("")

    if not has_hierarchical:
        c_code.append("// No hierarchical coverage data - using default assignments")
        c_code.append("")

    return "\n".join(c_code), coverage_data

def generate_service_array(services: List[Dict[str, Any]], coverage_data: dict, coverage_code: str) -> str:
    """Generate C array of ntrip_service_compact_t structures."""

    provider_map = {}
    provider_index = 0

    # Build provider index
    for service in services:
        provider = service['provider']
        if provider not in provider_map:
            provider_map[provider] = provider_index
            provider_index += 1

    c_code = []
    c_code.append("/**")
    c_code.append(" * GENERATED FILE - DO NOT EDIT")
    c_code.append(f" * Generated on: {datetime.now().isoformat()}")
    c_code.append(f" * Source: YAML service definitions")
    c_code.append(f" * Services: {len(services)}")
    c_code.append(" */")
    c_code.append("")
    c_code.append('#include "ntrip_atlas.h"')
    c_code.append('#include "ntrip_coverage_bitmaps.h"')
    c_code.append("")

    # Generate provider table
    c_code.append("// Provider name lookup table")
    c_code.append("static const char* provider_names[] = {")
    for provider, index in sorted(provider_map.items(), key=lambda x: x[1]):
        c_code.append(f'    "{provider}",  // Index {index}')
    c_code.append("};")
    c_code.append("")

    # Add hierarchical coverage code
    if coverage_code.strip():
        c_code.append(coverage_code)

    # Generate service array
    c_code.append("// Generated service database")
    c_code.append("static const ntrip_service_compact_t generated_services[] = {")

    for i, service in enumerate(services):
        endpoint = service['endpoints'][0]  # Use first endpoint
        bbox = service['coverage']['bounding_box']
        auth = service['authentication']
        quality = service['quality']

        # Map authentication method to flags
        flags = []
        if endpoint.get('ssl', False):
            flags.append("NTRIP_FLAG_SSL")
        if auth['method'] == 'basic':
            flags.append("NTRIP_FLAG_AUTH_BASIC")
        elif auth['method'] == 'digest':
            flags.append("NTRIP_FLAG_AUTH_DIGEST")
        if auth.get('registration_required', False):
            flags.append("NTRIP_FLAG_REQUIRES_REG")
        if not auth['required']:
            flags.append("NTRIP_FLAG_FREE_ACCESS")
        if service.get('country') == 'GLOBAL':
            flags.append("NTRIP_FLAG_GLOBAL_SERVICE")

        flag_value = " | ".join(flags) if flags else "0"

        # Map network type to integer
        network_type_map = {"government": 1, "commercial": 2, "community": 3}
        network_type = network_type_map[quality['network_type']]

        # Handle hierarchical coverage levels
        coverage_levels = coverage_data.get(service['id'], {}).get('levels', 0b11111)

        c_code.append(f"    // {service['id']} - {service['provider']}")
        c_code.append("    {")
        c_code.append(f'        .hostname = "{endpoint["hostname"]}",')
        c_code.append(f'        .port = {endpoint["port"]},')
        c_code.append(f'        .flags = {flag_value},')
        c_code.append(f'        .lat_min_deg100 = {int(bbox["lat_min"] * 100)},')
        c_code.append(f'        .lat_max_deg100 = {int(bbox["lat_max"] * 100)},')
        c_code.append(f'        .lon_min_deg100 = {int(bbox["lon_min"] * 100)},')
        c_code.append(f'        .lon_max_deg100 = {int(bbox["lon_max"] * 100)},')
        c_code.append(f'        .coverage_levels = 0b{coverage_levels:05b},  // {coverage_levels}')
        c_code.append(f'        .reserved = 0,')
        c_code.append(f'        .provider_index = {provider_map[service["provider"]]},')
        c_code.append(f'        .network_type = {network_type},')
        c_code.append(f'        .quality_rating = {quality["reliability_rating"]}')
        c_code.append("    }" + ("," if i < len(services) - 1 else ""))

    c_code.append("};")
    c_code.append("")

    # Generate accessor functions
    c_code.append(f"#define GENERATED_SERVICE_COUNT {len(services)}")
    c_code.append("")
    c_code.append("const ntrip_service_compact_t* get_generated_services(size_t* count) {")
    c_code.append("    *count = GENERATED_SERVICE_COUNT;")
    c_code.append("    return generated_services;")
    c_code.append("}")
    c_code.append("")
    c_code.append("const char* get_provider_name(uint8_t provider_index) {")
    c_code.append(f"    if (provider_index >= {len(provider_map)}) return \"Unknown\";")
    c_code.append("    return provider_names[provider_index];")
    c_code.append("}")

    return "\n".join(c_code)

def generate_header(services: List[Dict[str, Any]]) -> str:
    """Generate header file for generated services."""

    h_code = []
    h_code.append("/**")
    h_code.append(" * GENERATED FILE - DO NOT EDIT")
    h_code.append(f" * Generated on: {datetime.now().isoformat()}")
    h_code.append(f" * Source: YAML service definitions")
    h_code.append(f" * Services: {len(services)}")
    h_code.append(" */")
    h_code.append("")
    h_code.append("#ifndef NTRIP_GENERATED_SERVICES_H")
    h_code.append("#define NTRIP_GENERATED_SERVICES_H")
    h_code.append("")
    h_code.append('#include "ntrip_atlas.h"')
    h_code.append("")
    h_code.append("/**")
    h_code.append(" * Get generated service database")
    h_code.append(" * @param count Output parameter for service count")
    h_code.append(" * @return Pointer to service array")
    h_code.append(" */")
    h_code.append("const ntrip_service_compact_t* get_generated_services(size_t* count);")
    h_code.append("")
    h_code.append("/**")
    h_code.append(" * Get provider name by index")
    h_code.append(" * @param provider_index Provider index from service")
    h_code.append(" * @return Provider name string")
    h_code.append(" */")
    h_code.append("const char* get_provider_name(uint8_t provider_index);")
    h_code.append("")
    h_code.append("#endif // NTRIP_GENERATED_SERVICES_H")

    return "\n".join(h_code)

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 yaml_to_c.py <data_dir> <output_dir>")
        sys.exit(1)

    data_dir = sys.argv[1]
    output_dir = sys.argv[2]

    if not os.path.exists(data_dir):
        print(f"ERROR: Data directory not found: {data_dir}")
        sys.exit(1)

    # Create output directory
    os.makedirs(output_dir, exist_ok=True)

    # Load and validate services
    services = load_yaml_services(data_dir)

    if not services:
        print("ERROR: No services found in data directory")
        sys.exit(1)

    print(f"Loaded {len(services)} services")

    # Validate all services
    all_valid = True
    for service in services:
        if not validate_service(service):
            all_valid = False

    if not all_valid:
        print("ERROR: Service validation failed")
        sys.exit(1)

    # Generate hierarchical coverage bitmaps
    coverage_code, coverage_data = generate_coverage_bitmaps(services)

    # Generate C code
    c_code = generate_service_array(services, coverage_data, coverage_code)
    h_code = generate_header(services)

    # Write output files
    c_file = os.path.join(output_dir, "ntrip_generated_services.c")
    h_file = os.path.join(output_dir, "ntrip_generated_services.h")

    with open(c_file, 'w') as f:
        f.write(c_code)

    with open(h_file, 'w') as f:
        f.write(h_code)

    print(f"Generated: {c_file}")
    print(f"Generated: {h_file}")
    print(f"SUCCESS: Generated code for {len(services)} services")

if __name__ == "__main__":
    main()