/*
 * keyHashAsym.c - CSSM_APPLECSP_KEYDIGEST passthrough test for all
 *				   known asymmetric algorithms and key formats
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"
#include "cspdlTesting.h"
#include <security_cdsa_utils/cuFileIo.h>

#define USAGE_NAME			"noUsage"
#define USAGE_NAME_LEN		(strlen(USAGE_NAME))
#define LOOPS_DEF			10

#define DSA_PARAM_FILE		"dsaParams_512.der"
#define DH_PARAM_FILE		"dhParams_512.der"

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("  D (CSP/DL; default = bare CSP)\n");
	printf("  p(ause on each loop)\n");
	printf("  q(uiet)\n");
	printf("  v(erbose))\n");
	exit(1);
}

static CSSM_DATA	dsa512Params;
static CSSM_DATA	dh512Params;

/*
 * Describe parameters for one test iteration.
 */
typedef struct {
	CSSM_ALGORITHMS		keyAlg;
	CSSM_KEYBLOB_FORMAT	pubKeyForm;
	CSSM_KEYBLOB_FORMAT	privKeyForm;
	uint32				keySizeInBits;
	CSSM_DATA			*algParams;		// optional
} KeyHashTest;

KeyHashTest KeyHashTestParams[] = 
{
	/* RSA */
	{ 	CSSM_ALGID_RSA, 
		CSSM_KEYBLOB_RAW_FORMAT_NONE, 	CSSM_KEYBLOB_RAW_FORMAT_NONE,
		512, NULL
	},
	{ 	CSSM_ALGID_RSA, 
		CSSM_KEYBLOB_RAW_FORMAT_PKCS1, 	CSSM_KEYBLOB_RAW_FORMAT_NONE,
		512, NULL
	},
	{ 	CSSM_ALGID_RSA, 
		CSSM_KEYBLOB_RAW_FORMAT_X509, 	CSSM_KEYBLOB_RAW_FORMAT_NONE,
		512, NULL
	},
	{ 	CSSM_ALGID_RSA, 
		CSSM_KEYBLOB_RAW_FORMAT_NONE, 	CSSM_KEYBLOB_RAW_FORMAT_PKCS1,
		512, NULL
	},
	{ 	CSSM_ALGID_RSA, 
		CSSM_KEYBLOB_RAW_FORMAT_NONE, 	CSSM_KEYBLOB_RAW_FORMAT_PKCS8,
		512, NULL
	},
	
	/* ECDSA */
	{	CSSM_ALGID_ECDSA,
		CSSM_KEYBLOB_RAW_FORMAT_NONE, CSSM_KEYBLOB_RAW_FORMAT_NONE,
		192, NULL 
	},
	{	CSSM_ALGID_ECDSA,
		CSSM_KEYBLOB_RAW_FORMAT_X509, CSSM_KEYBLOB_RAW_FORMAT_PKCS8,
		256, NULL 
	},
	{	CSSM_ALGID_ECDSA,
		CSSM_KEYBLOB_RAW_FORMAT_NONE, CSSM_KEYBLOB_RAW_FORMAT_PKCS8,
		384, NULL 
	},
	{	CSSM_ALGID_ECDSA,
		CSSM_KEYBLOB_RAW_FORMAT_X509, CSSM_KEYBLOB_RAW_FORMAT_PKCS8,
		521, NULL 
	},

	/* DSA */
	{ 	CSSM_ALGID_DSA, 
		CSSM_KEYBLOB_RAW_FORMAT_NONE, 	CSSM_KEYBLOB_RAW_FORMAT_NONE,
		512, &dsa512Params
	},
	{ 	CSSM_ALGID_DSA, 
		CSSM_KEYBLOB_RAW_FORMAT_FIPS186, 	CSSM_KEYBLOB_RAW_FORMAT_NONE,
		512, &dsa512Params
	},
	{ 	CSSM_ALGID_DSA, 
		CSSM_KEYBLOB_RAW_FORMAT_X509, 	CSSM_KEYBLOB_RAW_FORMAT_NONE,
		512, &dsa512Params
	},
	{ 	CSSM_ALGID_DSA, 
		CSSM_KEYBLOB_RAW_FORMAT_NONE, 	CSSM_KEYBLOB_RAW_FORMAT_FIPS186,
		512, &dsa512Params
	},
	{ 	CSSM_ALGID_DSA, 
		CSSM_KEYBLOB_RAW_FORMAT_NONE, 	CSSM_KEYBLOB_RAW_FORMAT_OPENSSL,
		512, &dsa512Params
	},
	{ 	CSSM_ALGID_DSA, 
		CSSM_KEYBLOB_RAW_FORMAT_NONE, 	CSSM_KEYBLOB_RAW_FORMAT_PKCS8,
		512, &dsa512Params
	},
	{ 	CSSM_ALGID_DSA, 
		CSSM_KEYBLOB_RAW_FORMAT_X509, 	CSSM_KEYBLOB_RAW_FORMAT_PKCS8,
		512, &dsa512Params
	},
	
	/* Diffie-Hellman */
	{ 	CSSM_ALGID_DH, 
		CSSM_KEYBLOB_RAW_FORMAT_NONE, 	CSSM_KEYBLOB_RAW_FORMAT_NONE,
		512, &dh512Params
	},
	{ 	CSSM_ALGID_DH, 
		CSSM_KEYBLOB_RAW_FORMAT_PKCS3, 	CSSM_KEYBLOB_RAW_FORMAT_NONE,
		512, &dh512Params
	},
	{ 	CSSM_ALGID_DH, 
		CSSM_KEYBLOB_RAW_FORMAT_NONE, 	CSSM_KEYBLOB_RAW_FORMAT_PKCS3,
		512, &dh512Params
	},
	{ 	CSSM_ALGID_DH, 
		CSSM_KEYBLOB_RAW_FORMAT_NONE, 	CSSM_KEYBLOB_RAW_FORMAT_PKCS8,
		512, &dh512Params
	},
	{ 	CSSM_ALGID_DH, 
		CSSM_KEYBLOB_RAW_FORMAT_X509, 	CSSM_KEYBLOB_RAW_FORMAT_NONE,
		512, &dh512Params
	},
	{ 	CSSM_ALGID_DH, 
		CSSM_KEYBLOB_RAW_FORMAT_X509, 	CSSM_KEYBLOB_RAW_FORMAT_PKCS8,
		512, &dh512Params
	},
	
	/* FEE */
	{	CSSM_ALGID_FEE,
		CSSM_KEYBLOB_RAW_FORMAT_NONE, 	CSSM_KEYBLOB_RAW_FORMAT_NONE,
		127, NULL 
	},
	{	CSSM_ALGID_FEE,
		CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING, 	CSSM_KEYBLOB_RAW_FORMAT_NONE,
		128, NULL 
	},
	{	CSSM_ALGID_FEE,
		CSSM_KEYBLOB_RAW_FORMAT_NONE, 	CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING,
		161, NULL 
	},
	{	CSSM_ALGID_FEE,
		CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING, 	
		CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING,
		192, NULL 
	},
	
};
#define NUM_TEST_PARAMS\
	(sizeof(KeyHashTestParams) / sizeof(KeyHashTestParams[0]))

