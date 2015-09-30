/*
 * CryptKitAsn1.cpp - ASN1 templates for FEE keys and signatures
 */

#include "CryptKitAsn1.h"
#include <security_asn1/secasn1.h>

/*
 * Unlike RSA, DSA, and Diffie-Hellman, the integers in these
 * objects are indeed signed.
 */
#define SEC_ASN1_SIGNED  (SEC_ASN1_SIGNED_INT | SEC_ASN1_INTEGER)

/* FEECurveParametersASN1 */
const SecAsn1Template FEECurveParametersASN1Template[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(FEECurveParametersASN1) },
    { SEC_ASN1_INTEGER, offsetof(FEECurveParametersASN1,primeType) },
    { SEC_ASN1_INTEGER, offsetof(FEECurveParametersASN1,curveType) },
    { SEC_ASN1_SIGNED, offsetof(FEECurveParametersASN1,q) },
    { SEC_ASN1_SIGNED, offsetof(FEECurveParametersASN1,k) },
    { SEC_ASN1_SIGNED, offsetof(FEECurveParametersASN1,m) },
    { SEC_ASN1_SIGNED, offsetof(FEECurveParametersASN1,a) },
    { SEC_ASN1_SIGNED, offsetof(FEECurveParametersASN1,b_) },
    { SEC_ASN1_SIGNED, offsetof(FEECurveParametersASN1,c) },
    { SEC_ASN1_SIGNED, offsetof(FEECurveParametersASN1,x1Plus) },
    { SEC_ASN1_SIGNED, offsetof(FEECurveParametersASN1,x1Minus) },
    { SEC_ASN1_SIGNED, offsetof(FEECurveParametersASN1,cOrderPlus) },
    { SEC_ASN1_SIGNED, offsetof(FEECurveParametersASN1,cOrderMinus) },
    { SEC_ASN1_SIGNED, offsetof(FEECurveParametersASN1,x1OrderPlus) },
    { SEC_ASN1_SIGNED, offsetof(FEECurveParametersASN1,x1OrderMinus) },
    { SEC_ASN1_SIGNED | SEC_ASN1_OPTIONAL,
		offsetof(FEECurveParametersASN1,basePrime) },
    { 0, }
};

/* FEEElGamalSignatureASN1 */
const SecAsn1Template FEEElGamalSignatureASN1Template[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(FEEElGamalSignatureASN1) },
    { SEC_ASN1_SIGNED, offsetof(FEEElGamalSignatureASN1,u) },
    { SEC_ASN1_SIGNED, offsetof(FEEElGamalSignatureASN1,pmX) },
    { 0, }
};

/* FEEECDSASignatureASN1 */
const SecAsn1Template FEEECDSASignatureASN1Template[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(FEEECDSASignatureASN1) },
    { SEC_ASN1_SIGNED, offsetof(FEEECDSASignatureASN1,c) },
    { SEC_ASN1_SIGNED, offsetof(FEEECDSASignatureASN1,d) },
    { 0, }
};

/* FEEPublicKeyASN1 */
const SecAsn1Template FEEPublicKeyASN1Template[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(FEEPublicKeyASN1) },
    { SEC_ASN1_SIGNED, offsetof(FEEPublicKeyASN1,version) },
    { SEC_ASN1_INLINE,
	  offsetof(FEEPublicKeyASN1,curveParams),
	  FEECurveParametersASN1Template },
    { SEC_ASN1_SIGNED, offsetof(FEEPublicKeyASN1,plusX) },
    { SEC_ASN1_SIGNED, offsetof(FEEPublicKeyASN1,minusX) },
    { SEC_ASN1_SIGNED | SEC_ASN1_OPTIONAL, 
	  offsetof(FEEPublicKeyASN1,plusY) },
    { 0, }
};

/* FEEPrivateKeyASN1 */
const SecAsn1Template FEEPrivateKeyASN1Template[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(FEEPrivateKeyASN1) },
    { SEC_ASN1_SIGNED, offsetof(FEEPrivateKeyASN1,version) },
    { SEC_ASN1_INLINE,
	  offsetof(FEEPrivateKeyASN1,curveParams),
	  FEECurveParametersASN1Template },
    { SEC_ASN1_SIGNED, offsetof(FEEPrivateKeyASN1,privData) },
    { 0, }
};


