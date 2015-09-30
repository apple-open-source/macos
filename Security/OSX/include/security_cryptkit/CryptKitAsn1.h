/*
 * CryptKitAsn1.h -  ASN1 templates for FEE objects
 */

#ifndef	_CRYPT_KIT_ASN1_H_
#define _CRYPT_KIT_ASN1_H_

#include "ckconfig.h"

#if CRYPTKIT_DER_ENABLE

#include <Security/cssmtype.h>
#include <Security/secasn1t.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
  -- FEE Curve parameters (defined in <security_cryptkit/feeTypes.h>)
	FEEPrimeType ::=    INTEGER { FPT_Mersenne(0), FPT_FEE(1), FPT_General(2) }
	FEECurveType ::=    INTEGER { FCT_Montgomery(0), FCT_Weierstrass(1), 
								  FCT_General(2) }
 */

/*
	FEECurveParameters ::= SEQUENCE
	{
		primeType		FEEPrimeType,
		curveType		FEECurveType,
		q			INTEGER,	-- unsigned
		k			INTEGER,	-- signed 
		m			INTEGER,
		a			BigIntegerStr,
		bb			BigIntegerStr,	-- can't use variable/field b
		c			BigIntegerStr,
		x1Plus			BigIntegerStr,
		x1Minus			BigIntegerStr,
		cOrderPlus		BigIntegerStr,
		cOrderMinus		BigIntegerStr,
		x1OrderPlus		BigIntegerStr,
		x1OrderMinus	BigIntegerStr,
		basePrime		BigIntegerStr OPTIONAL	
										-- iff FEEPrimeType == CT_GENERAL
}
*/
typedef struct {
	CSSM_DATA primeType;
	CSSM_DATA curveType;
	CSSM_DATA q;
	CSSM_DATA k;	
	CSSM_DATA m;	
	CSSM_DATA a;	
	CSSM_DATA b_;			// can't use variable/field b
	CSSM_DATA c;		
	CSSM_DATA x1Plus;		
	CSSM_DATA x1Minus;		
	CSSM_DATA cOrderPlus;	
	CSSM_DATA cOrderMinus;	
	CSSM_DATA x1OrderPlus;	
	CSSM_DATA x1OrderMinus;	
	CSSM_DATA basePrime;		// OPTIONAL	
} FEECurveParametersASN1;

extern const SecAsn1Template FEECurveParametersASN1Template[];

/*
	-- FEE ElGamal-style signature
	FEEElGamalSignature ::= SEQUENCE {
		u     BigIntegerStr,
		pmX 	BigIntegerStr
	}
*/
typedef struct {
	CSSM_DATA	u;
	CSSM_DATA	pmX;
} FEEElGamalSignatureASN1;

extern const SecAsn1Template FEEElGamalSignatureASN1Template[];

/*
	-- FEE ECDSA-style signature
	FEEECDSASignature ::= SEQUENCE {
		c     BigIntegerStr, 
		d     BigIntegerStr
	}
*/
typedef struct {
	CSSM_DATA	c;
	CSSM_DATA	d;
} FEEECDSASignatureASN1;

extern const SecAsn1Template FEEECDSASignatureASN1Template[];

/*
	FEEPublicKey ::= SEQUENCE
	{
		version			INTEGER,
		curveParams		FEECurveParameters,
		plusX			BigIntegerStr,
		minusX			BigIntegerStr,
		plusY			BigIntegerStr	OPTIONAL	
				-- iff FEECurveType == ct-weierstrass
}
*/
typedef struct {
	CSSM_DATA		version;
	FEECurveParametersASN1	curveParams;
	CSSM_DATA		plusX;
	CSSM_DATA		minusX;
	CSSM_DATA		plusY;		// OPTIONAL
} FEEPublicKeyASN1;

extern const SecAsn1Template FEEPublicKeyASN1Template[];

/*
	FEEPrivateKey ::= SEQUENCE 
	{
		version			INTEGER,
		curveParams		FEECurveParameters,
		privData		BigIntegerStr
	}
*/
typedef struct {
	CSSM_DATA		version;
	FEECurveParametersASN1	curveParams;
	CSSM_DATA		privData;
} FEEPrivateKeyASN1;

extern const SecAsn1Template FEEPrivateKeyASN1Template[];

#ifdef	__cplusplus
}
#endif

#endif	/* CRYPTKIT_DER_ENABLE */

#endif	/* _CRYPT_KIT_ASN1_H_ */
