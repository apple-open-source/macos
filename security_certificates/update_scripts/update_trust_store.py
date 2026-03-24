#!/usr/bin/env python3

import argparse
import json
import os
import re
import sys
import plistlib
from datetime import datetime

from validate_update_json import validate_update_against_schema
from validate_update_json import readJson
from validate_update_json import readPlist
from oid_validation import validate_update_json_oids

from cryptography import x509
from cryptography.hazmat.primitives.hashes import SHA256
from cryptography.hazmat.primitives.hashes import SHA1
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.padding import PKCS1v15
from cryptography.x509 import SignatureAlgorithmOID

### WARNING: Not all possible cases have been tested, so if you encounter a case
### that doesn't seem to produce the diff you expected, please continue to improve
### this script. Let's all agree not to "just do it by hand"!

# verify_directly_issued_by does not support sha-1 signatures, but we
# do support them for anchors, so this fallback is used to check issuer/subject
# and the sha-1 signature
def legacy_verify_directly_issued_by(cert, ca_cert):
    if cert.issuer != ca_cert.subject:
        return False
    if cert.signature_algorithm_oid == SignatureAlgorithmOID.RSA_WITH_SHA1:
        try:
            ca_cert.public_key().verify(
                cert.signature, cert.tbs_certificate_bytes, PKCS1v15(), cert.signature_hash_algorithm
            )
        except:
            return False
        return True
    return False

def isSelfSigned(cert):
    try:
        cert.verify_directly_issued_by(cert)
    except:
        return legacy_verify_directly_issued_by(cert,cert)
    return True

def getCertFromUpdate(update):
    certDetails = update["certificate_details"]
    certPem = certDetails["pem"]
    certData = certPem.encode('utf-8')
    try:
        cert = x509.load_pem_x509_certificate(certData)
    except:
        ValueError("cert with fingerprint \"" + update["sha256_fingerprint"] + "\" does not parse")
    return cert

def certHash(cert):
    return cert.fingerprint(SHA256()).hex().upper()

def writeCert(cert, destDir):
    certFilename = destDir + certHash(cert) + ".cer"
    certData = cert.public_bytes(encoding=serialization.Encoding.DER)

    with open(certFilename,"w+b") as f:
        f.write(certData)

def updateConstraints(srcroot, dry_run, certHash, anchorType, newConstraints):
    constraints_file = srcroot + "/certificates/constraints.json"

    all_constraints = readJson(constraints_file)
    constraints = all_constraints[anchorType.lower()]

    if newConstraints is None:
        if certHash in constraints:
            print("Deleting constraints")
            del constraints[certHash]
    else:
        if certHash not in constraints:
            print("Adding constraints: " + str(newConstraints))
        else:
            print("Replacing constraints: " + str(newConstraints))
        constraints[certHash] = newConstraints

    all_constraints[anchorType.lower()] = constraints
    if not dry_run:
        with open(constraints_file, "w") as f:
            json.dump(all_constraints, f, indent=4, separators=(',', ': '), sort_keys=True)

def updateEVRoots(srcroot, dry_run, cert, new_ev_oids):
    evroots_file = srcroot + "/certificates/EVRoots.json"
    evroots = readJson(evroots_file)

    # the EVRoots.plist we need to construct during build uses the SHA1 fingerprint,
    # so the json has a map between the two fingerprints (for ease during build step)
    sha1_hash = cert.fingerprint(SHA1()).hex().upper()
    sha2_hash = certHash(cert)
    ev_config = evroots["EV_config"]
    fingerprint_map = evroots["fingerprint_map"]

    if new_ev_oids is None:
        if sha2_hash in ev_config:
            print("Deleting ev oids")
            del ev_config[sha2_hash]
        if sha2_hash in fingerprint_map:
            del fingerprint_map[sha2_hash]
    else:
        if sha2_hash in ev_config:
            print("Adding EV OIDs: " + str(new_ev_oids))
        else:
            print("Replacing EV OIDs: " + str(new_ev_oids))
        ev_config[sha2_hash] = new_ev_oids
        fingerprint_map[sha2_hash] = sha1_hash

    evroots["fingerprint_map"] = fingerprint_map
    evroots["EV_config"] = ev_config

    if not dry_run:
        with open(evroots_file, "w") as f:
            json.dump(evroots, f, indent=4, separators=(',', ': '), sort_keys=True)

