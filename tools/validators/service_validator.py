#!/usr/bin/env python3
"""
NTRIP Atlas Service Validator

Validates NTRIP service YAML files for correctness, completeness,
and adherence to the schema specification.
"""

import yaml
import sys
import os
import re
import urllib.parse
import subprocess
import socket
from datetime import datetime
from typing import Dict, List, Any, Optional

class ServiceValidator:
    """Validates NTRIP service YAML files"""

    REQUIRED_SCHEMA_VERSION = "1.0.0"
    # ISO 3166-1 alpha-2 and alpha-3 country codes (common subset) + regional codes
    VALID_COUNTRIES = {
        # Regional groupings
        "GLOBAL", "EMEA", "APAC", "AMER",
        # ISO 3166-1 alpha-2 codes (common)
        "AR", "AU", "AT", "BE", "BR", "CA", "CL", "CN", "CO", "CZ", "DK", "EG",
        "FI", "FR", "DE", "GR", "HK", "IN", "ID", "IL", "IT", "JP", "MY", "MX",
        "NL", "NZ", "NO", "PE", "PH", "PL", "PT", "RO", "RU", "SG", "ZA", "KR",
        "ES", "SE", "CH", "TW", "TH", "TR", "UA", "GB", "US", "VN",
        # ISO 3166-1 alpha-3 codes
        "AFG", "AGO", "ALB", "AND", "ARE", "ARG", "ARM", "AUS", "AUT", "AZE",
        "BEL", "BGD", "BGR", "BHR", "BIH", "BLR", "BOL", "BRA", "BTN", "BWA",
        "CAN", "CHE", "CHL", "CHN", "COL", "CRI", "CUB", "CYP", "CZE",
        "DEU", "DNK", "DOM", "DZA", "ECU", "EGY", "ESP", "EST", "ETH",
        "FIN", "FRA", "GAB", "GBR", "GEO", "GHA", "GRC", "GTM",
        "HKG", "HND", "HRV", "HUN",
        "IDN", "IND", "IRL", "IRN", "IRQ", "ISL", "ISR", "ITA",
        "JPN", "JOR", "KAZ", "KEN", "KGZ", "KHM", "KOR", "KWT",
        "LBN", "LBY", "LKA", "LTU", "LUX", "LVA",
        "MAR", "MDA", "MEX", "MKD", "MLI", "MMR", "MNG", "MOZ",
        "NGA", "NIC", "NLD", "NOR", "NPL", "NZL",
        "OMN", "PAK", "PAN", "PER", "PHL", "POL", "PRT", "PRY",
        "QAT", "ROU", "RUS", "RWA",
        "SAU", "SDN", "SEN", "SGP", "SRB", "SVK", "SVN", "SWE",
        "THA", "TJK", "TKM", "TUN", "TUR", "TWN",
        "UGA", "UKR", "URY", "USA", "UZB",
        "VEN", "VNM", "YEM", "ZAF", "ZMB", "ZWE"
    }
    VALID_PROTOCOLS = {"http", "https"}
    VALID_AUTH_METHODS = {"none", "basic", "digest"}
    VALID_ORG_TYPES = {"government", "commercial", "community", "research", "academic"}

    def __init__(self):
        self.errors = []
        self.warnings = []

    def validate_file(self, filepath: str, test_connectivity: bool = False) -> bool:
        """Validate a single YAML service file"""
        try:
            # Skip special metadata files that don't contain service definitions
            filename = os.path.basename(filepath)
            if filename.startswith('excluded_') or filename.endswith('_notes.yaml') or filename == 'schema.yaml':
                print(f"Skipping metadata file {filepath}...")
                return True

            with open(filepath, 'r', encoding='utf-8') as f:
                data = yaml.safe_load(f)

            self.errors = []
            self.warnings = []

            print(f"Validating {filepath}...")

            # Schema validation
            self._validate_schema(data)

            # Service validation
            if 'service' in data:
                self._validate_service(data['service'])

            # Examples validation (if present)
            if 'example_mountpoints' in data:
                self._validate_examples(data['example_mountpoints'])

            # Connectivity testing (if requested)
            if test_connectivity and 'service' in data:
                self._test_connectivity(data['service'])

            # Print results
            if self.errors:
                print(f"❌ ERRORS ({len(self.errors)}):")
                for error in self.errors:
                    print(f"  - {error}")

            if self.warnings:
                print(f"⚠️  WARNINGS ({len(self.warnings)}):")
                for warning in self.warnings:
                    print(f"  - {warning}")

            if not self.errors and not self.warnings:
                print("✅ Validation passed")
            elif not self.errors:
                print("✅ Validation passed with warnings")

            return len(self.errors) == 0

        except yaml.YAMLError as e:
            print(f"❌ YAML parsing error: {e}")
            return False
        except FileNotFoundError:
            print(f"❌ File not found: {filepath}")
            return False

    def _validate_schema(self, data: Dict[str, Any]) -> None:
        """Validate schema version and top-level structure"""
        # Required top-level fields
        required_fields = ['service']
        for field in required_fields:
            if field not in data:
                self.errors.append(f"Missing required field: {field}")

    def _validate_service(self, service: Dict[str, Any]) -> None:
        """Validate service configuration"""
        # Required service fields
        required_fields = ['id', 'country', 'provider', 'endpoints', 'coverage', 'authentication', 'quality']
        for field in required_fields:
            if field not in service:
                self.errors.append(f"Missing required service field: {field}")

        # Service ID validation
        if 'id' in service:
            service_id = service['id']
            if not re.match(r'^[a-z0-9_]+$', service_id):
                self.errors.append(f"Invalid service ID format: {service_id} (must be lowercase, numbers, underscores only)")

        # Country code validation
        if 'country' in service:
            if service['country'] not in self.VALID_COUNTRIES:
                self.warnings.append(f"Unknown country code: {service['country']} (consider adding to validator)")

        # Endpoints validation
        if 'endpoints' in service:
            self._validate_endpoints(service['endpoints'])

        # Coverage validation
        if 'coverage' in service:
            self._validate_coverage(service['coverage'])

        # Authentication validation
        if 'authentication' in service:
            self._validate_authentication(service['authentication'])

        # Quality validation
        if 'quality' in service:
            self._validate_quality(service['quality'])

    def _validate_endpoints(self, endpoints: List[Dict[str, Any]]) -> None:
        """Validate service endpoints"""
        if not isinstance(endpoints, list) or len(endpoints) == 0:
            self.errors.append("Endpoints must be a non-empty list")
            return

        for i, endpoint in enumerate(endpoints):
            prefix = f"endpoint[{i}]"

            # Required endpoint fields
            required_fields = ['hostname', 'port']
            for field in required_fields:
                if field not in endpoint:
                    self.errors.append(f"{prefix}: Missing required field: {field}")

            # SSL field should be present (defaults to false if missing)
            if 'ssl' not in endpoint:
                self.warnings.append(f"{prefix}: Missing 'ssl' field (defaults to false)")

            # SSL validation
            if 'ssl' in endpoint:
                ssl = endpoint['ssl']
                if not isinstance(ssl, bool):
                    self.errors.append(f"{prefix}: SSL must be boolean, got: {type(ssl).__name__}")

            # Hostname validation
            if 'hostname' in endpoint:
                hostname = endpoint['hostname']
                if not re.match(r'^[a-zA-Z0-9.-]+$', hostname):
                    self.errors.append(f"{prefix}: Invalid hostname format: {hostname}")

                # Check hostname length limit (31 chars for compact storage)
                if len(hostname) > 31:
                    self.errors.append(f"{prefix}: Hostname too long (max 31 chars): {hostname} ({len(hostname)} chars)")

            # Port validation
            if 'port' in endpoint:
                port = endpoint['port']
                if not isinstance(port, int) or port < 1 or port > 65535:
                    self.errors.append(f"{prefix}: Invalid port: {port}")

    def _validate_coverage(self, coverage: Dict[str, Any]) -> None:
        """Validate geographic coverage"""
        if 'bounding_box' in coverage:
            bbox = coverage['bounding_box']

            required_bbox_fields = ['lat_min', 'lat_max', 'lon_min', 'lon_max']
            for field in required_bbox_fields:
                if field not in bbox:
                    self.errors.append(f"Coverage bounding_box missing: {field}")

            # Coordinate range validation
            if all(field in bbox for field in required_bbox_fields):
                lat_min, lat_max = bbox['lat_min'], bbox['lat_max']
                lon_min, lon_max = bbox['lon_min'], bbox['lon_max']

                if not (-90 <= lat_min <= 90 and -90 <= lat_max <= 90):
                    self.errors.append(f"Invalid latitude range: {lat_min} to {lat_max}")

                if not (-180 <= lon_min <= 180 and -180 <= lon_max <= 180):
                    self.errors.append(f"Invalid longitude range: {lon_min} to {lon_max}")

                if lat_min >= lat_max:
                    self.errors.append(f"lat_min ({lat_min}) must be less than lat_max ({lat_max})")

                if lon_min >= lon_max and not (lon_min > 150 and lon_max < -150):  # Allow dateline crossing
                    self.warnings.append(f"lon_min ({lon_min}) >= lon_max ({lon_max}) - check for dateline crossing")

    def _validate_authentication(self, auth: Dict[str, Any]) -> None:
        """Validate authentication configuration"""
        # Required auth fields
        required_fields = ['required', 'method']
        for field in required_fields:
            if field not in auth:
                self.errors.append(f"Authentication missing field: {field}")

        # Method validation
        if 'method' in auth:
            method = auth['method']
            if method not in self.VALID_AUTH_METHODS:
                self.errors.append(f"Invalid authentication method: {method}")

        # Registration validation
        if 'registration_required' in auth and auth['registration_required']:
            if 'registration_url' not in auth or not auth['registration_url']:
                self.warnings.append("registration_url recommended when registration_required=true")

        # URL validation
        for url_field in ['registration_url', 'terms_url']:
            if url_field in auth:
                url = auth[url_field]
                # Skip empty URLs
                if not url:
                    continue
                try:
                    parsed = urllib.parse.urlparse(url)
                    # Accept http, https, and mailto URLs
                    if parsed.scheme not in ['http', 'https', 'mailto']:
                        self.errors.append(f"Invalid {url_field} scheme: {url} (expected http, https, or mailto)")
                    elif parsed.scheme in ['http', 'https'] and not parsed.netloc:
                        self.errors.append(f"Invalid {url_field}: {url} (missing hostname)")
                except Exception:
                    self.errors.append(f"Invalid {url_field}: {url}")

    def _validate_quality(self, quality: Dict[str, Any]) -> None:
        """Validate quality ratings"""
        # Required quality fields
        required_fields = ['reliability_rating', 'accuracy_rating', 'network_type']
        for field in required_fields:
            if field not in quality:
                self.errors.append(f"Quality missing required field: {field}")

        # Rating validation
        rating_fields = ['reliability_rating', 'accuracy_rating']
        for field in rating_fields:
            if field in quality:
                rating = quality[field]
                if not isinstance(rating, int) or rating < 1 or rating > 5:
                    self.errors.append(f"Quality {field} must be integer 1-5, got: {rating}")

        # Network type validation
        if 'network_type' in quality:
            network_type = quality['network_type']
            valid_types = ['government', 'commercial', 'community']
            if network_type not in valid_types:
                self.errors.append(f"Invalid network_type: {network_type} (must be one of {valid_types})")

    def _validate_examples(self, examples: List[Dict[str, Any]]) -> None:
        """Validate example mountpoints"""
        for i, example in enumerate(examples):
            prefix = f"example[{i}]"

            if 'coordinates' in example:
                coords = example['coordinates']
                if not isinstance(coords, list) or len(coords) != 2:
                    self.errors.append(f"{prefix}: coordinates must be [lat, lon] array")
                else:
                    lat, lon = coords
                    if not (-90 <= lat <= 90):
                        self.errors.append(f"{prefix}: invalid latitude: {lat}")
                    if not (-180 <= lon <= 180):
                        self.errors.append(f"{prefix}: invalid longitude: {lon}")


    def _test_connectivity(self, service: Dict[str, Any]) -> None:
        """Test NTRIP service connectivity"""
        print("🔗 Testing connectivity...")

        endpoints = service.get('endpoints', [])
        if not endpoints:
            self.warnings.append("No endpoints to test")
            return

        for i, endpoint in enumerate(endpoints):
            hostname = endpoint.get('hostname')
            port = endpoint.get('port', 2101)
            ssl = endpoint.get('ssl', False)

            if not hostname:
                self.warnings.append(f"Endpoint {i}: Missing hostname")
                continue

            print(f"  Testing {hostname}:{port} {'(SSL)' if ssl else ''}...")

            # Test basic TCP connectivity
            if self._test_port_connectivity(hostname, port):
                print(f"    ✅ Port {port} is reachable")

                # Test NTRIP sourcetable request
                if self._test_ntrip_sourcetable(hostname, port, ssl):
                    print(f"    ✅ NTRIP service responds")
                else:
                    self.warnings.append(f"NTRIP service at {hostname}:{port} did not respond correctly")
            else:
                self.warnings.append(f"Cannot connect to {hostname}:{port}")

    def _test_port_connectivity(self, hostname: str, port: int, timeout: int = 5) -> bool:
        """Test basic TCP connectivity to a port"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(timeout)
            result = sock.connect_ex((hostname, port))
            sock.close()
            return result == 0
        except Exception:
            return False

    def _test_ntrip_sourcetable(self, hostname: str, port: int, ssl: bool = False, timeout: int = 10) -> bool:
        """Test NTRIP sourcetable request using curl"""
        try:
            protocol = "https" if ssl else "http"
            url = f"{protocol}://{hostname}:{port}"

            # Use curl to request sourcetable (basic NTRIP request)
            cmd = [
                'curl', '-s', '--connect-timeout', str(timeout),
                '--max-time', str(timeout), '-I', url
            ]

            # On macOS, don't use timeout command
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 5)

            # Check if we got any response (even if it requires auth)
            if result.returncode == 0 or "401" in result.stdout or "WWW-Authenticate" in result.stdout:
                return True

            # Try with basic NTRIP GET request for sourcetable
            cmd = [
                'curl', '-s', '--connect-timeout', str(timeout),
                '--max-time', str(timeout), '-H', 'User-Agent: NTRIP client',
                url
            ]

            result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 5)

            # Success if we get any response, including auth requests
            return result.returncode == 0 or "SOURCETABLE" in result.stdout or "401" in result.stderr

        except subprocess.TimeoutExpired:
            return False
        except Exception:
            return False


def main():
    """Command-line interface"""
    if len(sys.argv) < 2:
        print("Usage: python3 service_validator.py <yaml_file> [yaml_file2 ...]")
        print("   or: python3 service_validator.py --directory <data_directory>")
        print("   or: python3 service_validator.py --directory <data_directory> --test-connectivity")
        sys.exit(1)

    validator = ServiceValidator()
    all_passed = True

    # Parse arguments
    test_connectivity = "--test-connectivity" in sys.argv
    args = [arg for arg in sys.argv[1:] if arg != "--test-connectivity"]

    if len(args) > 0 and args[0] == "--directory":
        if len(args) < 2:
            print("Error: --directory requires a directory path")
            sys.exit(1)

        data_dir = args[1]
        if not os.path.isdir(data_dir):
            print(f"Error: {data_dir} is not a directory")
            sys.exit(1)

        # Find all .yaml files recursively
        yaml_files = []
        for root, dirs, files in os.walk(data_dir):
            for file in files:
                if file.endswith('.yaml'):
                    yaml_files.append(os.path.join(root, file))

        if not yaml_files:
            print(f"No .yaml files found in {data_dir}")
            sys.exit(1)

        print(f"Found {len(yaml_files)} YAML files to validate\n")

        for yaml_file in sorted(yaml_files):
            if not validator.validate_file(yaml_file, test_connectivity):
                all_passed = False
            print()  # Blank line between files
    else:
        # Validate individual files
        for yaml_file in args:
            if not validator.validate_file(yaml_file, test_connectivity):
                all_passed = False
            print()

    if all_passed:
        print("🎉 All validations passed!")
        sys.exit(0)
    else:
        print("💥 Some validations failed!")
        sys.exit(1)


if __name__ == "__main__":
    main()