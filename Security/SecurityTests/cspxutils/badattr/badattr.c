/*
 * badattr.c - verify proper rejection of bad key attribute bits
 */
 
#ifdef pcode

partial description...

for each asymmetric alg {
	gen pub key with KEYUSE_ENCRYPT
		make sure you cannot use it for vfy or decrypt
		make sure you cannot use it for encrypting with other alg
	gen priv key with KEYUSE_DECRYPT
		make sure you cannot use it for sign or decrypt
		make sure you cannot use it for decrypting with other alg
	gen priv key with KEYUSE_SIGN
		make sure you cannot use it for encrypt or decrypt
	gen pub with KEYUSE_VERIFY 
		make sure you cannot use it for encrypt or decrypt

}

#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include "cspwrap.h"
#include "common.h"
#include "cspdlTesting.h"
/*
 * Enumerate algs our own way to allow iteration.
 */
typedef unsigned privAlg;
enum {
	ALG_ASC = 1,
	ALG_DES,
	ALG_RC2,
	ALG_RC4,
	ALG_RC5,
	ALG_3DES,
	ALG_AES,
	ALG_RSA,
	ALG_FEE,
	ALG_ECDSA,
	ALG_DSA
};

#define SYM_FIRST		ALG_ASC
#define SYM_LAST		ALG_AES
#define ASYM_FIRST		ALG_RSA
#define ASYM_LAST		ALG_ECDSA		/* DSA if we're patient */

/*
 * ops expressed at bitfields
 */
#define OP_SIGN		0x0001
#define OP_VERIFY	0x0002
#define OP_ENCRYPT	0x0004
#define OP_DECRYPT	0x0008
#define OP_GENMAC	0x0010
#define OP_VFYMAC	0x0020

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   s(ymmetric only)\n");
	printf("   a(symmetric only)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

/*
 * Common, flexible, error-tolerant symmetric key generator.
 */
static int genSymKey(
	CSSM_CSP_HANDLE 	cspHand,
	CSSM_KEY_PTR		symKey,
	uint32 				alg,
	const char			*keyAlgStr,
	uint32 				keySizeInBits,
	CSSM_KEYATTR_FLAGS	keyAttr,
	CSSM_KEYUSE			keyUsage,
	CSSM_RETURN			expectRtn,
	CSSM_BOOL			quiet,
	CSSM_BOOL 			freeKey,			// true: free the key on exit
	const char			*testStr)
{
	CSSM_RETURN			crtn;
	CSSM_CC_HANDLE 		ccHand;
	CSSM_DATA			dummyLabel = {4, (uint8 *)"foo"};
	int					irtn;
	
	memset(symKey, 0, sizeof(CSSM_KEY));
	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		alg,
		keySizeInBits,	// keySizeInBits
		NULL,			// Seed
		NULL,			// Salt
		NULL,			// StartDate
		NULL,			// EndDate
		NULL,			// Params
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateKeyGenContext", crtn);
		return testError(quiet);
	}
	crtn = CSSM_GenerateKey(ccHand,
		keyUsage,
		keyAttr,
		&dummyLabel,
		NULL,			// ACL
		symKey);
	if(crtn != expectRtn) {
		printf("***Testing %s for alg %s:\n", testStr, keyAlgStr);
		printf("   CSSM_GenerateKey: expect %s\n",	cssmErrToStr(expectRtn));
		printf("   CSSM_GenerateKey: got    %s\n",  cssmErrToStr(crtn));
		irtn = testError(quiet);
	}
	else {
		irtn = 0;
	}
	CSSM_DeleteContext(ccHand);
	if(freeKey && (crtn == CSSM_OK)) {
		cspFreeKey(cspHand, symKey);
	}
	return irtn;
}

/*
 * Common, flexible, error-tolerant key pair generator.
 */
static int genKeyPair(
	CSSM_CSP_HANDLE 	cspHand,
	uint32 				algorithm,
	const char			*keyAlgStr,
	uint32 				keySizeInBits,
	CSSM_KEY_PTR 		pubKey,			
	CSSM_KEYATTR_FLAGS 	pubKeyAttr,
	CSSM_KEYUSE 		pubKeyUsage,	
	CSSM_KEY_PTR 		privKey,		
	CSSM_KEYATTR_FLAGS 	privKeyAttr,
	CSSM_KEYUSE 		privKeyUsage,	
	CSSM_RETURN			expectRtn,
	CSSM_BOOL 			quiet,
	CSSM_BOOL 			freeKeys,			// true: free the keys on exit
	const char			*testStr)
{
	CSSM_RETURN			crtn;
	CSSM_CC_HANDLE 		ccHand;
	CSSM_DATA			keyLabelData = {4, (uint8 *)"foo"};
	int					irtn;
	
	memset(pubKey, 0, sizeof(CSSM_KEY));
	memset(privKey, 0, sizeof(CSSM_KEY));

	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		algorithm,
		keySizeInBits,
		NULL,					// Seed
		NULL,					// Salt
		NULL,					// StartDate
		NULL,					// EndDate
		NULL,					// Params
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateKeyGenContext", crtn);
		return testError(quiet);
	}

	/* post-context-create algorithm-specific stuff */
	switch(algorithm) {
		case CSSM_ALGID_RSA:
			break;
		 
		 case CSSM_ALGID_DSA:
			/* 
			 * extra step - generate params - this just adds some
			 * info to the context
			 */
			{
				CSSM_DATA dummy = {0, NULL};
				crtn = CSSM_GenerateAlgorithmParams(ccHand, 
					keySizeInBits, &dummy);
				if(crtn) {
					printError("CSSM_GenerateAlgorithmParams", crtn);
					return testError(quiet);
				}
				appFreeCssmData(&dummy, CSSM_FALSE);
			}
			break;
		default:
			break;
	}
	
	crtn = CSSM_GenerateKeyPair(ccHand,
		pubKeyUsage,
		pubKeyAttr,
		&keyLabelData,
		pubKey,
		privKeyUsage,
		privKeyAttr,
		&keyLabelData,			// same labels
		NULL,					// CredAndAclEntry
		privKey);
	if(crtn != expectRtn) {
		printf("***Testing %s for alg %s:\n", testStr, keyAlgStr);
		printf("   CSSM_GenerateKeyPair: expect %s\n",	cssmErrToStr(expectRtn));
		printf("   CSSM_GenerateKeyPair: got    %s\n",  cssmErrToStr(crtn));
		irtn = testError(quiet);
	}
	else {
		irtn = 0;
	}
	CSSM_DeleteContext(ccHand);
	if(freeKeys && (crtn == CSSM_OK)) {
		cspFreeKey(cspHand, pubKey);
		cspFreeKey(cspHand, privKey);
	}
	return irtn;
}

/* 
 * Perform NULL wrap, generally expecting an error (either 
 * CSSMERR_CSP_INVALID_KEYATTR_MASK, if the raw key bits should be inaccessible,
 * or CSSMERR_CSP_INVALID_KEY_REFERENCE, if the key's header has been munged.)
 */