def update_human_map(srcroot, cert):
    human_map_file = srcroot + "/certificates/hash_to_human_name.json"
    human_map = readJson(human_map_file)

    human_map[certHash(cert)] = cert.subject.rfc4514_string()

    with open(human_map_file, "w") as f:
        json.dump(human_map, f, indent=4, separators=(',', ': '), sort_keys=True)

def add(srcroot, dry_run, update, cert):
    anchorType = update["anchor_type"]
    print("Adding as " + anchorType + " anchor")

    custom_dir_path = srcroot + "/certificates/custom/"
    platform_dir_path = srcroot + "/certificates/platform/"
    roots_dir_path = srcroot + "/certificates/roots/"
    test_roots_dir_path = srcroot + "/certificates/test-roots/"
    test_platform_dir_path = srcroot + "/certificates/test-platform/"
    test_custom_dir_path = srcroot + "/certificates/test-custom/"

    if not dry_run:
        update_human_map(srcroot, cert)
        if anchorType == "System":
            writeCert(cert, roots_dir_path)
        elif anchorType == "Custom":
            writeCert(cert, custom_dir_path)
        elif anchorType == "Platform":
            writeCert(cert, platform_dir_path)
        elif anchorType == "Test-System":
            writeCert(cert, test_roots_dir_path)
        elif anchorType == "Test-Platform":
            writeCert(cert, test_platform_dir_path)
        elif anchorType == "Test-Custom":
            writeCert(cert, test_custom_dir_path)

    if "policy_constraints" in update:
        updateConstraints(srcroot, dry_run, certHash(cert), anchorType, update["policy_constraints"])

    if "ev_tls_oids" in update:
        updateEVRoots(srcroot, dry_run, cert, update["ev_tls_oids"])

def remove(srcroot, dry_run, update, cert):
    removed_dir_path = srcroot + "/certificates/removed/"
    destDir = removed_dir_path + certHash(cert) + ".cer"
    if not isSelfSigned(cert):
        print("Not a root, so removing to \"intermediates\"")
        destDir = removed_dir_path + "intermediates/" + certHash(cert) + ".cer"

    custom_dir_path = srcroot + "/certificates/custom/"
    platform_dir_path = srcroot + "/certificates/platform/"
    roots_dir_path = srcroot + "/certificates/roots/"
    test_roots_dir_path = srcroot + "/certificates/test-roots/"
    test_platform_dir_path = srcroot + "/certificates/test-platform/"
    test_custom_dir_path = srcroot + "/certificates/test-custom/"

    filename = certHash(cert) + ".cer"
    if os.path.isfile(platform_dir_path + filename):
        print("Removing from platform anchors")
        if not dry_run:
            os.rename(platform_dir_path + filename, destDir)
        updateConstraints(srcroot, dry_run, certHash(cert), "Platform", None)
    if os.path.isfile(custom_dir_path + filename):
        print("Removing from custom anchors")
        if not dry_run:
            os.rename(custom_dir_path + filename, destDir)
        updateConstraints(srcroot, dry_run, certHash(cert), "Custom", None)
    if os.path.isfile(roots_dir_path + filename):
        print("Removing from system anchors")
        if not dry_run:
            os.rename(roots_dir_path + filename, destDir)
        updateConstraints(srcroot, dry_run, certHash(cert), "System", None)
    if os.path.isfile(test_platform_dir_path + filename):
        print("Removing from test platform anchors")
        if not dry_run:
            os.rename(platform_dir_path + filename, destDir)
        updateConstraints(srcroot, dry_run, certHash(cert), "Test-Platform", None)
    if os.path.isfile(test_roots_dir_path + filename):
        print("Removing from test system anchors")
        if not dry_run:
            os.rename(test_roots_dir_path + filename, destDir)
        updateConstraints(srcroot, dry_run, certHash(cert), "Test-System", None)
    if os.path.isfile(test_custom_dir_path + filename):
        print("Removing from test custom anchors")
        if not dry_run:
            os.rename(test_custom_dir_path + filename, destDir)
        updateConstraints(srcroot, dry_run, certHash(cert), "Test-Custom", None)

    updateEVRoots(srcroot, dry_run, cert, None)

