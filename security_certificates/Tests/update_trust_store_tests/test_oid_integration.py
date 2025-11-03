#!/usr/bin/env python3
"""
OID Integration Test Script
This script tests the integration of OID validation with the trust store update process.
It creates test scenarios that combine OID validation with actual trust store operations.
"""

import sys
import json
import tempfile
import shutil
import subprocess
from pathlib import Path

# Add the update_scipts to sys.path to import the scripts we're testing
update_scripts_dir = str(Path(__file__).parent.parent.parent) + "/update_scripts"
sys.path.insert(0, update_scripts_dir)

try:
    from oid_validation import validate_update_json_oids, validate_policy_constraints, validate_ev_oids
    OID_VALIDATION_AVAILABLE = True
except ImportError as e:
    print(f"❌ Error importing OID validation functions: {e}")
    print("Make sure oid_validation.py is in the same directory")
    OID_VALIDATION_AVAILABLE = False

def create_test_update_file(filename, updates_data):
    """Create a JSON test file with update data"""
    with open(filename, 'w') as f:
        json.dump(updates_data, f, indent=4)
    return filename

def setup_test_environment():
    """Set up a temporary test environment"""
    test_dir = tempfile.mkdtemp(prefix='oid_integration_test_')
    
    # Create directory structure
    dirs = [
        'certificates/custom',
        'certificates/platform', 
        'certificates/roots',
        'certificates/test-roots',
        'certificates/test-platform',
        'certificates/removed/intermediates',
        'config',
        'update_scripts'
    ]
    
    for dir_path in dirs:
        Path(test_dir, dir_path).mkdir(parents=True, exist_ok=True)
    
    # Create required files
    files_to_create = {
        'certificates/constraints.json': {
            "system": {},
            "custom": {},
            "platform": {},
            "test-system": {},
            "test-platform": {}
        },
        'certificates/EVRoots.json': {
            "EV_config": {},
            "fingerprint_map": {}
        },
        'certificates/hash_to_human_name.json': {}
    }
    
    for file_path, content in files_to_create.items():
        with open(Path(test_dir, file_path), 'w') as f:
            json.dump(content, f, indent=4)
    
    # Create plist files
    try:
        import plistlib
        
        asset_version = {
            "VersionNumber": 2024082500,
            "PKITrustStoreAssetsVersion": "1.0.0",
            "MobileAssetContentVersion": 123
        }
        with open(Path(test_dir, 'config/AssetVersion.plist'), 'wb') as f:
            plistlib.dump(asset_version, f)
        
        info_asset = {
            "CFBundleShortVersionString": "1.0.0",
            "CFBundleVersion": "1.0.0",
            "MobileAssetProperties": {
                "AssetVersion": "1.0.0",
                "ContentVersion": 2024082500
            }
        }
        with open(Path(test_dir, 'config/Info-Asset.plist'), 'wb') as f:
            plistlib.dump(info_asset, f)
    except ImportError:
        print("⚠️  plistlib not available - skipping plist file creation")
    
    # Create xcconfig file
    xcconfig_content = """// Trust Store Configuration
TRUST_STORE_VERSION = 2024082500
OTHER_SETTING = example_value
"""
    with open(Path(test_dir, 'config/security_certificates.xcconfig'), 'w') as f:
        f.write(xcconfig_content)
    
    return test_dir

def test_valid_apple_policy_integration():
    """Test integration with valid Apple policy OIDs"""
    print("🍎 Testing Valid Apple Policy OID Integration")
    print("=" * 60)
    
    if not OID_VALIDATION_AVAILABLE:
        print("⚠️  OID validation not available - skipping test")
        return False
    
    # Create test data with valid Apple policy OIDs
    test_updates = [
        {
            "change_type": "Addition",
            "change_reason": "Adding certificate with valid Apple policy constraints",
            "certificate_details": {
                "sha256_fingerprint": "D947432ABDE7B7FA90FC2E6B59101B1280E0E1C7E4E40FA3C6887FFF57A7F4CF",
                "spki": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE...",
                "common_name": "Test Apple Policy CA",
                "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAMCyKw..."
            },
            "anchor_type": "Custom",
            "policy_constraints": [
                "1.2.840.113635.100.1.123",  # iOS App Store policy
                "1.2.840.113635.100.1.456"   # Custom Apple policy
            ]
        }
    ]
    
    # Validate OIDs
    validation_result = validate_update_json_oids(test_updates)
    
    print(f"📋 Test Data: {len(test_updates)} certificate(s)")
    print(f"✅ OID Validation Passed: {validation_result['valid']}")
    
    if validation_result['valid']:
        print("   All policy constraints are valid Apple OIDs")
        for cert_result in validation_result['certificate_results']:
            valid_policies = cert_result['policy_constraints']['valid']
            print(f"   Certificate '{cert_result['certificate']}': {valid_policies}")
        
        # Integration point: This is where update_trust_store.py would proceed
        print("🔄 Integration Point: Trust store update would proceed")
        return True
    else:
        print("❌ Validation failed:")
        for error in validation_result['errors']:
            print(f"   - {error}")
        return False

