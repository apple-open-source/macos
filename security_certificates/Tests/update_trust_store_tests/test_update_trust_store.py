#!/usr/bin/env python3

"""
Test suite for update_trust_store.py

This script tests various scenarios and edge cases for the trust store update functionality.
"""

import os
import sys
import json
import tempfile
import shutil
import subprocess
import unittest
from pathlib import Path

# Add the update_scipts to sys.path to import the scripts we're testing
update_scripts_dir = str(Path(__file__).parent.parent.parent) + "/update_scripts"
sys.path.insert(0, update_scripts_dir)

# Import OID validation functions if available
try:
    from oid_validation import validate_update_json_oids, validate_policy_constraints, validate_ev_oids
    OID_VALIDATION_AVAILABLE = True
except ImportError:
    OID_VALIDATION_AVAILABLE = False
    print("Warning: OID validation functions not available - skipping OID tests")

try:
    from validate_update_json import readJson
    JSON_READER_AVAILABLE = True
except ImportError:
    JSON_READER_AVAILABLE = False
    print("Warning: validate_update_json module not available - using fallback JSON reader")

class TrustStoreUpdateTests(unittest.TestCase):
    """Test cases for update_trust_store.py functionality"""
    
    def setUp(self):
        """Set up test environment before each test"""
        # Create temporary directory for test files
        self.test_dir = tempfile.mkdtemp(prefix='trust_store_test_')
        self.srcroot = self.test_dir
        self.script_path = update_scripts_dir + '/update_trust_store.py'

        # Create required directory structure
        self.create_test_directory_structure()
        
        # Create test configuration files
        self.create_test_config_files()
        
    def tearDown(self):
        """Clean up test environment after each test"""
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)
    
    def create_test_directory_structure(self):
        """Create the directory structure expected by update_trust_store.py"""
        dirs_to_create = [
            'certificates/custom',
            'certificates/platform', 
            'certificates/roots',
            'certificates/test-roots',
            'certificates/test-platform',
            'certificates/removed',
            'certificates/removed/intermediates',
            'config',
            'update_scripts'
        ]
        
        for dir_path in dirs_to_create:
            os.makedirs(os.path.join(self.srcroot, dir_path), exist_ok=True)
    
    def create_test_config_files(self):
        """Create test configuration files"""
        # Create constraints.json
        constraints = {
            "system": {},
            "custom": {},
            "platform": {},
            "test-system": {},
            "test-platform": {}
        }
        with open(os.path.join(self.srcroot, 'certificates/constraints.json'), 'w') as f:
            json.dump(constraints, f, indent=4)
        
        # Create EVRoots.json
        ev_roots = {
            "EV_config": {},
            "fingerprint_map": {}
        }
        with open(os.path.join(self.srcroot, 'certificates/EVRoots.json'), 'w') as f:
            json.dump(ev_roots, f, indent=4)
        
        # Create hash_to_human_name.json
        human_map = {}
        with open(os.path.join(self.srcroot, 'certificates/hash_to_human_name.json'), 'w') as f:
            json.dump(human_map, f, indent=4)
        
        # Create AssetVersion.plist
        import plistlib
        asset_version = {
            "VersionNumber": 2024082500,
            "PKITrustStoreAssetsVersion": "1.0.0",
            "MobileAssetContentVersion": 123
        }
        with open(os.path.join(self.srcroot, 'config/AssetVersion.plist'), 'wb') as f:
            plistlib.dump(asset_version, f)
        
        # Create Info-Asset.plist
        info_asset = {
            "CFBundleShortVersionString": "1.0.0",
            "CFBundleVersion": "1.0.0",
            "MobileAssetProperties": {
                "AssetVersion": "1.0.0",
                "ContentVersion": 2024082500
            }
        }
        with open(os.path.join(self.srcroot, 'config/Info-Asset.plist'), 'wb') as f:
            plistlib.dump(info_asset, f)
        
        # Create security_certificates.xcconfig
        xcconfig_content = """// Configuration for security certificates
TRUST_STORE_VERSION = 2024082500
OTHER_SETTING = value
"""
        with open(os.path.join(self.srcroot, 'config/security_certificates.xcconfig'), 'w') as f:
            f.write(xcconfig_content)
        
        # Create schema file
        schema_content = """{
    "id": "com.apple.trust_store_updates_schema_v2",
    "$schema": "http://json-schema.org/draft-07/schema",
    "type": "array",
    "items": {
        "type": "object",
        "properties": {
            "change_type": {"type": "string", "enum": ["Removal", "Addition", "Modification"]},
            "change_reason": {"type": "string"},
            "certificate_details": {
                "type": "object",
                "properties": {
                    "sha256_fingerprint": {"type": "string"},
                    "spki": {"type": "string"},
                    "common_name": {"type": "string"},
                    "pem": {"type": "string"}
                },
                "required": ["sha256_fingerprint", "spki", "pem"]
            },
            "anchor_type": {"type": "string", "enum": ["System", "Custom", "Platform", "Test-System", "Test-Platform"]},
            "policy_constraints": {"type": "array"},
            "ev_tls_oids": {"type": "array"}
        },
        "required": ["change_type", "change_reason", "certificate_details", "anchor_type"]
    }
}"""
        with open(os.path.join(self.srcroot, 'update_scripts/trust_store_updates_schema_v2.json'), 'w') as f:
            f.write(schema_content)
    
    def run_update_script(self, json_file, additional_args=None):
        """Run the update_trust_store.py script with given parameters"""
        if additional_args is None:
            additional_args = []
        
        cmd = [
            sys.executable,
            str(self.script_path),
            '--update_json', json_file,
            '--srcroot', self.srcroot
        ] + additional_args
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            return result
        except subprocess.TimeoutExpired:
            self.fail(f"Script timed out: {' '.join(cmd)}")
        except Exception as e:
            self.fail(f"Failed to run script: {e}")
    
    def test_dry_run_addition(self):
        """Test dry run with addition operation"""
        test_file = Path(__file__).parent / 'test_data_addition.json'
        result = self.run_update_script(str(test_file), ['--dry_run'])
        
        self.assertEqual(result.returncode, 0, f"Script failed: {result.stderr}")
        self.assertIn("[DRY RUN]", result.stdout)
        self.assertIn("Adding as Custom anchor", result.stdout)
        self.assertIn("Adding constraints", result.stdout)
    
    def test_dry_run_removal(self):
        """Test dry run with removal operation"""
        test_file = Path(__file__).parent / 'test_data_removal.json'
        result = self.run_update_script(str(test_file), ['--dry_run'])
        
        self.assertEqual(result.returncode, 0, f"Script failed: {result.stderr}")
        self.assertIn("[DRY RUN]", result.stdout)
    
    def test_dry_run_modification(self):
        """Test dry run with modification operation"""
        test_file = Path(__file__).parent / 'test_data_modification.json'
        result = self.run_update_script(str(test_file), ['--dry_run'])
        
        self.assertEqual(result.returncode, 0, f"Script failed: {result.stderr}")
        self.assertIn("[DRY RUN]", result.stdout)
        self.assertIn("Replacing EV OIDs", result.stdout)
    
    def test_dry_run_mixed_operations(self):
        """Test dry run with mixed operations"""
        test_file = Path(__file__).parent / 'test_data_mixed.json'
        result = self.run_update_script(str(test_file), ['--dry_run'])
        
        self.assertEqual(result.returncode, 0, f"Script failed: {result.stderr}")
        self.assertIn("[DRY RUN]", result.stdout)
        self.assertIn("Version files would be updated automatically", result.stdout)
    
    def test_version_override(self):
        """Test asset version override functionality"""
        test_file = Path(__file__).parent / 'test_data_addition.json'
        result = self.run_update_script(str(test_file), ['--dry_run', '--asset_version', '2.5.0'])
        
        self.assertEqual(result.returncode, 0, f"Script failed: {result.stderr}")
        self.assertIn("1.0.0 -> 2.5.0", result.stdout)
    
    def test_no_version_update(self):
        """Test skipping version updates"""
        test_file = Path(__file__).parent / 'test_data_addition.json'
        result = self.run_update_script(str(test_file), ['--dry_run', '--no_version_update'])
        
        self.assertEqual(result.returncode, 0, f"Script failed: {result.stderr}")
        self.assertIn("Would skip version updates (--no_version_update specified)", result.stdout)
    
    def test_actual_addition(self):
        """Test actual addition operation (not dry run)"""
        test_file = Path(__file__).parent / 'test_data_addition.json'
        result = self.run_update_script(str(test_file))
        
        self.assertEqual(result.returncode, 0, f"Script failed: {result.stderr}")
        self.assertNotIn("[DRY RUN]", result.stdout)
        self.assertIn("Adding as Custom anchor", result.stdout)
        
        # Verify certificate was written
        expected_cert_path = os.path.join(
            self.srcroot, 
            'certificates/custom',
            'D947432ABDE7B7FA90FC2E6B59101B1280E0E1C7E4E40FA3C6887FFF57A7F4CF.cer'
        )
        self.assertTrue(os.path.exists(expected_cert_path), "Certificate file was not created")
        
        # Verify constraints were updated
        with open(os.path.join(self.srcroot, 'certificates/constraints.json'), 'r') as f:
            constraints = json.load(f)
            fingerprint = "D947432ABDE7B7FA90FC2E6B59101B1280E0E1C7E4E40FA3C6887FFF57A7F4CF"
            self.assertIn(fingerprint, constraints['custom'])
            self.assertEqual(constraints['custom'][fingerprint], ["1.2.840.113635.100.1.123"])
    
    def test_actual_modification_with_ev(self):
        """Test actual modification operation with EV OIDs"""
        test_file = Path(__file__).parent / 'test_data_modification.json'
        result = self.run_update_script(str(test_file))
        
        self.assertEqual(result.returncode, 0, f"Script failed: {result.stderr}")
        self.assertIn("Replacing EV OIDs", result.stdout)

        # Verify EV roots were updated
        with open(os.path.join(self.srcroot, 'certificates/EVRoots.json'), 'r') as f:
            ev_roots = json.load(f)
            fingerprint = "349DFA4058C5E263123B398AE795573C4E1313C83FE68F93556CD5E8031B3C7D"
            self.assertIn(fingerprint, ev_roots['EV_config'])
            self.assertEqual(ev_roots['EV_config'][fingerprint], ["2.23.140.1.1"])
    
    def test_version_file_updates(self):
        """Test that version files are properly updated"""
        test_file = Path(__file__).parent / 'test_data_addition.json'
        result = self.run_update_script(str(test_file))
        
        self.assertEqual(result.returncode, 0, f"Script failed: {result.stderr}")
        
        # Check AssetVersion.plist was updated
        import plistlib
        with open(os.path.join(self.srcroot, 'config/AssetVersion.plist'), 'rb') as f:
            asset_version = plistlib.load(f)
            self.assertGreater(asset_version['VersionNumber'], 2024082500)
            self.assertEqual(asset_version['PKITrustStoreAssetsVersion'], '1.0.1')
        
        # Check Info-Asset.plist was updated
        with open(os.path.join(self.srcroot, 'config/Info-Asset.plist'), 'rb') as f:
            info_asset = plistlib.load(f)
            self.assertEqual(info_asset['CFBundleShortVersionString'], '1.0.1')
            self.assertEqual(info_asset['CFBundleVersion'], '1.0.1')
            self.assertEqual(info_asset['MobileAssetProperties']['AssetVersion'], '1.0.1.0.0,0')

        # Check xcconfig was updated
        with open(os.path.join(self.srcroot, 'config/security_certificates.xcconfig'), 'r') as f:
            content = f.read()
            self.assertRegex(content, r'TRUST_STORE_VERSION = \d{10}')
    
    def test_empty_update_json(self):
        """Test with empty update JSON"""
        test_file = Path(__file__).parent / 'test_data_bad_cert.json'
        result = self.run_update_script(str(test_file), ['--dry_run'])
        
        self.assertNotEqual(result.returncode, 0)

    def test_invalid_json_file(self):
        """Test with non-existent JSON file"""
        result = self.run_update_script('/nonexistent/file.json', ['--dry_run'])
        self.assertNotEqual(result.returncode, 0)
    
    def test_missing_srcroot(self):
        """Test with invalid srcroot path"""
        test_file = Path(__file__).parent / 'test_data_addition.json'
        cmd = [
            sys.executable,
            str(self.script_path),
            '--update_json', str(test_file),
            '--srcroot', '/nonexistent/path',
            '--dry_run'
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)


