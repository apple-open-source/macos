#!/usr/bin/env python3

import argparse
import json
import sys
import os
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives.hashes import SHA256
from cryptography.hazmat.primitives import serialization
import inquirer
from validate_update_json import validate_update_against_schema
from validate_update_json import readJson

def read_cert(cert_file):
    if not os.path.isfile(cert_file):
        raise ValueError("file \"" + cert_file + "\" does not exist")
    with open(cert_file,"r+b") as f:
        certData = f.read()
    try:
        cert = x509.load_der_x509_certificate(certData)
    except:
        try:
            cert = x509.load_pem_x509_certificate(certData)
        except:
            ValueError("cert \"" + filename + "\" does not parse")
    return cert


def spki_bytes(cert):
    key = cert.public_key()
    return key.public_bytes(
        encoding=serialization.Encoding.DER,
        format=serialization.PublicFormat.SubjectPublicKeyInfo
    )

def common_name(cert):
    name = cert.subject
    attrs = name.get_attributes_for_oid(NameOID.COMMON_NAME)
    if len(attrs) > 0:
        return attrs[0].value
    return None

def organizational_unit(cert):
    name = cert.subject
    attrs = name.get_attributes_for_oid(NameOID.ORGANIZATIONAL_UNIT_NAME)
    if len(attrs) > 0:
        return attrs[0].value
    return None

def genCertDetails(cert):
    details = {}
    details["sha256_fingerprint"] = cert.fingerprint(SHA256()).hex()
    details["spki"] = spki_bytes(cert).hex()
    cn = common_name(cert)
    if cn is not None:
        details["common_name"] = cn
    ou = organizational_unit(cert)
    if ou is not None:
        details["organizational_unit"] = ou
    details["not_before"] = cert.not_valid_before_utc.isoformat()
    details["not_after"] = cert.not_valid_after_utc.isoformat()
    details["pem"] = cert.public_bytes(encoding=serialization.Encoding.PEM).decode("utf-8")
    return details

def getUpdateDetails():
    questions = [
        inquirer.List('type', message="What change type?", choices=["Removal","Addition","Modification"],  default="Addition"),
        inquirer.List('anchor_type', message="What anchor type?", choices=["System","Custom","Platform","Test-System","Test-Platform"],  default="System"),
        inquirer.Text('reason', message="What is the change reason"),
        inquirer.Text('constraints', message="Policy constraints (oids separated by ',')?", default=None),
        inquirer.Checkbox('valid_policies',
                            message="Which allowed Valid policies?",
                            choices=["Server Authentication","Client Authentication","Code Signing","Timestamp Signing","Email Protection","Any"],
                            default=None),
        inquirer.Text('valid_not_before', message="Not Before for Valid?", default=None, ignore=lambda x: len(x['valid_policies']) == 0),
        inquirer.Text('valid_not_after', message="Not After for Valid?", default=None, ignore=lambda x: len(x['valid_policies']) == 0),
        inquirer.Text('valid_crl_url', message="Full CRL URL for Valid?", default=None, ignore=lambda x: len(x['valid_policies']) == 0),
        inquirer.Text('ev_tls_oids', message="EV Policy OIDs (separated by ',')?", default=None),
    ]

    answers = inquirer.prompt(questions)

    new_update_item = {}
    new_update_item["change_type"] = answers['type']
    new_update_item["anchor_type"] = answers['anchor_type']
    new_update_item["change_reason"] = answers['reason']
    if len(answers['constraints']) > 0:
        new_update_item["policy_constraints"] = answers['constraints'].split(sep=',')
    valid_props = {}
    if len(answers['valid_policies']) > 0:
        valid_props["allowed_policies"] = answers['valid_policies']
    if answers['valid_not_before'] is not None and len(answers['valid_not_before']) > 0:
        valid_props["not_before"] = answers['valid_not_before']
    if answers['valid_not_after'] is not None and len(answers['valid_not_after']) > 0:
        valid_props["not_after"] = answers['valid_not_after']
    if answers['valid_crl_url'] is not None and len(answers['valid_crl_url']) > 0:
        valid_props["full_crl_url"] = answers['valid_crl_url']

    if len(valid_props) > 0:
        new_update_item["valid"] = valid_props

    if len(answers['ev_tls_oids']) > 0:
        new_update_item["ev_tls_oids"] = answers['ev_tls_oids'].split(sep=',')

    return new_update_item

def main():
    parser = argparse.ArgumentParser(description="Create or modify a trust store update json",
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--srcroot', help="source root path (for schema)", required=True)
    parser.add_argument('--update_json', help="new or existing update json", required=True)
    parser.add_argument('--certificate', required=True)
    args = parser.parse_args()

    # read cert details
    cert = read_cert(args.certificate)
    cert_details = genCertDetails(cert)

    # get update details from user
    cert_name = cert.subject.rfc4514_string()
    bold_start = "\033[1m"
    bold_end = "\033[0m"
    print("Enter trust store update details for " + bold_start + cert_name + bold_end)

    new_update_item = getUpdateDetails()
    new_update_item["certificate_details"] = cert_details

    # validate existing updates file
    schema_file = args.srcroot + "/update_scripts/trust_store_updates_schema_v2.json"
    if os.path.isfile(args.update_json):
        validate_update_against_schema(args.update_json, schema_file)

    # read and add new entry
    try:
        updates = readJson(args.update_json)
    except:
        updates = []
    updates.append(new_update_item)

    # write updates
    with open(args.update_json, "w") as f:
        json.dump(updates, f, indent=4, separators=(',', ': '), sort_keys=True)

    # re-validate against schema
    if os.path.isfile(args.update_json):
        validate_update_against_schema(args.update_json, schema_file)

if __name__ == "__main__":
    main()
