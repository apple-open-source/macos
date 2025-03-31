#!/usr/bin/env python3

import argparse
import json
from validate_update_json import validate_update_against_schema
from validate_update_json import validate_update_against_version
from validate_update_json import readJson
from validate_update_json import readPlist
import base64
import hashlib
import os
from cryptography import x509

def isSelfSigned(certData):
    try:
        cert = x509.load_der_x509_certificate(certData)
        return cert.verify_directly_issued_by(cert)
    except:
        return False


parser = argparse.ArgumentParser(description="Update the certificates and constraints json",
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('--operations', help="json describing updates", required=True)
parser.add_argument('--srcroot', help="source root path", required=True)

args = parser.parse_args()

schema_file = args.srcroot + "/update_scripts/root_store_updates_schema.json"
validate_update_against_schema(args.operations, schema_file)

version_file = args.srcroot + "/config/AssetVersion.plist"
validate_update_against_version(args.operations, version_file)

constraints_file = args.srcroot + "/certificates/constraints.json"
updates = readJson(args.operations)
constraints = readJson(constraints_file)

custom_dir_path = args.srcroot + "/certificates/custom/"
platform_dir_path = args.srcroot + "/certificates/platform/"
removed_dir_path = args.srcroot + "/certificates/removed/"
roots_dir_path = args.srcroot + "/certificates/roots/"

for anchor in updates["anchors"]:
    b64cert = anchor["anchorCertificate"]
    cert = base64.b64decode(b64cert)
    certHash = hashlib.sha256(cert).digest()
    certHashStr = certHash.hex().upper()

    op = anchor["operation"]
    if "remove" in op:
        sourceDir = roots_dir_path + certHashStr + ".cer"
        destDir = removed_dir_path + certHashStr + ".cer"
        if not isSelfSigned(cert):
            destDir = removed_dir_path + "intermediates/" + certHashStr + ".cer"
        if constraints[certHashStr]:
            sourceDir = custom_dir_path + certHashStr + ".cer"
            del constraints[certHashStr]
        os.rename(sourceDir, destDir)
        #TODO: radar to convert ev config to json and do like constraints json

    elif "add" in op:
        add_op = op["add"]
        destDir = roots_dir_path
        if add_op["anchorType"] == "system":
            if add_op["evOids"]:
                raise ValueError("EV updates unimplemented")
        elif add_op["anchorType"] == "custom":
            destDir = custom_dir_path
            if add_op["constraints"] is None:
                raise ValueError("Custom anchors must specify constraints")
            constraints[certHashStr] = add_op["constraints"]
        elif add_op["anchorType"] == "platform":
            destDir = platform_dir_path
            if add_op["constraints"]:
                constraints[certHashStr] = add_op["constraints"]
        else:
            raise ValueError("Unsupported anchor type" + anchor["operation"]["anchorType"])

        certFilename = destDir + certHashStr + ".cer"
        with open(certFilename,"w+b") as f:
            f.write(cert)
    else:
        raise ValueError("unknown operation " + anchor["operation"])

with open(constraints_file, "w") as f:
    json.dump(constraints, f, indent=4, separators=(',', ': '), sort_keys=True)

#TODO: update trust store version