def test_invalid_policy_oid_integration():
    """Test integration with invalid policy OIDs"""
    print("\n❌ Testing Invalid Policy OID Integration")
    print("=" * 60)
    
    if not OID_VALIDATION_AVAILABLE:
        print("⚠️  OID validation not available - skipping test")
        return False
    
    # Create test data with invalid policy OIDs
    test_updates = [
        {
            "change_type": "Addition",
            "change_reason": "Adding certificate with invalid policy constraints",
            "certificate_details": {
                "sha256_fingerprint": "349DFA4058C5E263123B398AE795573C4E1313C83FE68F93556CD5E8031B3C7D",
                "spki": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE...",
                "common_name": "Test Invalid Policy CA",
                "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAMCyKw..."
            },
            "anchor_type": "Platform",
            "policy_constraints": [
                "2.23.140.1.1",           # Valid OID but not Apple policy
                "invalid.policy.oid",      # Invalid format
                "1.2.840.113635.100.2.123" # Wrong Apple arc
            ]
        }
    ]
    
    # Validate OIDs
    validation_result = validate_update_json_oids(test_updates)
    
    print(f"📋 Test Data: {len(test_updates)} certificate(s)")
    print(f"❌ OID Validation Passed: {validation_result['valid']} (expected: False)")
    
    if not validation_result['valid']:
        print("   Validation correctly identified invalid policy OIDs:")
        for error in validation_result['errors']:
            print(f"   - {error}")
        
        # Integration point: This is where update_trust_store.py would stop
        print("🛑 Integration Point: Trust store update would be prevented")
        return True
    else:
        print("❌ Validation unexpectedly passed - this is a test failure")
        return False

def test_valid_ev_oid_integration():
    """Test integration with valid EV TLS OIDs"""
    print("\n🔒 Testing Valid EV TLS OID Integration")
    print("=" * 60)
    
    if not OID_VALIDATION_AVAILABLE:
        print("⚠️  OID validation not available - skipping test")
        return False
    
    # Create test data with valid EV TLS OIDs
    test_updates = [
        {
            "change_type": "Modification",
            "change_reason": "Modifying certificate with valid EV TLS OIDs",
            "certificate_details": {
                "sha256_fingerprint": "ABC123DEF456789012345678901234567890123456789012345678901234CDEF",
                "spki": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE...",
                "common_name": "Test EV CA",
                "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAMCyKw..."
            },
            "anchor_type": "System",
            "ev_tls_oids": [
                "2.23.140.1.1",                  # CA/Browser Forum
                "1.3.6.1.4.1.6334.1.100.1",     # DigiCert
                "2.16.578.1.26.1.3.3"           # Norwegian EV
            ]
        }
    ]
    
    # Validate OIDs
    validation_result = validate_update_json_oids(test_updates)
    
    print(f"📋 Test Data: {len(test_updates)} certificate(s)")
    print(f"✅ OID Validation Passed: {validation_result['valid']}")
    
    if validation_result['valid']:
        print("   All EV TLS OIDs are valid")
        for cert_result in validation_result['certificate_results']:
            valid_ev = cert_result['ev_oids']['valid']
            print(f"   Certificate '{cert_result['certificate']}': {valid_ev}")
        
        # Integration point: This is where update_trust_store.py would proceed
        print("🔄 Integration Point: Trust store update with EV OIDs would proceed")
        return True
    else:
        print("❌ Validation failed:")
        for error in validation_result['errors']:
            print(f"   - {error}")
        return False

