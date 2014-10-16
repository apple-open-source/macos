/*
 * dhTest - simple Diffie-Hellman exerciser. 
 */
#include <stdlib.h>
#include <stdio.h>
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"
#include <security_cdsa_utils/cuFileIo.h>
#include <strings.h>

#define USAGE_DEF		"noUsage"
#define LOOPS_DEF		10
#define KEY_SIZE_DEF	512
#define DERIVE_KEY_SIZE	128
#define DERIVE_KEY_ALG	CSSM_ALGID_AES

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  k=keySize (default = %d)\n", KEY_SIZE_DEF);
	printf("  l=loops (0=forever)\n");
	printf("  p=pauseInterval (default=0, no pause)\n");
	printf("  D (CSP/DL; default = bare CSP)\n");
	printf("  o=fileName (dump key and param blobs to filename)\n");
	printf("  i=filename (obtain param blobs from filename)\n");
	printf("  8 (private key in PKCS8 format, default is PKCS3)\n");
	printf("  x (public key in X509 format, default is PKCS3)\n");
	printf("  f (public key is ref form; default is raw)\n");
	printf("  q(uiet)\n");
	printf("  v(erbose))\n");
	exit(1);
}

/*
 * Generate a Diffie-Hellman key pair. Optionally allows specification of 
 * algorithm parameters, and optionally returns algorithm parameters if
 * we generate them. 
 */
static int dhKeyGen(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_KEY_PTR		pubKey,
	CSSM_KEY_PTR		privKey,
	const CSSM_DATA		*inParams,		// optional 
	CSSM_DATA_PTR		outParams,		// optional, we malloc
	uint32				keySizeInBits,
	CSSM_KEYBLOB_FORMAT privForm,
	CSSM_KEYBLOB_FORMAT pubForm,
	CSSM_BOOL			pubIsRef,
	CSSM_BOOL			quiet)
{
	CSSM_RETURN		crtn;
	CSSM_CC_HANDLE	ccHand;
	CSSM_DATA		labelData = { strlen(USAGE_DEF), (uint8 *)USAGE_DEF };
	
	if(inParams && outParams) {
		printf("***dhKeyGen: inParams and outParams are mutually "
			"exclusive.\n");
		return -1;
	}
	memset(pubKey, 0, sizeof(CSSM_KEY));
	memset(privKey, 0, sizeof(CSSM_KEY));
	
	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		CSSM_ALGID_DH,
		keySizeInBits,
		NULL,					// Seed
		NULL,					// Salt
		NULL,					// StartDate
		NULL,					// EndDate
		inParams,				// Params, may be NULL
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateKeyGenContext", crtn);
		return testError(quiet);
	}
	
	if((inParams == NULL) && (outParams != NULL)) {
		/* explicitly generate params and return them to caller */
		outParams->Data = NULL;
		outParams->Length = 0;
		crtn = CSSM_GenerateAlgorithmParams(ccHand, 
			keySizeInBits, outParams);
		if(crtn) {
			printError("CSSM_GenerateAlgorithmParams", crtn);
			return testError(quiet);
		}
	}

	uint32 privAttr = CSSM_KEYATTR_RETURN_REF;
	uint32 pubAttr = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	if(privForm != CSSM_KEYBLOB_RAW_FORMAT_NONE) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT,
			sizeof(uint32),	
			CAT_Uint32,
			NULL,
			privForm);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_PRIVATE_KEY"
				"_FORMAT)", crtn);
			return crtn;
		}
		privAttr = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}
	if(pubIsRef) {
		pubAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE;
	}
	else if(pubForm != CSSM_KEYBLOB_RAW_FORMAT_NONE) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT,
			sizeof(uint32),	
			CAT_Uint32,
			NULL,
			pubForm);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_PUBLIC_KEY"
				"_FORMAT)", crtn);
			return crtn;
		}
	}
	crtn = CSSM_GenerateKeyPair(ccHand,
		CSSM_KEYUSE_DERIVE,		// only legal use of a Diffie-Hellman key 
		pubAttr,
		&labelData,
		pubKey,
		/* private key specification */
		CSSM_KEYUSE_DERIVE,
		privAttr,
		&labelData,				// same labels
		NULL,					// CredAndAclEntry
		privKey);
	if(crtn) {
		printError("CSSM_GenerateKeyPair", crtn);
			return testError(quiet);
	}
	
	CSSM_DeleteContext(ccHand);
	return crtn;
}

/*
 * Perform Diffie-Hellman key exchange. 
 * Given "our" private key (in the form of a CSSM_KEY) and "their" public
 * key (in the form of a raw blob of bytes), cook up a symmetric key.
 */
