/*
 * dhFullTest - Diffie-Hellman exerciser. 
 */
#include <stdlib.h>
#include <stdio.h>
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"
#include <security_cdsa_utils/cuFileIo.h>
#include <strings.h>
#include "cspdlTesting.h"

#define USAGE_DEF		"noUsage"
#define LOOPS_DEF		10
#define KEY_SIZE_DEF	512
#define DERIVE_KEY_SIZE	128
#define DERIVE_KEY_ALG	CSSM_ALGID_AES
#define ENCR_ALG		CSSM_ALGID_AES
#define ENCR_MODE		CSSM_ALGMODE_CBCPadIV8
#define ENCR_PADDING	CSSM_PADDING_PKCS7
#define MAX_PTEXT_SIZE	1024

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  k=keySize (default = %d)\n", KEY_SIZE_DEF);
	printf("  l=loops (0=forever)\n");
	printf("  p=pauseInterval (default=0, no pause)\n");
	printf("  D (CSP/DL; default = bare CSP)\n");
	printf("  o=fileName (dump key and param blobs to filename)\n");
	printf("  i=filename (obtain param blobs from filename\n");
	printf("  q(uiet)\n");
	printf("  v(erbose))\n");
	exit(1);
}

/* 
 * Translate blob format to strings.
 */
typedef struct {
	CSSM_KEYBLOB_FORMAT form;
	const char *str;
} BlobDesc;

static const BlobDesc BlobDescs[] = {
	{ CSSM_KEYBLOB_RAW_FORMAT_NONE,  "NONE" },
	{ CSSM_KEYBLOB_RAW_FORMAT_PKCS3, "PKCS3" },
	{ CSSM_KEYBLOB_RAW_FORMAT_PKCS8, "PKCS8" },
	{ CSSM_KEYBLOB_RAW_FORMAT_X509,  "X509" },
};
#define NUM_BLOB_DESCS	(sizeof(BlobDescs) / sizeof(BlobDesc))

static const char *blobFormStr(
	CSSM_KEYBLOB_FORMAT form)
{
	for(unsigned i=0; i<NUM_BLOB_DESCS; i++) {
		const BlobDesc *bdp = &BlobDescs[i];
		if(bdp->form == form) {
			return bdp->str;
		}
	}
	return "***UNKNOWN BLOB FORM""";
}

/*
 * Generate a Diffie-Hellman key pair. Optionally allows specification of 
 * algorithm parameters, and optionally returns algorithm parameters if
 * we generate them. 
 */