int nullWrapTest(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_KEY_PTR	key,
	CSSM_BOOL 		quiet,
	CSSM_RETURN		expectRtn,
	const char		*keyAlgStr,
	const char 		*testStr)
{
	CSSM_CC_HANDLE				ccHand;
	CSSM_RETURN					crtn;
	CSSM_ACCESS_CREDENTIALS		creds;
	CSSM_KEY					wrappedKey;		// should not get created
	int							irtn;
	
	memset(&wrappedKey, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
			CSSM_ALGID_NONE,
			CSSM_ALGMODE_NONE,
			&creds,				// passPhrase,
			NULL,				// wrappingKey,
			NULL,				// IV
			CSSM_PADDING_NONE,	
			0,					// Params
			&ccHand);
	if(crtn) {
		printError("cspWrapKey/CreateContext", crtn);
		return testError(quiet);
	}
	crtn = CSSM_WrapKey(ccHand,
		&creds,
		key,
		NULL,				// DescriptiveData
		&wrappedKey);
	if(crtn != expectRtn) {
		printf("***Testing %s for alg %s:\n", testStr, keyAlgStr);
		printf("   CSSM_WrapKey: expect %s\n",	cssmErrToStr(expectRtn));
		printf("   CSSM_WrapKey: got    %s\n",  cssmErrToStr(crtn));
		irtn = testError(quiet);
	}
	else {
		irtn = 0;
	}
	CSSM_DeleteContext(ccHand);
	return irtn;
}

/* 
 * Attempt to wrap incoming key with a DES key that we generate. Expect
 * CSSMERR_CSP_INVALID_KEYATTR_MASK since the unwrapped key is marked 
 * !EXTRACTABLE.
 */
#define WRAPPING_KEY_ALG	CSSM_ALGID_DES
#define WRAPPING_KEY_SIZE	CSP_DES_KEY_SIZE_DEFAULT

static int badWrapTest(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_KEY_PTR		unwrappedKey,
	CSSM_KEYBLOB_FORMAT	wrapForm,
	CSSM_BOOL 			quiet,
	const char			*keyAlgStr,
	const char			*testStr)
{
	CSSM_CC_HANDLE				ccHand;
	CSSM_RETURN					crtn;
	CSSM_ACCESS_CREDENTIALS		creds;
	CSSM_KEY					wrappedKey;		// should not get created
	CSSM_KEY					wrappingKey;
	int							irtn;
		
	/* first generate a DES wrapping key */
	if(genSymKey(cspHand, &wrappingKey, CSSM_ALGID_DES, "DES", 
			CSP_DES_KEY_SIZE_DEFAULT, 
			CSSM_KEYATTR_RETURN_REF,
			CSSM_KEYUSE_ANY, CSSM_OK, quiet, 
			CSSM_FALSE, "not a test case")) {
		return 1;
	}

	memset(&wrappedKey, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	
	/* symmetric wrapping context */
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
			CSSM_ALGID_DES,
			CSSM_ALGMODE_CBCPadIV8,
			&creds,				// passPhrase,
			&wrappingKey,	
			NULL,				// IV
			CSSM_PADDING_PKCS5,	
			0,					// Params
			&ccHand);
	if(crtn) {
		printError("cspWrapKey/CreateContext", crtn);
		return testError(quiet);
	}
	
	/* do it, demand error */
	crtn = CSSM_WrapKey(ccHand,
		&creds,
		unwrappedKey,
		NULL,				// DescriptiveData
		&wrappedKey);
	if(crtn != CSSMERR_CSP_INVALID_KEYATTR_MASK) {
		printf("***Testing %s for alg %s:\n", testStr, keyAlgStr);
		printf("   CSSM_WrapKey: expect CSSMERR_CSP_INVALID_KEYATTR_MASK, got %s\n",
			cssmErrToStr(crtn));
		irtn = testError(quiet);
	}
	else {
		irtn = 0;
	}
	CSSM_DeleteContext(ccHand);
	cspFreeKey(cspHand, &wrappingKey);
	return irtn;
}


/*
 * Note for these op stubs, the data, mode, padding, etc. are unimportant as 
 * the ops are expected to fail during key extraction. 
 */ 
static int badEncrypt(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_KEY_PTR	key,
	const char		*keyAlgStr,
	CSSM_ALGORITHMS	opAlg,
	CSSM_RETURN		expectRtn,
	CSSM_BOOL		quiet,
	const char		*goodUseStr,
	const char		*badUseStr)
{
	CSSM_CC_HANDLE 	cryptHand;
	CSSM_DATA		ptext = {4, (uint8 *)"foo"};
	CSSM_DATA		ctext = {0, NULL};
	CSSM_DATA		remData = {0, NULL};
	CSSM_RETURN		crtn;
	CSSM_SIZE		bytesEncrypted;
	int				irtn;
	
	cryptHand = genCryptHandle(cspHand, opAlg, CSSM_ALGMODE_NONE, CSSM_PADDING_NONE,
		key, NULL /* key2 */, NULL /* iv */, 0, 0);
	if(cryptHand == 0) {
		return testError(quiet);
	}
	crtn = CSSM_EncryptData(cryptHand, &ptext, 1, &ctext, 1, &bytesEncrypted, &remData);
	if(crtn != expectRtn) {
		printf("***Testing %s key w/%s during %s:\n", keyAlgStr, goodUseStr, badUseStr);
		printf("   CSSM_EncryptData: expect %s\n",	cssmErrToStr(expectRtn));
		printf("   CSSM_EncryptData: got    %s\n",  cssmErrToStr(crtn));
		irtn = testError(quiet);
	}
	else {
		irtn = 0;
	}
	/* assume no ctext or remdata - OK? */
	CSSM_DeleteContext(cryptHand);
	return irtn;
}

static int badDecrypt(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_KEY_PTR	key,
	const char		*keyAlgStr,
	CSSM_ALGORITHMS	opAlg,
	CSSM_RETURN		expectRtn,
	CSSM_BOOL		quiet,
	const char		*goodUseStr,
	const char		*badUseStr)
{
	CSSM_CC_HANDLE 	cryptHand;
	CSSM_DATA		ctext = {4, (uint8 *)"foo"};
	CSSM_DATA		ptext = {0, NULL};
	CSSM_DATA		remData = {0, NULL};
	CSSM_RETURN		crtn;
	CSSM_SIZE		bytesDecrypted;
	int				irtn;
	
	
	cryptHand = genCryptHandle(cspHand, opAlg, CSSM_ALGMODE_NONE, CSSM_PADDING_NONE,
		key, NULL /* key2 */, NULL /* iv */, 0, 0);
	if(cryptHand == 0) {
		return testError(quiet);
	}
	crtn = CSSM_DecryptData(cryptHand, &ctext, 1, &ptext, 1, &bytesDecrypted, &remData);
	if(crtn != expectRtn) {
		printf("***Testing %s key w/%s during %s:\n", keyAlgStr, goodUseStr, badUseStr);
		printf("   CSSM_DecryptData: expect %s\n",	cssmErrToStr(expectRtn));
		printf("   CSSM_DecryptData: got    %s\n",  cssmErrToStr(crtn));
		irtn = testError(quiet);
	}
	else {
		irtn = 0;
	}
	/* assume no ptext or remdata - OK? */
	CSSM_DeleteContext(cryptHand);
	return irtn;
}