static int dhKeyExchange(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_KEY_PTR	myPrivKey,
	CSSM_KEY_PTR	theirPubKey,
	CSSM_KEY_PTR	derivedKey,				// RETURNED
	uint32			deriveKeySizeInBits,
	CSSM_ALGORITHMS	derivedKeyAlg,
	uint32			derivedKeyUsage,
	uint32			derivedKeyAttr,
	CSSM_BOOL		quiet)
{
	CSSM_RETURN 			crtn;
	CSSM_ACCESS_CREDENTIALS	creds;
	CSSM_CC_HANDLE			ccHand;
	CSSM_DATA				labelData = { strlen(USAGE_DEF), (uint8 *)USAGE_DEF };
	
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	memset(derivedKey, 0, sizeof(CSSM_KEY));
	
	crtn = CSSM_CSP_CreateDeriveKeyContext(cspHand,
		CSSM_ALGID_DH,
		derivedKeyAlg,
		deriveKeySizeInBits,
		&creds,
		myPrivKey,		// BaseKey
		0,				// IterationCount
		0,				// Salt
		0,				// Seed
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateDeriveKeyContext", crtn);
		return testError(quiet);
	}
	
	/* 
	 * Public key passed in as CSSM_DATA *Param - only if
	 * the pub key is in raw PKCS3 form
	 */
	CSSM_DATA nullParam = {0, NULL};
	CSSM_DATA_PTR paramPtr;
	CSSM_KEYHEADER &hdr = theirPubKey->KeyHeader;
	if((hdr.BlobType == CSSM_KEYBLOB_RAW) &&
	   (hdr.Format == CSSM_KEYBLOB_RAW_FORMAT_PKCS3)) {
		/* simple case */
		paramPtr = &theirPubKey->KeyData;
	}
	else {
		/* add this pub key as a context attr */
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PUBLIC_KEY,
			sizeof(CSSM_KEY),	
			CAT_Ptr,
			(void *)theirPubKey,
			0);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_PUBLIC_KEY)",
				crtn);
			return crtn;
		}
		paramPtr = &nullParam;
	}
	crtn = CSSM_DeriveKey(ccHand,
		paramPtr,
		derivedKeyUsage,
		derivedKeyAttr,
		&labelData,
		NULL,				// cread/acl
		derivedKey);
	if(crtn) {
		printError("CSSM_DeriveKey", crtn);
	}
	CSSM_DeleteContext(ccHand);
	return crtn;
}

int doTest(
	CSSM_CSP_HANDLE		cspHand,
	const CSSM_DATA		*inParams,		// optional
	CSSM_DATA_PTR		outParams,		// optional
	uint32				keySizeInBits,
	CSSM_KEYBLOB_FORMAT	privForm,
	CSSM_KEYBLOB_FORMAT	pubForm,
	CSSM_BOOL			pubIsRef,
	CSSM_BOOL			quiet)
{
	CSSM_KEY	myPriv;
	CSSM_KEY	myPub;
	CSSM_KEY	theirPriv;
	CSSM_KEY	theirPub;
	int			rtn = 0;
	
	/* generate two key pairs */
	if(dhKeyGen(cspHand,
		&myPub,
		&myPriv,
		inParams,
		outParams,
		keySizeInBits,
		privForm,
		pubForm,
		pubIsRef,
		quiet)) {
			return 1;
	}
	
	/* note this MUST match the params either specified or generated in previous
	 * call */
	if((inParams == NULL) && (outParams == NULL)) {
		printf("***BRRZAP! Must provide a way to match D-H parameters!\n");
		exit(1);
	}
	const CSSM_DATA *theParams = inParams;
	if(theParams == NULL) {
		theParams = outParams;
	}
	if(dhKeyGen(cspHand,
		&theirPub,
		&theirPriv,
		theParams,
		NULL,				// outParams
		keySizeInBits,
		privForm,
		pubForm,
		pubIsRef,
		quiet)) {
			return 1;
	}
	
	/* derive two keys, ensure they match */
	CSSM_KEY myDerive;
	CSSM_KEY theirDerive;
	if(dhKeyExchange(cspHand,
		&myPriv,
		&theirPub,
		&myDerive,
		DERIVE_KEY_SIZE,
		DERIVE_KEY_ALG,
		CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
		CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE,
		quiet)) {
			return testError(quiet);
	}
	if(dhKeyExchange(cspHand,
		&theirPriv,
		&myPub,
		&theirDerive,
		DERIVE_KEY_SIZE,
		DERIVE_KEY_ALG,
		CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
		CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE,
		quiet)) {
			return testError(quiet);
	}
	
	if(!appCompareCssmData(&myDerive.KeyData, &theirDerive.KeyData)) {
		printf("***Key Exchange data miscompare***\n");
		rtn = testError(quiet);
	}
	cspFreeKey(cspHand, &myPub);
	cspFreeKey(cspHand, &myPriv);
	cspFreeKey(cspHand, &theirPub);
	cspFreeKey(cspHand, &theirPriv);
	cspFreeKey(cspHand, &myDerive);
	cspFreeKey(cspHand, &theirDerive);
	return rtn;
}

