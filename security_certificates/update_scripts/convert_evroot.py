#!/usr/bin/env python3

## Poorly written script to convert the old EV config file to the new json style, just the once
## But leaving the breadcrumb here in case we ever need need to re-run it or a modified version of it.

import os
import re
import json
from cryptography import x509
from cryptography.hazmat.primitives.hashes import SHA1

srcroot = "/Volumes/src/security_certificates/"
evroot_config_file = srcroot + "certificates/evroot.config"
rename_file = srcroot + "update_scripts/renames.txt"

def read_cert(cert_file):
    if not os.path.isfile(cert_file):
        raise ValueError("file \"" + cert_file + "\" does not exist")
    with open(cert_file,"r+b") as f:
        certData = f.read()
    try:
        cert = x509.load_der_x509_certificate(certData)
    except:
        ValueError("cert \"" + filename + "\" does not parse")
    return cert

def sha1_fingerprint(cert_file):
    cert = read_cert(cert_file)
    return cert.fingerprint(SHA1()).hex().upper()

# follow the root renames (pulled from the git history)
renames = {}
with open(rename_file, "r") as f:
    for line in f:
        match = re.search('diff --git a\/certificates\/roots\/([a-zA-Z0-9-_. ]+) b\/certificates\/roots\/([A-F0-9]+.cer)', line)
        if match:
            renames[match.group(1)] = match.group(2)

# create new ev config indexed by root hash (which we'll convert to indexed by OID during build)
ev_config = {}
hash_map = {}
with open(evroot_config_file, "r") as f:
    all_roots = set()
    for line in f:
        if line.startswith('#'): # skip comments
            continue
        if line.isspace(): # skip whitespace lines
            continue
        # find the OID and cert neams
        match = re.search('([0-9.]+) ([\"A-Za-z0-9-_. \"]+)', line)
        if match:
            oid = match.group(1)
            root_name_string = match.group(2)
            # get each root cert name
            roots = re.findall('\"([A-Za-z0-9-_. ]+)\"', root_name_string)
            for root in roots:
                # folow the cert rename
                renamed_root = renames[root]
                # get SHA-1 cert hash (as in existing ev file)
                sha1_hash = sha1_fingerprint(srcroot + "certificates/roots/" + renamed_root)
                sha2_hash = renamed_root.removesuffix('.cer')
                all_roots.add(hash)
                if sha2_hash in ev_config:
                    if oid != "2.23.140.1.1":
                        ev_config[sha2_hash].append(oid)
                else:
                    ev_config[sha2_hash] = ["2.23.140.1.1"]
                    if oid != "2.23.140.1.1":
                        ev_config[sha2_hash].append(oid)
                hash_map[sha2_hash] = sha1_hash

ev_roots = {}
ev_roots["fingerprint_map"] = hash_map
ev_roots["EV_config"] = ev_config

with open(srcroot + "certificates/EVRoots.json", "w") as f:
    json.dump(ev_roots, f, indent=4, separators=(',', ': '), sort_keys=True)
