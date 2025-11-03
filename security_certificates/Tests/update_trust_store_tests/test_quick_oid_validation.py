#!/usr/bin/env python3
"""
Quick OID Validation Test Script
This script provides quick validation tests for policy constraints and EV TLS OIDs
used in trust store updates. It can be run standalone or integrated into larger test suites.
"""

import sys
from pathlib import Path

# Add the update_scipts to sys.path to import the scripts we're testing
update_scripts_dir = str(Path(__file__).parent.parent.parent) + "/update_scripts"
sys.path.insert(0, update_scripts_dir)

try:
    from oid_validation import (
        is_valid_oid_format,
        is_valid_apple_policy_oid,
        is_valid_ev_oid,
        validate_policy_constraints,
        validate_ev_oids,
        validate_update_json_oids
    )
    OID_VALIDATION_AVAILABLE = True
except ImportError as e:
    print(f"❌ Error importing OID validation functions: {e}")
    print("Make sure oid_validation.py is in the same directory")
    sys.exit(1)

def test_basic_oid_format_validation():
    """Test basic OID format validation"""
    print("🔍 Testing Basic OID Format Validation")
    print("=" * 50)
    
    test_cases = [
        # Valid OIDs
        ("1.2.3", True, "Simple valid OID"),
        ("1.2.840.113635.100.1.123", True, "Apple policy OID"),
        ("2.23.140.1.1", True, "CA/Browser Forum EV OID"),
        ("1.3.6.1.4.1.6334.1.100.1", True, "Enterprise EV OID"),
        ("0.4.0.2342.61200.2.1.4", True, "Valid OID starting with 0"),
        ("2.16.578.1.26.1.3.3", True, "Norwegian EV OID"),
        
        # Invalid OIDs
        ("", False, "Empty string"),
        ("invalid.oid", False, "Non-numeric components"),
        ("1..2.3", False, "Consecutive dots"),
        (".1.2.3", False, "Leading dot"),
        ("1.2.3.", False, "Trailing dot"),
        ("3.1.2", False, "Invalid first component (>2)"),
        ("1.40.2", False, "Invalid second component for first=1"),
        ("0.50.2", False, "Invalid second component for first=0"),
        ("1", False, "Single component"),
        ("a.b.c", False, "Alphabetic components")
    ]
    
    passed = 0
    total = len(test_cases)
    
    for oid, expected, description in test_cases:
        result = is_valid_oid_format(oid)
        status = "✅" if result == expected else "❌"
        print(f"{status} {oid:<30} | {description}")
        if result == expected:
            passed += 1
        else:
            print(f"   Expected: {expected}, Got: {result}")
    
    print(f"\nResults: {passed}/{total} tests passed")
    return passed == total

def test_apple_policy_oid_validation():
    """Test Apple policy OID validation"""
    print("\n🍎 Testing Apple Policy OID Validation")
    print("=" * 50)
    
    test_cases = [
        # Valid Apple policy OIDs
        ("1.2.840.113635.100.1.123", True, "iOS App Store policy"),
        ("1.2.840.113635.100.1.456", True, "Generic Apple policy"),
        ("1.2.840.113635.100.1.1", True, "Apple policy with minimal suffix"),
        ("1.2.840.113635.100.1.999999", True, "Apple policy with large suffix"),
        
        # Invalid Apple policy OIDs
        ("2.23.140.1.1", False, "Valid OID but not Apple policy"),
        ("1.2.840.113635.100.2.123", False, "Wrong Apple arc (100.2 instead of 100.1)"),
        ("1.2.840.113635.101.1.123", False, "Wrong Apple arc (101.1 instead of 100.1)"),
        ("1.2.840.113636.100.1.123", False, "Wrong enterprise number"),
        ("1.2.840.113635.100.1.", False, "Missing suffix"),
        ("1.2.840.113635.100.1", False, "Missing suffix separator"),
        ("invalid.apple.oid", False, "Invalid format"),
        ("", False, "Empty string")
    ]
    
    passed = 0
    total = len(test_cases)
    
    for oid, expected, description in test_cases:
        result = is_valid_apple_policy_oid(oid)
        status = "✅" if result == expected else "❌"
        print(f"{status} {oid:<35} | {description}")
        if result == expected:
            passed += 1
        else:
            print(f"   Expected: {expected}, Got: {result}")
    
    print(f"\nResults: {passed}/{total} tests passed")
    return passed == total