static int dhKeyGen(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_KEY_PTR	pubKey,
	CSSM_BOOL		pubIsRef,
	CSSM_KEYBLOB_FORMAT	pubForm,		// NONE, PKCS3, X509
	CSSM_KEYBLOB_FORMAT	expectPubForm,	// PKCS3, X509
	CSSM_KEY_PTR	privKey,
	CSSM_BOOL		privIsRef,
	CSSM_KEYBLOB_FORMAT	privForm,		// NONE, PKCS3, PKCS8
	CSSM_KEYBLOB_FORMAT	expectPrivForm,	// PKCS3, PKCS8
	const CSSM_DATA	*inParams,		// optional 
	CSSM_DATA_PTR	outParams,		// optional, we malloc
	uint32			keySizeInBits,
	CSSM_BOOL		quiet)
{
	CSSM_RETURN		crtn;
	CSSM_CC_HANDLE 	ccHand;
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
	
	if((privForm != CSSM_KEYBLOB_RAW_FORMAT_NONE) && !privIsRef) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT,
			sizeof(uint32),	
			CAT_Uint32,
			NULL,
			privForm);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_PRIVATE_KEY_"
				"FORMAT)", crtn);
			return crtn;
		}
	}
	if((pubForm != CSSM_KEYBLOB_RAW_FORMAT_NONE) && !pubIsRef) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT,
			sizeof(uint32),	
			CAT_Uint32,
			NULL,
			pubForm);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_PUBLIC_KEY_"
				"FORMAT)", crtn);
			return crtn;
		}
	}

	uint32 privAttr;
	uint32 pubAttr = CSSM_KEYATTR_EXTRACTABLE;
	if(pubIsRef) {
		pubAttr |= CSSM_KEYATTR_RETURN_REF;
	}
	else {
		pubAttr |= CSSM_KEYATTR_RETURN_DATA;
	}
	if(privIsRef) {
		privAttr = CSSM_KEYATTR_RETURN_REF;
	}
	else {
		privAttr = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}
	crtn = CSSM_GenerateKeyPair(ccHand,
		/* 
		 * Public key specification. We specify raw key format 
		 * here since we have to have access to the raw public key 
		 * bits in order to perform D-H key exchange.
		 */
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
	if(!privIsRef && (privKey->KeyHeader.Format != expectPrivForm)) {
		printf("***Expected priv format %s got %s\n",
			blobFormStr(expectPrivForm),
			blobFormStr(privKey->KeyHeader.Format));
		return testError(quiet);
	}
	if(!pubIsRef && (pubKey->KeyHeader.Format != expectPubForm)) {
		printf("***Expected pub format %s got %s\n",
			blobFormStr(expectPubForm),
			blobFormStr(pubKey->KeyHeader.Format));
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
	CSSM_DATA				labelData = 
								{ strlen(USAGE_DEF), (uint8 *)USAGE_DEF };
	
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

static CSSM_DATA someIv = {16, (uint8 *)"Some enchanted init vector" };

/*
 * Encrypt ptext with myDeriveKey ==> ctext
 * decrypt ctext with theirDeriveKey ==> rptext
 * ensure ptext == rptext
 */  
static int doEncryptTest(
	CSSM_CSP_HANDLE		cspHand,
	const CSSM_DATA		*ptext,
	CSSM_KEY_PTR		myDeriveKey,
	CSSM_KEY_PTR		theirDeriveKey,
	CSSM_ALGORITHMS		encrAlg,
	uint32				encrMode,
	CSSM_PADDING		encrPadding,
	CSSM_BOOL			quiet)
{
	CSSM_DATA ctext = {0, NULL};
	CSSM_DATA rptext = {0, NULL};
	CSSM_RETURN crtn;
	
	crtn = cspEncrypt(cspHand,
		encrAlg,
		encrMode,
		encrPadding,
		myDeriveKey,
		NULL,			// 2nd key
		0,				// effective key size
		0,				// rounds
		&someIv,
		ptext,
		&ctext,
		CSSM_FALSE);	// mallocCtext
	if(crtn) {
		return testError(quiet);
	}
	crtn = cspDecrypt(cspHand,
		encrAlg,
		encrMode,
		encrPadding,
		theirDeriveKey,
		NULL,			// 2nd key
		0,				// effective key size
		0,				// rounds
		&someIv,
		&ctext,
		&rptext,
		CSSM_FALSE);	// mallocCtext
	if(crtn) {
		return testError(quiet);
	}
	if(!appCompareCssmData(ptext, &rptext)) {
		printf("***DATA MISCOMPARE***\n");
		return testError(quiet);
	}
	appFree(ctext.Data, NULL);
	appFree(rptext.Data, NULL);
	return 0;
}

int doTest(
	CSSM_CSP_HANDLE		cspHand,
	const CSSM_DATA		*ptext,
	CSSM_BOOL			pubIsRef,
	CSSM_KEYBLOB_FORMAT	pubForm,		// NONE, PKCS3, X509
	CSSM_KEYBLOB_FORMAT	expectPubForm,	// PKCS3, X509
	CSSM_BOOL			privIsRef,
	CSSM_KEYBLOB_FORMAT	privForm,		// NONE, PKCS3, PKCS8
	CSSM_KEYBLOB_FORMAT	expectPrivForm,	// PKCS3, PKCS8
	CSSM_BOOL			sym1IsRef,
	CSSM_BOOL			sym2IsRef,
	const CSSM_DATA		*inParams,		// optional
	CSSM_DATA_PTR		outParams,		// optional
	uint32				keySizeInBits,
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
		pubIsRef,
		pubForm,
		expectPubForm,
		&myPriv,
		privIsRef,
		privForm,
		expectPrivForm,
		inParams,
		outParams,
		keySizeInBits,
		quiet)) {
			return 1;
	}
	
	/* note this MUST match the params either specified or generated 
	 * in previous call */
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
		pubIsRef,
		/* let's always use default for this pair */
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_KEYBLOB_RAW_FORMAT_PKCS3,		// we know this is the default
		&theirPriv,
		privIsRef,
		/* let's always use default for this pair */
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_KEYBLOB_RAW_FORMAT_PKCS3,		// we know this is the default
		theParams,
		NULL,				// outParams
		keySizeInBits,
		quiet)) {
			return 1;
	}
		
	/* derive two keys, ensure they match */
	CSSM_KEY myDerive;
	CSSM_KEY theirDerive;
	uint32 myDeriveAttr;
	uint32 theirDeriveAttr;
	if(sym1IsRef) {
		myDeriveAttr = CSSM_KEYATTR_RETURN_REF;
	}
	else {
		myDeriveAttr = 
			CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}
	if(sym2IsRef) {
		theirDeriveAttr = CSSM_KEYATTR_RETURN_REF;
	}
	else {
		theirDeriveAttr = 
			CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}
	if(dhKeyExchange(cspHand,
		&myPriv,
		&theirPub,
		&myDerive,
		DERIVE_KEY_SIZE,
		DERIVE_KEY_ALG,
		CSSM_KEYUSE_ENCRYPT,
		myDeriveAttr,
		quiet)) {
			return testError(quiet);
	}
	if(dhKeyExchange(cspHand,
		&theirPriv,
		&myPub,
		&theirDerive,
		DERIVE_KEY_SIZE,
		DERIVE_KEY_ALG,
		CSSM_KEYUSE_DECRYPT,
		theirDeriveAttr,
		quiet)) {
			return testError(quiet);
	}
	
	if(doEncryptTest(cspHand,
		ptext,
		&myDerive,
		&theirDerive,
		ENCR_ALG,
		ENCR_MODE,
		ENCR_PADDING,
		quiet)) {
			return 1;
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
	CSSM_DATA				ptext;
	CSSM_BOOL				pubIsRef;
	CSSM_BOOL				privIsRef;
	CSSM_BOOL				sym1IsRef;
	CSSM_BOOL				sym2IsRef;
	CSSM_KEYBLOB_FORMAT		pubForm;		// NONE, PKCS3, X509
	CSSM_KEYBLOB_FORMAT		expectPubForm;	// PKCS3, X509
	CSSM_KEYBLOB_FORMAT		privForm;		// NONE, PKCS3, PKCS8
	CSSM_KEYBLOB_FORMAT		expectPrivForm;	// PKCS3, PKCS8
	
	/* user-spec'd parameters */
	unsigned				keySize = KEY_SIZE_DEF;
	unsigned				pauseInterval = 0;
	unsigned				loops = LOOPS_DEF;
	CSSM_BOOL				quiet = CSSM_FALSE;
	CSSM_BOOL				verbose = CSSM_FALSE;
	CSSM_BOOL				bareCsp = CSSM_TRUE;
	char					*inFileName = NULL;
	char					*outFileName = NULL;
	
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
	
	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	
	if(!bareCsp && 
	   !CSPDL_DSA_GEN_PARAMS &&
	   (inFileName == NULL)) {
		/*
		 * For now, CSPDL can not do gen parameters. This only
		 * works if we're supplied params externally (which most
		 * likely were generated from the bare CSP).
		 */
		printf("*** %s can't run with CSPDL unless you supply DH "
			"parameters.\n", argv[0]);
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
	
	ptext.Data = (uint8 *)appMalloc(MAX_PTEXT_SIZE, NULL);
	/* ptext length set in test loop */

	printf("Starting %s; args: ", argv[0]);
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	for(loop=1; ; loop++) {
		if(!quiet) {
			printf("...Loop %d\n", loop);
		}
		simpleGenData(&ptext, 10, MAX_PTEXT_SIZE);
		
		/* mix up raw and ref keys, except for CSPDL which 
		 * requires all ref keys */
		if(bareCsp) {
			pubIsRef  = (loop & 1) ? CSSM_TRUE : CSSM_FALSE;
			privIsRef = (loop & 2) ? CSSM_TRUE : CSSM_FALSE;
			sym1IsRef = (loop & 4) ? CSSM_TRUE : CSSM_FALSE;
			sym2IsRef = (loop & 8) ? CSSM_TRUE : CSSM_FALSE;
		}
		else {
			pubIsRef = privIsRef = sym1IsRef = sym2IsRef = CSSM_TRUE;
		}
		if(!privIsRef) {
			int die = genRand(1,3);
			switch(die) {
				case 1:
					privForm = CSSM_KEYBLOB_RAW_FORMAT_NONE;
					expectPrivForm = CSSM_KEYBLOB_RAW_FORMAT_PKCS3;
					break;
				case 2:
					privForm = expectPrivForm = 
						CSSM_KEYBLOB_RAW_FORMAT_PKCS3;
					break;
				case 3:
					privForm = expectPrivForm = 
						CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
					break;
			}
			if(verbose) {
				printf("...privIsRef false; form %s, expectForm %s\n",
					blobFormStr(privForm), blobFormStr(expectPrivForm));
			}
		}
		else {
			privForm = expectPrivForm = CSSM_KEYBLOB_RAW_FORMAT_NONE;
			if(verbose) {
				printf("...privIsRef true\n");
			}
		}
		if(!pubIsRef) {
			int die = genRand(1,3);
			switch(die) {
				case 1:
					pubForm = CSSM_KEYBLOB_RAW_FORMAT_NONE;
					expectPubForm = CSSM_KEYBLOB_RAW_FORMAT_PKCS3;
					break;
				case 2:
					pubForm = expectPubForm = 
						CSSM_KEYBLOB_RAW_FORMAT_PKCS3;
					break;
				case 3:
					pubForm = expectPubForm = 
						CSSM_KEYBLOB_RAW_FORMAT_X509;
					break;
			}
			if(verbose) {
				printf("...pubIsRef false; form %s, expectForm %s\n",
					blobFormStr(pubForm), blobFormStr(expectPubForm));
			}
		}
		else {
			pubForm = expectPubForm = CSSM_KEYBLOB_RAW_FORMAT_NONE;
			if(verbose) {
				printf("...pubIsRef true\n");
			}
		}
		i = doTest(cspHand, 
			&ptext,
			pubIsRef,
			pubForm,
			expectPubForm,
			privIsRef,
			privForm,
			expectPrivForm,
			sym1IsRef,
			sym2IsRef,
			inParamPtr, 
			outParamPtr, 
			keySize, 
			quiet);
		if(i) {
			break;
		}
		if(loop == 1) {
			/* first time thru */
			if(outFileName) {
				/* save parameters for another run */
				i = writeFile(outFileName, outParams.Data, outParams.Length);
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
	appFree(ptext.Data, NULL);
	CSSM_ModuleDetach(cspHand);
	if(!quiet) {
		printf("OK\n");
	}
	return 0;
}