/*
 * Given a reference key (any class, any alg), attempt to perform null wrap after
 * munging various fields in the header. Every attempt should result in
 * CSSMERR_CSP_INVALID_KEY_REFERENCE.
 */
static int badHdrTest(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_KEY_PTR		key, 
	CSSM_BOOL			quiet,
	const char			*keyAlgStr)
{
	CSSM_KEYHEADER	*hdr = &key->KeyHeader;
	CSSM_KEYHEADER	savedHdr = *hdr;

	hdr->HeaderVersion++;
	if(nullWrapTest(cspHand, key, quiet, CSSMERR_CSP_INVALID_KEY_REFERENCE,
			keyAlgStr, "Munged hdr(HeaderVersion)")) {
		return 1;
	}
	*hdr = savedHdr;

	hdr->CspId.Data1++;
	if(nullWrapTest(cspHand, key, quiet, CSSMERR_CSP_INVALID_KEY_REFERENCE,
			keyAlgStr, "Munged hdr(CspId.Data1)")) {
		return 1;
	}
	*hdr = savedHdr;

	/* can't test BlobType for Format, they're known to differ */
	
	hdr->AlgorithmId++;
	if(nullWrapTest(cspHand, key, quiet, CSSMERR_CSP_INVALID_KEY_REFERENCE,
			keyAlgStr, "Munged hdr(AlgorithmId)")) {
		return 1;
	}
	*hdr = savedHdr;

	/* have to come up with valid KeyClass here */
	switch(hdr->KeyClass) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			hdr->KeyClass = CSSM_KEYCLASS_PRIVATE_KEY; break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			hdr->KeyClass = CSSM_KEYCLASS_SESSION_KEY; break;
		case CSSM_KEYCLASS_SESSION_KEY:
			hdr->KeyClass = CSSM_KEYCLASS_PUBLIC_KEY; break;
		default:
			printf("***BRZZAP! badHdrTest needs work\n");
			exit(1);
	}
	if(nullWrapTest(cspHand, key, quiet, CSSMERR_CSP_INVALID_KEY_REFERENCE,
			keyAlgStr, "Munged hdr(KeyClass)")) {
		return 1;
	}
	*hdr = savedHdr;

	hdr->LogicalKeySizeInBits++;
	if(nullWrapTest(cspHand, key, quiet, CSSMERR_CSP_INVALID_KEY_REFERENCE,
			keyAlgStr, "Munged hdr(LogicalKeySizeInBits)")) {
		return 1;
	}
	*hdr = savedHdr;

	hdr->KeyAttr++;
	if(nullWrapTest(cspHand, key, quiet, CSSMERR_CSP_INVALID_KEY_REFERENCE,
			keyAlgStr, "Munged hdr(KeyAttr)")) {
		return 1;
	}
	*hdr = savedHdr;

	hdr->StartDate.Day[0]++;
	if(nullWrapTest(cspHand, key, quiet, CSSMERR_CSP_INVALID_KEY_REFERENCE,
			keyAlgStr, "Munged hdr(StartDate.Day)")) {
		return 1;
	}
	*hdr = savedHdr;

	hdr->EndDate.Year[1]++;
	if(nullWrapTest(cspHand, key, quiet, CSSMERR_CSP_INVALID_KEY_REFERENCE,
			keyAlgStr, "Munged hdr(EndDate.Year)")) {
		return 1;
	}
	*hdr = savedHdr;

	hdr->WrapAlgorithmId++;
	if(nullWrapTest(cspHand, key, quiet, CSSMERR_CSP_INVALID_KEY_REFERENCE,
			keyAlgStr, "Munged hdr(WrapAlgorithmId)")) {
		return 1;
	}
	*hdr = savedHdr;

	hdr->WrapMode++;
	if(nullWrapTest(cspHand, key, quiet, CSSMERR_CSP_INVALID_KEY_REFERENCE,
			keyAlgStr, "Munged hdr(WrapMode)")) {
		return 1;
	}
	*hdr = savedHdr;

	return 0;
}

/*
 * Given some op alg, return a different op alg which is of the same class
 * but should not work with an opAlg-related key.
 */
CSSM_ALGORITHMS badOpAlg(
	CSSM_ALGORITHMS	opAlg)
{
	switch(opAlg) {
		/* symmetric block ciphers */
		case CSSM_ALGID_DES: return CSSM_ALGID_3DES_3KEY_EDE;
		case CSSM_ALGID_3DES_3KEY_EDE: return CSSM_ALGID_RC2;
		case CSSM_ALGID_RC2: return CSSM_ALGID_RC5;
		case CSSM_ALGID_RC5: return CSSM_ALGID_AES;
		case CSSM_ALGID_AES: return CSSM_ALGID_DES;

		/* symmetric stream ciphers */
		case CSSM_ALGID_ASC: return CSSM_ALGID_RC4;	
		case CSSM_ALGID_RC4: return CSSM_ALGID_ASC;	
		
		/* asymmetric ciphers */
		case CSSM_ALGID_RSA: return CSSM_ALGID_FEEDEXP;
		case CSSM_ALGID_FEEDEXP: return CSSM_ALGID_RSA;
		
		/* digital signature */
		case CSSM_ALGID_SHA1WithRSA: return CSSM_ALGID_SHA1WithDSA;
		case CSSM_ALGID_SHA1WithDSA: return CSSM_ALGID_SHA1WithECDSA;
		case CSSM_ALGID_SHA1WithECDSA: return CSSM_ALGID_SHA1WithRSA;
		
		default: printf("***BRRZAP! badOpAlg needs work.\n"); exit(1);
	}
	/* NOT REACHED */
	return 0;
}

/*
 * -- Generate symmetric key with specified alg and usage;
 * -- Verify that it can't be used for any of the ops specified 
 *    in badOpFlags using goodEncrAlg/goodSignAlg;
 * -- Verify that it can't be used for goodOp/badAlg;
 *
 * Used by symUsageTest().
 *
 */
#define SYM_USAGE_ENABLE	1
 