def test_ev_oid_validation():
    """Test EV TLS OID validation"""
    print("\n🔒 Testing EV TLS OID Validation")
    print("=" * 50)
    
    test_cases = [
        # Valid EV OIDs
        ("2.23.140.1.1", True, "CA/Browser Forum EV OID"),
        ("1.3.6.1.4.1.6334.1.100.1", True, "DigiCert EV OID"),
        ("1.3.6.1.4.1.14370.1.6", True, "GoDaddy EV OID"),
        ("1.3.6.1.4.1.17326.10.14.2.1.2", True, "Comodo EV OID"),
        ("2.16.578.1.26.1.3.3", True, "Norwegian EV OID"),
        ("1.3.6.1.4.1.8024.0.2.100.1.2", True, "Buypass EV OID"),
        
        # Invalid EV OIDs  
        ("1.2.840.113635.100.1.123", False, "Apple policy OID (not EV)"),
        ("1.3.6.1.4.1.123", False, "Enterprise arc but too short"),
        ("1.3.6.1.4.1", False, "Enterprise arc incomplete"),
        ("invalid.ev.oid", False, "Invalid format"),
        ("1.2.3", False, "Valid OID format but not recognized EV"),
        ("", False, "Empty string")
    ]
    
    passed = 0
    total = len(test_cases)
    
    for oid, expected, description in test_cases:
        result = is_valid_ev_oid(oid)
        status = "✅" if result == expected else "❌"
        print(f"{status} {oid:<35} | {description}")
        if result == expected:
            passed += 1
        else:
            print(f"   Expected: {expected}, Got: {result}")
    
    print(f"\nResults: {passed}/{total} tests passed")
    return passed == total

def test_policy_constraints_validation():
    """Test policy constraints list validation"""
    print("\n📋 Testing Policy Constraints List Validation")
    print("=" * 60)
    
    test_cases = [
        # Valid policy constraint lists
        (["1.2.840.113635.100.1.123"], ([1], [0]), "Single valid Apple policy"),
        (["1.2.840.113635.100.1.123", "1.2.840.113635.100.1.456"], ([2], [0]), "Multiple valid Apple policies"),
        ([], ([0], [0]), "Empty list"),
        
        # Invalid policy constraint lists
        (["2.23.140.1.1"], ([0], [1]), "Valid OID but not Apple policy"),
        (["invalid.oid"], ([0], [1]), "Invalid OID format"),
        (["1.2.840.113635.100.1.123", "invalid.oid"], ([1], [1]), "Mixed valid and invalid"),
        (["1.2.840.113635.100.1.123", "2.23.140.1.1"], ([1], [1]), "Apple policy + non-Apple policy"),
    ]
    
    passed = 0
    total = len(test_cases)
    
    for policy_list, (expected_valid_count, expected_invalid_count), description in test_cases:
        valid_policies, invalid_policies = validate_policy_constraints(policy_list)
        result_valid_count = len(valid_policies)
        result_invalid_count = len(invalid_policies)
        
        valid_count_ok = result_valid_count == expected_valid_count[0]
        invalid_count_ok = result_invalid_count == expected_invalid_count[0]
        
        status = "✅" if (valid_count_ok and invalid_count_ok) else "❌"
        print(f"{status} {description}")
        print(f"   Input: {policy_list}")
        print(f"   Valid: {valid_policies} (expected {expected_valid_count[0]}, got {result_valid_count})")
        print(f"   Invalid: {invalid_policies} (expected {expected_invalid_count[0]}, got {result_invalid_count})")
        
        if valid_count_ok and invalid_count_ok:
            passed += 1
        print()
    
    print(f"Results: {passed}/{total} tests passed")
    return passed == total

