#!/bin/sh -x -e
SUBJECT="/C=US/O=Apple Inc./OU=CoreOS Platform Security/CN=localhost"

openssl req -x509 -newkey rsa:1024 -sha1 -days 3650  -subj "$SUBJECT" -nodes -keyout privkey-1.pem  -out cert-1.pem
openssl x509 -in cert-1.pem -out cert-1.der -outform DER
openssl rsa -in privkey-1.pem -out privkey-1.der -outform DER
openssl pkcs12 -export -passout pass:password -out identity-1.p12 -inkey privkey-1.pem -in cert-1.pem

xxd -i privkey-1.der privkey-1.h
xxd -i cert-1.der cert-1.h
xxd -i identity-1.p12 identity-1.h
