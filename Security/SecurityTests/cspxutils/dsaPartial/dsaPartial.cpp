/*
 * dsaPartial.cpp - test for partial DSA public handling
 */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include <string.h>
#include "cspwrap.h"
#include "common.h"
#include <security_cdsa_utils/cuFileIo.h>
#include "nssAppUtils.h"

/*
 * generate key pairs with one set of parameters, dsa1Priv and dsa1Pub;
 * genenate another pair with a different set of params, dsa2Priv and 
 *		dsa2Pub;
 * manually cook up dsa1PubPartial from dsa1Pub;
 * manually cook up dsa2PubPartial from dsa2Pub;
 *
 * with all legal and/or specified combos of {ref,raw} keys {
 * 		sign with dsa1Priv;
 * 		vfy with dsa1Pub;
 * 		vfy with dsa1PubPartial: CSSMERR_CSP_APPLE_DSA_PUBLIC_KEY_INCOMPLETE
 * 		vfy with dsa1PubPartial and dsa1Pub (attrs)
 * 		vfy with dsa2PubPartial and dsa1Pub (attrs) --> vfy fail
 * 		vfy with dsa1PubPartial and dsa2Pub (attrs) --> vfy fail
 *		merge dsa1PubPartial + dsa1Pub --> merged;
 *		vfy with merged, should be good
 *		merge dsa1PubPartial + dsa2Pub -->merged;
 *		vfy with merged; vfy fail;
 * }
 */
 
/*
 * Static parameter files. 
 *
 * Regenerate these every once in a while with rsatool:
 *
 * # rsatool g a=d k=/tmp/foo M=dsaParam512_1.der
 */
#define PARAMS_512_1	"dsaParam512_1.der"
#define PARAMS_512_2	"dsaParam512_2.der"

#define MAX_PTEXT_SIZE	512
#define KEY_ALG			CSSM_ALGID_DSA
#define SIG_ALG			CSSM_ALGID_SHA1WithDSA
#define LOOPS_DEF		32
#define KEY_SIZE_DEF	512

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  l=loops\n");
	printf("  p(ause on loop)\n");
	printf("  q(uiet)\n");
	printf("  v(erbose)\n");
	printf("  D (CSPDL)\n");
	printf("  r (all keys are raw)\n");
	printf("  f (all keys are ref)\n");
	exit(1);
}

/*
 * Generate DSA key pair with required alg parameters.
 */
static CSSM_RETURN genDsaKeyPair(
	CSSM_CSP_HANDLE cspHand,
	uint32 keySize,					// in bits
	CSSM_KEY_PTR pubKey,			// mallocd by caller
	CSSM_BOOL pubIsRef,				// true - reference key, false - data
	CSSM_KEY_PTR privKey,			// mallocd by caller
	CSSM_BOOL privIsRef,			// true - reference key, false - data
	const CSSM_DATA	*params)	
{
	CSSM_RETURN				crtn;
	CSSM_CC_HANDLE 			ccHand;
	CSSM_DATA				keyLabelData;
	uint32					pubAttr;
	uint32					privAttr;
	
	if(params == NULL) {
		return CSSMERR_CSSM_INVALID_POINTER;
	}

	keyLabelData.Data   = (uint8 *)"foobar",
	keyLabelData.Length = 6;
	memset(pubKey, 0, sizeof(CSSM_KEY));
	memset(privKey, 0, sizeof(CSSM_KEY));
	
	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		CSSM_ALGID_DSA,
		keySize,
		NULL,					// Seed
		NULL,					// Salt
		NULL,					// StartDate
		NULL,					// EndDate
		params,	
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateKeyGenContext", crtn);
		return crtn;
	}
	
	/* cook up attribute bits */
	if(pubIsRef) {
		pubAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE;
	}
	else {
		pubAttr = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}
	if(privIsRef) {
		privAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE;
	}
	else {
		privAttr = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}

	crtn = CSSM_GenerateKeyPair(ccHand,
		CSSM_KEYUSE_VERIFY,
		pubAttr,
		&keyLabelData,
		pubKey,
		CSSM_KEYUSE_SIGN,
		privAttr,
		&keyLabelData,			// same labels
		NULL,					// CredAndAclEntry
		privKey);
	if(crtn) {
		printError("CSSM_GenerateKeyPair", crtn);
	}
	CSSM_DeleteContext(ccHand);
	return crtn;
}