class CLITests(unittest.TestCase):
    """Test command-line interface and argument parsing"""
    
    def setUp(self):
        self.script_path = update_scripts_dir + '/update_trust_store.py'

    def test_help_output(self):
        """Test that help output is generated"""
        cmd = [sys.executable, str(self.script_path), '--help']
        result = subprocess.run(cmd, capture_output=True, text=True)
        print(result)

        self.assertEqual(result.returncode, 0)
        self.assertIn('--update_json', result.stdout)
        self.assertIn('--srcroot', result.stdout)
        self.assertIn('--dry_run', result.stdout)
        self.assertIn('--asset_version', result.stdout)
        self.assertIn('--no_version_update', result.stdout)
    
    def test_missing_required_args(self):
        """Test that missing required arguments cause failure"""
        # Missing both required args
        result = subprocess.run([sys.executable, str(self.script_path)], 
                              capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        
        # Missing srcroot
        result = subprocess.run([sys.executable, str(self.script_path), '--update_json', 'test.json'], 
                              capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        
        # Missing update_json
        result = subprocess.run([sys.executable, str(self.script_path), '--srcroot', '/tmp'], 
                              capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)


class OIDValidationTests(unittest.TestCase):
    """Test OID validation functionality"""
    
    def setUp(self):
        """Set up for OID validation tests"""
        if not OID_VALIDATION_AVAILABLE:
            self.skipTest("OID validation functions not available")
    
    def test_policy_constraint_validation(self):
        """Test policy constraint OID validation"""
        test_policies = [
            "1.2.840.113635.100.1.123",    # Valid Apple policy
            "1.2.840.113635.100.1.456",    # Valid Apple policy
            "2.23.140.1.1",                # Valid OID but not Apple policy
            "invalid.oid.format",           # Invalid format
            ""                             # Empty string
        ]
        
        valid_policies, invalid_policies = validate_policy_constraints(test_policies)
        
        # Should find valid Apple policies
        self.assertIn("1.2.840.113635.100.1.123", valid_policies)
        self.assertIn("1.2.840.113635.100.1.456", valid_policies)
        
        # Should reject non-Apple policies and invalid formats
        self.assertIn("2.23.140.1.1", invalid_policies)
        self.assertIn("invalid.oid.format", invalid_policies)
        self.assertIn("", invalid_policies)
    
    def test_ev_oid_validation(self):
        """Test EV OID validation"""
        test_ev_oids = [
            "2.23.140.1.1",                # Valid CA/Browser Forum EV
            "1.3.6.1.4.1.6334.1.100.1",   # Valid enterprise EV
            "2.16.578.1.26.1.3.3",        # Valid national arc EV
            "invalid.ev.oid",              # Invalid format
            "1.2.3"                        # Valid OID format but not recognized EV
        ]
        
        valid_ev, invalid_ev = validate_ev_oids(test_ev_oids)
        
        # Should find valid EV OIDs
        self.assertIn("2.23.140.1.1", valid_ev)
        
        # Should reject invalid formats
        self.assertIn("invalid.ev.oid", invalid_ev)
    
    def test_update_json_oid_validation_with_mock_data(self):
        """Test OID validation on mock update data"""
        mock_updates = [
            {
                "change_type": "Addition",
                "change_reason": "Test certificate addition",
                "certificate_details": {
                    "sha256_fingerprint": "D947432ABDE7B7FA90FC2E6B59101B1280E0E1C7E4E40FA3C6887FFF57A7F4CF",
                    "spki": "test_spki",
                    "common_name": "Test CA",
                    "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAK..."
                },
                "anchor_type": "Custom",
                "policy_constraints": ["1.2.840.113635.100.1.123"],  # Valid Apple policy
                "ev_tls_oids": ["2.23.140.1.1"]  # Valid EV OID
            },
            {
                "change_type": "Modification",
                "change_reason": "Test certificate modification with invalid OIDs",
                "certificate_details": {
                    "sha256_fingerprint": "349DFA4058C5E263123B398AE795573C4E1313C83FE68F93556CD5E8031B3C7D",
                    "spki": "test_spki2",
                    "common_name": "Test CA 2",
                    "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAK..."
                },
                "anchor_type": "Platform",
                "policy_constraints": ["invalid.policy.oid"],  # Invalid policy
                "ev_tls_oids": ["invalid.ev.oid"]  # Invalid EV OID
            }
        ]
        
        result = validate_update_json_oids(mock_updates)
        
        # Should fail overall validation due to invalid OIDs in second certificate
        self.assertFalse(result["valid"])
        
        # Should have errors reported
        self.assertTrue(len(result["errors"]) > 0)
        
        # Should have results for both certificates
        self.assertEqual(len(result["certificate_results"]), 2)
        
        # First certificate should have valid OIDs
        cert1_result = result["certificate_results"][0]
        self.assertEqual(len(cert1_result["policy_constraints"]["valid"]), 1)
        self.assertEqual(len(cert1_result["ev_oids"]["valid"]), 1)
        
        # Second certificate should have invalid OIDs
        cert2_result = result["certificate_results"][1]
        self.assertEqual(len(cert2_result["policy_constraints"]["invalid"]), 1)
        self.assertEqual(len(cert2_result["ev_oids"]["invalid"]), 1)
    
    def test_empty_oid_lists(self):
        """Test validation with empty OID lists"""
        mock_updates = [
            {
                "change_type": "Addition",
                "change_reason": "Test certificate with no OIDs",
                "certificate_details": {
                    "sha256_fingerprint": "D947432ABDE7B7FA90FC2E6B59101B1280E0E1C7E4E40FA3C6887FFF57A7F4CF",
                    "spki": "test_spki",
                    "common_name": "Test CA",
                    "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAK..."
                },
                "anchor_type": "Custom",
                "policy_constraints": [],
                "ev_tls_oids": []
            }
        ]
        
        result = validate_update_json_oids(mock_updates)
        
        # Should pass validation with empty OID lists
        self.assertTrue(result["valid"])
        self.assertEqual(len(result["errors"]), 0)


class OIDIntegrationTests(unittest.TestCase):
    """Test OID validation integration with trust store updates"""
    
    def setUp(self):
        """Set up test environment for OID integration tests"""
        if not OID_VALIDATION_AVAILABLE:
            self.skipTest("OID validation functions not available")
        
        self.test_dir = tempfile.mkdtemp(prefix='oid_integration_test_')
        self.script_path = update_scripts_dir + '/update_trust_store.py'

        # Create test JSON files for OID validation
        self.create_oid_test_files()
    
    def tearDown(self):
        """Clean up test environment"""
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)
    
    def create_oid_test_files(self):
        """Create test JSON files for OID validation testing"""
        
        # Valid OIDs test file
        valid_oids_data = [
            {
                "change_type": "Addition",
                "change_reason": "Test certificate with valid OIDs",
                "certificate_details": {
                    "sha256_fingerprint": "D947432ABDE7B7FA90FC2E6B59101B1280E0E1C7E4E40FA3C6887FFF57A7F4CF",
                    "spki": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE...",
                    "common_name": "Test Valid OIDs CA",
                    "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAMCyKw..."
                },
                "anchor_type": "Custom",
                "policy_constraints": ["1.2.840.113635.100.1.123", "1.2.840.113635.100.1.456"],
                "ev_tls_oids": ["2.23.140.1.1"]
            }
        ]
        
        with open(os.path.join(self.test_dir, 'test_data_valid_oids.json'), 'w') as f:
            json.dump(valid_oids_data, f, indent=4)
        
        # Invalid policy OIDs test file
        invalid_policy_data = [
            {
                "change_type": "Addition",
                "change_reason": "Test certificate with invalid policy OIDs",
                "certificate_details": {
                    "sha256_fingerprint": "349DFA4058C5E263123B398AE795573C4E1313C83FE68F93556CD5E8031B3C7D",
                    "spki": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE...",
                    "common_name": "Test Invalid Policy CA",
                    "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAMCyKw..."
                },
                "anchor_type": "Platform",
                "policy_constraints": ["2.23.140.1.1", "invalid.policy.oid"],  # Not Apple policies
                "ev_tls_oids": []
            }
        ]
        
        with open(os.path.join(self.test_dir, 'test_data_invalid_policy.json'), 'w') as f:
            json.dump(invalid_policy_data, f, indent=4)
        
        # Invalid EV OIDs test file
        invalid_ev_data = [
            {
                "change_type": "Modification",
                "change_reason": "Test certificate with invalid EV OIDs",
                "certificate_details": {
                    "sha256_fingerprint": "ABC123DEF456789012345678901234567890123456789012345678901234CDEF",
                    "spki": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE...",
                    "common_name": "Test Invalid EV CA",
                    "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAMCyKw..."
                },
                "anchor_type": "System",
                "policy_constraints": ["1.2.840.113635.100.1.123"],
                "ev_tls_oids": ["invalid.ev.oid", "not.a.real.ev.oid"]
            }
        ]
        
        with open(os.path.join(self.test_dir, 'test_data_invalid_ev_oids.json'), 'w') as f:
            json.dump(invalid_ev_data, f, indent=4)
        
        # Edge cases test file
        edge_cases_data = [
            {
                "change_type": "Addition",
                "change_reason": "Test certificate with edge case OIDs",
                "certificate_details": {
                    "sha256_fingerprint": "EDGE123CASE456789012345678901234567890123456789012345678901234CASE",
                    "spki": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE...",
                    "common_name": "Test Edge Cases CA",
                    "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAMCyKw..."
                },
                "anchor_type": "Custom",
                "policy_constraints": ["", "1.2.840.113635.100.1.999"],  # Empty string and high number
                "ev_tls_oids": ["1.2.3", ""]  # Short OID and empty string
            },
            {
                "change_type": "Removal",
                "change_reason": "Test removal with no OIDs",
                "certificate_details": {
                    "sha256_fingerprint": "REMOVE123456789012345678901234567890123456789012345678901234567890",
                    "spki": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE...",
                    "common_name": "Test Removal CA",
                    "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAMCyKw..."
                },
                "anchor_type": "System"
                # Note: No policy_constraints or ev_tls_oids fields for removal
            }
        ]
        
        with open(os.path.join(self.test_dir, 'test_data_edge_cases.json'), 'w') as f:
            json.dump(edge_cases_data, f, indent=4)
    
    def read_json_file(self, filepath):
        """Read JSON file with fallback if validate_update_json is not available"""
        if JSON_READER_AVAILABLE:
            return readJson(filepath)
        else:
            with open(filepath, 'r') as f:
                return json.load(f)
    
    def test_valid_oids_file(self):
        """Test OID validation on file with valid OIDs"""
        test_file = os.path.join(self.test_dir, 'test_data_valid_oids.json')
        updates = self.read_json_file(test_file)
        result = validate_update_json_oids(updates)
        
        self.assertTrue(result["valid"], "Valid OIDs file should pass validation")
        self.assertEqual(len(result["errors"]), 0, "Valid OIDs file should have no errors")
        self.assertEqual(len(result["certificate_results"]), 1)
        
        cert_result = result["certificate_results"][0]
        self.assertEqual(len(cert_result["policy_constraints"]["valid"]), 2)
        self.assertEqual(len(cert_result["ev_oids"]["valid"]), 1)
    
    def test_invalid_policy_file(self):
        """Test OID validation on file with invalid policy OIDs"""
        test_file = os.path.join(self.test_dir, 'test_data_invalid_policy.json')
        updates = self.read_json_file(test_file)
        result = validate_update_json_oids(updates)
        
        self.assertFalse(result["valid"], "Invalid policy OIDs file should fail validation")
        self.assertGreater(len(result["errors"]), 0, "Invalid policy OIDs file should have errors")
        
        cert_result = result["certificate_results"][0]
        self.assertGreater(len(cert_result["policy_constraints"]["invalid"]), 0)
    
    def test_invalid_ev_oids_file(self):
        """Test OID validation on file with invalid EV OIDs"""
        test_file = os.path.join(self.test_dir, 'test_data_invalid_ev_oids.json')
        updates = self.read_json_file(test_file)
        result = validate_update_json_oids(updates)
        
        self.assertFalse(result["valid"], "Invalid EV OIDs file should fail validation")
        self.assertGreater(len(result["errors"]), 0, "Invalid EV OIDs file should have errors")
        
        cert_result = result["certificate_results"][0]
        self.assertGreater(len(cert_result["ev_oids"]["invalid"]), 0)
    
    def test_edge_cases_file(self):
        """Test OID validation on file with edge cases"""
        test_file = os.path.join(self.test_dir, 'test_data_edge_cases.json')
        updates = self.read_json_file(test_file)
        result = validate_update_json_oids(updates)
        
        # Should fail due to invalid OIDs (empty strings, etc.)
        self.assertFalse(result["valid"], "Edge cases file should fail validation")
        self.assertGreater(len(result["errors"]), 0, "Edge cases file should have errors")
        
        # Should process both certificates (one with OIDs, one without)
        self.assertEqual(len(result["certificate_results"]), 2)
    
    def test_oid_validation_integration_demonstration(self):
        """Demonstrate how OID validation integrates with update_trust_store.py"""
        # This test documents the integration points
        test_file = os.path.join(self.test_dir, 'test_data_valid_oids.json')
        updates = self.read_json_file(test_file)
        
        # Step 1: Schema validation would happen first (not tested here)
        # Step 2: OID validation
        oid_result = validate_update_json_oids(updates)
        
        # Step 3: Check result and proceed or exit
        if oid_result["valid"]:
            print("✅ OID validation passed - would proceed with trust store update")
        else:
            print("❌ OID validation failed - would exit before making changes")
            for error in oid_result["errors"]:
                print(f"  Error: {error}")
        
        # For this test, we expect success
        self.assertTrue(oid_result["valid"])


def create_cli_test_examples():
    """Generate CLI examples for manual testing"""
    test_dir = Path(__file__).parent
    examples = []
    
    # Basic dry run
    examples.append(f"""
# Basic dry run test
python3 update_scripts/update_trust_store.py \\
    --update_json {test_dir}/test_data_addition.json \\
    --srcroot /tmp/test_trust_store \\
    --dry_run
""")
    
    # Version override test
    examples.append(f"""
# Test with version override
python3 update_scripts/update_trust_store.py \\
    --update_json {test_dir}/test_data_mixed.json \\
    --srcroot /tmp/test_trust_store \\
    --dry_run \\
    --asset_version "3.0.0"
""")
    
    # No version update test
    examples.append(f"""
# Test skipping version updates
python3 update_scripts/update_trust_store.py \\
    --update_json {test_dir}/test_data_modification.json \\
    --srcroot /tmp/test_trust_store \\
    --dry_run \\
    --no_version_update
""")
    
    # Actual execution test
    examples.append(f"""
# Actual execution (be careful with this!)
python3 update_scripts/update_trust_store.py \\
    --update_json {test_dir}/test_data_addition.json \\
    --srcroot /tmp/test_trust_store
""")
    
    # OID validation tests (if available)
    if OID_VALIDATION_AVAILABLE:
        examples.append(f"""
# Test OID validation standalone (merged from test_quick_oid_validation.py)
python3 -m unittest test_update_trust_store.OIDValidationTests -v
""")
        
        examples.append(f"""
# Test OID integration (merged from test_oid_integration.py)
python3 -m unittest test_update_trust_store.OIDIntegrationTests -v
""")
    
    return examples


if __name__ == '__main__':
    # Print CLI examples
    print("CLI Test Examples:")
    print("=" * 50)
    for i, example in enumerate(create_cli_test_examples(), 1):
        print(f"Example {i}:{example}")
    
    print("\nRunning automated tests...")
    print("=" * 50)
    
    # Print information about merged tests
    print("Test suites included:")
    print("  - TrustStoreUpdateTests: Core trust store update functionality")
    print("  - CLITests: Command-line interface and argument parsing")
    if OID_VALIDATION_AVAILABLE:
        print("  - OIDValidationTests: OID validation functionality (merged from test_quick_oid_validation.py)")
        print("  - OIDIntegrationTests: OID validation integration (merged from test_oid_integration.py)")
    else:
        print("  - OID tests skipped: oid_validation module not available")
    print()
    
    # Run the tests
    unittest.main(verbosity=2)