static int badSymUsage(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_ALGORITHMS	keyAlg,			// alg of the key
	const char		*keyAlgStr, 
	uint32 			keySizeInBits,
	CSSM_KEYUSE		keyUse,			// gen key with this usage
	CSSM_ALGORITHMS	goodEncrAlg,	// key is good for this encryption alg
	CSSM_ALGORITHMS	goodSignAlg,	// key is good for this sign alg (may not be used)
	unsigned		badOpFlags,		// array of (OP_DECRYPT,...)
	unsigned		goodOp,			// one good op...
	CSSM_ALGORITHMS	badAlg,			// ..which fails for this alg
	CSSM_BOOL		quiet,
	const char		*useStr)
{
	CSSM_KEY		symKey;
	int				irtn;
	
	if(genSymKey(cspHand, &symKey, keyAlg, keyAlgStr, keySizeInBits,
			CSSM_KEYATTR_RETURN_REF, keyUse, CSSM_OK, quiet, CSSM_FALSE, useStr)) {
		return 1;
	}
	#if		SYM_USAGE_ENABLE
	if(!quiet) {
		printf("         ...testing key usage\n");
	}
	if(badOpFlags & OP_ENCRYPT) {
		irtn = badEncrypt(cspHand, &symKey, keyAlgStr, goodEncrAlg,
			CSSMERR_CSP_KEY_USAGE_INCORRECT, quiet, useStr, "ENCRYPT");
		if(irtn) {
			goto abort;
		}
	}
	if(badOpFlags & OP_DECRYPT) {
		irtn = badDecrypt(cspHand, &symKey, keyAlgStr, goodEncrAlg,
			CSSMERR_CSP_KEY_USAGE_INCORRECT, quiet, useStr, "DECRYPT");
		if(irtn) {
			goto abort;
		}
	}
	#endif	/* SYM_USAGE_ENABLE */
	
	/* now do a good op with an incorrect algorithm */
	if(!quiet) {
		printf("         ...testing key/algorithm match\n");
	}
	if(goodOp & OP_ENCRYPT) {
		irtn = badEncrypt(cspHand, &symKey, keyAlgStr, badAlg,
			CSSMERR_CSP_ALGID_MISMATCH, quiet, useStr, "ENCRYPT w/bad alg");
		if(irtn) {
			goto abort;
		}
	}
	if(goodOp & OP_DECRYPT) {
		irtn = badDecrypt(cspHand, &symKey, keyAlgStr, badAlg,
			CSSMERR_CSP_ALGID_MISMATCH, quiet, useStr, "DECRYPT w/bad alg");
		if(irtn) {
			goto abort;
		}
	}
abort:
	cspFreeKey(cspHand, &symKey);
	return irtn;
}

/* 
 * Verify symmetric key usage behavior:
 *
 *	gen key with KEYUSE_ENCRYPT
 *		make sure you can't use it for decrypt
 *		make sure you can't use it for encrypting with other alg
 *	gen key with KEYUSE_DECRYPT
 *		make sure you can't use it for encrypt
 *		make sure you can't use it for decrypting with other alg
 *	gen key with KEYUSE_SIGN (mac)
 *		make sure you can't use it for encrypt or decrypt
 *	gen key with KEYUSE_VERIFY (mac verify)
 *		make sure you can't use it for encrypt or decrypt
 */
int symUsageTest(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_ALGORITHMS	keyAlg,
	const char		*keyAlgStr,
	CSSM_ALGORITHMS	encrAlg,
	CSSM_ALGORITHMS	signAlg,
	uint32			keySizeInBits,
	CSSM_BOOL 		quiet)
{
	if(!quiet) {
		printf("      ...testing encrypt-enabled key\n");
	}
	if(badSymUsage(cspHand, keyAlg, keyAlgStr, keySizeInBits, CSSM_KEYUSE_ENCRYPT,
			encrAlg, signAlg, OP_DECRYPT, OP_ENCRYPT, badOpAlg(encrAlg), 
			quiet, "ENCRYPT")) {
		return 1;
	}
	if(!quiet) {
		printf("      ...testing decrypt-enabled key\n");
	}
	if(badSymUsage(cspHand, keyAlg, keyAlgStr, keySizeInBits, CSSM_KEYUSE_DECRYPT,
			encrAlg, signAlg, OP_ENCRYPT, OP_DECRYPT, badOpAlg(encrAlg), 
			quiet, "DECRYPT")) {
		return 1;
	}
	return 0;
}

/* 
 * Verify symmetric key attribute behavior:
 *
 *	check that you can not gen a key with {
 *		CSSM_KEYATTR_ALWAYS_SENSITIVE
 *		CSSM_KEYATTR_NEVER_EXTRACTABLE
 *		CSSM_KEYATTR_PERMANENT
 *		CSSM_KEYATTR_PRIVATE
 *		CSSM_KEYATTR_RETURN_DATA | !CSSM_KEYATTR_EXTRACTABLE
 *		CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_SENSITIVE
 *		CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_RETURN_REF
 *	}
 */
int symAttrTest(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_ALGORITHMS	alg,
	const char		*keyAlgStr,
	uint32			keySizeInBits,
	CSSM_BOOL		bareCsp,
	CSSM_BOOL 		quiet)
{
	CSSM_KEY key;
	
	if(!quiet) {
		printf("      ...testing key attr\n");
	}
	if(bareCsp || CSPDL_ALWAYS_SENSITIVE_CHECK) {
		if(genSymKey(cspHand, &key, alg, keyAlgStr, keySizeInBits, 
				CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_ALWAYS_SENSITIVE,
				CSSM_KEYUSE_ANY, CSSMERR_CSP_INVALID_KEYATTR_MASK, quiet,
				CSSM_TRUE, "ALWAYS_SENSITIVE")) {
			return 1;
		}	
	}
	if(bareCsp || CSPDL_NEVER_EXTRACTABLE_CHECK) {
		if(genSymKey(cspHand, &key, alg, keyAlgStr, keySizeInBits, 
				CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_NEVER_EXTRACTABLE,
				CSSM_KEYUSE_ANY, CSSMERR_CSP_INVALID_KEYATTR_MASK, quiet,
				CSSM_TRUE, "NEVER_EXTRACTABLE")) {
			return 1;
		}
	}
	/*
	 * bare CSP : CSSMERR_CSP_UNSUPPORTED_KEYATTR_MASK
	 * CSPDL    : CSSMERR_CSP_MISSING_ATTR_DL_DB_HANDLE
	 */
	if(genSymKey(cspHand, &key, alg, keyAlgStr, keySizeInBits, 
			CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT,
			CSSM_KEYUSE_ANY,
			bareCsp ?  CSSMERR_CSP_UNSUPPORTED_KEYATTR_MASK : 
				CSSMERR_CSP_MISSING_ATTR_DL_DB_HANDLE,
			quiet,
			CSSM_TRUE, "PERMANENT")) {
		return 1;
	}
	if(genSymKey(cspHand, &key, alg, keyAlgStr, keySizeInBits, 
			CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PRIVATE,
			CSSM_KEYUSE_ANY, CSSMERR_CSP_UNSUPPORTED_KEYATTR_MASK, quiet,
			CSSM_TRUE, "PRIVATE")) {
		return 1;
	}
	if(bareCsp) {
		/* CSPDL doesn't support RETURN_DATA */
		if(genSymKey(cspHand, &key, alg, keyAlgStr, keySizeInBits, 
				CSSM_KEYATTR_RETURN_DATA /* and !extractable */,
				CSSM_KEYUSE_ANY, 
				CSSMERR_CSP_INVALID_KEYATTR_MASK, quiet,
				CSSM_TRUE, "RETURN_DATA | !EXTRACTABLE")) {
			return 1;
		}
		if(genSymKey(cspHand, &key, alg, keyAlgStr, keySizeInBits, 
				CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_SENSITIVE,
				CSSM_KEYUSE_ANY, CSSMERR_CSP_INVALID_KEYATTR_MASK, quiet,
				CSSM_TRUE, "RETURN_DATA | SENSITIVE")) {
			return 1;
		}
		if(genSymKey(cspHand, &key, alg, keyAlgStr, keySizeInBits, 
				CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_RETURN_REF,
				CSSM_KEYUSE_ANY, CSSMERR_CSP_INVALID_KEYATTR_MASK, quiet,
				CSSM_TRUE, "RETURN_DATA | RETURN_REF")) {
			return 1;
		}
	}
	return 0;
}