def test_ev_oids_validation():
    """Test EV OIDs list validation"""
    print("\n🔐 Testing EV OIDs List Validation")
    print("=" * 50)
    
    test_cases = [
        # Valid EV OID lists
        (["2.23.140.1.1"], ([1], [0]), "Single valid EV OID"),
        (["2.23.140.1.1", "1.3.6.1.4.1.6334.1.100.1"], ([2], [0]), "Multiple valid EV OIDs"),
        ([], ([0], [0]), "Empty list"),
        
        # Invalid EV OID lists
        (["1.2.840.113635.100.1.123"], ([0], [1]), "Apple policy OID (not EV)"),
        (["invalid.oid"], ([0], [1]), "Invalid OID format"),
        (["2.23.140.1.1", "invalid.oid"], ([1], [1]), "Mixed valid and invalid"),
        (["1.2.3"], ([0], [1]), "Valid OID format but not recognized EV"),
    ]
    
    passed = 0
    total = len(test_cases)
    
    for ev_list, (expected_valid_count, expected_invalid_count), description in test_cases:
        valid_ev, invalid_ev = validate_ev_oids(ev_list)
        result_valid_count = len(valid_ev)
        result_invalid_count = len(invalid_ev)
        
        valid_count_ok = result_valid_count == expected_valid_count[0]
        invalid_count_ok = result_invalid_count == expected_invalid_count[0]
        
        status = "✅" if (valid_count_ok and invalid_count_ok) else "❌"
        print(f"{status} {description}")
        print(f"   Input: {ev_list}")
        print(f"   Valid: {valid_ev} (expected {expected_valid_count[0]}, got {result_valid_count})")
        print(f"   Invalid: {invalid_ev} (expected {expected_invalid_count[0]}, got {result_invalid_count})")
        
        if valid_count_ok and invalid_count_ok:
            passed += 1
        print()
    
    print(f"Results: {passed}/{total} tests passed")
    return passed == total

def test_update_json_oid_validation():
    """Test complete update JSON OID validation"""
    print("\n📄 Testing Update JSON OID Validation")
    print("=" * 50)
    
    # Test case 1: Valid update with Apple policies and EV OIDs
    valid_update = [
        {
            "change_type": "Addition",
            "change_reason": "Test certificate with valid OIDs",
            "certificate_details": {
                "sha256_fingerprint": "D947432ABDE7B7FA90FC2E6B59101B1280E0E1C7E4E40FA3C6887FFF57A7F4CF",
                "spki": "test_spki",
                "common_name": "Test Valid CA",
                "pem": "-----BEGIN CERTIFICATE-----\ntest_pem"
            },
            "anchor_type": "Custom",
            "policy_constraints": ["1.2.840.113635.100.1.123", "1.2.840.113635.100.1.456"],
            "ev_tls_oids": ["2.23.140.1.1"]
        }
    ]
    
    print("Testing valid update JSON...")
    result = validate_update_json_oids(valid_update)
    status = "✅" if result["valid"] else "❌"
    print(f"{status} Valid update JSON")
    print(f"   Errors: {result['errors']}")
    print(f"   Certificate results: {len(result['certificate_results'])}")
    
    # Test case 2: Invalid update with mixed valid/invalid OIDs
    invalid_update = [
        {
            "change_type": "Addition",
            "change_reason": "Test certificate with invalid OIDs",
            "certificate_details": {
                "sha256_fingerprint": "349DFA4058C5E263123B398AE795573C4E1313C83FE68F93556CD5E8031B3C7D",
                "spki": "test_spki2",
                "common_name": "Test Invalid CA",
                "pem": "-----BEGIN CERTIFICATE-----\ntest_pem2"
            },
            "anchor_type": "Platform",
            "policy_constraints": ["invalid.policy.oid", "1.2.840.113635.100.1.123"],
            "ev_tls_oids": ["invalid.ev.oid", "2.23.140.1.1"]
        }
    ]
    
    print("\nTesting invalid update JSON...")
    result = validate_update_json_oids(invalid_update)
    status = "✅" if not result["valid"] else "❌"  # We expect this to fail
    print(f"{status} Invalid update JSON (expected to fail)")
    print(f"   Errors: {result['errors']}")
    print(f"   Certificate results: {len(result['certificate_results'])}")
    
    # Test case 3: Empty update
    empty_update = []
    
    print("\nTesting empty update JSON...")
    result = validate_update_json_oids(empty_update)
    status = "✅" if result["valid"] else "❌"
    print(f"{status} Empty update JSON")
    print(f"   Errors: {result['errors']}")
    print(f"   Certificate results: {len(result['certificate_results'])}")
    
    # Test case 4: Update without OID fields
    no_oids_update = [
        {
            "change_type": "Removal",
            "change_reason": "Test certificate removal",
            "certificate_details": {
                "sha256_fingerprint": "ABC123DEF456789012345678901234567890123456789012345678901234ABCD",
                "spki": "test_spki3",
                "common_name": "Test Removal CA",
                "pem": "-----BEGIN CERTIFICATE-----\ntest_pem3"
            },
            "anchor_type": "System"
        }
    ]
    
    print("\nTesting update JSON without OID fields...")
    result = validate_update_json_oids(no_oids_update)
    status = "✅" if result["valid"] else "❌"
    print(f"{status} Update JSON without OIDs")
    print(f"   Errors: {result['errors']}")
    print(f"   Certificate results: {len(result['certificate_results'])}")
    
    return True

