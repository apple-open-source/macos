#!/bin/sh -e

#  CreateCerts.sh
#  Security
#
#  Created by Fabrice Gautier on 6/7/11.
#  Copyright 2011 Apple, Inc. All rights reserved.

echo "Create Certs"


#Overrride which openssl to use:
OPENSSL=/usr/bin/openssl 
#OPENSSL=/opt/openssl/bin/openssl

#Override which gnutls-certtool to use:
GNUTLS_CERTTOOL=/opt/gnutls/bin/certtool

DIR=test-certs


mkdir -p $DIR
cd $DIR

gen_rsa_cert()
{
    ${OPENSSL} req -x509 -nodes -days 365 -subj "$2"  -newkey rsa:1024 -keyout $1.Key.rsa.pem -out $1.Cert.rsa.pem
    ${OPENSSL} rsa -outform DER -in $1.Key.rsa.pem -out $1.Key.rsa.der
    ${OPENSSL} x509 -outform DER -in $1.Cert.rsa.pem -out $1.Cert.rsa.der
    xxd -i $1.Key.rsa.der > $1_Key_rsa.h
    xxd -i $1.Cert.rsa.der > $1_Cert_rsa.h

}

gen_rsa_req()
{
    ${OPENSSL} req -new -nodes -days 365 -subj "$2"  -newkey rsa:1024 -keyout $1.Key.rsa.pem -out $1.Req.rsa.pem
    ${OPENSSL} rsa -outform DER -in $1.Key.rsa.pem -out $1.Key.rsa.der
    xxd -i $1.Key.rsa.der > $1_Key_rsa.h
}

gen_ec_cert()
{
    ${OPENSSL} req -x509 -nodes -days 365 -subj "$2"  -newkey ec:ecparam.pem -keyout $1.Key.ecc.pem -out $1.Cert.ecc.pem
    ${OPENSSL} ec -outform DER -in $1.Key.ecc.pem -out $1.Key.ecc.der
    ${OPENSSL} x509 -outform DER -in $1.Cert.ecc.pem -out $1.Cert.ecc.der
    xxd -i $1.Key.ecc.der > $1_Key_ecc.h
    xxd -i $1.Cert.ecc.der > $1_Cert_ecc.h
}

gen_ec_req()
{
    ${OPENSSL} req -new -nodes -days 365 -subj "$2"  -newkey ec:ecparam.pem -keyout $1.Key.ecc.pem -out $1.Req.ecc.pem
    ${OPENSSL} ec -outform DER -in $1.Key.ecc.pem -out $1.Key.ecc.der
    xxd -i $1.Key.ecc.der > $1_Key_ecc.h
}

sign_cert()
{
    ${OPENSSL} x509 -req -in $1.Req.$2.pem -CA CA.Cert.$3.pem -CAkey CA.Key.$3.pem -set_serial $4 -out $1.Cert.$2.$3.pem
    ${OPENSSL} x509 -outform DER -in $1.Cert.$2.$3.pem -out $1.Cert.$2.$3.der
    xxd -i $1.Cert.$2.$3.der > $1_Cert_$2_$3.h

    ${OPENSSL} pkcs12 -export -passout pass:password -out $1.$2.$3.p12 -inkey $1.Key.$2.pem -in $1.Cert.$2.$3.pem
    xxd -i $1.$2.$3.p12 > $1_$2_$3_p12.h 
}

#generate EC params
${OPENSSL} ecparam -name secp256k1 -out ecparam.pem

echo "**** Generating CA keys and certs..."
# generate CA certs
gen_rsa_cert CA '/CN=coreTLS CA Cert (RSA)'
gen_ec_cert CA '/CN=coreTLS CA Cert (ECC)'

echo "**** Generating Server keys and csr..."
# generate Server EC key
#${GNUTLS_CERTTOOL} -p  --ecc --sec-param high --outfile Server.Key.ecc.pem

# generate Server CSR
gen_rsa_req Server1 '/CN=coreTLS Server1 Cert (RSA)'
gen_ec_req Server1 '/CN=coreTLS Server1 Cert (ECC)'
gen_rsa_req Server2 '/CN=coreTLS Server2 Cert (RSA)'
gen_ec_req Server2 '/CN=coreTLS Server2 Cert (ECC)'

    
#${OPENSSL} req -new -nodes -days 365 -subj '/CN=SecurityTests Server Cert (RSA)'  -newkey rsa:1024 -keyout Server.Key.rsa.pem -out Server.Req.rsa.pem
#${OPENSSL} req -new -nodes -days 365 -subj '/CN=SecurityTests Server Cert (ECC)'  -key Server.Key.ecc.pem -out Server.Req.ecc.pem
#${OPENSSL} req -new -nodes -days 365 -subj '/CN=SecurityTests Server Cert (ECC)'  -newkey ec:ecparam.pem -keyout Server.Key.ecc.pem -out Server.Req.ecc.pem

echo "**** Generating Client keys and csr..."
# generate Client EC key
#${GNUTLS_CERTTOOL} -p  --ecc --sec-param high --outfile Client.Key.ecc.pem

# generate client certs
#${OPENSSL} req -new -nodes -days 365 -subj '/CN=SecurityTests Client Cert (RSA)'  -newkey rsa:1024 -keyout Client.Key.rsa.pem -out Client.Req.rsa.pem
#${OPENSSL} req -new -nodes -days 365 -subj '/CN=SecurityTests Client Cert (ECC)' -key Client.Key.ecc.pem -out Client.Req.ecc.pem
#${OPENSSL} req -new -nodes -days 365 -subj '/CN=SecurityTests Client Cert (ECC)' -newkey ec:ecparam.pem -keyout Client.Key.ecc.pem -out Client.Req.ecc.pem

echo "**** Signing Servers certs..."
sign_cert Server1 rsa rsa 1
sign_cert Server1 rsa ecc 2
sign_cert Server1 ecc rsa 3
sign_cert Server1 ecc ecc 4

sign_cert Server2 rsa rsa 5
sign_cert Server2 rsa ecc 6
sign_cert Server2 ecc rsa 7
sign_cert Server2 ecc ecc 8

#echo "**** Signing Clients certs..."
#sign_cert Client rsa rsa 1001
#sign_cert Client rsa ecc 1002
#sign_cert Client ecc rsa 1003
#sign_cert Client ecc ecc 1004

#export client keys and cert into .h
#${OPENSSL} ec -outform DER -in Server.Key.ecc.pem -out Server.Key.ecc.der
#${OPENSSL} rsa -outform DER -in Server.Key.rsa.pem -out Server.Key.rsa.der

#xxd -i Server.Key.ecc.der > Server_Key_ecc.h
#xxd -i Server.Key.rsa.der > Server_Key_rsa.h

#${OPENSSL} ec -outform DER -in Client.Key.ecc.pem -out Client.Key.ecc.der
#${OPENSSL} rsa -outform DER -in Client.Key.rsa.pem -out Client.Key.rsa.der

#xxd -i Client.Key.ecc.der > Client_Key_ecc.h
#xxd -i Client.Key.rsa.der > Client_Key_rsa.h