/*
 * Create new public key by merging specified partial key and 
 * parameter-bearing key. All keys can be in any format (though
 * it's the caller's responsibility to avoid using a ref paramKey
 * with the CSPDL). 
 */
static CSSM_RETURN dsaMergeParams(
	CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY	*partialKey,
	const CSSM_KEY	*paramKey,
	CSSM_KEY		&fullKey,		// RETURNED
	bool			fullIsRef)		// ref/raw
{
	/*
	 * First step is a null wrap or unwrap depending on 
	 * format of partialKey.
	 */
	CSSM_CC_HANDLE ccHand;
	CSSM_RETURN crtn;
	CSSM_ACCESS_CREDENTIALS	creds;
	CSSM_DATA label = {10, (uint8 *)"dummyLabel"};
	CSSM_DATA descrData = {0, NULL};
	const CSSM_KEYHEADER &hdr = partialKey->KeyHeader;

	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
			CSSM_ALGID_NONE,
			CSSM_ALGMODE_NONE,
			&creds,		
			NULL,				// wrapping key
			NULL,				// initVector
			CSSM_PADDING_NONE,	
			0,					// Params
			&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateSymmetricContext", crtn);
		return crtn;
	}
	
	/* add in paramKey */
	crtn = AddContextAttribute(ccHand,
		CSSM_ATTRIBUTE_PARAM_KEY,
		sizeof(CSSM_KEY),
		CAT_Ptr,
		paramKey,
		0);
	if(crtn) {
		printError("AddContextAttribute", crtn);
		return crtn;
	}
	
	/* go */
	CSSM_KEY targetKey;
	memset(&targetKey, 0, sizeof(targetKey));
	if(hdr.BlobType == CSSM_KEYBLOB_RAW) {
		/* raw --> ref : null unwrap */
		crtn = CSSM_UnwrapKey(ccHand,
			NULL,				// PublicKey
			partialKey,
			hdr.KeyUsage,		// same as original
			CSSM_KEYATTR_EXTRACTABLE |CSSM_KEYATTR_RETURN_REF,
			&label,
			NULL,				// CredAndAclEntry
			&targetKey,
			&descrData);		// required
		if(crtn) {
			printError("dsaMergeParams CSSM_UnwrapKey (1)", crtn);
			return crtn;
		}
	}
	else {
		/* ref --> raw : null wrap */
		crtn = CSSM_WrapKey(ccHand,
			&creds,
			partialKey,
			NULL,			// DescriptiveData
			&targetKey);
		if(crtn) {
			printError("dsaMergeParams CSSM_WrapKey (1)", crtn);
			return crtn;
		}
	}
	
	if(targetKey.KeyHeader.KeyAttr & CSSM_KEYATTR_PARTIAL) {
		printf("***merged key still has CSSM_KEYATTR_PARTIAL\n");
		return CSSMERR_CSSM_INTERNAL_ERROR;
	}

	CSSM_KEYBLOB_TYPE targetBlob;
	if(fullIsRef) {
		targetBlob = CSSM_KEYBLOB_REFERENCE;
	}
	else {
		targetBlob = CSSM_KEYBLOB_RAW;
	}
	
	if(targetKey.KeyHeader.BlobType == targetBlob) {
		/* we're done */
		fullKey = targetKey;
		CSSM_DeleteContext(ccHand);
		return CSSM_OK;
	}

	/*
	 * We're going to reuse the context, but since the parameter merge
	 * has already been done, remove the CSSM_ATTRIBUTE_PARAM_KEY
	 * attribute.
	 */
	CSSM_CONTEXT_ATTRIBUTE attr;
	memset(&attr, 0, sizeof(attr));
	attr.AttributeType = CSSM_ATTRIBUTE_PARAM_KEY;
	crtn = CSSM_DeleteContextAttributes(ccHand, 1, &attr);
	if(crtn) {
		printError("CSSM_DeleteContextAttributes", crtn);
		return crtn;
	}
	
	/* one more conversion */
	if(targetBlob == CSSM_KEYBLOB_REFERENCE) {
		/* raw --> ref : null unwrap */
		crtn = CSSM_UnwrapKey(ccHand,
			NULL,				// PublicKey
			&targetKey,
			hdr.KeyUsage,		// same as original
			CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_RETURN_REF,
			&label,
			NULL,				// CredAndAclEntry
			&fullKey,
			&descrData);		// required
		if(crtn) {
			printError("dsaMergeParams CSSM_UnwrapKey (2)", crtn);
			return crtn;
		}
	}
	else {
		/* ref --> raw : null wrap */
		crtn = CSSM_WrapKey(ccHand,
			&creds,
			&targetKey,
			NULL,			// DescriptiveData
			&fullKey);
		if(crtn) {
			printError("dsaMergeParams CSSM_WrapKey (2)", crtn);
			return crtn;
		}
	}
	CSSM_FreeKey(cspHand, NULL, &targetKey, CSSM_FALSE);
	CSSM_DeleteContext(ccHand);
	return CSSM_OK;
}