def modify(srcroot, dry_run, update, cert):
    anchorType = update["anchor_type"]
    if "policy_constraints" in update:
        updateConstraints(srcroot, dry_run, certHash(cert), anchorType, update["policy_constraints"])

    if "ev_tls_oids" in update:
        updateEVRoots(srcroot, dry_run, cert, update["ev_tls_oids"])

def generate_new_trust_store_version():
    """Generate a new trust store version based on current date (YYYYMMDDXX format)"""
    now = datetime.now()
    base_version = now.strftime("%Y%m%d")
    return int(base_version + "00")  # XX starts at 00 for each day

def increment_asset_version(current_version, override_version=None):
    """Increment the PKITrustStoreAssetsVersion by 1 on the last number after the last dot, or use override"""
    if override_version:
        return override_version

    version_parts = current_version.split('.')
    if len(version_parts) > 1:
        # Increment the last part
        last_part = int(version_parts[-1]) + 1
        version_parts[-1] = str(last_part)
        return '.'.join(version_parts)
    else:
        # If no dots, just increment the whole number
        return str(int(current_version) + 1)

def update_asset_version_plist(srcroot, dry_run, override_asset_version=None):
    """Update AssetVersion.plist with new VersionNumber and incremented PKITrustStoreAssetsVersion"""
    asset_version_file = srcroot + "/config/AssetVersion.plist"

    try:
        asset_version_plist = readPlist(asset_version_file)
    except FileNotFoundError as e:
        if dry_run:
            print(f"Warning: {asset_version_file} not found, would skip AssetVersion.plist update")
            return None, None
        else:
            raise FileNotFoundError(f"Required file {asset_version_file} not found") from e

    # Generate new trust store version
    new_trust_store_version = generate_new_trust_store_version()
    old_trust_store_version = asset_version_plist.get("VersionNumber", 0)

    # Increment asset version
    current_asset_version = asset_version_plist.get("PKITrustStoreAssetsVersion", "1.0.0")
    new_asset_version = increment_asset_version(current_asset_version, override_asset_version)

    print(f"Updating trust store version: {old_trust_store_version} -> {new_trust_store_version}")
    print(f"Updating asset version: {current_asset_version} -> {new_asset_version}")

    if dry_run:
        print(f"[DRY RUN] Would update {asset_version_file}:")
        print(f"  VersionNumber: {old_trust_store_version} -> {new_trust_store_version}")
        print(f"  PKITrustStoreAssetsVersion: {current_asset_version} -> {new_asset_version}")
    else:
        asset_version_plist["VersionNumber"] = new_trust_store_version
        asset_version_plist["PKITrustStoreAssetsVersion"] = new_asset_version

        with open(asset_version_file, 'wb') as f:
            plistlib.dump(asset_version_plist, f)

    return new_trust_store_version, new_asset_version

