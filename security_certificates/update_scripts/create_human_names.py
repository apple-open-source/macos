#!/usr/bin/env python3

## Poorly written script to fill in a hash to human name mapping, just the once.
## But leaving the breadcrumb here in case we ever need need to re-run it or a modified version of it.

import os
import json
from cryptography import x509
from cryptography.hazmat.primitives.hashes import SHA256

system_roots_path = "/Volumes/src/security_certificates/certificates/roots/"
custom_roots_path = "/Volumes/src/security_certificates/certificates/custom/"
platform_roots_path = "/Volumes/src/security_certificates/certificates/platform/"
human_map_file = "/Volumes/src/security_certificates/certificates/hash_to_human_name.json"

human_map = {}
dirs = [system_roots_path, custom_roots_path, platform_roots_path]
for dir in dirs:
    for filename in os.listdir(dir):
        file_path = os.path.join(dir, filename)

        with open(file_path,"r+b") as f:
            certData = f.read()

        try:
            cert = x509.load_der_x509_certificate(certData)
        except:
            ValueError(file_path + " does not parse")
        certHash = cert.fingerprint(SHA256()).hex().upper()
        humanName = cert.subject.rfc4514_string()
        human_map[certHash] = humanName

with open(human_map_file, "w") as f:
    json.dump(human_map, f, indent=4, separators=(',', ': '), sort_keys=True)