/*
 * Custom cspSigVerify with optional CSSM_ATTRIBUTE_PARAM_KEY
 */
CSSM_RETURN sigVerify(CSSM_CSP_HANDLE cspHand,
	uint32 algorithm,				// CSSM_ALGID_SHA1WithDSA, etc. 
	CSSM_KEY_PTR key,				// public key
	CSSM_KEY_PTR paramKey,			// optional parameter key
	const CSSM_DATA *ptext,
	const CSSM_DATA *sig,
	CSSM_RETURN expectResult,
	const char *op,
	CSSM_BOOL verbose)
{
	CSSM_CC_HANDLE	sigHand;
	CSSM_RETURN		ocrtn = CSSM_OK;
	CSSM_RETURN		crtn;

	if(verbose) {
		printf("   ...%s\n", op);
	}
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		algorithm,
		NULL,				// passPhrase
		key,
		&sigHand);
	if(crtn) {
		printError("CSSM_CSP_CreateSignatureContext", crtn);
		return crtn;
	}
	if(paramKey) {
		crtn = AddContextAttribute(sigHand,
			CSSM_ATTRIBUTE_PARAM_KEY,
			sizeof(CSSM_KEY),
			CAT_Ptr,
			paramKey,
			0);
		if(crtn) {
			printError("AddContextAttribute", crtn);
			return crtn;
		}
	}
	crtn = CSSM_VerifyData(sigHand,
		ptext,
		1,
		CSSM_ALGID_NONE,
		sig);
	if(crtn != expectResult) {
		if(!crtn) {
			printf("%s: Unexpected good Sig Verify (expect %s)\n",
				op, cssmErrToStr(expectResult));
			ocrtn = CSSMERR_CSSM_INTERNAL_ERROR;
		}
		else {
			printError(op, crtn);
			ocrtn = crtn;
		}
	}
	CSSM_DeleteContext(sigHand);
	return ocrtn;
}