def update_info_asset_plist(srcroot, dry_run, new_trust_store_version, new_asset_version):
    """Update Info-Asset.plist with new versions"""
    info_asset_file = srcroot + "/config/Info-Asset.plist"
    new_ma_asset_version = new_asset_version + ".0.0,0"

    try:
        info_asset_plist = readPlist(info_asset_file)
    except FileNotFoundError as e:
        if dry_run:
            print(f"Warning: {info_asset_file} not found, would skip Info-Asset.plist update")
            return
        else:
            raise FileNotFoundError(f"Required file {info_asset_file} not found") from e

    if dry_run:
        print(f"[DRY RUN] Would update {info_asset_file}:")
        print(f"  CFBundleShortVersionString: {info_asset_plist.get('CFBundleShortVersionString', 'N/A')} -> {new_asset_version}")
        print(f"  CFBundleVersion: {info_asset_plist.get('CFBundleVersion', 'N/A')} -> {new_asset_version}")

        mobile_asset_props = info_asset_plist.get("MobileAssetProperties", {})
        print(f"  MobileAssetProperties/AssetVersion: {mobile_asset_props.get('AssetVersion', 'N/A')} -> {new_ma_asset_version}")
        print(f"  MobileAssetProperties/ContentVersion: {mobile_asset_props.get('ContentVersion', 'N/A')} -> {new_trust_store_version}")
    else:
        # Update CFBundleShortVersionString and CFBundleVersion
        info_asset_plist["CFBundleShortVersionString"] = new_asset_version
        info_asset_plist["CFBundleVersion"] = new_asset_version

        # Update MobileAssetProperties
        if "MobileAssetProperties" not in info_asset_plist:
            info_asset_plist["MobileAssetProperties"] = {}

        mobile_asset_props = info_asset_plist["MobileAssetProperties"]
        mobile_asset_props["AssetVersion"] = new_ma_asset_version
        mobile_asset_props["ContentVersion"] = new_trust_store_version

        print(f"Updated Info-Asset.plist CFBundleShortVersionString: {new_asset_version}")
        print(f"Updated Info-Asset.plist CFBundleVersion: {new_asset_version}")
        print(f"Updated Info-Asset.plist MobileAssetProperties/AssetVersion: {new_ma_asset_version}")
        print(f"Updated Info-Asset.plist MobileAssetProperties/ContentVersion: {new_trust_store_version}")

        with open(info_asset_file, 'wb') as f:
            plistlib.dump(info_asset_plist, f)

def update_security_certificates_xcconfig(srcroot, dry_run, new_trust_store_version):
    """Update security_certificates.xcconfig with new TRUST_STORE_VERSION"""
    xcconfig_file = srcroot + "/config/security_certificates.xcconfig"

    try:
        with open(xcconfig_file, 'r') as f:
            content = f.read()
    except FileNotFoundError as e:
        if dry_run:
            print(f"Warning: {xcconfig_file} not found, would skip xcconfig update")
            return
        else:
            raise FileNotFoundError(f"Required file {xcconfig_file} not found") from e

    # Update TRUST_STORE_VERSION
    pattern = r'^TRUST_STORE_VERSION\s*=\s*\d+$'
    new_line = f"TRUST_STORE_VERSION = {new_trust_store_version}"

    # Find current value for dry run output
    import re
    match = re.search(pattern, content, flags=re.MULTILINE)
    current_value = match.group() if match else "TRUST_STORE_VERSION = <not found>"

    if dry_run:
        print(f"[DRY RUN] Would update {xcconfig_file}:")
        print(f"  {current_value} -> {new_line}")
    else:
        updated_content = re.sub(pattern, new_line, content, flags=re.MULTILINE)
        print(f"Updated security_certificates.xcconfig TRUST_STORE_VERSION: {new_trust_store_version}")

        with open(xcconfig_file, 'w') as f:
            f.write(updated_content)

def update_version_files(srcroot, dry_run, override_asset_version=None):
    """Update all version-related files"""
    if dry_run:
        print("[DRY RUN] Would update version files:")
    else:
        print("Updating version files...")

    # Update AssetVersion.plist and get new versions
    new_trust_store_version, new_asset_version = update_asset_version_plist(srcroot, dry_run, override_asset_version)

    if new_trust_store_version is None or new_asset_version is None:
        error_msg = "Failed to update AssetVersion.plist, cannot proceed with version updates"
        if dry_run:
            print(f"[DRY RUN] {error_msg}")
            return
        else:
            raise RuntimeError(error_msg)

    # Update Info-Asset.plist
    update_info_asset_plist(srcroot, dry_run, new_trust_store_version, new_asset_version)

    # Update security_certificates.xcconfig
    update_security_certificates_xcconfig(srcroot, dry_run, new_trust_store_version)

