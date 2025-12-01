#!/usr/bin/env python3
"""
Schema Converter for NTRIP Atlas
Converts data-repo YAML format to our unified format with hierarchical coverage.
"""

import os
import sys
import yaml
import glob
from typing import Dict, Any

def convert_endpoints(old_endpoints: list) -> list:
    """Convert detailed endpoint format to simplified format."""
    new_endpoints = []
    for ep in old_endpoints:
        new_ep = {
            'hostname': ep.get('hostname', ''),
            'port': ep.get('port', 2101),
            'ssl': ep.get('ssl', False)
        }
        new_endpoints.append(new_ep)
    return new_endpoints

def convert_authentication(old_auth: Dict[str, Any]) -> Dict[str, Any]:
    """Convert authentication format."""
    return {
        'method': old_auth.get('method', 'none'),
        'required': old_auth.get('required', False),
        'registration_required': old_auth.get('registration_required', False),
        'registration_url': old_auth.get('registration_url', '')
    }

def guess_hierarchical_coverage(service: Dict[str, Any]) -> Dict[str, int]:
    """Assign reasonable hierarchical coverage based on organization type and scope."""
    org_type = service.get('organization_type', 'community')
    country = service.get('country', 'UNKNOWN')

    if org_type == 'government':
        if country in ['USA', 'GLOBAL']:
            # US state networks or global government
            return {
                'regional': 3,
                'national': 4,
                'state': 5,
                'local': 4
            }
        else:
            # Other government networks
            return {
                'regional': 4,
                'national': 5,
                'state': 4,
                'local': 3
            }
    elif org_type == 'commercial':
        return {
            'continental': 5,
            'regional': 5,
            'national': 4,
            'state': 4,
            'local': 3
        }
    else:  # community
        return {
            'continental': 4,
            'regional': 4,
            'national': 3,
            'state': 3,
            'local': 2
        }

def convert_service(old_service: Dict[str, Any]) -> Dict[str, Any]:
    """Convert a single service from old format to new format."""
    service = old_service['service']

    # Map organization_type to network_type
    org_type_map = {
        'government': 'government',
        'commercial': 'commercial',
        'community': 'community'
    }

    # Build new service structure
    new_service = {
        'id': service['id'],
        'provider': service.get('provider', service.get('name', '')),
        'country': service.get('country', 'UNKNOWN'),
        'endpoints': convert_endpoints(service.get('endpoints', [])),
        'coverage': {
            'bounding_box': service.get('coverage', {}).get('bounding_box', {}),
            'hierarchical': guess_hierarchical_coverage(service)
        },
        'authentication': convert_authentication(service.get('authentication', {})),
        'quality': {
            'reliability_rating': 4,  # Default reasonable rating
            'accuracy_rating': 4,     # Default reasonable rating
            'network_type': org_type_map.get(service.get('organization_type', 'community'), 'community')
        },
        'metadata': {
            'description': service.get('coverage', {}).get('description', f"{service.get('name', 'NTRIP Service')}"),
            'stations': 50,  # Default estimate
            'coverage_area': service.get('coverage', {}).get('description', 'Regional coverage'),
            'last_verified': '2024-12-01'
        }
    }

    return {'service': new_service}

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 convert_schema.py <input_data_repo_dir> <output_dir>")
        sys.exit(1)

    input_dir = sys.argv[1]
    output_dir = sys.argv[2]

    # Create output directory structure
    os.makedirs(output_dir, exist_ok=True)
    for subdir in ['global', 'americas', 'emea', 'apac', 'africa']:
        os.makedirs(os.path.join(output_dir, subdir), exist_ok=True)

    # Find all YAML files
    yaml_files = glob.glob(os.path.join(input_dir, "**/*.yaml"), recursive=True)
    yaml_files = [f for f in yaml_files if not f.endswith("schema.yaml")]

    converted_count = 0

    for yaml_file in sorted(yaml_files):
        try:
            with open(yaml_file, 'r') as f:
                old_data = yaml.safe_load(f)

            if 'service' not in old_data:
                print(f"Skipping {yaml_file}: No service section")
                continue

            # Convert to new format
            new_data = convert_service(old_data)

            # Determine output path
            rel_path = os.path.relpath(yaml_file, input_dir)
            output_path = os.path.join(output_dir, rel_path)

            # Write converted file
            os.makedirs(os.path.dirname(output_path), exist_ok=True)
            with open(output_path, 'w') as f:
                yaml.dump(new_data, f, default_flow_style=False, sort_keys=False)

            print(f"Converted: {rel_path}")
            converted_count += 1

        except Exception as e:
            print(f"ERROR converting {yaml_file}: {e}")

    print(f"\nConverted {converted_count} services to unified format")
    print(f"Output directory: {output_dir}")

if __name__ == "__main__":
    main()