static void dumpBuf(uint8 *buf,
	unsigned len)
{
	unsigned i;
	
	printf("   ");
	for(i=0; i<len; i++) {
		printf("%02X ", buf[i]);
		if((i % 24) == 23) {
			printf("\n      ");
		}
	}
	printf("\n");
}

const char *formStr(CSSM_KEYBLOB_FORMAT form)
{
	switch(form) {
		case CSSM_KEYBLOB_RAW_FORMAT_NONE: return "NONE";
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS1: return "PKCS1";
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS3: return "PKCS3";
		case CSSM_KEYBLOB_RAW_FORMAT_FIPS186: return "FIPS186";
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS8: return "PKCS8";
		case CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING: return "OCTET_STRING";
		case CSSM_KEYBLOB_RAW_FORMAT_OTHER: return "OTHER";
		case CSSM_KEYBLOB_RAW_FORMAT_X509: return "X509";
		case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH: return "OPENSSH";
		case CSSM_KEYBLOB_RAW_FORMAT_OPENSSL: return "OPENSSL";
		default: 
			printf("**BRRZAP! formStr needs work\n");
			exit(1);
	}
}

const char *algStr(CSSM_ALGORITHMS alg)
{
	switch(alg) {
		case CSSM_ALGID_RSA: return "RSA";
		case CSSM_ALGID_DSA: return "DSA";
		case CSSM_ALGID_DH: return "DH";
		case CSSM_ALGID_FEE: return "FEE";
		case CSSM_ALGID_ECDSA: return "ECDSA";
		default: 
			printf("**BRRZAP! algStr needs work\n");
			exit(1);
	}
}

static void showTestParams(KeyHashTest *testParam)
{
	printf("alg %s  pubForm %s  privForm %s\n",
			algStr(testParam->keyAlg),
		formStr(testParam->pubKeyForm),
		formStr(testParam->privKeyForm));

}

/*
 * Generate key pair of specified alg and raw format. 
 * Alg params are optional, though they are expected to be here
 * for DH and DSA.
 */
