#!/bin/sh -x -e
CA_SUBJECT="/C=US/O=Apple Inc./OU=Secure Transport Test CA"
SERVER_SUBJECT="/C=US/O=Apple Inc./OU=Secure Transport Test Server/CN=localhost"
TRUSTED_CLIENT_SUBJECT="/C=US/O=Apple Inc./OU=Secure Transport Test Client (Trusted)/CN=localhost"
UNTRUSTED_CLIENT_SUBJECT="/C=US/O=Apple Inc./OU=Secure Transport Test Client (Untrusted)/CN=localhost"

openssl req -x509 -newkey rsa:1024 -sha1 -days 3650  -subj "$CA_SUBJECT" -nodes -keyout ca_key.pem  -out ca_cert.pem
openssl req -x509 -newkey rsa:1024 -sha1 -days 3650  -subj "$UNTRUSTED_CLIENT_SUBJECT" -nodes -keyout untrusted_client_key.pem  -out untrusted_client_cert.pem
openssl x509 -in cert-1.pem -out cert-1.der -outform DER
openssl rsa -in privkey-1.pem -out privkey-1.der -outform DER
openssl pkcs12 -export -passout pass:password -out identity-1.p12 -inkey privkey-1.pem -in cert-1.pem

xxd -i privkey-1.der privkey-1.h
xxd -i cert-1.der cert-1.h
xxd -i identity-1.p12 identity-1.h
