#!/usr/bin/env python3

import argparse
import json
import os

from validate_update_json import validate_update_against_schema
from validate_update_json import readJson
from validate_update_json import readPlist

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

    updateEVRoots(srcroot, dry_run, cert, None)

def modify(srcroot, dry_run, update, cert):
    anchorType = update["anchor_type"]
    if "policy_constraints" in update:
        updateConstraints(srcroot, dry_run, certHash(cert), anchorType, update["policy_constraints"])

    if "ev_tls_oids" in update:
        updateEVRoots(srcroot, dry_run, cert, update["ev_tls_oids"])

def main():
    parser = argparse.ArgumentParser(description="Update the certificates and constraints json",
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--update_json', help="json describing updates", required=True)
    parser.add_argument('--srcroot', help="source root path", required=True)
    parser.add_argument('--dry_run', action='store_true', help="run without modifying trust store")

    args = parser.parse_args()

    schema_file = args.srcroot + "/update_scripts/trust_store_updates_schema_v2.json"
    validate_update_against_schema(args.update_json, schema_file)

    updates = readJson(args.update_json)

    print("----------------------\n")
    for update in updates:
        type = update["change_type"]
        cert = getCertFromUpdate(update)

        print("Processing update for \033[1m" + cert.subject.rfc4514_string() + "\033[0m:")

        if type == "Addition":
            add(args.srcroot, args.dry_run, update, cert)
        elif type == "Modification":
            modify(args.srcroot, args.dry_run, update, cert)
        elif type == "Removal":
            remove(args.srcroot, args.dry_run, update, cert)

        print("----------------------\n")

if __name__ == "__main__":
    main()


#TODO: update trust store version and asset info automagically!
# AssetVersion.plist:
#   VersionNumber to date
#   PKITrustStoreAssetsVersion +1 on last # after last .
    # TODO: What should cause us to change major, minor, and minor-minor versions??
# Info-Asset.plist
#   CFBundleShortVersionString to new asset version
#   CFBundleVersion to new asset version
#   MobileAssetProperties/AssetVersion to new asset version
#   MobileAssetProperties/ContentVersion to trust store version
# security_certificates.xcconfig
#   TRUST_STORE_VERSION to strust store version