static int doTest(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_KEY_PTR privKey_0,
	CSSM_KEY_PTR pubKeyBase_0,
	CSSM_KEY_PTR pubKeyPartial_0,
	CSSM_KEY_PTR pubKeyParam_0,		// full, raw format if CSPDL
	CSSM_KEY_PTR pubKeyPartial_1,
	CSSM_KEY_PTR pubKeyParam_1,		// full, raw format if CSPDL
	bool mergedIsRef,
	CSSM_BOOL quiet,
	CSSM_BOOL verbose)
{
	uint8 ptextBuf[MAX_PTEXT_SIZE];
	CSSM_DATA ptext = {0, ptextBuf};
	simpleGenData(&ptext, 1, MAX_PTEXT_SIZE);
	CSSM_DATA sig = {0, NULL};
	CSSM_RETURN crtn;
	
	/* the single sign op for this routine */
	crtn = cspSign(cspHand, SIG_ALG, privKey_0, &ptext, &sig);
	if(crtn) {
		return testError(quiet);
	}
	
	/* normal verify with full key */
	crtn = sigVerify(cspHand, SIG_ALG, pubKeyBase_0, NULL,
		&ptext, &sig, CSSM_OK, "vfy with full key", verbose);
	if(crtn) {
		return testError(quiet);
	}
	
	/* good verify with partial key plus params */
	crtn = sigVerify(cspHand, SIG_ALG, pubKeyPartial_0, pubKeyParam_0, 
		&ptext, &sig, CSSM_OK, "vfy with partial key and params",
		verbose);
	if(crtn) {
		if(testError(quiet)) {
			return 1;
		}
	}
	
	/* partial key failure */
	crtn = sigVerify(cspHand, SIG_ALG, pubKeyPartial_0, NULL, 
		&ptext, &sig, 
		CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE, 
		"vfy with partial key no params", verbose);
	if(crtn) {
		if(testError(quiet)) {
			return 1;
		}
	}

	/* partial key, wrong params */
	crtn = sigVerify(cspHand, SIG_ALG, pubKeyPartial_0, pubKeyParam_1, 
		&ptext, &sig, 
		CSSMERR_CSP_VERIFY_FAILED, 
		"vfy with partial key wrong params", verbose);
	if(crtn) {
		if(testError(quiet)) {
			return 1;
		}
	}
	
	/* wrong partial key, good params */
	crtn = sigVerify(cspHand, SIG_ALG, pubKeyPartial_1, pubKeyParam_0, 
		&ptext, &sig, 
		CSSMERR_CSP_VERIFY_FAILED, 
		"vfy with wrong partial key, good params", verbose);
	if(crtn) {
		if(testError(quiet)) {
			return 1;
		}
	}
	
	/* 
	 * Test merge via wrap/unwrap.
	 * First, a good merged key.
	 */
	CSSM_KEY merged;
	crtn = dsaMergeParams(cspHand,
		pubKeyPartial_0,
		pubKeyParam_0,
		merged,
		mergedIsRef);
	if(crtn) {
		return testError(quiet);
	}
	crtn = sigVerify(cspHand, SIG_ALG, &merged, NULL,
		&ptext, &sig, CSSM_OK, "vfy with good merged key", verbose);
	if(crtn) {
		return testError(quiet);
	}
	CSSM_FreeKey(cspHand, NULL, &merged, CSSM_FALSE);

	/* now with a badly merged key (with the wrong params) */
	crtn = dsaMergeParams(cspHand,
		pubKeyPartial_0,
		pubKeyParam_1,
		merged,
		mergedIsRef);
	if(crtn) {
		return testError(quiet);
	}
	crtn = sigVerify(cspHand, SIG_ALG, &merged, NULL, 
		&ptext, &sig, 
		CSSMERR_CSP_VERIFY_FAILED, 
		"vfy with merged key wrong params", verbose);
	if(crtn) {
		if(testError(quiet)) {
			return 1;
		}
	}
	CSSM_FreeKey(cspHand, NULL, &merged, CSSM_FALSE);
	
	CSSM_FREE(sig.Data);
	return CSSM_OK;
}


