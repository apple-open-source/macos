/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * openRsaSnacc.cpp - glue between openrsa and SNACC
 */
#include "openRsaSnacc.h"
#include "opensslUtils.h"
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/crypto.h>

// bring in a ton of snacc-related stuff
#include <Security/asn-incl.h>
#include <Security/sm_vdatypes.h>

// snacc-generated - snacc really should place these in pkcs[78].h
#include <Security/sm_x501ud.h>
#include <Security/sm_x411ub.h>
#include <Security/sm_x411mtsas.h>
#include <Security/sm_x501if.h>
#include <Security/sm_x520sa.h>
#include <Security/sm_x509cmn.h>
#include <Security/sm_x509af.h>
#include <Security/sm_x509ce.h>
#include <Security/pkcs1oids.h>
#include <Security/pkcs9oids.h>
#include <Security/sm_cms.h>
#include <Security/sm_ess.h>
#include <Security/pkcs7.h>
#include <Security/pkcs8.h>

#include <Security/cdsaUtils.h>
#include <Security/debugging.h>
#include <Security/appleoids.h>


#define sslSnaccDebug(args...)	debug("sslSnacc", ##args)

/*
 * Convert between SNACC-style BigIntegerStr and openssl-style BIGNUM.
 */
BIGNUM *bigIntStrToBn(
	BigIntegerStr &snaccInt)
{
	BIGNUM *bn = BN_new();
	BIGNUM *rtn;
	char *rawOcts = snaccInt;
	unsigned numBytes = snaccInt.Len();
	
	rtn = BN_bin2bn((unsigned char *)rawOcts, numBytes, bn);
	if(rtn == NULL) {
		BN_free(bn);
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
	}
	return bn;
}

void bnToBigIntStr(
	BIGNUM *bn,
	BigIntegerStr &snaccInt)
{
	unsigned numBytes = BN_num_bytes(bn);
	unsigned char *buf;
	unsigned char *bp;
	
	/* 
	 * BSAFE is peculiar here. When IT DER-encodes public keys, it often generates
	 * a publicExponent whose leading bit (m.s. bit in the first byte) is 1. It
	 * reads these fine, of course. But when it DER-encodes the same value in the
	 * private key, it hews to DER rules and prepends a leading zero. If WE 
	 * generate a private key with a field with a leading bit set, without the
	 * (technically) required leading zero, BSAFE pukes....but only when parsing
	 * private keys, not public keys. Same field (public exponent), different 
	 * requirements for public and private keys. So we're cautious and prepend
	 * a zero if the leading field is one. 
	 *
	 * This assumes of course that ALL numbers we're dealing with are positive....
	 */ 
	buf = (unsigned char *)Malloc(numBytes + 1);		// extra for possible prepend
	if(buf == NULL) {
		throw openSslException(CSSMERR_CSP_MEMORY_ERROR);
	}
	BN_bn2bin(bn, buf + 1);			
	if(buf[1] & 0x80) {
		/* pedantic DER rules for BSAFE - make sure first byte is zero */
		buf[0] = 0;
		bp = buf;
		numBytes++;
	}
	else {
		/* use what BN_bn2bin gave us */
		bp = buf+1;
	}
	snaccInt.ReSet((char *)bp, numBytes);
	Free(buf);
}

/* estimate size of encoded BigIntegerStr */
unsigned sizeofBigInt(
	BigIntegerStr &bigInt)
{
	return bigInt.Len() + 4;
}

/* set up a encoded NULL for AlgorithmIdentifier.parameters, required for RSA */
static void nullAlgParams(
	AlgorithmIdentifier	&snaccAlgId)
{
	snaccAlgId.parameters = new AsnAny;
	char encodedNull[2] = {NULLTYPE_TAG_CODE, 0};
	CSM_Buffer *cbuf = new CSM_Buffer(encodedNull, 2);
	snaccAlgId.parameters->value = cbuf;
}

/*
 * int --> BigIntegerStr
 */
