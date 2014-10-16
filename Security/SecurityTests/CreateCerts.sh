#!/bin/sh

#  CreateCerts.sh
#  Security
#
#  Created by Fabrice Gautier on 6/7/11.
#  Copyright 2011 Apple, Inc. All rights reserved.

echo "Create Certs"


#Overrride which openssl to use:
# System openssl
#OPENSSL=/usr/bin/openssl
# Macport  openssl
#OPENSSL=/opt/local/bin/openssl
# your own openssl
OPENSSL=/usr/local/ssl/bin/openssl

#Override which gnutls-certtool to use:
# Macport  gnutls
#GNUTLS_CERTTOOL=/opt/local/gnutls-certtool
# your own gnutls
GNUTLS_CERTTOOL=/usr/local/bin/certtool


DIR=test-certs

mkdir -p $DIR
cd $DIR

#generate EC params
${OPENSSL} ecparam -name secp256k1 -out ecparam.pem

echo "**** Generating CA keys and certs..."
# generate CA certs
${OPENSSL} req -x509 -nodes -days 365 -subj '/CN=SecurityTest CA Cert (RSA)'  -newkey rsa:1024 -keyout CAKey.rsa.pem -out CACert.rsa.pem
${OPENSSL} req -x509 -nodes -days 365 -subj '/CN=SecurityTest CA Cert (ECC)'  -newkey ec:ecparam.pem -keyout CAKey.ecc.pem -out CACert.ecc.pem

echo "**** Generating Server keys and csr..."
# generate Server EC key
${GNUTLS_CERTTOOL} -p  --ecc --sec-param high --outfile ServerKey.ecc.pem

# generate Server certs
${OPENSSL} req -new -nodes -days 365 -subj '/CN=SecurityTests Server Cert (RSA)'  -newkey rsa:1024 -keyout ServerKey.rsa.pem -out ServerReq.rsa.pem
${OPENSSL} req -new -nodes -days 365 -subj '/CN=SecurityTests Server Cert (ECC)'  -key ServerKey.ecc.pem -out ServerReq.ecc.pem

echo "**** Generating Client keys and csr..."
# generate Client EC key
${GNUTLS_CERTTOOL} -p  --ecc --sec-param high --outfile ClientKey.ecc.pem

# generate client certs
${OPENSSL} req -new -nodes -days 365 -subj '/CN=SecurityTests Client Cert (RSA)'  -newkey rsa:1024 -keyout ClientKey.rsa.pem -out ClientReq.rsa.pem
${OPENSSL} req -new -nodes -days 365 -subj '/CN=SecurityTests Client Cert (ECC)'  -key ClientKey.ecc.pem -out ClientReq.ecc.pem

echo "**** Signing Servers certs..."
# sign certs
${OPENSSL} x509 -req -in ServerReq.rsa.pem -CA CACert.rsa.pem -CAkey CAKey.rsa.pem -set_serial 1 -out ServerCert.rsa.rsa.pem
${OPENSSL} x509 -req -in ServerReq.rsa.pem -CA CACert.ecc.pem -CAkey CAKey.ecc.pem -set_serial 2 -out ServerCert.rsa.ecc.pem
${OPENSSL} x509 -req -in ServerReq.ecc.pem -CA CACert.rsa.pem -CAkey CAKey.rsa.pem -set_serial 3 -out ServerCert.ecc.rsa.pem
${OPENSSL} x509 -req -in ServerReq.ecc.pem -CA CACert.ecc.pem -CAkey CAKey.ecc.pem -set_serial 4 -out ServerCert.ecc.ecc.pem

echo "**** Signing Clients certs..."
${OPENSSL} x509 -req -in ClientReq.rsa.pem -CA CACert.rsa.pem -CAkey CAKey.rsa.pem -set_serial 1001 -out ClientCert.rsa.rsa.pem
${OPENSSL} x509 -req -in ClientReq.rsa.pem -CA CACert.ecc.pem -CAkey CAKey.ecc.pem -set_serial 1002 -out ClientCert.rsa.ecc.pem
${OPENSSL} x509 -req -in ClientReq.ecc.pem -CA CACert.rsa.pem -CAkey CAKey.rsa.pem -set_serial 1003 -out ClientCert.ecc.rsa.pem
${OPENSSL} x509 -req -in ClientReq.ecc.pem -CA CACert.ecc.pem -CAkey CAKey.ecc.pem -set_serial 1004 -out ClientCert.ecc.ecc.pem


#export client keys and cert into .h

${OPENSSL} ec -outform DER -in ClientKey.ecc.pem -out ClientKey.ecc.der
${OPENSSL} rsa -outform DER -in ClientKey.rsa.pem -out ClientKey.rsa.der

xxd -i ClientKey.ecc.der > ClientKey_ecc.h
xxd -i ClientKey.rsa.der > ClientKey_rsa.h

${OPENSSL} x509 -outform DER -in ClientCert.rsa.rsa.pem -out ClientCert.rsa.rsa.der
${OPENSSL} x509 -outform DER -in ClientCert.rsa.ecc.pem -out ClientCert.rsa.ecc.der
${OPENSSL} x509 -outform DER -in ClientCert.ecc.rsa.pem -out ClientCert.ecc.rsa.der
${OPENSSL} x509 -outform DER -in ClientCert.ecc.ecc.pem -out ClientCert.ecc.ecc.der

xxd -i ClientCert.rsa.rsa.der > ClientCert_rsa_rsa.h
xxd -i ClientCert.rsa.ecc.der > ClientCert_rsa_ecc.h
xxd -i ClientCert.ecc.rsa.der > ClientCert_ecc_rsa.h
xxd -i ClientCert.ecc.ecc.der > ClientCert_ecc_ecc.h