static CSSM_RETURN genKeyPair(
	CSSM_CSP_HANDLE cspHand,
	CSSM_ALGORITHMS keyAlg,
	uint32 keySize,					// in bits
	CSSM_KEY_PTR pubKey,			
	CSSM_KEYBLOB_FORMAT pubFormat,
	CSSM_KEY_PTR privKey,			
	CSSM_KEYBLOB_FORMAT privFormat,
	const CSSM_DATA	*inParams)		// optional 
{
	CSSM_RETURN				crtn;
	CSSM_CC_HANDLE 			ccHand;
	CSSM_DATA				keyLabelData;
	CSSM_RETURN 			ocrtn = CSSM_OK;
	
	keyLabelData.Data        = (uint8 *)USAGE_NAME,
	keyLabelData.Length      = USAGE_NAME_LEN;
	memset(pubKey, 0, sizeof(CSSM_KEY));
	memset(privKey, 0, sizeof(CSSM_KEY));
	
	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		keyAlg,
		keySize,
		NULL,					// Seed
		NULL,					// Salt
		NULL,					// StartDate
		NULL,					// EndDate
		inParams,				// Params, may be NULL
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateKeyGenContext", crtn);
		return crtn;
	}
	
	/* optional format specifiers */
	if(pubFormat != CSSM_KEYBLOB_RAW_FORMAT_NONE) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT,
			sizeof(uint32),	
			CAT_Uint32,
			NULL,
			pubFormat);
		if(crtn) {
				printError("AddContextAttribute("
					"CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT)", crtn);
			return crtn;
		}
	}
	if(privFormat != CSSM_KEYBLOB_RAW_FORMAT_NONE) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT,
			sizeof(uint32),
			CAT_Uint32,
			NULL,
			privFormat);
		if(crtn) {
			printError("AddContextAttribute("
				"CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT)", crtn);
			return crtn;
		}
	}
	CSSM_KEYATTR_FLAGS attrFlags = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	crtn = CSSM_GenerateKeyPair(ccHand,
		CSSM_KEYUSE_DERIVE,
		attrFlags,
		&keyLabelData,
		pubKey,
		CSSM_KEYUSE_DERIVE,
		attrFlags,
		&keyLabelData,			// same labels
		NULL,					// CredAndAclEntry
		privKey);
	if(crtn) {
		printError("CSSM_GenerateKeyPair", crtn);
		ocrtn = crtn;
	}
	if(ccHand != 0) {
		crtn = CSSM_DeleteContext(ccHand);
		if(crtn) {
			printError("CSSM_DeleteContext", crtn);
			ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
		}
	}
	return ocrtn;
}

/*
 * Given two keys (presumably, in this test, one a raw key and 
 * one an equivalent ref key), calculate the key digest of both of them
 * and ensure they're the same. 
 */
static int compareKeyHashes(
	const CSSM_DATA *key1Hash,
	const char *key1Descr,
	const CSSM_DATA *key2Hash,
	const char *key2Descr,
	CSSM_BOOL verbose)
{
	if(appCompareCssmData(key1Hash, key2Hash)) {
		return 0;
	}
	printf("***Key Digest miscompare (%s,%s)***\n", key1Descr, key2Descr);
	if(!verbose) {
		printf("...%s hash:\n", key1Descr);
		dumpBuf(key1Hash->Data, key1Hash->Length);
		printf("...%s hash:\n", key2Descr);
		dumpBuf(key2Hash->Data, key2Hash->Length);
	}
	return 1;
}

/*
 * Given a KeyHashTest:
 *	-- cook up key pair, raw, specified formats
 *  -- NULL unwrap each raw to ref;
 *  -- obtain four key digests;
 *  -- ensure all digests match;
 */