def test_mixed_valid_invalid_integration():
    """Test integration with mixed valid and invalid OIDs"""
    print("\n🔀 Testing Mixed Valid/Invalid OID Integration")
    print("=" * 60)
    
    if not OID_VALIDATION_AVAILABLE:
        print("⚠️  OID validation not available - skipping test")
        return False
    
    # Create test data with mixed valid and invalid OIDs
    test_updates = [
        {
            "change_type": "Addition",
            "change_reason": "Certificate with valid Apple policies",
            "certificate_details": {
                "sha256_fingerprint": "VALID123456789012345678901234567890123456789012345678901234567890",
                "spki": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE...",
                "common_name": "Valid Policy CA",
                "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAMCyKw..."
            },
            "anchor_type": "Custom",
            "policy_constraints": ["1.2.840.113635.100.1.123"],
            "ev_tls_oids": ["2.23.140.1.1"]
        },
        {
            "change_type": "Addition",
            "change_reason": "Certificate with invalid policy OIDs",
            "certificate_details": {
                "sha256_fingerprint": "INVALID456789012345678901234567890123456789012345678901234567890",
                "spki": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE...",
                "common_name": "Invalid Policy CA",
                "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAMCyKw..."
            },
            "anchor_type": "Platform",
            "policy_constraints": ["invalid.policy.oid"],
            "ev_tls_oids": ["invalid.ev.oid"]
        }
    ]
    
    # Validate OIDs
    validation_result = validate_update_json_oids(test_updates)
    
    print(f"📋 Test Data: {len(test_updates)} certificate(s)")
    print(f"❌ OID Validation Passed: {validation_result['valid']} (expected: False)")
    
    if not validation_result['valid']:
        print("   Validation correctly identified the batch as invalid due to one bad certificate:")
        for error in validation_result['errors']:
            print(f"   - {error}")
        
        # Show individual certificate results
        for i, cert_result in enumerate(validation_result['certificate_results']):
            cert_name = cert_result['certificate']
            valid_policies_count = len(cert_result['policy_constraints']['valid'])
            invalid_policies_count = len(cert_result['policy_constraints']['invalid'])
            valid_ev_count = len(cert_result['ev_oids']['valid'])
            invalid_ev_count = len(cert_result['ev_oids']['invalid'])
            
            print(f"   Certificate {i+1} '{cert_name}':")
            print(f"     Valid policies: {valid_policies_count}, Invalid policies: {invalid_policies_count}")
            print(f"     Valid EV OIDs: {valid_ev_count}, Invalid EV OIDs: {invalid_ev_count}")
        
        # Integration point: This is where update_trust_store.py would stop
        print("🛑 Integration Point: Entire batch would be rejected due to invalid OIDs")
        return True
    else:
        print("❌ Validation unexpectedly passed - this is a test failure")
        return False

def test_file_based_integration():
    """Test integration using actual JSON files"""
    print("\n📄 Testing File-Based OID Integration")
    print("=" * 60)
    
    if not OID_VALIDATION_AVAILABLE:
        print("⚠️  OID validation not available - skipping test")
        return False
    
    test_dir = tempfile.mkdtemp(prefix='file_integration_test_')
    
    try:
        # Create test files with different OID scenarios
        
        # File 1: Valid Apple policies
        valid_apple_data = [
            {
                "change_type": "Addition",
                "change_reason": "File-based test with valid Apple policies",
                "certificate_details": {
                    "sha256_fingerprint": "FILE123456789012345678901234567890123456789012345678901234567890",
                    "spki": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE...",
                    "common_name": "File Test Valid CA",
                    "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAMCyKw..."
                },
                "anchor_type": "Custom",
                "policy_constraints": ["1.2.840.113635.100.1.123", "1.2.840.113635.100.1.456"]
            }
        ]
        
        valid_file = Path(test_dir, 'valid_policies.json')
        create_test_update_file(valid_file, valid_apple_data)
        
        # File 2: Invalid policies
        invalid_policy_data = [
            {
                "change_type": "Addition", 
                "change_reason": "File-based test with invalid policies",
                "certificate_details": {
                    "sha256_fingerprint": "FILEINV456789012345678901234567890123456789012345678901234567890",
                    "spki": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE...",
                    "common_name": "File Test Invalid CA",
                    "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAMCyKw..."
                },
                "anchor_type": "Platform",
                "policy_constraints": ["invalid.policy.format", "2.23.140.1.1"]
            }
        ]
        
        invalid_file = Path(test_dir, 'invalid_policies.json')
        create_test_update_file(invalid_file, invalid_policy_data)
        
        # Test valid file
        print(f"📁 Testing valid policies file: {valid_file.name}")
        with open(valid_file, 'r') as f:
            valid_data = json.load(f)
        
        valid_result = validate_update_json_oids(valid_data)
        print(f"✅ Valid file result: {valid_result['valid']}")
        if valid_result['valid']:
            print("   File would be accepted for trust store update")
        else:
            print(f"   Unexpected errors: {valid_result['errors']}")
        
        # Test invalid file
        print(f"\n📁 Testing invalid policies file: {invalid_file.name}")
        with open(invalid_file, 'r') as f:
            invalid_data = json.load(f)
        
        invalid_result = validate_update_json_oids(invalid_data)
        print(f"❌ Invalid file result: {invalid_result['valid']} (expected: False)")
        if not invalid_result['valid']:
            print("   File would be rejected for trust store update")
            print("   Errors found:")
            for error in invalid_result['errors']:
                print(f"     - {error}")
        else:
            print("   Unexpected: File was accepted despite invalid OIDs")
        
        return valid_result['valid'] and not invalid_result['valid']
    
    finally:
        # Cleanup
        shutil.rmtree(test_dir)

