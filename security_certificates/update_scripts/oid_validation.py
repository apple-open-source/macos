#!/usr/bin/env python3

"""
OID Validation Utilities for Trust Store Updates

This module provides validation functions for Object Identifiers (OIDs) used in
policy constraints and EV TLS OIDs in trust store updates.
"""

import re
from typing import List, Tuple


def is_valid_oid_format(oid: str) -> bool:
    """
    Validate that a string follows proper OID format.
    
    OIDs must:
    - Contain only digits and dots
    - Start and end with digits (not dots)
    - Have no consecutive dots
    - Have at least two components (e.g., "1.2")
    - First component must be 0, 1, or 2
    - If first component is 0 or 1, second component must be 0-39
    - If first component is 2, second component can be any value
    
    Args:
        oid: String to validate
        
    Returns:
        bool: True if OID format is valid, False otherwise
    """
    if not oid or not isinstance(oid, str):
        return False
    
    # Basic format check: only digits and dots, no consecutive dots
    if not re.match(r'^[0-9]+(\.[0-9]+)*$', oid):
        return False
    
    # Split into components
    components = oid.split('.')
    
    # Must have at least 2 components
    if len(components) < 2:
        return False
    
    try:
        first = int(components[0])
        second = int(components[1])
        
        # First component validation
        if first not in [0, 1, 2]:
            return False
        
        # Second component validation
        if first in [0, 1] and second > 39:
            return False
            
    except ValueError:
        return False
    
    return True


def is_valid_apple_policy_oid(oid: str) -> bool:
    """
    Validate that an OID is in the Apple policy arc (1.2.840.113635.100.1.X).
    
    Args:
        oid: OID string to validate
        
    Returns:
        bool: True if OID is valid Apple policy OID, False otherwise
    """
    if not is_valid_oid_format(oid):
        return False
    
    # Apple policy arc prefix
    apple_prefix = "1.2.840.113635.100.1."
    
    if not oid.startswith(apple_prefix):
        return False
    
    # Check that there's something after the prefix
    suffix = oid[len(apple_prefix):]
    if not suffix:
        return False
    
    # Suffix should be numeric
    try:
        int(suffix)
        return True
    except ValueError:
        return False


def is_valid_ev_oid(oid: str) -> bool:
    """
    Validate that an OID is a properly formatted EV TLS OID.
    
    Common EV OID arcs include:
    - 2.23.140.1.1 (CA/Browser Forum)
    - 1.3.6.1.4.1.* (Enterprise arc)
    - 2.16.* (National arcs)
    
    Args:
        oid: OID string to validate
        
    Returns:
        bool: True if OID format is valid for EV use, False otherwise
    """
    if not is_valid_oid_format(oid):
        return False
    
    # Common EV OID prefixes (not exhaustive, but covers major CAs)
    common_ev_prefixes = [
        "2.23.140.1.1",  # CA/Browser Forum
        "2.16.",          # National arcs
        "1.2.40.0.",      # Austrian government
        "2.16.578.1.",    # Norwegian arc
    ]
    
    # Check if it matches known EV patterns
    for prefix in common_ev_prefixes:
        if oid.startswith(prefix):
            return True
    
    # For enterprise arc (1.3.6.1.4.1.*), be more specific
    if oid.startswith("1.3.6.1.4.1."):
        # Must have enterprise number and additional components
        components = oid.split('.')
        if len(components) > 7:  # 1.3.6.1.4.1.XXXXX at minimum
            return True
    
    return False


def validate_policy_constraints(policy_constraints: List[str]) -> Tuple[List[str], List[str]]:
    """
    Validate a list of policy constraint OIDs.
    
    Args:
        policy_constraints: List of OID strings to validate
        
    Returns:
        Tuple of (valid_oids, invalid_oids)
    """
    valid_oids = []
    invalid_oids = []
    
    for oid in policy_constraints:
        if is_valid_apple_policy_oid(oid):
            valid_oids.append(oid)
        else:
            invalid_oids.append(oid)
    
    return valid_oids, invalid_oids


def validate_ev_oids(ev_oids: List[str]) -> Tuple[List[str], List[str]]:
    """
    Validate a list of EV TLS OIDs.
    
    Args:
        ev_oids: List of OID strings to validate
        
    Returns:
        Tuple of (valid_oids, invalid_oids)
    """
    valid_oids = []
    invalid_oids = []
    
    for oid in ev_oids:
        if is_valid_ev_oid(oid):
            valid_oids.append(oid)
        else:
            invalid_oids.append(oid)
    
    return valid_oids, invalid_oids


def validate_update_json_oids(updates: List[dict]) -> dict:
    """
    Validate all OIDs in a trust store update JSON.
    
    Args:
        updates: List of update dictionaries
        
    Returns:
        Dictionary with validation results
    """
    results = {
        "valid": True,
        "errors": [],
        "warnings": [],
        "certificate_results": []
    }
    
    for i, update in enumerate(updates):
        cert_name = update.get("certificate_details", {}).get("common_name", f"Certificate {i+1}")
        cert_result = {
            "certificate": cert_name,
            "policy_constraints": {"valid": [], "invalid": []},
            "ev_oids": {"valid": [], "invalid": []}
        }
        
        # Validate policy constraints
        if "policy_constraints" in update:
            valid_policies, invalid_policies = validate_policy_constraints(update["policy_constraints"])
            cert_result["policy_constraints"]["valid"] = valid_policies
            cert_result["policy_constraints"]["invalid"] = invalid_policies
            
            if invalid_policies:
                results["valid"] = False
                results["errors"].append(f"{cert_name}: Invalid policy constraints: {invalid_policies}")
        
        # Validate EV OIDs
        if "ev_tls_oids" in update:
            valid_ev, invalid_ev = validate_ev_oids(update["ev_tls_oids"])
            cert_result["ev_oids"]["valid"] = valid_ev
            cert_result["ev_oids"]["invalid"] = invalid_ev
            
            if invalid_ev:
                results["valid"] = False
                results["errors"].append(f"{cert_name}: Invalid EV OIDs: {invalid_ev}")
        
        results["certificate_results"].append(cert_result)
    
    return results


if __name__ == "__main__":
    # Test the validation functions
    print("Testing OID validation functions...")
    
    # Test valid OIDs
    valid_oids = [
        "1.2.840.113635.100.1.123",
        "2.23.140.1.1",
        "1.3.6.1.4.1.6334.1.100.1"
    ]
    
    # Test invalid OIDs
    invalid_oids = [
        "invalid.oid.format",
        "1..2..3",
        "",
        "not-an-oid",
        "apple.policy.oid"
    ]
    
    print("\nValid OID format tests:")
    for oid in valid_oids:
        print(f"  {oid}: {is_valid_oid_format(oid)}")
    
    print("\nInvalid OID format tests:")
    for oid in invalid_oids:
        print(f"  {oid}: {is_valid_oid_format(oid)}")
    
    print("\nApple policy OID tests:")
    apple_oids = ["1.2.840.113635.100.1.123", "1.2.840.113635.100.1.999", "2.23.140.1.1"]
    for oid in apple_oids:
        print(f"  {oid}: {is_valid_apple_policy_oid(oid)}")
    
    print("\nEV OID tests:")
    ev_oids = ["2.23.140.1.1", "1.3.6.1.4.1.6334.1.100.1", "1.2.840.113635.100.1.123"]
    for oid in ev_oids:
        print(f"  {oid}: {is_valid_ev_oid(oid)}")