/*
 * Verify proper symmetric key null wrap operation.
 *
 *	gen ref key, CSSM_KEYATTR_SENSITIVE, vfy you can't do null wrap;
 *	gen ref key, !CSSM_KEYATTR_EXTRACTABLE, vfy you can't do null wrap;
 */
int symNullWrapTest(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_ALGORITHMS	alg,
	const char		*keyAlgStr,
	uint32			keySizeInBits,
	CSSM_BOOL 		quiet)
{
	CSSM_KEY key;

	if(!quiet) {
		printf("      ...testing access to inaccessible key bits via NULL wrap\n");
	}
	
	/* gen ref key, CSSM_KEYATTR_SENSITIVE, vfy you can't do null wrap */
	if(genSymKey(cspHand, &key, alg, keyAlgStr, keySizeInBits, 
			CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_SENSITIVE,
			CSSM_KEYUSE_ANY, CSSM_OK, quiet, 
			CSSM_FALSE, "SENSITIVE | RETURN_REF")) {
		return 1;
	}
	if(nullWrapTest(cspHand, &key, quiet, CSSMERR_CSP_INVALID_KEYATTR_MASK,
			keyAlgStr, "KEYATTR_SENSITIVE")) {
		return 1;
	}
	cspFreeKey(cspHand, &key);
	
	/* gen ref key, !CSSM_KEYATTR_EXTRACTABLE, vfy you can't do null wrap */
	if(genSymKey(cspHand, &key, alg, keyAlgStr, keySizeInBits, 
			CSSM_KEYATTR_RETURN_REF /* !CSSM_KEYATTR_EXTRACTABLE */,
			CSSM_KEYUSE_ANY, CSSM_OK, quiet, 
			CSSM_FALSE, "!EXTRACTABLE | RETURN_REF")) {
		return 1;
	}
	if(nullWrapTest(cspHand, &key, quiet, CSSMERR_CSP_INVALID_KEYATTR_MASK,
			keyAlgStr, "!EXTRACTABLE")) {
		return 1;
	}
	cspFreeKey(cspHand, &key);

	return 0;
}

/*
 * Verify proper symmetric key wrap !EXTRACTABLE handling.
 *
 * Gen unwrapped ref key, !CSSM_KEYATTR_EXTRACTABLE;
 * Gen wrapping key - a simple DES key;
 * vfy you can't wrap unwrappedKey with wrappingKey;
 */
int symBadWrapTest(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_ALGORITHMS	alg,
	const char		*keyAlgStr,
	uint32			keySizeInBits,
	CSSM_BOOL 		quiet)
{
	CSSM_KEY unwrappedKey;
	
	if(!quiet) {
		printf("      ...testing access to !EXTRACTABLE key bits via PKCS7 wrap\n");
	}
	
	/* gen ref key, CSSM_KEYATTR_SENSITIVE, !EXTRACTABLE */
	if(genSymKey(cspHand, &unwrappedKey, alg, keyAlgStr, keySizeInBits, 
			CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_SENSITIVE,
			CSSM_KEYUSE_ANY, CSSM_OK, quiet, 
			CSSM_FALSE, "SENSITIVE | RETURN_REF")) {
		return 1;
	}
	if(badWrapTest(cspHand, 
		&unwrappedKey, 
		CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7,
		quiet, keyAlgStr, 
		"!EXTRACTABLE wrap")) {
		return 1;
	}
	cspFreeKey(cspHand, &unwrappedKey);
	return 0;
}

/*
 * Verify proper asymmetric key wrap !EXTRACTABLE handling.
 *
 * Gen unwrapped ref key, !CSSM_KEYATTR_EXTRACTABLE;
 * Gen wrapping key - a simple DES key;
 * vfy you can't wrap unwrappedKey with wrappingKey;
 */
int asymBadWrapTest(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_ALGORITHMS	alg,
	const char		*keyAlgStr,
	uint32			keySizeInBits,
	CSSM_BOOL 		quiet)
{
	CSSM_KEY pubKey;
	CSSM_KEY privKey;
	
	if(!quiet) {
		printf("      ...testing access to !EXTRACTABLE key bits via CUSTOM wrap\n");
	}
	
	/* gen ref key, CSSM_KEYATTR_SENSITIVE, !EXTRACTABLE */
	if(genKeyPair(cspHand, alg, keyAlgStr, keySizeInBits, 
			&pubKey, CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE, 
				CSSM_KEYUSE_ANY, 
			&privKey, CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_SENSITIVE, 
				CSSM_KEYUSE_ANY, 
			CSSM_OK, quiet, CSSM_FALSE, "RETURN_REF | SENSITIVE")) {
		return 1;
	}	
	if(badWrapTest(cspHand, 
		&privKey, 
		CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM,
		quiet, keyAlgStr, 
		"!EXTRACTABLE wrap")) {
		return 1;
	}
	cspFreeKey(cspHand, &privKey);
	cspFreeKey(cspHand, &pubKey);
	return 0;
}

/*
 * Generate a ref key, munge various fields in the header, verify that attempts
 * to use the munged key result in CSSMERR_CSP_INVALID_KEY_REFERENCE.
 */
int symHeaderTest(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_ALGORITHMS	keyAlg,
	const char		*keyAlgStr,
	CSSM_ALGORITHMS	encrAlg,
	CSSM_ALGORITHMS	signAlg,
	uint32			keySizeInBits,
	CSSM_BOOL 		quiet)
{
	CSSM_KEY key;

	if(!quiet) {
		printf("      ...testing munged ref key header\n");
	}
	if(genSymKey(cspHand, &key, keyAlg, keyAlgStr, keySizeInBits, 
			CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE,
			CSSM_KEYUSE_ANY, CSSM_OK, quiet, 
			CSSM_FALSE, "RETURN_REF")) {
		return 1;
	}
	if(badHdrTest(cspHand, &key, quiet, keyAlgStr)) {
		return 1;
	}
	cspFreeKey(cspHand, &key);
	return 0;
}