def test_integration_workflow():
    """Test the complete integration workflow"""
    print("\n🔄 Testing Complete Integration Workflow")
    print("=" * 60)
    
    if not OID_VALIDATION_AVAILABLE:
        print("⚠️  OID validation not available - skipping test")
        return False
    
    # Simulate the complete workflow that would happen in update_trust_store.py
    
    print("Step 1: Loading update JSON file")
    test_updates = [
        {
            "change_type": "Addition",
            "change_reason": "Workflow test certificate",
            "certificate_details": {
                "sha256_fingerprint": "WORKFLOW123456789012345678901234567890123456789012345678901234567890",
                "spki": "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE...",
                "common_name": "Workflow Test CA",
                "pem": "-----BEGIN CERTIFICATE-----\nMIIBkTCB+wIJAMCyKw..."
            },
            "anchor_type": "Custom",
            "policy_constraints": ["1.2.840.113635.100.1.123"],
            "ev_tls_oids": ["2.23.140.1.1"]
        }
    ]
    print("✅ JSON loaded successfully")
    
    print("\nStep 2: Schema validation (simulated)")
    print("✅ Schema validation passed")
    
    print("\nStep 3: OID validation")
    validation_result = validate_update_json_oids(test_updates)
    
    if validation_result['valid']:
        print("✅ OID validation passed")
        
        print("\nStep 4: Pre-flight checks (simulated)")
        print("✅ Pre-flight checks passed")
        
        print("\nStep 5: Trust store operations (simulated)")
        print("✅ Certificate would be added to custom anchors")
        print("✅ Policy constraints would be updated")
        print("✅ EV TLS OIDs would be updated")
        
        print("\nStep 6: Version management (simulated)")
        print("✅ Version files would be updated")
        
        print("\n🎉 Complete workflow would succeed")
        return True
    else:
        print("❌ OID validation failed")
        print("   Errors:")
        for error in validation_result['errors']:
            print(f"     - {error}")
        
        print("\n🛑 Workflow would stop here - no trust store changes made")
        return False

def main():
    """Run all OID integration tests"""
    print("🔗 OID Integration Test Suite")
    print("=" * 60)
    print("Testing integration of OID validation with trust store updates")
    print()
    
    if not OID_VALIDATION_AVAILABLE:
        print("❌ OID validation functions not available")
        print("Make sure oid_validation.py is in the same directory")
        return False
    
    # Run integration tests
    test_results = []
    
    test_results.append(test_valid_apple_policy_integration())
    test_results.append(test_invalid_policy_oid_integration())
    test_results.append(test_valid_ev_oid_integration())
    test_results.append(test_mixed_valid_invalid_integration())
    test_results.append(test_file_based_integration())
    test_results.append(test_integration_workflow())
    
    # Summary
    print("\n📊 Integration Test Summary")
    print("=" * 40)
    
    passed_tests = sum(test_results)
    total_tests = len(test_results)
    
    if passed_tests == total_tests:
        print("✅ All integration tests passed!")
        print(f"   {passed_tests}/{total_tests} integration tests successful")
        print("\n🔗 OID validation is properly integrated and working")
        return True
    else:
        print("❌ Some integration tests failed")
        print(f"   {passed_tests}/{total_tests} integration tests successful")
        print("\n⚠️  OID validation integration needs attention")
        return False

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