def validate_update_json(srcroot, update_json_file):
    # Validate file against json schema
    schema_file = srcroot + "/update_scripts/trust_store_updates_schema_v2.json"
    validate_update_against_schema(update_json_file, schema_file)
    print("✅ schema validation completed successfully!")

    updates = readJson(update_json_file)

    # Validate oids in update json
    oid_validation_result = validate_update_json_oids(updates)
    if not oid_validation_result["valid"]:
        print("\\033[91mOID Validation Errors:\\033[0m")
        for error in oid_validation_result["errors"]:
            print(f"  ❌ {error}")
        print("\\nPlease fix the OID validation errors before proceeding.")
        sys.exit(1)

    if oid_validation_result["warnings"]:
        print("\\033[93mOID Validation Warnings:\\033[0m")
        for warning in oid_validation_result["warnings"]:
            print(f"  ⚠️  {warning}")

    # Show detailed validation results for each certificate
    if oid_validation_result["certificate_results"]:
        print("\\nOID Validation Results:")
        for cert_result in oid_validation_result["certificate_results"]:
            cert_name = cert_result["certificate"]
            print(f"📋 {cert_name}:")

            # Policy constraints results
            policy_valid = cert_result["policy_constraints"]["valid"]
            policy_invalid = cert_result["policy_constraints"]["invalid"]
            if policy_valid:
                print(f"  ✅ Valid policy constraints: {policy_valid}")
            if policy_invalid:
                print(f"  ❌ Invalid policy constraints: {policy_invalid}")

            # EV OIDs results
            ev_valid = cert_result["ev_oids"]["valid"]
            ev_invalid = cert_result["ev_oids"]["invalid"]
            if ev_valid:
                print(f"  ✅ Valid EV OIDs: {ev_valid}")
            if ev_invalid:
                print(f"  ❌ Invalid EV OIDs: {ev_invalid}")

    print("✅ OID validation completed successfully!")

    return updates

def main():
    parser = argparse.ArgumentParser(description="Update the certificates and constraints json",
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--update_json', help="json describing updates", required=True)
    parser.add_argument('--srcroot', help="source root path", required=True)
    parser.add_argument('--dry_run', action='store_true', help="run without modifying trust store")
    parser.add_argument('--no_version_update', action='store_true', help="skip automatic version updates")
    parser.add_argument('--asset_version', help="override asset version (e.g., '2.1.0') instead of auto-incrementing")

    args = parser.parse_args()

    print("Validating input update json...")
    updates = validate_update_json(args.srcroot, args.update_json)

    print("----------------------\n")

    # Track if any changes would be made that require version updates
    # For dry_run, we consider all operations as "changes made" to show version updates
    changes_made = False

    for update in updates:
        type = update["change_type"]
        cert = getCertFromUpdate(update)

        print("Processing update for \033[1m" + cert.subject.rfc4514_string() + "\033[0m:")

        if type == "Addition":
            add(args.srcroot, args.dry_run, update, cert)
            changes_made = True
        elif type == "Modification":
            modify(args.srcroot, args.dry_run, update, cert)
            changes_made = True
        elif type == "Removal":
            remove(args.srcroot, args.dry_run, update, cert)
            changes_made = True

        print("----------------------\n")

    # Update version files if changes were made and not explicitly disabled
    if changes_made and not args.no_version_update:
        update_version_files(args.srcroot, args.dry_run, args.asset_version)
        if args.dry_run:
            print("[DRY RUN] Version files would be updated automatically")
        else:
            print("Version files updated automatically")
    elif args.no_version_update:
        if args.dry_run:
            print("[DRY RUN] Would skip version updates (--no_version_update specified)")
        else:
            print("Skipping version updates (--no_version_update specified)")
    else:
        if args.dry_run:
            print("[DRY RUN] No changes would be made, would skip version updates")
        else:
            print("No changes made, skipping version updates")

if __name__ == "__main__":
    main()