/*
 * Generate key pair, specified pub key attr and expected result, standard
 * "good" priv key attr. Used by asymAttrTest().
 */
static int pubKeyAttrTest(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_ALGORITHMS		alg,
	const char			*keyAlgStr,
	uint32				keySizeInBits,
	CSSM_KEYATTR_FLAGS 	pubKeyAttr,
	CSSM_RETURN			expectRtn,
	CSSM_BOOL 			quiet,
	const char			*testStr)
{
	CSSM_KEY pubKey;
	CSSM_KEY privKey;
	
	return genKeyPair(cspHand, alg, keyAlgStr, keySizeInBits, 
			&pubKey, pubKeyAttr, CSSM_KEYUSE_ANY, 
			&privKey, CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_SENSITIVE, CSSM_KEYUSE_ANY, 
			expectRtn, quiet, CSSM_TRUE, testStr);
}

/*
 * Generate key pair, specified priv key attr and expected result, standard
 * "good" pub key attr. Used by asymAttrTest().
 */
static int privKeyAttrTest(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_ALGORITHMS		alg,
	const char			*keyAlgStr,
	uint32				keySizeInBits,
	CSSM_KEYATTR_FLAGS 	privKeyAttr,
	CSSM_RETURN			expectRtn,
	CSSM_BOOL 			quiet,
	const char			*testStr)
{
	CSSM_KEY pubKey;
	CSSM_KEY privKey;
	
	return genKeyPair(cspHand, alg, keyAlgStr, keySizeInBits, 
			&pubKey, CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE, 
				CSSM_KEYUSE_ANY, 
			&privKey, privKeyAttr, CSSM_KEYUSE_ANY,
			expectRtn, quiet, CSSM_TRUE, testStr);
}

/*
 * Verify asymmetric key attribute behavior.
 *
 *	check that you can't gen pub key with {
 *		CSSM_KEYATTR_ALWAYS_SENSITIVE
 *		CSSM_KEYATTR_NEVER_EXTRACTABLE
 *		CSSM_KEYATTR_PERMANENT
 *		CSSM_KEYATTR_PRIVATE
 *		CSSM_KEYATTR_RETURN_DATA | !CSSM_KEYATTR_EXTRACTABLE
 *		CSSM_KEYATTR_RETURN_REF  | !CSSM_KEYATTR_EXTRACTABLE
 *		CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_SENSITIVE
 *		CSSM_KEYATTR_RETURN_REF  | CSSM_KEYATTR_SENSITIVE
 *	}
 *	check that you can't gen priv key with {
 *		CSSM_KEYATTR_ALWAYS_SENSITIVE
 *		CSSM_KEYATTR_NEVER_EXTRACTABLE
 *		CSSM_KEYATTR_PERMANENT
 *		CSSM_KEYATTR_PRIVATE
 *		CSSM_KEYATTR_RETURN_DATA | !CSSM_KEYATTR_EXTRACTABLE
 *		CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_SENSITIVE
 *	}
 */
static int asymAttrTest(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_ALGORITHMS	alg,
	const char		*keyAlgStr,
	uint32			keySizeInBits,
	CSSM_BOOL		bareCsp,
	CSSM_BOOL 		quiet)
{
	#if CSPDL_ALL_KEYS_ARE_PERMANENT
	printf("      ...SKIPING asymAttrTest due to Radar 3732910\n");
	return 0;
	#endif
	if(!quiet) {
		printf("      ...testing key attr\n");
	}
	if(bareCsp || CSPDL_ALWAYS_SENSITIVE_CHECK) {
		if(pubKeyAttrTest(cspHand, alg, keyAlgStr, keySizeInBits,
				CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_ALWAYS_SENSITIVE,
				CSSMERR_CSP_INVALID_KEYATTR_MASK, quiet, "ALWAYS_SENSITIVE pub")) {
			return 1;
		}	
	}
	if(bareCsp || CSPDL_NEVER_EXTRACTABLE_CHECK) {
		if(pubKeyAttrTest(cspHand, alg, keyAlgStr, keySizeInBits, 
				CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_NEVER_EXTRACTABLE,
				CSSMERR_CSP_INVALID_KEYATTR_MASK, quiet, "NEVER_EXTRACTABLE pub")) {
			return 1;
		}	
	}
	if(pubKeyAttrTest(cspHand, alg, keyAlgStr, keySizeInBits, 
			CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT | 
				CSSM_KEYATTR_EXTRACTABLE,
			bareCsp ?
				/* bare CSP - permanent is checked first, this is the error */
				CSSMERR_CSP_UNSUPPORTED_KEYATTR_MASK :
				/* CSPDL - SS strips off permanent, then does key gen (so we'd
					* better specify EXTRACTABLE!), *then* checks for DLDB. */
				CSSMERR_CSP_MISSING_ATTR_DL_DB_HANDLE,
		quiet, "PERMANENT pub")) {
		return 1;
	}	
	if(pubKeyAttrTest(cspHand, alg, keyAlgStr, keySizeInBits, 
			CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PRIVATE |
				CSSM_KEYATTR_EXTRACTABLE,
			CSSMERR_CSP_UNSUPPORTED_KEYATTR_MASK, quiet, "PRIVATE pub")) {
		return 1;
	}	
	if(bareCsp) {
		/* CSPDL doesn't support RETURN_DATA */
		if(pubKeyAttrTest(cspHand, alg, keyAlgStr, keySizeInBits, 
				CSSM_KEYATTR_RETURN_DATA /* | !CSSM_KEYATTR_EXTRACTABLE */,
				CSSMERR_CSP_INVALID_KEYATTR_MASK, quiet, 
					"RETURN_DATA | !EXTRACTABLE pub")) {
			return 1;
		}
	}	
	/* pub key should always be extractable */ 
	if(pubKeyAttrTest(cspHand, alg, keyAlgStr, keySizeInBits, 
			CSSM_KEYATTR_RETURN_REF /* | !CSSM_KEYATTR_EXTRACTABLE */,
			CSSMERR_CSP_INVALID_KEYATTR_MASK, quiet, 
				"RETURN_REF | !EXTRACTABLE pub")) {
		return 1;
	}	
	/* pub keys can't be sensitive */
	if(bareCsp) {
		/* CSPDL doesn't support RETURN_DATA */
		if(pubKeyAttrTest(cspHand, alg, keyAlgStr, keySizeInBits, 
				CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_SENSITIVE,
				CSSMERR_CSP_INVALID_KEYATTR_MASK, quiet, "RETURN_DATA | SENSITIVE pub")) {
			return 1;
		}
	}	
	if(pubKeyAttrTest(cspHand, alg, keyAlgStr, keySizeInBits, 
			CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_SENSITIVE,
			CSSMERR_CSP_INVALID_KEYATTR_MASK, quiet, "RETURN_REF | !SENSITIVE pub")) {
		return 1;
	}	
	
	/* priv key attr tests */
	if(bareCsp || CSPDL_ALWAYS_SENSITIVE_CHECK) {
		if(privKeyAttrTest(cspHand, alg, keyAlgStr, keySizeInBits,
				CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_ALWAYS_SENSITIVE,
				CSSMERR_CSP_INVALID_KEYATTR_MASK, quiet, "ALWAYS_SENSITIVE priv")) {
			return 1;
		}	
	}
	if(bareCsp || CSPDL_NEVER_EXTRACTABLE_CHECK) {
		if(privKeyAttrTest(cspHand, alg, keyAlgStr, keySizeInBits,
				CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_NEVER_EXTRACTABLE,
				CSSMERR_CSP_INVALID_KEYATTR_MASK, quiet, "NEVER_EXTRACTABLE priv")) {
			return 1;
		}	
	}
	if(privKeyAttrTest(cspHand, alg, keyAlgStr, keySizeInBits,
			CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT,
			bareCsp ?  CSSMERR_CSP_UNSUPPORTED_KEYATTR_MASK : 
				CSSMERR_CSP_MISSING_ATTR_DL_DB_HANDLE,
			quiet, "PERMANENT priv")) {
		return 1;
	}	
	if(privKeyAttrTest(cspHand, alg, keyAlgStr, keySizeInBits,
			CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PRIVATE,
			CSSMERR_CSP_UNSUPPORTED_KEYATTR_MASK, quiet, "PRIVATE priv")) {
		return 1;
	}	
	if(bareCsp) {
		/* CSPDL doesn't support RETURN_DATA */
		if(privKeyAttrTest(cspHand, alg, keyAlgStr, keySizeInBits,
				CSSM_KEYATTR_RETURN_DATA /* | CSSM_KEYATTR_EXTRACTABLE */,
				CSSMERR_CSP_INVALID_KEYATTR_MASK, quiet, 
				"RETURN_DATA | !EXTRACTABLE priv")) {
			return 1;
		}
		if(privKeyAttrTest(cspHand, alg, keyAlgStr, keySizeInBits,
				CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_SENSITIVE,
				CSSMERR_CSP_INVALID_KEYATTR_MASK, quiet, "RETURN_DATA | SENSITIVE priv")) {
			return 1;
		}	
	}
	return 0;
}

