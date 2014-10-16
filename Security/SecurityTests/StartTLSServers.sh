#!/bin/sh

#  StartTLSServers.sh
#  Security
#
#  Created by Fabrice Gautier on 6/7/11.
#  Copyright 2011 Apple, Inc. All rights reserved.

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
${OPENSSL_DIR}/bin/openssl s_server -accept 4001 -state -key test-certs/ServerKey.rsa.pem -cert test-certs/ServerCert.rsa.rsa.pem -www -cipher ALL:eNULL > /tmp/s_server.rsa.rsa.log 2>&1 &

echo "openssl s_server RSA/ECC..."
${OPENSSL_DIR}/bin/openssl s_server -accept 4002 -state -key test-certs/ServerKey.rsa.pem -cert test-certs/ServerCert.rsa.ecc.pem -www -cipher ALL:eNULL > /tmp/s_server.rsa.ecc.log 2>&1 &

echo "openssl s_server ECC/RSA..."
${OPENSSL_DIR}/bin/openssl s_server -accept 4003 -state -key test-certs/ServerKey.ecc.pem -cert test-certs/ServerCert.ecc.rsa.pem -www -cipher ALL:eNULL > /tmp/s_server.ecc.rsa.log 2>&1 &

echo "openssl s_server ECC/ECC..."
${OPENSSL_DIR}/bin/openssl s_server -accept 4004 -state -key test-certs/ServerKey.ecc.pem -cert test-certs/ServerCert.ecc.ecc.pem -www -cipher ALL:eNULL > /tmp/s_server.ecc.ecc.log 2>&1 &

echo "gnutls-serv RSA/RSA..."
${GNUTLS_DIR}/bin/gnutls-serv -p 5001 -d 4 --http -a --x509keyfile test-certs/ServerKey.rsa.pem --x509certfile test-certs/ServerCert.rsa.rsa.pem  --priority "NORMAL:+ANON-DH:+NULL" > /tmp/gnutls-serv.rsa.rsa.log 2>&1 &

echo "gnutls-serv RSA/ECC..."
${GNUTLS_DIR}/bin/gnutls-serv -p 5002 -d 4 --http -a --x509keyfile test-certs/ServerKey.rsa.pem --x509certfile test-certs/ServerCert.rsa.ecc.pem  --priority "NORMAL:+ANON-DH:+NULL" > /tmp/gnutls-serv.rsa.ecc.log 2>&1 &

echo "gnutls-serv ECC/RSA..."
${GNUTLS_DIR}/bin/gnutls-serv -p 5003 -d 4 --http -a --x509keyfile test-certs/ServerKey.ecc.pem --x509certfile test-certs/ServerCert.ecc.rsa.pem  --priority "NORMAL:+ANON-DH:+NULL" > /tmp/gnutls-serv.ecc.rsa.log 2>&1 &

echo "gnutls-serv ECC/ECC..."
${GNUTLS_DIR}/bin/gnutls-serv -p 5004 -d 4 --http -a --x509keyfile test-certs/ServerKey.ecc.pem --x509certfile test-certs/ServerCert.ecc.ecc.pem  --priority "NORMAL:+ANON-DH:+NULL" > /tmp/gnutls-serv.ecc.ecc.log 2>&1 &

echo "openssl s_server RSA/RSA + Client Side Auth..."
${OPENSSL_DIR}/bin/openssl s_server -accept 4011 -verify 3 -CAfile test-certs/CACert.rsa.pem -state -key test-certs/ServerKey.rsa.pem -cert test-certs/ServerCert.rsa.rsa.pem -www -cipher ALL:eNULL > /tmp/s_server.rsa.rsa.csa.log 2>&1 &

echo "openssl s_server RSA/ECC + Client Side Auth...."
${OPENSSL_DIR}/bin/openssl s_server -accept 4012 -verify 3 -CAfile test-certs/CACert.rsa.pem -state -key test-certs/ServerKey.rsa.pem -cert test-certs/ServerCert.rsa.ecc.pem -www -cipher ALL:eNULL > /tmp/s_server.rsa.ecc.csa.log 2>&1 &

echo "openssl s_server ECC/RSA + Client Side Auth...."
${OPENSSL_DIR}/bin/openssl s_server -accept 4013 -verify 3 -CAfile test-certs/CACert.rsa.pem -state -key test-certs/ServerKey.ecc.pem -cert test-certs/ServerCert.ecc.rsa.pem -www -cipher ALL:eNULL > /tmp/s_server.ecc.rsa.csa.log 2>&1 &

echo "openssl s_server ECC/ECC + Client Side Auth...."
${OPENSSL_DIR}/bin/openssl s_server -accept 4014 -verify 3 -CAfile test-certs/CACert.rsa.pem -state -key test-certs/ServerKey.ecc.pem -cert test-certs/ServerCert.ecc.ecc.pem -www -cipher ALL:eNULL > /tmp/s_server.ecc.ecc.csa.log 2>&1 &

echo "gnutls-serv RSA/RSA + Client Side Auth...."
${GNUTLS_DIR}/bin/gnutls-serv -p 5011 -d 4 --http --x509keyfile test-certs/ServerKey.rsa.pem --x509certfile test-certs/ServerCert.rsa.rsa.pem  --priority "NORMAL:+ANON-DH:+NULL" > /tmp/gnutls-serv.rsa.rsa.csa.log 2>&1 &

echo "gnutls-serv RSA/ECC + Client Side Auth...."
${GNUTLS_DIR}/bin/gnutls-serv -p 5012 -d 4 --http --x509keyfile test-certs/ServerKey.rsa.pem --x509certfile test-certs/ServerCert.rsa.ecc.pem  --priority "NORMAL:+ANON-DH:+NULL" > /tmp/gnutls-serv.rsa.ecc.csa.log 2>&1 &

echo "gnutls-serv ECC/RSA + Client Side Auth...."
${GNUTLS_DIR}/bin/gnutls-serv -p 5013 -d 4 --http --x509keyfile test-certs/ServerKey.ecc.pem --x509certfile test-certs/ServerCert.ecc.rsa.pem  --priority "NORMAL:+ANON-DH:+NULL" > /tmp/gnutls-serv.ecc.rsa.csa.log 2>&1 &

echo "gnutls-serv ECC/ECC + Client Side Auth...."
${GNUTLS_DIR}/bin/gnutls-serv -p 5014 -d 4 --http --x509keyfile test-certs/ServerKey.ecc.pem --x509certfile test-certs/ServerCert.ecc.ecc.pem  --priority "NORMAL:+ANON-DH:+NULL" > /tmp/gnutls-serv.ecc.ecc.csa.log 2>&1 &
