#!/bin/sh

#  PreSecurityTest.sh
#  Security
#
#  Created by Fabrice Gautier on 6/7/11.
#  Copyright 2011 Apple, Inc. All rights reserved.

echo "PreSecuritTests.sh: pre-run script"

# Use this for macport install of gnutls:
#GNUTLS_DIR=/opt/local

# Use this if you compiled your own gnutls:
GNUTLS_DIR=/usr/local

# System openssl
#OPENSSL_DIR=/usr

# Macport  openssl
OPENSSL_DIR=/opt/local

# your own openssl
#OPENSSL_DIR=/usr/local


echo "Starting servers"

echo "openssl s_server RSA/RSA..."
${OPENSSL_DIR}/bin/openssl s_server -accept 4445 -state -key test-certs/ServerKey.rsa.pem -cert test-certs/ServerCert.rsa.rsa.pem -www -cipher ALL:eNULL > /tmp/s_server.rsa.rsa.log 2>&1 &

echo "openssl s_server RSA/ECC..."
${OPENSSL_DIR}/bin/openssl s_server -accept 4446 -state -key test-certs/ServerKey.rsa.pem -cert test-certs/ServerCert.rsa.ecc.pem -www -cipher ALL:eNULL > /tmp/s_server.rsa.ecc.log 2>&1 &

echo "openssl s_server ECC/RSA..."
${OPENSSL_DIR}/bin/openssl s_server -accept 4447 -state -key test-certs/ServerKey.ecc.pem -cert test-certs/ServerCert.ecc.rsa.pem -www -cipher ALL:eNULL > /tmp/s_server.ecc.rsa.log 2>&1 &

echo "openssl s_server ECC/ECC..."
${OPENSSL_DIR}/bin/openssl s_server -accept 4448 -state -key test-certs/ServerKey.ecc.pem -cert test-certs/ServerCert.ecc.ecc.pem -www -cipher ALL:eNULL > /tmp/s_server.ecc.ecc.log 2>&1 &

echo "gnutls-serv RSA/RSA..."
${GNUTLS_DIR}/bin/gnutls-serv -p 5556 -d 4 --http --x509keyfile test-certs/ServerKey.rsa.pem --x509certfile test-certs/ServerCert.rsa.rsa.pem  --priority "NORMAL:+ANON-DH:+NULL" > /tmp/gnutls-serv.rsa.rsa.log 2>&1 &

echo "gnutls-serv RSA/ECC..."
${GNUTLS_DIR}/bin/gnutls-serv -p 5557 -d 4 --http --x509keyfile test-certs/ServerKey.rsa.pem --x509certfile test-certs/ServerCert.rsa.ecc.pem  --priority "NORMAL:+ANON-DH:+NULL" > /tmp/gnutls-serv.rsa.ecc.log 2>&1 &

echo "gnutls-serv ECC/RSA..."
${GNUTLS_DIR}/bin/gnutls-serv -p 5558 -d 4 --http --x509keyfile test-certs/ServerKey.ecc.pem --x509certfile test-certs/ServerCert.ecc.rsa.pem  --priority "NORMAL:+ANON-DH:+NULL" > /tmp/gnutls-serv.ecc.rsa.log 2>&1 &

echo "gnutls-serv ECC/ECC..."
${GNUTLS_DIR}/bin/gnutls-serv -p 5559 -d 4 --http --x509keyfile test-certs/ServerKey.ecc.pem --x509certfile test-certs/ServerCert.ecc.ecc.pem  --priority "NORMAL:+ANON-DH:+NULL" > /tmp/gnutls-serv.ecc.ecc.log 2>&1 &


echo "tcprelay..."
/usr/local/bin/tcprelay localhost:4445 localhost:4446 localhost:4447 localhost:4448  localhost:5556 localhost:5557 localhost:5558 localhost:5559  > tcprelay.log 2>&1 &
echo $! > /tmp/tcprelay.pid
cat /tmp/tcprelay.pid

echo "Logs in $DIR"