int main(int argc, char **argv)
{
	int		 				arg;
	char					*argp;
	CSSM_CSP_HANDLE 		cspHand;
	unsigned				loop;
	int 					i;
	CSSM_DATA				inParams = {0, NULL};
	CSSM_DATA				outParams = {0, NULL};
	CSSM_DATA_PTR			inParamPtr = NULL;
	CSSM_DATA_PTR			outParamPtr = NULL;
	
	/* user-spec'd parameters */
	unsigned				keySize = KEY_SIZE_DEF;
	unsigned				pauseInterval = 0;
	unsigned				loops = LOOPS_DEF;
	CSSM_BOOL				quiet = CSSM_FALSE;
	CSSM_BOOL				verbose = CSSM_FALSE;
	CSSM_BOOL				bareCsp = CSSM_TRUE;
	char					*inFileName = NULL;
	char					*outFileName = NULL;
	/* default: "use default blob form", i.e., PKCS3 */
	CSSM_KEYBLOB_FORMAT		privForm = CSSM_KEYBLOB_RAW_FORMAT_NONE;
	CSSM_KEYBLOB_FORMAT		pubForm = CSSM_KEYBLOB_RAW_FORMAT_NONE;
	CSSM_BOOL				pubIsRef = CSSM_FALSE;
	
	for(arg=1; arg<argc; arg++) { 
		argp = argv[arg];
	    switch(argp[0]) {
			case 'k':
				keySize = atoi(&argp[2]);
				break;
		    case 'l':
				loops = atoi(&argp[2]);
				break;
		    case 'p':
				pauseInterval = atoi(&argp[2]);
				break;
			case 'i':
				inFileName = &argp[2];
				break;
			case 'o':
				outFileName = &argp[2];
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				break;
			case '8':
				privForm = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
				break;
			case 'x':
				pubForm = CSSM_KEYBLOB_RAW_FORMAT_X509;
				break;
			case 'f':
				pubIsRef = CSSM_TRUE;
				break;
			case 'q':
				quiet = CSSM_TRUE;
				break;
		    case 'v':
		    	verbose = CSSM_TRUE;
				break;
			default:
				usage(argv);
		}
	}
	
	/* Actually this test does NOT run with CSPDL */
	if(!bareCsp) {
		printf("***%s does not run with CSPDL; aborting.\n", argv[0]);
		exit(1);
	}
	
	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	
	/* optionally fetch algorithm parameters from a file */
	if(inFileName) {
		unsigned len;
		int r = readFile(inFileName, &inParams.Data, &len);
		if(r) {
			printf("***Can't read parameters from %s; aborting.\n",
				inFileName);
			exit(1);
		}
		inParams.Length = len;
		/* constant from now on */
		inParamPtr = &inParams;
	}
	else {
		/* first time thru, no user-supplied parameters; generate them and
		 * save in outParams */
		outParamPtr = &outParams;
	}
	printf("Starting dhTest; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	for(loop=1; ; loop++) {
		if(!quiet) {
			printf("...Loop %d\n", loop);
		}
		i = doTest(cspHand, inParamPtr, outParamPtr, keySize, privForm, 
			pubForm, pubIsRef, quiet);
		if(i) {
			break;
		}
		if(loop == 1) {
			/* first time thru */
			if(outFileName) {
				/* save parameters for another run */
				i = writeFile(outFileName, outParams.Data,
					outParams.Length);
				if(i) {
					printf("***Error writing params to %s; continuing.\n",
						outFileName);
				}
				else {
					if(!quiet) {
						printf("...wrote %lu bytes to %s\n", 
							outParams.Length, outFileName);
					}
				}
			}
			if(!inFileName) {
				/* from now on, use the parameters we just generated */
				inParamPtr = &outParams;
			}
			/* and in any case don't fetch any more params */
			outParamPtr = NULL;
		}
		if(loops && (loop == loops)) {
			break;
		}
		if(pauseInterval && ((loop % pauseInterval) == 0)) {
			char inch;
			
			fpurge(stdin);
			printf("Hit CR to proceed, q to quit: ");
			inch = getchar();
			if(inch == 'q') {
				break;
			}
		}
	}
	CSSM_ModuleDetach(cspHand);
	if(!quiet) {
		printf("OK\n");
	}
	return 0;
}
