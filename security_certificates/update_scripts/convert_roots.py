#!/usr/bin/env python3

## Poorly written script to convert the old root files to the fingerprint-hash style, just the once.
## But leaving the breadcrumb here in case we ever need need to re-run it or a modified version of it.

import os
from cryptography import x509
from cryptography.hazmat.primitives.hashes import SHA256

directory_path = "/Volumes/src/security_certificates/certificates/roots/"

for filename in os.listdir(directory_path):
    file_path = os.path.join(directory_path, filename)

    with open(file_path,"r+b") as f:
        certData = f.read()

    cert = x509.load_der_x509_certificate(certData)
    certHash = cert.fingerprint(SHA256()).hex().upper()
    new_file_path = directory_path + certHash + ".cer"
    os.rename(file_path, new_file_path)