static int doTest(
	CSSM_CSP_HANDLE		rawCspHand,		// generate keys here
	CSSM_CSP_HANDLE		refCspHand,		// null unwrap here
	KeyHashTest			*testParam,
	CSSM_BOOL			verbose,
	CSSM_BOOL			quiet)
{
	CSSM_RETURN crtn;
	CSSM_KEY pubKey;
	CSSM_KEY privKey;
	CSSM_KEY pubKeyRef;			
	CSSM_KEY privKeyRef;
	CSSM_DATA_PTR rawPubHash;
	CSSM_DATA_PTR rawPrivHash;
	CSSM_DATA_PTR refPubHash;
	CSSM_DATA_PTR refPrivHash;
	int rtn = 0;
	
	/* generate key pair, specified raw form */
	crtn = genKeyPair(rawCspHand,
		testParam->keyAlg,
		testParam->keySizeInBits,
		&pubKey,
		testParam->pubKeyForm,
		&privKey,
		testParam->privKeyForm,
		testParam->algParams);
	if(crtn) {
		return testError(quiet);
	}
	
	/* null unwrap both raw keys to ref form */
	crtn = cspRawKeyToRef(refCspHand, &pubKey, &pubKeyRef);
	if(crtn) {
		return testError(quiet);
	}
	crtn = cspRawKeyToRef(refCspHand, &privKey, &privKeyRef);
	if(crtn) {
		return testError(quiet);
	}
	
	/* calculate four key digests */
	crtn = cspKeyHash(rawCspHand, &pubKey, &rawPubHash);
	if(crtn) {
		return testError(quiet);
	}
	crtn = cspKeyHash(rawCspHand, &privKey, &rawPrivHash);
	if(crtn) {
		return testError(quiet);
	}
	crtn = cspKeyHash(refCspHand, &pubKeyRef, &refPubHash);
	if(crtn) {
		return testError(quiet);
	}
	crtn = cspKeyHash(refCspHand, &privKeyRef, &refPrivHash);
	if(crtn) {
		return testError(quiet);
	}

	if(verbose) {
		printf("...raw pub key hash:\n");
		dumpBuf(rawPubHash->Data, rawPubHash->Length);
		printf("...ref pub key hash:\n");
		dumpBuf(refPubHash->Data, refPubHash->Length);
		printf("...raw priv key hash:\n");
		dumpBuf(rawPrivHash->Data, rawPrivHash->Length);
		printf("...ref priv key hash:\n");
		dumpBuf(refPrivHash->Data, refPrivHash->Length);
	}

	/* compare */
	rtn += compareKeyHashes(rawPubHash, "Raw public",
		refPubHash, "Ref public", verbose);
	rtn += compareKeyHashes(rawPrivHash, "Raw private",
		refPrivHash, "Ref private", verbose);
	rtn += compareKeyHashes(refPubHash, "Ref public",
		refPrivHash, "Ref private", verbose);
	if(rtn) {
		rtn = testError(quiet);
	}
	cspFreeKey(rawCspHand, &pubKey);
	cspFreeKey(rawCspHand, &privKey);
	cspFreeKey(refCspHand, &pubKeyRef);
	cspFreeKey(refCspHand, &privKeyRef);
	appFreeCssmData(rawPubHash, CSSM_TRUE);
	appFreeCssmData(rawPrivHash, CSSM_TRUE);
	appFreeCssmData(refPubHash, CSSM_TRUE);
	appFreeCssmData(refPrivHash, CSSM_TRUE);
	return rtn;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	CSSM_CSP_HANDLE 	rawCspHand;		// always Raw CSP
	CSSM_CSP_HANDLE		refCspHand;		// CSPDL if !bareCsp
	int					rtn = 0;
	int					i;
	unsigned			len;
	
	/*
	 * User-spec'd params
	 */
	unsigned			loops = LOOPS_DEF;
	CSSM_BOOL			verbose = CSSM_FALSE;
	CSSM_BOOL			quiet = CSSM_FALSE;
	CSSM_BOOL			bareCsp = CSSM_TRUE;
	CSSM_BOOL			doPause = CSSM_FALSE;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'l':
				loops = atoi(&argp[2]);
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				break;
		    case 'p':
		    	doPause = CSSM_TRUE;
				break;
		    case 'v':
		    	verbose = CSSM_TRUE;
				break;
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}

	/* prefetch the alg params */
	rtn = readFile(DSA_PARAM_FILE, &dsa512Params.Data, &len);
	if(rtn) {
		printf("***%s file missing. Aborting.\n", DSA_PARAM_FILE);
		exit(1);
	}
	dsa512Params.Length = len;
	rtn = readFile(DH_PARAM_FILE, &dh512Params.Data, &len);
	if(rtn) {
		printf("***%s file missing. Aborting.\n", DH_PARAM_FILE);
		exit(1);
	}
	dh512Params.Length = len;
	
	printf("Starting keyHashAsym; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	refCspHand = cspDlDbStartup(bareCsp, NULL);
	if(refCspHand == 0) {
		exit(1);
	}
	if(bareCsp) {
		/* raw and ref on same CSP */
		rawCspHand = refCspHand;
	}
	else {
		/* generate on CSPDL, NULL unwrap to bare CSP */
		rawCspHand = cspDlDbStartup(CSSM_TRUE, NULL);
		if(rawCspHand == 0) {
			exit(1);
		}
	}
	for(loop=1; ; loop++) {
		if(!quiet) {
			printf("...loop %d\n", loop);
		}
		for(unsigned testNum=0; testNum<NUM_TEST_PARAMS; testNum++) {
			KeyHashTest *testParams = &KeyHashTestParams[testNum];
			if(!quiet) {
				printf("..."); showTestParams(testParams);
			}
			rtn = doTest(rawCspHand, refCspHand, testParams, verbose, quiet);
			if(rtn) {
				goto done;
			}
			if(doPause) {
				fpurge(stdin);
				printf("Hit CR to proceed: ");
				getchar();
			}
		}
		if(loops && (loop == loops)) {
			break;
		}
	}
done:
	if((rtn == 0) && !quiet) {
		printf("...%s complete\n", argv[0]);
	}
	return rtn;
}