void snaccIntToBigIntegerStr(
	int i,
	BigIntegerStr &bigInt)
{
	char c[4];
	int dex;
	int numChars;
	
	if(i >= 0x1000000) {
		numChars = 4;
	}
	else if(i > 0x10000) {
		numChars = 3;
	}	
	else if(i > 0x100) {
		numChars = 2;
	}
	else {
		numChars = 1;
	}
	/* i ==> DER */
	for(dex=numChars-1; dex>=0; dex--) {
		c[dex] = i & 0xff;
		i >>= 8;
	}
	
	bigInt.ReSet(c, 4);
}

/*
 * Replacements for d2i_RSAPublicKey, etc. 
 */
CSSM_RETURN RSAPublicKeyDecode(
	RSA 			*openKey, 
	void 			*p, 
	size_t			length)
{
	RSAPublicKey snaccPubKey;
	
	CssmData cData(p, length);
	try {
		SC_decodeAsnObj(cData, snaccPubKey);
	}
	catch(...) {
		return CSSMERR_CSP_INVALID_KEY;
	}
	try {
		openKey->n = bigIntStrToBn(snaccPubKey.modulus);
		openKey->e = bigIntStrToBn(snaccPubKey.publicExponent);
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

CSSM_RETURN	RSAPublicKeyEncode(
	RSA 			*openKey, 
	CssmOwnedData	&encodedKey)
{
	/* First convert into a snacc-style public key */
	RSAPublicKey snaccPubKey;
	
	try {
		bnToBigIntStr(openKey->n, snaccPubKey.modulus);
		bnToBigIntStr(openKey->e, snaccPubKey.publicExponent);
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	
	/* conservative guess for max size of encoded key */
	unsigned maxSize = sizeofBigInt(snaccPubKey.modulus) +
					   sizeofBigInt(snaccPubKey.publicExponent) +
					   20;
					   
	/* DER encode */
	try {
		SC_encodeAsnObj(snaccPubKey, encodedKey, maxSize);
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

CSSM_RETURN RSAPrivateKeyDecode(
	RSA 			*openKey, 
	void 			*p, 
	size_t			length)
{
	PrivateKeyInfo snaccPrivKeyInfo;
	CssmData cData(p, length);
	try {
		SC_decodeAsnObj(cData, snaccPrivKeyInfo);
	}
	catch(...) {
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	/* verify alg identifier */
	if(snaccPrivKeyInfo.privateKeyAlgorithm == NULL) {
		sslSnaccDebug("RSAPrivateKeyDecode: no privateKeyAlgorithm");
		return CSSMERR_CSP_INVALID_KEY;
	}
	if(snaccPrivKeyInfo.privateKeyAlgorithm->algorithm != rsaEncryption) {
		sslSnaccDebug("RSAPrivateKeyDecode: bad privateKeyAlgorithm");
		return CSSMERR_CSP_ALGID_MISMATCH;
	}
	
	/* 
	 * snaccPrivKeyInfo.privateKey is an octet string which needs 
	 * subsequent decoding 
	 */
	char *rawOcts = snaccPrivKeyInfo.privateKey;
	unsigned numBytes = snaccPrivKeyInfo.privateKey.Len();
	RSAPrivateKey snaccPrivKey;
	CssmData cData2(rawOcts, numBytes);
	try {
		SC_decodeAsnObj(cData2, snaccPrivKey);
	}
	catch(...) {
		sslSnaccDebug("RSAPrivateKeyDecode: bad snaccPrivKeyInfo.privateKey");
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	/* convert snaccPrivKey fields to RSA key fields */
	try {
		openKey->version = snaccPrivKey.version;
		openKey->n	  = bigIntStrToBn(snaccPrivKey.modulus);
		openKey->e	  = bigIntStrToBn(snaccPrivKey.publicExponent);
		openKey->d 	  = bigIntStrToBn(snaccPrivKey.privateExponent);
		openKey->p 	  = bigIntStrToBn(snaccPrivKey.prime1);
		openKey->q 	  = bigIntStrToBn(snaccPrivKey.prime2);
		openKey->dmp1 = bigIntStrToBn(snaccPrivKey.exponent1);
		openKey->dmq1 = bigIntStrToBn(snaccPrivKey.exponent2);
		openKey->iqmp = bigIntStrToBn(snaccPrivKey.coefficient);
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

CSSM_RETURN	RSAPrivateKeyEncode(
	RSA 			*openKey, 
	CssmOwnedData	&encodedKey)
{
	/* First convert into a snacc-style private key */
	RSAPrivateKey snaccPrivKey;
	
	try {
		snaccPrivKey.version = openKey->version;
		bnToBigIntStr(openKey->n, 	 snaccPrivKey.modulus);
		bnToBigIntStr(openKey->e, 	 snaccPrivKey.publicExponent);
		bnToBigIntStr(openKey->d, 	 snaccPrivKey.privateExponent);
		bnToBigIntStr(openKey->p, 	 snaccPrivKey.prime1);
		bnToBigIntStr(openKey->q, 	 snaccPrivKey.prime2);
		bnToBigIntStr(openKey->dmp1, snaccPrivKey.exponent1);
		bnToBigIntStr(openKey->dmq1, snaccPrivKey.exponent2);
		bnToBigIntStr(openKey->iqmp, snaccPrivKey.coefficient);
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	
	/* conservative guess for max size of encoded key */
	unsigned maxSize = sizeofBigInt(snaccPrivKey.modulus) +
					   sizeofBigInt(snaccPrivKey.publicExponent) +
					   sizeofBigInt(snaccPrivKey.privateExponent) +
					   sizeofBigInt(snaccPrivKey.prime1) +
					   sizeofBigInt(snaccPrivKey.prime2) +
					   sizeofBigInt(snaccPrivKey.exponent1) +
					   sizeofBigInt(snaccPrivKey.exponent2) +
					   sizeofBigInt(snaccPrivKey.coefficient) +
					   64;		// includes the to-be-generated algId
					   
	/* DER encode */
	try {
		SC_encodeAsnObj(snaccPrivKey, encodedKey, maxSize);
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	
	/* that encoding is the privateKey field of a PrivateKeyInfo */
	PrivateKeyInfo snaccPrivKeyInfo;
	snaccPrivKeyInfo.version = 0;		/* I think.... */
	snaccPrivKeyInfo.privateKeyAlgorithm = new AlgorithmIdentifier;
	snaccPrivKeyInfo.privateKeyAlgorithm->algorithm = rsaEncryption;
	nullAlgParams(*snaccPrivKeyInfo.privateKeyAlgorithm);
	snaccPrivKeyInfo.privateKey.Set((char *)encodedKey.data(), encodedKey.length());
	
	/* now encode the privateKeyInfo */
	encodedKey.reset();
	try {
		SC_encodeAsnObj(snaccPrivKeyInfo, encodedKey, maxSize);
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

/*
 * Given a message digest and associated algorithm, cook up a PKCS1-style
 * DigestInfo and return its DER encoding. This is a necessary step for 
 * RSA signature (both generating and verifying) - the output of this 
 * routine is what gets encrypted during signing, and what is expected when
 * verifying (i.e., decrypting the signature).
 *
 * A good guess for the length of the output digestInfo is the size of the
 * key being used to sign/verify. The digest can never be larger than that. 
 */
CSSM_RETURN generateDigestInfo(
	const void		*msgDigest,
	size_t			digestLen,
	CSSM_ALGORITHMS	digestAlg,		// CSSM_ALGID_SHA1, etc.
	CssmOwnedData	&encodedInfo,
	size_t			maxEncodedSize)
{
	if(digestAlg == CSSM_ALGID_NONE) {
		/* special case, no encode, just copy */
		encodedInfo.copy(msgDigest, digestLen);
		return 0;
	}
	
	DigestInfo	info;
	info.digest.Set((char *)msgDigest, digestLen);
	info.digestAlgorithm = new DigestAlgorithmIdentifier;
	
	switch(digestAlg) {
		case CSSM_ALGID_MD5:
			info.digestAlgorithm->algorithm = md5;
			break;
		case CSSM_ALGID_MD2:
			info.digestAlgorithm->algorithm = md2;
			break;
		case CSSM_ALGID_SHA1:
			info.digestAlgorithm->algorithm = sha_1;
			break;
		default:
			return CSSMERR_CSP_INVALID_ALGORITHM;
	}
	nullAlgParams(*info.digestAlgorithm);
	try {
		SC_encodeAsnObj(info, encodedInfo, maxEncodedSize);
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

unsigned sizeofAsnBits(
	AsnBits &bits)
{
	return (bits.BitLen() * 8) + 4;
}

unsigned sizeofAsnOcts(
	AsnOcts &octs)
{
	return octs.Len() + 4;
}


/***
 *** DSA
 ***/

/* SNACC DSAAlgorithmId <--> DSA->{p,g,q} */
static DSAAlgorithmId *dsaToSnaccAlgId(
	const DSA *openKey)
{
	try {
		DSAAlgorithmId *algId = new DSAAlgorithmId;
		
		algId->algorithm = dsa_bsafe;
		algId->params = new DSABsafeParams;
		algId->params->keySizeInBits = BN_num_bits(openKey->p);
		bnToBigIntStr(openKey->p, algId->params->p);
		bnToBigIntStr(openKey->q, algId->params->q);
		bnToBigIntStr(openKey->g, algId->params->g);
		return algId;
	}
	catch(...) {
		return NULL;
	}
}

static CSSM_RETURN snaccAlgIdToDsa(
	DSAAlgorithmId &algId,
	DSA *openKey)
{
	if(algId.algorithm != dsa_bsafe) {
		sslSnaccDebug("snaccAlgIdToDsa: bad algorithm");
		return CSSMERR_CSP_ALGID_MISMATCH;
	}
	if(algId.params == NULL) {
		sslSnaccDebug("snaccAlgIdToDsa: bad params");
		return CSSMERR_CSP_INVALID_KEY;
	}
	openKey->p = bigIntStrToBn(algId.params->p);
	openKey->q = bigIntStrToBn(algId.params->q);
	openKey->g = bigIntStrToBn(algId.params->g);
	return 0;
}

static unsigned sizeOfDsaAlg(
	const DSAAlgorithmId &algId)
{
	return sizeofBigInt(algId.params->p) + 
		   sizeofBigInt(algId.params->g) +
		   sizeofBigInt(algId.params->q) +
		   30;
}

CSSM_RETURN DSAPublicKeyDecode(
	DSA 			*openKey, 
	unsigned char 	*p, 
	unsigned		length)
{
	DSAPublicKey snaccPubKey;
	CSSM_RETURN rtn;
	
	CssmData cData(p, length);
	try {
		SC_decodeAsnObj(cData, snaccPubKey);
		rtn = snaccAlgIdToDsa(*snaccPubKey.dsaAlg, openKey);
		if(rtn) {
			return rtn;
		}
		
		/* inside of snaccPubKey.publicKey is the DER-encoding of a BigIntegerStr */
		char *keyOcts = (char *)snaccPubKey.publicKey.BitOcts();
		CssmData kData(keyOcts, (snaccPubKey.publicKey.BitLen() + 7) / 8);
		BigIntegerStr pubKeyOcts;
		SC_decodeAsnObj(kData, pubKeyOcts);
		openKey->pub_key = bigIntStrToBn(pubKeyOcts);

		if(openKey->pub_key == NULL) {
			return CSSMERR_CSP_INVALID_KEY;
		}
		return 0;
	}
	catch(...) {
		return CSSMERR_CSP_INVALID_KEY;
	}
}

CSSM_RETURN	DSAPublicKeyEncode(
	DSA 			*openKey, 
	CssmOwnedData	&encodedKey)
{
	try {
		/* First convert into a snacc-style public key */
		DSAPublicKey snaccPubKey;

		snaccPubKey.dsaAlg = dsaToSnaccAlgId(openKey);
		if(snaccPubKey.dsaAlg == NULL) {
			return CSSMERR_CSP_MEMORY_ERROR;
		}
		
		/* 
		 * publicKey is the DER-encoding of a BigIntegerStr wrapped in 
		 * an AsnBits
		 */
		BigIntegerStr pubKeyInt;
		bnToBigIntStr(openKey->pub_key, pubKeyInt);
		unsigned maxSize = sizeofBigInt(pubKeyInt);
		SC_encodeAsnObj(pubKeyInt, encodedKey, maxSize);

		/* that encoding goes into DSAPublicKey.publicKey */
		snaccPubKey.publicKey.Set((char *)encodedKey.data(), encodedKey.length() * 8);
		
		/* conservative guess for max size of encoded key */
		maxSize = sizeOfDsaAlg(*snaccPubKey.dsaAlg) +
				  sizeofAsnBits(snaccPubKey.publicKey) +
				  20;
					   
		/* DER encode */
		encodedKey.reset();
		SC_encodeAsnObj(snaccPubKey, encodedKey, maxSize);
		return 0;
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
}

CSSM_RETURN DSAPrivateKeyDecode(
	DSA 			*openKey, 
	unsigned char 	*p, 
	unsigned		length)
{
	DSAPrivateKey snaccPrivKey;
	int rtn;
	
	CssmData cData(p, length);
	try {
		SC_decodeAsnObj(cData, snaccPrivKey);
		openKey->version = snaccPrivKey.version;
		
		rtn = snaccAlgIdToDsa(*snaccPrivKey.dsaAlg, openKey);
		if(rtn) {
			return rtn;
		}
		
		/* snaccPrivKey.privateKey is the DER-encoding of a DSAPrivateKeyOcts... */
		char *keyOcts = snaccPrivKey.privateKey;
		CssmData kData(keyOcts, snaccPrivKey.privateKey.Len());
		DSAPrivateKeyOcts privKeyOcts;
		SC_decodeAsnObj(kData, privKeyOcts);
		
		openKey->priv_key = bigIntStrToBn(privKeyOcts.privateKey);
		if(openKey->priv_key == NULL) {
			return CSSMERR_CSP_INVALID_KEY;
		}
		return 0;
	}
	catch(...) {
		return CSSMERR_CSP_INVALID_KEY;
	}
}

CSSM_RETURN	DSAPrivateKeyEncode(
	DSA 			*openKey, 
	CssmOwnedData	&encodedKey)
{
	try {
		/* First convert into a snacc-style private key */
		DSAPrivateKey snaccPrivKey;

		snaccPrivKey.version = openKey->version;
		snaccPrivKey.dsaAlg = dsaToSnaccAlgId(openKey);
		if(snaccPrivKey.dsaAlg == NULL) {
			return CSSMERR_CSP_MEMORY_ERROR;
		}
		
		/* DSAPrivateKey.privateKey is the DER-encoding of one of these... */
		DSAPrivateKeyOcts privKeyOcts;
		bnToBigIntStr(openKey->priv_key, privKeyOcts.privateKey);

		/* conservative guess for max size of encoded privKey bits */
		unsigned maxSize = sizeofBigInt(privKeyOcts.privateKey) +
					   10;		// includes the to-be-generated algId
					   
		/* DER encode */
		SC_encodeAsnObj(privKeyOcts, encodedKey, maxSize);

		/* that encoding goes into DSAPrivateKey.privateKey */
		snaccPrivKey.privateKey.Set((char *)encodedKey.data(), encodedKey.length());
		
		/* conservative guess for max size of the whole thing */
		maxSize = maxSize + 				// what we just did
				sizeOfDsaAlg(*snaccPrivKey.dsaAlg) +
				40;
					   
		/* DER encode */
		encodedKey.reset();
		SC_encodeAsnObj(snaccPrivKey, encodedKey, maxSize);
		return 0;
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
}

CSSM_RETURN DSASigEncode(
	DSA_SIG			*openSig,
	CssmOwnedData	&encodedSig)
{
	/* First convert into a snacc-style sig */
	DSASignature	snaccSig;
	
	try {
		bnToBigIntStr(openSig->r, snaccSig.r);
		bnToBigIntStr(openSig->s, snaccSig.s);
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	
	/* conservative guess for max size of encoded key */
	unsigned maxSize = sizeofBigInt(snaccSig.r) +
					   sizeofBigInt(snaccSig.s) +
					   10;
					   
	/* DER encode */
	try {
		SC_encodeAsnObj(snaccSig, encodedSig, maxSize);
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

CSSM_RETURN DSASigDecode(
	DSA_SIG 		*openSig, 
	const void 		*p, 
	unsigned		length)
{
	DSASignature snaccSig;
	
	CssmData cData((char *)p, length);
	try {
		SC_decodeAsnObj(cData, snaccSig);
	}
	catch(...) {
		return CSSMERR_CSP_INVALID_SIGNATURE;
	}
	try {
		openSig->r = bigIntStrToBn(snaccSig.r);
		openSig->s = bigIntStrToBn(snaccSig.s);
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

CSSM_RETURN DHPrivateKeyDecode(
	DH	 			*openKey, 
	unsigned char 	*p, 
	unsigned 		length)
{
	DHPrivateKey snaccPrivKey;
	CssmData cData(p, length);
	try {
		SC_decodeAsnObj(cData, snaccPrivKey);
	}
	catch(...) {
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	/* verify alg identifier */
	if(snaccPrivKey.dHOid != dhKeyAgreement) {
		sslSnaccDebug("DHPrivateKeyDecode: bad privateKeyAlgorithm");
		return CSSMERR_CSP_ALGID_MISMATCH;
	}

	DHParameter	*params = snaccPrivKey.params;
	if(params == NULL) {
		/* not optional */
		sslSnaccDebug("DHPrivateKeyDecode: missing key params");
		return CSSMERR_CSP_INVALID_KEY;
	}
	
	/* convert snaccPrivKey fields to DH key fields */
	try {
		openKey->priv_key = bigIntStrToBn(snaccPrivKey.secretPart);
		openKey->p	      = bigIntStrToBn(params->prime);
		openKey->g 	      = bigIntStrToBn(params->base);
		/* TBD - ignore privateValueLength for now */
	}
	catch(...) {
		/* FIXME - bad sig? memory? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

CSSM_RETURN	DHPrivateKeyEncode(
	DH	 			*openKey, 
	CssmOwnedData	&encodedKey)
{
	/* First convert into a snacc-style private key */
	DHPrivateKey snaccPrivKey;
	snaccPrivKey.params = new DHParameter;
	DHParameter *params = snaccPrivKey.params;
	
	try {
		snaccPrivKey.dHOid.Set(dhKeyAgreement_arc);
		bnToBigIntStr(openKey->priv_key, snaccPrivKey.secretPart);
		bnToBigIntStr(openKey->p, params->prime);
		bnToBigIntStr(openKey->g, params->base);
		if(openKey->length) {
			/* actually currently not supported */
			params->privateValueLength = new BigIntegerStr();
			snaccIntToBigIntegerStr(openKey->length, *params->privateValueLength);
		}
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	
	/* conservative guess for max size of encoded key */
	unsigned maxSize = sizeofBigInt(snaccPrivKey.secretPart) +
					   sizeofBigInt(params->prime) +
					   sizeofBigInt(params->base) +
					   60;		// includes dHOid, tags, lenghts
	if(openKey->length) {
		maxSize += sizeofBigInt(*params->privateValueLength);
	}
					   
	/* DER encode */
	try {
		SC_encodeAsnObj(snaccPrivKey, encodedKey, maxSize);
	}
	catch(...) {
		/* ? */
		return CSSMERR_CSP_MEMORY_ERROR;
	}
	return 0;
}

