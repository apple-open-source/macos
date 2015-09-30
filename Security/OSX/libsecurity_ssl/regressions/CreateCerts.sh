#!/bin/sh -e

#  CreateCerts.sh
#  Security
#
#  Copyright 2011,2015 Apple, Inc. All rights reserved.

# This script may require modern version of openssl 

echo "Create Certs"

#Overrride which openssl to use:
#OPENSSL=/opt/openssl/bin/openssl
OPENSSL=openssl

DIR=test-certs


mkdir -p $DIR
cd $DIR

gen_config()
{
    cat >ext.conf << _EOF_
  basicConstraints            = CA:FALSE
_EOF_
}

gen_rsa_cert()
{
    ${OPENSSL} req -x509 -days 14600 -nodes -subj "$2"  -newkey rsa:2048 -keyout $1.Key.pem -out $1.Cert.pem
    ${OPENSSL} rsa -outform DER -in $1.Key.pem -out $1.Key.der
    ${OPENSSL} x509 -outform DER -in $1.Cert.pem -out $1.Cert.der
    xxd -i $1.Key.der > $1_Key.h
    xxd -i $1.Cert.der > $1_Cert.h
}

gen_ec_cert()
{
    ${OPENSSL} req -x509 -days 14600 -nodes -subj "$2"  -newkey ec:ecparam.pem -keyout $1.Key.pem -out $1.Cert.pem
    ${OPENSSL} ec -outform DER -in $1.Key.pem -out $1.Key.der
    ${OPENSSL} x509 -outform DER -in $1.Cert.pem -out $1.Cert.der
    xxd -i $1.Key.der > $1_Key.h
    xxd -i $1.Cert.der > $1_Cert.h
}


create_rsa_key()
{
    ${OPENSSL} req -new -nodes -subj "$2" -newkey rsa:1024 -keyout $1.Key.pem -out $1.Req.pem
    ${OPENSSL} rsa -outform DER -in $1.Key.pem -out $1.Key.der
    xxd -i $1.Key.der > $1_Key.h
}

create_ec_key()
{
    ${OPENSSL} req -new -nodes -subj "$2" -newkey ec:ecparam.pem -keyout $1.Key.pem -out $1.Req.pem
    ${OPENSSL} ec -outform DER -in $1.Key.pem -out $1.Key.der
    xxd -i $1.Key.der > $1_Key.h
}

sign_cert()
{
    ${OPENSSL} x509 -days 14600 -req -in $1.Req.pem -CA $2.Cert.pem -CAkey $2.Key.pem -set_serial $3 -out $1.Cert.$2.pem -extfile ext.conf
    ${OPENSSL} x509 -outform DER -in $1.Cert.$2.pem -out $1.Cert.$2.der
    xxd -i $1.Cert.$2.der > $1_Cert_$2.h
}

#generate openssl config file
gen_config

#generate EC params
${OPENSSL} ecparam -name prime256v1 -out ecparam.pem

echo "**** Generating CA keys and certs..."
# generate CA certs
gen_rsa_cert CA-RSA '/CN=SecurityTest CA Cert (RSA)'
gen_rsa_cert Untrusted-CA-RSA '/CN=SecurityTest CA Cert (RSA)'
gen_ec_cert CA-ECC '/CN=SecurityTest CA Cert (ECC)'

echo "**** Generating Server keys and csr..."
# generate Server keys and CSR
create_rsa_key ServerRSA '/OU=SecurityTests Server Cert (RSA)/CN=localhost'
create_ec_key ServerECC '/OU=SecurityTests Server Cert (ECC)/CN=localhost'

echo "**** Generating Client keys and csr..."
# generate client certs
create_rsa_key ClientRSA '/OU=SecurityTests Client Cert (RSA)/CN=localhost'
create_ec_key ClientECC '/OU=SecurityTests Client Cert (ECC)/CN=localhost'
create_rsa_key UntrustedClientRSA '/OU=SecurityTests Client Cert (RSA)(Untrusted)/CN=localhost'

echo "**** Signing Servers certs..."
sign_cert ServerRSA CA-RSA 1
sign_cert ServerRSA CA-ECC 2
sign_cert ServerECC CA-RSA 3
sign_cert ServerECC CA-ECC 4

echo "**** Signing Clients certs..."
sign_cert ClientRSA CA-RSA 1001
sign_cert ClientRSA CA-ECC 1002
sign_cert ClientECC CA-RSA 1003
sign_cert ClientECC CA-ECC 1004

sign_cert UntrustedClientRSA Untrusted-CA-RSA 9999