int main(int argc, char **argv)
{
	char *argp;
	CSSM_CSP_HANDLE cspHand;
	CSSM_RETURN crtn;
	
	/* user spec'd variables */
	unsigned loops = LOOPS_DEF;
	CSSM_BOOL doPause = CSSM_FALSE;
	CSSM_BOOL quiet = CSSM_FALSE;
	CSSM_BOOL rawCSP = CSSM_TRUE;
	CSSM_BOOL verbose = CSSM_FALSE;
	uint32 keySize = KEY_SIZE_DEF;
	CSSM_BOOL allRaw = CSSM_FALSE;
	CSSM_BOOL allRef = CSSM_FALSE;
	
	for(int arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'l':
				loops = atoi(&argp[2]);
				break;
			case 'q':
				quiet = CSSM_TRUE;
				break;
			case 'p':
				doPause = CSSM_TRUE;
				break;
			case 'v':
				verbose = CSSM_TRUE;
				break;
			case 'D':
				rawCSP = CSSM_FALSE;
				break;
			case 'r':
				allRaw = CSSM_TRUE;
				break;
			case 'f':
				allRef = CSSM_TRUE;
				break;
			default:
				usage(argv);
		}
	}
	
	if(!rawCSP && (allRaw || allRef)) {
		printf("CSPDL inconsistent with allRef and allRaw\n");
		usage(argv);
	}
	if(allRef && allRaw) {
		printf("allRef and allRaw are mutually exclusive\n");
		usage(argv);
	}
	
	/* read in params for two keypairs */
	CSSM_DATA params1;
	CSSM_DATA params2;
	unsigned len;
	if(readFile(PARAMS_512_1, (unsigned char **)&params1.Data, &len)) {
		printf("***Error reading %s. Aborting.\n", PARAMS_512_1);
		printf("***This test must be run from the cspxutils/dsaPartial directory.\n");
		exit(1);
	}
	params1.Length = len;
	if(readFile(PARAMS_512_2, (unsigned char **)&params2.Data, &len)) {
		printf("***Error reading %s. Aborting.\n", PARAMS_512_2);
		printf("***This test must be run from the cspxutils/dsaPartial directory.\n");
		exit(1);
	}
	params2.Length = len;
	
	printf("Starting dsaPartial; args: ");
	for(int i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	cspHand = cspDlDbStartup(rawCSP, NULL);
	if(cspHand == 0) {
		exit(1);
	}

	/* generate two keypairs */
	CSSM_KEY dsa1Priv;
	CSSM_KEY dsa1Pub;
	CSSM_KEY dsa2Priv;
	CSSM_KEY dsa2Pub;
	
	if(verbose) {
		printf("...generating keys...\n");
	}
	CSSM_BOOL genRefKeys = CSSM_FALSE;
	if(!rawCSP || allRef) {
		genRefKeys = CSSM_TRUE;
	}
	crtn = genDsaKeyPair(cspHand, keySize, 
		&dsa1Pub,  genRefKeys,
		&dsa1Priv, genRefKeys,
		&params1);
	if(crtn) {
		exit(1);
	}
	crtn = genDsaKeyPair(cspHand, keySize, 
		&dsa2Pub,  genRefKeys,
		&dsa2Priv, genRefKeys,
		&params2);
	if(crtn) {
		exit(1);
	}
	
	/* CSPDL also requires separate raw parameter keys */
	CSSM_KEY dsa1PubParam;
	CSSM_KEY dsa2PubParam;
	if(!rawCSP) {
		if(cspRefKeyToRaw(cspHand, &dsa1Pub, &dsa1PubParam) ||
		   cspRefKeyToRaw(cspHand, &dsa2Pub, &dsa2PubParam)) {
			exit(1);
		}
	}
	
	/* generate partial pub keys in raw form */
	CSSM_KEY dsa1PubPartial;
	CSSM_KEY dsa2PubPartial;
	crtn = extractDsaPartial(cspHand, &dsa1Pub, &dsa1PubPartial);
	if(crtn) {
		exit(1);
	}
	crtn = extractDsaPartial(cspHand, &dsa2Pub, &dsa2PubPartial);
	if(crtn) {
		exit(1);
	}
	
	/* 
	 * Reference version of all 4 pub keys if we're going to mix & match 
	 */
	CSSM_KEY dsa1PubRef;
	CSSM_KEY dsa2PubRef;
	CSSM_KEY dsa1PubPartialRef;
	CSSM_KEY dsa2PubPartialRef;
	if(rawCSP && 		// CSPDL --> these were created as ref keys
	  !allRaw &&		// allRaw --> don't want ref keys
	  !allRef) {		// allRef --> these were created as ref keys
		if(cspRawKeyToRef(cspHand, &dsa1Pub, &dsa1PubRef) ||
		   cspRawKeyToRef(cspHand, &dsa2Pub, &dsa2PubRef)) {
			exit(1);
		}
	}
	if(!rawCSP || !allRaw) {
		/* these were created in raw form unconditionally */
		if(cspRawKeyToRef(cspHand, &dsa1PubPartial, 
				&dsa1PubPartialRef) ||
			cspRawKeyToRef(cspHand, &dsa2PubPartial, 
				&dsa2PubPartialRef)) {
			exit(1);
		}
		
		/* verify that these came back with the partial flag set */
		if(!(dsa1PubPartialRef.KeyHeader.KeyAttr &
				CSSM_KEYATTR_PARTIAL)) {
			printf("***CSSM_KEYATTR_PARTIAL not set after null unwrap"
				" of partial DSA key\n");
			if(testError(quiet)) {
				exit(1);
			}
		}
		if(!(dsa2PubPartialRef.KeyHeader.KeyAttr &
				CSSM_KEYATTR_PARTIAL)) {
			printf("***CSSM_KEYATTR_PARTIAL not set after null unwrap"
				" of partial DSA key\n");
			if(testError(quiet)) {
				exit(1);
			}
		}
	}
	
	int rtn = 0;
	for(unsigned loop=0; loop<loops; loop++) {
		/* four pub keys - raw or ref */
		CSSM_KEY_PTR pubKey_a;
		CSSM_KEY_PTR pubKey_b;
		CSSM_KEY_PTR pubKeyPartial_a;
		CSSM_KEY_PTR pubKeyPartial_b;
		bool mergedIsRef;
		
		if(allRef) {
			/* raw CSP only - all ref keys */
			/* base keys were generated as ref */
			pubKey_a = &dsa1Pub;
			pubKey_b = &dsa2Pub;
			/* these alwasy generated as raw */
			pubKeyPartial_a = &dsa1PubPartialRef;
			pubKeyPartial_b = &dsa2PubPartialRef;
			/* generated merged key ref too */
			mergedIsRef = true;
		}
		else if(allRaw) {
			/* raw CSP only - all raw keys */
			pubKey_a = &dsa1Pub;
			pubKey_b = &dsa2Pub;
			pubKeyPartial_a = &dsa1PubPartial;
			pubKeyPartial_b = &dsa2PubPartial;
			/* generated merged key ref too */
			mergedIsRef = false;
		}
		else if(!rawCSP) {
			/* CSPDL - base keys are ref, partials are raw */
			pubKey_a = &dsa1Pub;
			pubKey_b = &dsa2Pub;
			pubKeyPartial_a = &dsa1PubPartialRef;
			pubKeyPartial_b = &dsa2PubPartialRef;
			/* generated merged key ref too */
			mergedIsRef = true;
		}
		else {
			/* default: mix & match */
			pubKey_a = (loop & 1) ? &dsa1Pub : &dsa1PubRef;
			pubKey_b = (loop & 2) ? &dsa2Pub : &dsa2PubRef;
			pubKeyPartial_a = (loop & 4) ? 
				&dsa1PubPartial : &dsa1PubPartialRef;
			pubKeyPartial_b = (loop & 8) ? 
				&dsa2PubPartial : &dsa2PubPartialRef;
			/* generated merged key different from partial_a*/
			mergedIsRef = (loop & 2) ? true : false;
		}
		
		/* and two param keys - CSPDL requires raw, else the same as
		 * the "base" public key */
		CSSM_KEY_PTR pubKeyParam_a = pubKey_a;
		CSSM_KEY_PTR pubKeyParam_b = pubKey_b;
		if(!rawCSP) {
			pubKeyParam_a = &dsa1PubParam;
			pubKeyParam_b = &dsa2PubParam;
		}
		if(!quiet) {
			printf("...loop %u\n", loop);
		}
		rtn = doTest(cspHand, &dsa1Priv,
			pubKey_a, pubKeyPartial_a, pubKeyParam_a,
			pubKeyPartial_b, pubKeyParam_b,
			mergedIsRef, quiet, verbose);
		if(rtn) {
			break;
		}
		if(doPause) {
			fpurge(stdin);
			printf("Hit CR to proceed, q to quit: ");
			char inch = getchar();
			if(inch == 'q') {
				break;
			}
		}
	}
	
	/* cleanup */
	return(rtn);
}