/* 
 * Verify asymmetric key null wrap behavior:
 *	gen ref key, CSSM_KEYATTR_SENSITIVE, vfy you can't do null wrap;
 *	gen ref key, !CSSM_KEYATTR_EXTRACTABLE, vfy you can't do null wrap;
 */
static int asymNullWrapTest(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_ALGORITHMS	alg,
	const char		*keyAlgStr,
	uint32			keySizeInBits,
	CSSM_BOOL 		quiet)
{
	CSSM_KEY pubKey;
	CSSM_KEY privKey;
	
	if(!quiet) {
		printf("      ...testing access to inaccessible key bits via NULL wrap\n");
	}
	/* gen priv ref key, CSSM_KEYATTR_SENSITIVE, vfy you can't do null wrap */
	if(genKeyPair(cspHand, alg, keyAlgStr, keySizeInBits, 
			&pubKey, CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE, 
				CSSM_KEYUSE_ANY, 
			&privKey, CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_SENSITIVE, 
				CSSM_KEYUSE_ANY, 
			CSSM_OK, quiet, CSSM_FALSE, "RETURN_REF | SENSITIVE")) {
		return 1;
	}	
	if(nullWrapTest(cspHand, &privKey, quiet, CSSMERR_CSP_INVALID_KEYATTR_MASK,
			keyAlgStr, "SENSITIVE")) {
		return 1;
	}
	cspFreeKey(cspHand, &privKey);
	cspFreeKey(cspHand, &pubKey);
	
	/* gen priv ref key, !CSSM_KEYATTR_EXTRACTABLE, vfy you can't do null wrap */
	if(genKeyPair(cspHand, alg, keyAlgStr, keySizeInBits, 
			&pubKey, CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE, 
				CSSM_KEYUSE_ANY, 
			&privKey, CSSM_KEYATTR_RETURN_REF /* | !EXTRACTABLE */, CSSM_KEYUSE_ANY, 
			CSSM_OK, quiet, CSSM_FALSE, "RETURN_REF | !EXTRACTABLE")) {
		return 1;
	}	
	if(nullWrapTest(cspHand, &privKey, quiet, CSSMERR_CSP_INVALID_KEYATTR_MASK,
			keyAlgStr, "!EXTRACTABLE")) {
		return 1;
	}
	cspFreeKey(cspHand, &privKey);
	cspFreeKey(cspHand, &pubKey);
	return 0;
}

/*
 * Generate public and private ref keys, munge various fields in the header, 
 * verify that attempts to use the munged key result in 
 * CSSMERR_CSP_INVALID_KEY_REFERENCE.
 */
int asymHeaderTest(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_ALGORITHMS	keyAlg,
	const char		*keyAlgStr,
	CSSM_ALGORITHMS	encrAlg,
	CSSM_ALGORITHMS	signAlg,
	uint32			keySizeInBits,
	CSSM_BOOL 		quiet)
{
	CSSM_KEY privKey;
	CSSM_KEY pubKey;

	if(!quiet) {
		printf("      ...testing munged ref key header\n");
	}
	if(genKeyPair(cspHand, keyAlg, keyAlgStr, keySizeInBits, 
			&pubKey, CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE, 
				CSSM_KEYUSE_ANY, 
			&privKey, CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE, 
				CSSM_KEYUSE_ANY, 
			CSSM_OK, quiet, CSSM_FALSE, "RETURN_REF")) {
		return 1;
	}	
	if(badHdrTest(cspHand, &privKey, quiet, keyAlgStr)) {
		return 1;
	}
	if(badHdrTest(cspHand, &pubKey, quiet, keyAlgStr)) {
		return 1;
	}
	cspFreeKey(cspHand, &privKey);
	cspFreeKey(cspHand, &pubKey);
	return 0;
}