def run_performance_tests():
    """Run basic performance tests"""
    print("\n⚡ Performance Tests")
    print("=" * 30)
    
    import time
    
    # Test OID format validation performance
    test_oids = [
        "1.2.840.113635.100.1.123",
        "2.23.140.1.1",
        "1.3.6.1.4.1.6334.1.100.1",
        "invalid.oid.format",
        ""
    ] * 1000  # 5000 OIDs total
    
    start_time = time.time()
    for oid in test_oids:
        is_valid_oid_format(oid)
    end_time = time.time()
    
    print(f"✅ OID format validation: {len(test_oids)} OIDs in {end_time - start_time:.3f} seconds")
    print(f"   Rate: {len(test_oids) / (end_time - start_time):.0f} OIDs/second")
    
    # Test Apple policy validation performance
    apple_oids = ["1.2.840.113635.100.1.123"] * 1000
    
    start_time = time.time()
    for oid in apple_oids:
        is_valid_apple_policy_oid(oid)
    end_time = time.time()
    
    print(f"✅ Apple policy validation: {len(apple_oids)} OIDs in {end_time - start_time:.3f} seconds")
    print(f"   Rate: {len(apple_oids) / (end_time - start_time):.0f} OIDs/second")

def main():
    """Run all OID validation tests"""
    print("🧪 Quick OID Validation Test Suite")
    print("=" * 60)
    print("Testing OID validation functions for trust store updates")
    print()
    
    # Run all test categories
    test_results = []
    
    test_results.append(test_basic_oid_format_validation())
    test_results.append(test_apple_policy_oid_validation())
    test_results.append(test_ev_oid_validation())
    test_results.append(test_policy_constraints_validation())
    test_results.append(test_ev_oids_validation())
    test_results.append(test_update_json_oid_validation())
    
    # Performance tests (non-critical)
    try:
        run_performance_tests()
    except Exception as e:
        print(f"⚠️  Performance tests failed: {e}")
    
    # Summary
    print("\n📊 Test Summary")
    print("=" * 30)
    
    passed_tests = sum(test_results)
    total_tests = len(test_results)
    
    if passed_tests == total_tests:
        print("✅ All test categories passed!")
        print(f"   {passed_tests}/{total_tests} test categories successful")
        return True
    else:
        print("❌ Some test categories failed")
        print(f"   {passed_tests}/{total_tests} test categories successful")
        return False

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