/* map one of our private privAlgs (ALG_DES, etc.) to associated CSSM info. */
void privAlgToCssm(
	privAlg 		palg,
	CSSM_ALGORITHMS	*keyAlg,
	CSSM_ALGORITHMS *signAlg,		// CSSM_ALGID_NONE means incapable (e.g., DES)
	CSSM_ALGORITHMS	*encrAlg,		// CSSM_ALGID_NONE means incapable (e.g., DSA)
	uint32			*keySizeInBits,
	const char		**keyAlgStr)
{
	*signAlg = *encrAlg = CSSM_ALGID_NONE;	// default
	switch(palg) {
		case ALG_ASC:
			*encrAlg = *keyAlg = CSSM_ALGID_ASC;
			*keySizeInBits = CSP_ASC_KEY_SIZE_DEFAULT;
			*keyAlgStr = "ASC";
			break;
		case ALG_DES:
			*encrAlg = *keyAlg = CSSM_ALGID_DES;
			*keySizeInBits = CSP_DES_KEY_SIZE_DEFAULT;
			*keyAlgStr = "DES";
			break;
		case ALG_3DES:
			*encrAlg = CSSM_ALGID_3DES_3KEY_EDE;
			*keyAlg = CSSM_ALGID_3DES_3KEY;
			*keySizeInBits = CSP_DES3_KEY_SIZE_DEFAULT;
			*keyAlgStr = "3DES";
			break;
		case ALG_RC2:
			*encrAlg = *keyAlg = CSSM_ALGID_RC2;
			*keySizeInBits = CSP_RC2_KEY_SIZE_DEFAULT;
			*keyAlgStr = "RC2";
			break;
		case ALG_RC4:
			*encrAlg = *keyAlg = CSSM_ALGID_RC4;
			*keySizeInBits = CSP_RC4_KEY_SIZE_DEFAULT;
			*keyAlgStr = "RC4";
			break;
		case ALG_RC5:
			*encrAlg = *keyAlg = CSSM_ALGID_RC5;
			*keySizeInBits = CSP_RC5_KEY_SIZE_DEFAULT;
			*keyAlgStr = "RC5";
			break;
		case ALG_AES:
			*encrAlg = *keyAlg = CSSM_ALGID_AES;
			*keySizeInBits = CSP_AES_KEY_SIZE_DEFAULT;
			*keyAlgStr = "AES";
			break;
		case ALG_RSA:
			*keyAlg = CSSM_ALGID_RSA;
			*encrAlg = CSSM_ALGID_RSA;
			*signAlg = CSSM_ALGID_SHA1WithRSA;
			*keySizeInBits = CSP_RSA_KEY_SIZE_DEFAULT;
			*keyAlgStr = "RSA";
			break;
		case ALG_DSA:
			*keyAlg = CSSM_ALGID_DSA;
			*signAlg = CSSM_ALGID_SHA1WithDSA;
			*keySizeInBits = CSP_DSA_KEY_SIZE_DEFAULT;
			*keyAlgStr = "DSA";
			break;
		case ALG_FEE:
			*keyAlg = CSSM_ALGID_FEE;
			*signAlg = CSSM_ALGID_SHA1WithECDSA;
			*encrAlg = CSSM_ALGID_FEEDEXP;
			*keySizeInBits = CSP_FEE_KEY_SIZE_DEFAULT;
			*keyAlgStr = "FEE";
			break;
		case ALG_ECDSA:
			*keyAlg = CSSM_ALGID_ECDSA;
			*signAlg = CSSM_ALGID_SHA1WithECDSA;
			*keySizeInBits = CSP_ECDSA_KEY_SIZE_DEFAULT;
			*keyAlgStr = "ECDSA";
			break;
		default:
			printf("***BRRZAP! privAlgToCssm needs work\n");
			exit(1);
	}
	return;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	CSSM_CSP_HANDLE 	cspHand;
	CSSM_ALGORITHMS		keyAlg;			// CSSM_ALGID_xxx of the key
	CSSM_ALGORITHMS		signAlg;		// CSSM_ALGID_xxx of the associated signing op
	CSSM_ALGORITHMS		encrAlg;		// CSSM_ALGID_xxx of the associated encrypt op
	privAlg				palg;
	uint32				keySizeInBits;
	int					rtn;
	int					i;
	const char			*keyAlgStr;
	
	/*
	 * User-spec'd params
	 */
	CSSM_BOOL	quiet = CSSM_FALSE;
	CSSM_BOOL	doSym = CSSM_TRUE;
	CSSM_BOOL	doAsym = CSSM_TRUE;
	CSSM_BOOL	bareCsp = CSSM_TRUE;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
		    case 's':
		    	doAsym = CSSM_FALSE;
				break;
		    case 'a':
		    	doSym = CSSM_FALSE;
				break;
		    case 'D':
		    	bareCsp = CSSM_FALSE;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	printf("Starting badattr; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	
	if(doSym) {
		for(palg=SYM_FIRST; palg<=SYM_LAST; palg++) {
			privAlgToCssm(palg, &keyAlg, &signAlg, &encrAlg, &keySizeInBits, &keyAlgStr);
			if(!quiet) {
				printf("   ...alg %s\n", keyAlgStr);
			}
			rtn = symAttrTest(cspHand, keyAlg, keyAlgStr, keySizeInBits, bareCsp, 
					quiet);
			if(rtn) {
				goto abort;
			}
			rtn = symNullWrapTest(cspHand, keyAlg, keyAlgStr, keySizeInBits, quiet);
			if(rtn) {
				goto abort;
			}
			rtn = symBadWrapTest(cspHand, keyAlg, keyAlgStr, keySizeInBits, quiet);
			if(rtn) {
				goto abort;
			}
			rtn = symUsageTest(cspHand, keyAlg, keyAlgStr, encrAlg, signAlg, 
					keySizeInBits, quiet);
			if(rtn) {
				goto abort;
			}
			if(bareCsp || CSPDL_MUNGE_HEADER_CHECK) {
				rtn = symHeaderTest(cspHand, keyAlg, keyAlgStr, encrAlg, signAlg, 
						keySizeInBits, quiet);
				if(rtn) {
					goto abort;
				}
			}
			else if(!quiet) {
				printf("      ...SKIPPING munged ref key header test\n");
			}
		}
	}
	
	if(doAsym) {
		for(palg=ASYM_FIRST; palg<=ASYM_LAST; palg++) {
			privAlgToCssm(palg, &keyAlg, &signAlg, &encrAlg, &keySizeInBits, 
				&keyAlgStr);
			if(!quiet) {
				printf("   ...alg %s\n", keyAlgStr);
			}
			rtn = asymAttrTest(cspHand, keyAlg, keyAlgStr, keySizeInBits, 
				bareCsp, quiet);
			if(rtn) {
				goto abort;
			}
			rtn = asymNullWrapTest(cspHand, keyAlg, keyAlgStr, keySizeInBits, quiet);
			if(rtn) {
				goto abort;
			}
			rtn = asymBadWrapTest(cspHand, keyAlg, keyAlgStr, keySizeInBits, quiet);
			if(rtn) {
				goto abort;
			}
			if(bareCsp || CSPDL_MUNGE_HEADER_CHECK) {
				rtn = asymHeaderTest(cspHand, keyAlg, keyAlgStr, encrAlg, signAlg, 
						keySizeInBits, quiet);
				if(rtn) {
					goto abort;
				}
			}
			else if(!quiet) {
				printf("      ...SKIPPING munged ref key header test\n");
			}
		}
	}
abort:
	cspShutdown(cspHand, bareCsp);
	if((rtn == 0) && !quiet) {
		printf("%s complete\n", argv[0]);
	}
	return rtn;
}
	
