/* Copyright (c) 1998,2003-2005,2008 Apple Inc.
 *
 * wrapTest.c -  wrap/unwrap exerciser.
 *
 * Revision History
 * ----------------
 *   4 May 2000  Doug Mitchell
 *		Ported to X/CDSA2. 
 *  6 Aug 1998 Doug Mitchell at Apple
 *		Created.
 */
 
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"
#include "cspdlTesting.h"

/*
 * Currently the CSP can use wrapping keys flagged exclusively for wrapping
 * (CSSM_KEYUSE_{WRAP,UNWRAP} for the actual wrap sinceÊthe wrp/unwrap op is 
 * done with an encrypt/decrypt op. The WrapKey op doesn't even see the 
 * wrapping key - it's in the context we pass it. Thus for now wrap/unwrap
 * keys have to be marked with CSSM_KEYUSE_ANY.
 */
#define WRAP_USAGE_ANY			0

/*
 * When false, the CMS wrap algorithm can't deal with RSA encryption - we 
 * have to encrypt something twice with the same key. An impossibility with 
 * BSAFE-based RSA encryption because the output of the first encrypt is 
 * the size of the key modulus, and you can't encrypt something that big 
 * with that key. 
 * This is not a limitation with openssl-based RSA. 
 */
#define WRAP_WITH_RSA			1

/* 
 * When false, can't wrap with RC4 because the RC4 context is stateful 
 * but doesn't get reinit'd for the second CMS encrypt.
 */
#define WRAP_WITH_RC4			1

/*
 * Temporary hack to use CSSM_KEYBLOB_WRAPPED_FORMAT_{PKCS7,PKCS8}, which
 * are no longer supported as of 7/28/00
 */
#define PKCS7_FORMAT_ENABLE		1		// for wrapping symmetric keys
#define PKCS8_FORMAT_ENABLE		1		// for wrapping private keys


#define ENCR_LABEL		"encrKey"
#define ENCR_LABEL_LEN	(strlen(ENCR_LABEL))
#define WRAP_LABEL		"wrapKey"
#define WRAP_LABEL_LEN	(strlen(WRAP_LABEL))
#define LOOPS_DEF		10
#define MAX_PTEXT_SIZE	100
#define LOOP_PAUSE		100
#define MAX_DESC_DATA_SIZE		16

/*
 * Enumerate algorithms our way to allow loop interations.
 */
typedef unsigned PrivAlg;
enum {
	ALG_DES	= 1,
	ALG_3DES,
	ALG_RC2,
	ALG_RC4,
	ALG_RSA,
	ALG_NULL,
	ALG_FEEDEXP,
	ALG_ASC,
	ALG_AES
};

#define ALG_MIN			ALG_DES
#define ALG_MAX_WRAP	ALG_AES
#define ALG_MAX_ENCR	ALG_AES

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   w=wrapAlg (d=DES, 3=3DES, f=FEEDEXP, r=RSA, A=ASC, 4=RC4, "
			"a=AES, n=null)\n");
	printf("   e=encrAlg (d=DES, 3=3DES, f=FEEDEXP, r=RSA, A=ASC, 4=RC4, "
			"a=AES)\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   r (ref keys only)\n");
	printf("   p(ause every loop)\n");
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   v(erbose)\n");
	printf("   k (quick; small keys)\n");
	printf("   h(elp)\n");
	exit(1);
}

/* wrapped format to string */
static const char *formatString(CSSM_KEYBLOB_FORMAT format)
{
	static char noform[100];
	
	switch(format) {
		case CSSM_KEYBLOB_WRAPPED_FORMAT_NONE:
			return "NONE (default)";
		case CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7:
			return "PKCS7";
		case CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8:
			return "PKCS8";
		case CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM:
			return "APPLE_CUSTOM";
		default:
			sprintf(noform, "***UNKNOWN (%u)***", (unsigned)format);
			return noform;
	}
}

static int vfyWrapHeader(
	const CSSM_KEYHEADER *srcHdr,
	const CSSM_KEYHEADER *dstHdr,
	CSSM_KEYBLOB_TYPE expectBlob,
	const char *op,
	CSSM_BOOL bareCsp,
	int quiet)
{
	if(dstHdr->BlobType != expectBlob) {
		printf("***%s.BlobType error: expect %u  got %u\n",
			op, (unsigned)expectBlob, (unsigned)dstHdr->BlobType);
		if(testError(quiet)) {
			return 1;
		}
	}
	if(srcHdr->KeyClass != dstHdr->KeyClass) {
		printf("***%s.KeyClass error: expect %u  got %u\n",
			op, (unsigned)srcHdr->KeyClass, (unsigned)dstHdr->KeyClass);
		if(testError(quiet)) {
			return 1;
		}
	}
	if(srcHdr->AlgorithmId != dstHdr->AlgorithmId) {
		printf("***%s.AlgorithmId error: expect %u  got %u\n",
			op, (unsigned)srcHdr->AlgorithmId, (unsigned)dstHdr->AlgorithmId);
		if(testError(quiet)) {
			return 1;
		}
	}
	if(srcHdr->KeyUsage != dstHdr->KeyUsage) {
		printf("***%s.KeyUsage error: expect 0x%x  got 0x%x\n",
			op, (unsigned)srcHdr->KeyUsage, (unsigned)dstHdr->KeyUsage);
		if(testError(quiet)) {
			return 1;
		}
	}
	if(bareCsp) {
		/* GUIDs must match */
		if(memcmp(&srcHdr->CspId, &dstHdr->CspId, sizeof(CSSM_GUID))) {
			printf("***%s.CspId mismatch\n", op);
			if(testError(quiet)) {
				return 1;
			}
		}
	}
	else {
		/* CSPDL - GUIDs do NOT match - ref keys are in the CSPDL's domain;
		 * wrapped keys are in the bare CSP's domain. */
		if(!memcmp(&srcHdr->CspId, &dstHdr->CspId, sizeof(CSSM_GUID))) {
			printf("***Unexpected %s.CspId compare\n", op);
			if(testError(quiet)) {
				return 1;
			}
		}
	}
	return 0;
}

#define UNWRAPPED_LABEL	"unwrapped thing"
#define SHOW_WRAP_FORMAT	0

/* not all algs need this */
CSSM_DATA initVector = {16, (uint8 *)"SomeReallyStrangeInitVect"};

static int doTest(CSSM_CSP_HANDLE cspHand,
	CSSM_KEY_PTR encrKey,
	CSSM_BOOL	 wrapEncrKey,	// wrap encrKey before using
	CSSM_KEY_PTR decrKey,		// we wrap this one
	CSSM_KEY_PTR wrappingKey,	// ...using this key
	CSSM_KEY_PTR unwrappingKey,
	CSSM_ALGORITHMS wrapAlg,
	CSSM_ENCRYPT_MODE wrapMode,
	CSSM_KEYBLOB_FORMAT	wrapFormat,		// NONE, PKCS7, PKCS8, APPLE_CUSTOM
	CSSM_KEYBLOB_FORMAT	expectFormat,	// PKCS7, PKCS8, APPLE_CUSTOM
	CSSM_PADDING wrapPad,
	uint32 wrapIvSize,
	CSSM_ALGORITHMS encrAlg,
	CSSM_ENCRYPT_MODE encrMode,
	CSSM_PADDING encrPad,
	uint32 encrIvSize,
	uint32 effectiveKeySizeInBits,	// for encr/decr - 0 means none specified
	CSSM_DATA_PTR ptext,
	CSSM_DATA_PTR descData,
	CSSM_BOOL quiet,
	CSSM_BOOL bareCsp)
{
	CSSM_DATA		ctext;
	CSSM_DATA		rptext;
	CSSM_KEY		wrappedDecrKey;
	CSSM_KEY		unwrappedDecrKey;
	CSSM_KEY		wrappedEncrKey;
	CSSM_RETURN		crtn;
	CSSM_KEY_PTR	actualEncrKey;
	uint32			maxPtextSize = MAX_PTEXT_SIZE;
	CSSM_DATA		outDescData1 = {0, NULL};	// for encr key
	CSSM_DATA		outDescData2 = {0, NULL};	// for decr key, must match descData
	CSSM_DATA		nullInitVect = {0, NULL};	// for custom unwrap 
	CSSM_DATA_PTR	wrapIvp;
	CSSM_DATA_PTR	encrIvp;
	
	/* Hack to deal with RSA's max encrypt size */
	#if 0
	/* no more */
	if(encrAlg == CSSM_ALGID_RSA) {
		uint32 keySizeBytes = encrKey->KeyHeader.LogicalKeySizeInBits / 8;
		maxPtextSize = keySizeBytes - 11;
		if(maxPtextSize > MAX_PTEXT_SIZE) {
			maxPtextSize = MAX_PTEXT_SIZE;
		}
	}
	else {
		maxPtextSize = MAX_PTEXT_SIZE;
	}
	#endif
	simpleGenData(ptext, 1, maxPtextSize);
	
	/* 
	 * Optionaly wrap/unwrap encrKey. If encrKey is a ref key, do a 
	 * NULL wrap. If encrKey is a raw key, do a NULL unwrap.
	 */
	if(wrapEncrKey) {
		CSSM_KEYBLOB_TYPE expectBlob;
		
		if(encrKey->KeyHeader.BlobType == CSSM_KEYBLOB_REFERENCE) {
			crtn = cspWrapKey(cspHand,
				encrKey,
				NULL,				// wrappingKey
				CSSM_ALGID_NONE,
				CSSM_ALGMODE_NONE,
				wrapFormat,
				CSSM_PADDING_NONE,
				NULL,				// iv
				descData,
				&wrappedEncrKey);
			expectBlob = CSSM_KEYBLOB_RAW;
		}
		else {
			crtn = cspUnwrapKey(cspHand,
				encrKey,
				NULL,				// unwrappingKey
				CSSM_ALGID_NONE,
				CSSM_ALGMODE_NONE,
				CSSM_PADDING_NONE,
				NULL,				// iv
				&wrappedEncrKey,
				&outDescData1,
				WRAP_LABEL,
				WRAP_LABEL_LEN);
			expectBlob = CSSM_KEYBLOB_REFERENCE;
		}
		if(crtn) {
			return testError(quiet);
		}
		if(vfyWrapHeader(&encrKey->KeyHeader,
			&wrappedEncrKey.KeyHeader,
			expectBlob,
			"wrappedEncrKey",
			bareCsp,
			quiet)) {
				return 1;
		}
		actualEncrKey = &wrappedEncrKey;
	}
	else {
		actualEncrKey = encrKey;
	}
	/* encrypt using actualEncrKey ==> ctext */
	ctext.Data = NULL;
	ctext.Length = 0;
	if(encrIvSize) {
		initVector.Length = encrIvSize;
		encrIvp = &initVector;
	}
	else {
		encrIvp = NULL;
	}
	crtn = cspEncrypt(cspHand,
		encrAlg,
		encrMode,
		encrPad,
		actualEncrKey,
		NULL,		// no 2nd key
		effectiveKeySizeInBits,
		0,			// rounds
		encrIvp,
		ptext,
		&ctext,
		CSSM_TRUE);	// mallocCtext
	if(crtn) {
		return testError(quiet);
	}
	/* wrap decrKey using wrappingKey ==> wrappedDecrKey */
	/* Note that APPLE_CUSTOM wrap alg REQUIRES an 8-byte IV */
	if(expectFormat == CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM) {
		initVector.Length = 8;
	}
	else {
		initVector.Length = wrapIvSize;
	}
	crtn = cspWrapKey(cspHand,
		decrKey,
		wrappingKey,
		wrapAlg,
		wrapMode,
		wrapFormat,
		wrapPad,
		&initVector,
		descData,
		&wrappedDecrKey);
	if(crtn) {
		return testError(quiet);
	}
	if(wrapAlg != CSSM_ALGID_NONE) {
		if(wrappedDecrKey.KeyHeader.Format != expectFormat) {
			printf("***Wrap format mismatch expect %s got %s\n",
				formatString(wrappedDecrKey.KeyHeader.Format),
				formatString(expectFormat)); 
			if(testError(quiet)) {
				return 1;
			}
		}
	}
	
	if(vfyWrapHeader(&decrKey->KeyHeader,
		&wrappedDecrKey.KeyHeader,
		(wrapAlg == CSSM_ALGID_NONE) ? CSSM_KEYBLOB_RAW : CSSM_KEYBLOB_WRAPPED,
		"wrappedDecrKey",
		bareCsp,
		quiet)) {
			return 1;
	}
	
	if(wrappedDecrKey.KeyHeader.Format == CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM) {
		/* special case - no IV needed - test it */
		wrapIvp = &nullInitVect;
	}
	else {
		wrapIvp = &initVector;
		initVector.Length = wrapIvSize;
	}
	
	/* unwrap wrappedDecrKey using unwrappingKey ==> unwrappedDecrKey; */
	crtn = cspUnwrapKey(cspHand,
		&wrappedDecrKey,
		unwrappingKey,
		wrapAlg,
		wrapMode,
		wrapPad,
		wrapIvp,
		&unwrappedDecrKey,
		&outDescData2,
		"unwrapped thing",
		15);
	if(crtn) {
		return testError(quiet);
	}
	
	if(vfyWrapHeader(&wrappedDecrKey.KeyHeader,
		&unwrappedDecrKey.KeyHeader,
		CSSM_KEYBLOB_REFERENCE,
		"unwrappedDecrKey",
		bareCsp,
		quiet)) {
			return 1;
	}

	/* compare descData to outDescData2 */
	if(descData) {
		if(descData->Length != outDescData2.Length) {
			printf("descData length mismatch\n");
			if(testError(quiet)) {
				return 1;
			}
		}
		if(memcmp(descData->Data, outDescData2.Data, outDescData2.Length)) {
			printf("***descDatadata miscompare\n");
			if(testError(quiet)) {
				return 1;
			}
		}
	}

	/* decrypt ctext with unwrappedDecrKey ==> rptext; */
	rptext.Data = NULL;
	rptext.Length = 0;
	if(encrIvSize) {
		initVector.Length = encrIvSize;
	}
	crtn = cspDecrypt(cspHand,
		encrAlg,
		encrMode,
		encrPad,
		&unwrappedDecrKey,
		NULL,			// no 2nd key
		effectiveKeySizeInBits,
		0,			// rounds
		&initVector,
		&ctext,
		&rptext,
		CSSM_TRUE);
	if(crtn) {
		return testError(quiet);
	}
	/* compare ptext vs. rptext; */
	if(ptext->Length != rptext.Length) {
		printf("ptext length mismatch\n");
		return testError(quiet);
	}
	if(memcmp(ptext->Data, rptext.Data, ptext->Length)) {
		printf("***data miscompare\n");
		return testError(quiet);
	}
	/* free resources */
	cspFreeKey(cspHand, &wrappedDecrKey);
	cspFreeKey(cspHand, &unwrappedDecrKey);
	if(wrapEncrKey) {
		cspFreeKey(cspHand, actualEncrKey);
	}
	CSSM_FREE(ctext.Data);
	CSSM_FREE(rptext.Data);
	if(outDescData2.Data != NULL) {
		CSSM_FREE(outDescData2.Data);
	}
	if(outDescData1.Data != NULL) {
		CSSM_FREE(outDescData1.Data);
	}
	return 0;
}

/*
 * values associated with a private algorithm (e.g., ALG_DES).
 */
typedef enum {
	WT_Symmetric,
	WT_Asymmetric,
	WT_Null
} wrapType;

typedef struct {
	uint32 				keyGenAlg;
	wrapType  			wtype;
	CSSM_ALGORITHMS		encrAlg;
	CSSM_ENCRYPT_MODE	encrMode;
	CSSM_PADDING		encrPad;
	uint32				ivSize;		// in bytes; 0 means no IV
	const char			*algName;
} AlgInfo;

/*
 * Convert our private alg to CDSA keygen alg, encr alg, encr mode, pad
 */
static void getAlgInfo(PrivAlg privAlg, 	// e.g., ALG_DES
	AlgInfo *algInfo)
{
	switch(privAlg) {
		case ALG_DES:
			algInfo->keyGenAlg = CSSM_ALGID_DES;
			algInfo->wtype     = WT_Symmetric;
			algInfo->encrAlg   = CSSM_ALGID_DES;
			algInfo->encrMode  = CSSM_ALGMODE_CBCPadIV8;
			algInfo->encrPad   = CSSM_PADDING_PKCS5;
			algInfo->ivSize	   = 8;
			algInfo->algName   = "DES";
			break;
		case ALG_3DES:
			algInfo->keyGenAlg = CSSM_ALGID_3DES_3KEY;
			algInfo->wtype     = WT_Symmetric;
			algInfo->encrAlg   = CSSM_ALGID_3DES_3KEY_EDE;
			algInfo->encrMode  = CSSM_ALGMODE_CBCPadIV8;
			algInfo->encrPad   = CSSM_PADDING_PKCS5;
			algInfo->ivSize	   = 8;
			algInfo->algName   = "3DES";
			break;
		case ALG_FEEDEXP:
			algInfo->keyGenAlg = CSSM_ALGID_FEE;
			algInfo->wtype     = WT_Asymmetric;
			algInfo->encrAlg   = CSSM_ALGID_FEEDEXP;
			algInfo->encrMode  = CSSM_ALGMODE_NONE;
			algInfo->encrPad   = CSSM_PADDING_NONE;
			algInfo->ivSize	   = 0;
			algInfo->algName   = "FEEDEXP";
			break;
		case ALG_RSA:
			algInfo->keyGenAlg = CSSM_ALGID_RSA;
			algInfo->wtype     = WT_Asymmetric;
			algInfo->encrAlg   = CSSM_ALGID_RSA;
			algInfo->encrMode  = CSSM_ALGMODE_NONE;
			algInfo->encrPad   = CSSM_PADDING_PKCS1;
			algInfo->ivSize	   = 0;
			algInfo->algName   = "RSA";
			break;
		case ALG_ASC:
			algInfo->keyGenAlg = CSSM_ALGID_ASC;
			algInfo->wtype     = WT_Symmetric;
			algInfo->encrAlg   = CSSM_ALGID_ASC;
			algInfo->encrMode  = CSSM_ALGMODE_NONE;
			algInfo->encrPad   = CSSM_PADDING_NONE;
			algInfo->ivSize	   = 0;
			algInfo->algName   = "ASC";
			break;
		case ALG_RC2:
			algInfo->keyGenAlg = CSSM_ALGID_RC2;
			algInfo->wtype     = WT_Symmetric;
			algInfo->encrAlg   = CSSM_ALGID_RC2;
			algInfo->encrMode  = CSSM_ALGMODE_CBCPadIV8;
			algInfo->encrPad   = CSSM_PADDING_PKCS5;
			algInfo->ivSize	   = 8;
			algInfo->algName   = "RC2";
			break;
		case ALG_RC4:
			algInfo->keyGenAlg = CSSM_ALGID_RC4;
			algInfo->wtype     = WT_Symmetric;
			algInfo->encrAlg   = CSSM_ALGID_RC4;
			algInfo->encrMode  = CSSM_ALGMODE_CBCPadIV8;
			algInfo->encrPad   = CSSM_PADDING_PKCS5;
			algInfo->ivSize	   = 0;
			algInfo->algName   = "RC4";
			break;
		case ALG_NULL:
			algInfo->keyGenAlg = CSSM_ALGID_NONE;
			algInfo->wtype     = WT_Null;
			algInfo->encrAlg   = CSSM_ALGID_NONE;
			algInfo->encrMode  = CSSM_ALGMODE_NONE;
			algInfo->encrPad   = CSSM_PADDING_NONE;
			algInfo->ivSize	   = 0;
			algInfo->algName   = "Null";
			break;
		case ALG_AES:
			algInfo->keyGenAlg = CSSM_ALGID_AES;
			algInfo->wtype     = WT_Symmetric;
			algInfo->encrAlg   = CSSM_ALGID_AES;
			algInfo->encrMode  = CSSM_ALGMODE_CBCPadIV8;
			algInfo->encrPad   = CSSM_PADDING_PKCS7;
			algInfo->ivSize	   = 16;
			algInfo->algName   = "AES";
			break;
		default:
			printf("Bogus privAlg\n");
			exit(1);
	}
	return;
}

/* argv letter to private alg */
static PrivAlg letterToAlg(char **argv, char letter)
{
	switch(letter) {
		case 'd': return ALG_DES;
		case '3': return ALG_3DES;
		case 'f': return ALG_FEEDEXP;
		case 'r': return ALG_RSA;
		case 'A': return ALG_ASC;
		case '4': return ALG_RC4;
		case 'a': return ALG_AES;
		default:
			usage(argv);
			return 0;
	}
}

/*
 * Null wrapping of symmetric keys now allowed
 */
#define SYMM_NULL_WRAP_ENABLE	1

/* indices into algInfo[] */
#define AI_WRAP		0
#define AI_ENCR		1

int main(int argc, char **argv)
{
	int						arg;
	char					*argp;
	unsigned				loop;
	CSSM_CSP_HANDLE 		cspHand;
	CSSM_RETURN				crtn;
	CSSM_DATA				ptext;
	uint32					encrKeySizeBits;			// well aligned
	uint32					wrapKeySizeBits;
	uint32 					effectiveKeySizeInBits;		// for encr, may be odd
	int						rtn = 0;
	uint32					maxRsaKeySize  = 1024;
	uint32					maxFeeKeySize  = 192;
	CSSM_KEYBLOB_FORMAT		wrapFormat;		// NONE, PKCS7, PKCS8, APPLE_CUSTOM
	CSSM_KEYBLOB_FORMAT		expectFormat;	// PKCS7, PKCS8, APPLE_CUSTOM
	CSSM_DATA				descData = {0, NULL};
	CSSM_DATA_PTR			descDataP;
	
	/*
	 * key pointers passed to doTest() - for symmetric algs, the pairs
	 * might point to the same key
	 */
	CSSM_KEY_PTR			encrKeyPtr;
	CSSM_KEY_PTR			decrKeyPtr;
	CSSM_KEY_PTR			wrapKeyPtr;
	CSSM_KEY_PTR			unwrapKeyPtr;
	
	/* persistent asymmetric keys - symm keys are dynamically allocated */
	CSSM_KEY				pubEncrKey;
	CSSM_KEY				privEncrKey;
	CSSM_KEY				pubWrapKey;
	CSSM_KEY				privWrapKey;
	
	/* we iterate these values thru all possible algs */
	PrivAlg					privEncrAlg;	// ALG_xxx
	PrivAlg					privWrapAlg;
	
	/* two AlgInfo which contain everything we need to know per alg */
	AlgInfo					algInfo[2];
	AlgInfo					*encrInfo;
	AlgInfo					*wrapInfo;
	CSSM_BOOL				wrapEncrKey = CSSM_FALSE;	// varies loop-to-loop
	CSSM_BOOL				encrKeyIsRef = CSSM_TRUE;	// ditto
	
	CSSM_BOOL				genSeed;					// for FEE key gen
	int						i;
	
	/* user-specified vars */
	unsigned				loops = LOOPS_DEF;
	CSSM_BOOL				pause = CSSM_FALSE;
	CSSM_BOOL				verbose = CSSM_FALSE;
	PrivAlg					minWrapAlg = ALG_MIN;
	PrivAlg					maxWrapAlg = ALG_MAX_WRAP;
	PrivAlg					minEncrAlg = ALG_MIN;
	PrivAlg					maxEncrAlg = ALG_MAX_ENCR;
	CSSM_BOOL				quick = CSSM_FALSE;
	CSSM_BOOL				quiet = CSSM_FALSE;
	CSSM_BOOL				bareCsp = CSSM_TRUE;
	CSSM_BOOL				refKeysOnly = CSSM_FALSE;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'w':
				if(argp[2] == 'n') {
					minWrapAlg = maxWrapAlg = ALG_NULL;
				}
				else {
					minWrapAlg = maxWrapAlg = letterToAlg(argv, argp[2]);
				}
				break;
			case 'e':
				minEncrAlg = maxEncrAlg = letterToAlg(argv, argp[2]);
				break;
		    case 'l':
				loops = atoi(&argp[2]);
				break;
			case 'p':
				pause = CSSM_TRUE;
				break;
			case 'v':
				verbose = CSSM_TRUE;
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				#if CSPDL_ALL_KEYS_ARE_REF
		    	refKeysOnly = CSSM_TRUE;
				#endif
				break;
			case 'r':
				refKeysOnly = CSSM_TRUE;
				break;
			case 'q':
				quiet = CSSM_TRUE;
				break;
			case 'k':
				quick = CSSM_TRUE;
				maxRsaKeySize = 512;
				maxFeeKeySize = 127;
				break;
			default:
				usage(argv);
		}
	}
	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	wrapInfo = &algInfo[AI_WRAP];
	encrInfo = &algInfo[AI_ENCR];
	
	/* cook up ptext, descData */
	ptext.Data = (uint8 *)CSSM_MALLOC(MAX_PTEXT_SIZE); 
	descData.Data = (uint8 *)CSSM_MALLOC(MAX_DESC_DATA_SIZE);
	
	printf("Starting wrapTest; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	
	for(loop=0; loop<loops; loop++) {
		if(!quiet) {
			printf("...loop %d\n", loop);
		}
		if(pause) {
			fpurge(stdin);
			printf("Hit CR to proceed: ");
			getchar();
		}
		
		/* iterate thru all encryption algs */
		for(privEncrAlg=minEncrAlg; privEncrAlg<=maxEncrAlg; privEncrAlg++) {
			/* handle disabled algs */
			switch(privEncrAlg) {
				case ALG_NULL:		/* just skip this one, it's just for wrap */
					continue;
				default:
					break;
			}
		
			/* generate key(s) to be wrapped */
			getAlgInfo(privEncrAlg, encrInfo);
			effectiveKeySizeInBits = randKeySizeBits(encrInfo->keyGenAlg, OT_Encrypt);
			if(!refKeysOnly) {
				encrKeyIsRef = (loop & 2) ? CSSM_TRUE : CSSM_FALSE;
			}
			
			switch(encrInfo->wtype) {
				case WT_Symmetric:
					/* round up to even byte */
					encrKeySizeBits = (effectiveKeySizeInBits + 7) & ~7;
					if(encrKeySizeBits == effectiveKeySizeInBits) {
						effectiveKeySizeInBits = 0;
					}
					encrKeyPtr = decrKeyPtr = cspGenSymKey(cspHand,
						encrInfo->keyGenAlg,
						ENCR_LABEL,
						ENCR_LABEL_LEN,
						CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
						encrKeySizeBits,
						encrKeyIsRef);
					if(encrKeyPtr == NULL) {
						rtn = 1;
						goto testDone;
					}
					#if		SYMM_NULL_WRAP_ENABLE
					/* wrapEncrKey every other loop */
					if(!refKeysOnly) {
						wrapEncrKey = (loop & 1) ? CSSM_TRUE : CSSM_FALSE;
					}
					#else
					wrapEncrKey = CSSM_FALSE;
					#endif	/* SYMM_NULL_WRAP_ENABLE */
					break;
				case WT_Asymmetric:
					/* handle alg-specific cases */
					genSeed = CSSM_FALSE;
					switch(privEncrAlg) {
						case ALG_RSA:
							if(effectiveKeySizeInBits > maxRsaKeySize) {
								effectiveKeySizeInBits = maxRsaKeySize;
							}
							break;
						case ALG_FEEDEXP:
							if(effectiveKeySizeInBits > maxFeeKeySize) {
								effectiveKeySizeInBits = maxFeeKeySize;
							}
							if(loop & 4) {
								genSeed = CSSM_TRUE;
							}
							break;
						default:
							break;
					}
					encrKeySizeBits = effectiveKeySizeInBits;
					effectiveKeySizeInBits = 0;		// i.e., not specified
					crtn = cspGenKeyPair(cspHand,
						encrInfo->keyGenAlg,
						ENCR_LABEL,
						ENCR_LABEL_LEN,
						encrKeySizeBits,
						&pubEncrKey,
						encrKeyIsRef,		// pubIsRef
						CSSM_KEYUSE_ENCRYPT,
						CSSM_KEYBLOB_RAW_FORMAT_NONE,
						&privEncrKey,
						CSSM_TRUE,			// privIsRef
						CSSM_KEYUSE_DECRYPT,
						CSSM_KEYBLOB_RAW_FORMAT_NONE,
						genSeed);
					if(crtn) {
						rtn = testError(quiet);
						goto testDone;
					}
					encrKeyPtr = &pubEncrKey;
					decrKeyPtr = &privEncrKey;
					/* wrapEncrKey every other loop */
					if(!refKeysOnly) {					
						wrapEncrKey = (loop & 1) ? CSSM_TRUE : CSSM_FALSE;
					}
					break;
				case WT_Null:
					printf("***BRRZAP: can't do null encrypt\n");
					goto testDone;
			}
			if(verbose) {
				printf("   ...encrAlg %s  wrapEncrKey %d encrKeyIsRef %d  size %u "
					"bits  effectSize %u\n",
					encrInfo->algName, (int)wrapEncrKey, (int)encrKeyIsRef, 
					(unsigned)encrKeySizeBits, (unsigned)effectiveKeySizeInBits);
			}
			/* iterate thru all wrap algs */
			for(privWrapAlg=minWrapAlg; privWrapAlg<=maxWrapAlg; privWrapAlg++) {
				/* handle disabled algs */
				if((privWrapAlg == ALG_AES) && (privEncrAlg == ALG_FEEDEXP)) {
					/*
					 * Can't do it. FEED can't do PKCS8 because it doesn't
					 * support PKCS8 private key format, and AES can't 
					 * do APPLE_CUSTOM because AES needs a 16-byte IV.
					 */
					continue;
				}
				/* any other restrictions/ */
				
				/* generate wrapping key(s) */
				getAlgInfo(privWrapAlg, wrapInfo);
				switch(wrapInfo->wtype) {
					case WT_Symmetric:
					/* note we can't do odd-size wrapping keys */
						wrapKeySizeBits = randKeySizeBits(wrapInfo->keyGenAlg, 
							OT_KeyExch);
						wrapKeySizeBits &= ~7;
						wrapKeyPtr = unwrapKeyPtr = cspGenSymKey(cspHand,
							wrapInfo->keyGenAlg,
							WRAP_LABEL,
							WRAP_LABEL_LEN,
							WRAP_USAGE_ANY ? CSSM_KEYUSE_ANY : 
								CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP,
							wrapKeySizeBits,
							CSSM_TRUE);
						if(wrapKeyPtr == NULL) {
							rtn = 1;
							goto testDone;
						}
						break;
					case WT_Asymmetric:
						wrapKeySizeBits = randKeySizeBits(wrapInfo->keyGenAlg, 
							OT_KeyExch);
						genSeed = CSSM_FALSE;
						switch(privWrapAlg) {
							case ALG_RSA:
								if(wrapKeySizeBits > maxRsaKeySize) {
									wrapKeySizeBits = maxRsaKeySize;
								}
								break;
							case ALG_FEEDEXP:
								if(wrapKeySizeBits > maxFeeKeySize) {
									wrapKeySizeBits = maxFeeKeySize;
								}
								if(loop & 2) {
									genSeed = CSSM_TRUE;
								}
								break;
							default:
								break;
						}
						crtn = cspGenKeyPair(cspHand,
							wrapInfo->keyGenAlg,
							WRAP_LABEL,
							WRAP_LABEL_LEN,
							wrapKeySizeBits,
							&pubWrapKey,
							CSSM_TRUE,			// pubIsRef
							WRAP_USAGE_ANY ? CSSM_KEYUSE_ANY : CSSM_KEYUSE_WRAP,
							CSSM_KEYBLOB_RAW_FORMAT_NONE,
							&privWrapKey,
							CSSM_TRUE,			// privIsRef
							WRAP_USAGE_ANY ? CSSM_KEYUSE_ANY : CSSM_KEYUSE_UNWRAP,
							CSSM_KEYBLOB_RAW_FORMAT_NONE,
							genSeed);
						if(crtn) {
							rtn = testError(quiet);
							goto testDone;
						}
						wrapKeyPtr   = &pubWrapKey;
						unwrapKeyPtr = &privWrapKey;
						break;
					case WT_Null:
						#if		!SYMM_NULL_WRAP_ENABLE
						if(encrInfo->wtype == WT_Symmetric) {
							/* can't do null wrap of symmetric key */
							continue;
						}
						#endif
						wrapKeySizeBits = 0;
						wrapKeyPtr   = NULL;
						unwrapKeyPtr = NULL;
						break;
				}
				
				/* special case for 3DES/3DES */
				#if 0
				if((wrapKeyPtr != NULL) &&
					(wrapKeyPtr->KeyHeader.AlgorithmId == CSSM_ALGID_3DES_3KEY) &&
					(decrKeyPtr->KeyHeader.AlgorithmId == CSSM_ALGID_3DES_3KEY)) {
					isAppleCustom = CSSM_TRUE;
				}
				else {
					isAppleCustom = CSSM_FALSE;
				}
				#endif
				
				/* cook up a wrapFormat - every other loop use default, others
				 * specify a reasonable one */
				if(wrapInfo->wtype == WT_Null) {
					wrapFormat = expectFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_NONE;
				}
				else if((loop & 1)) {
					/*
					 * FORMAT_NONE - default - figure out expected format;
					 * this has to track CSP behavior
					 */
					wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_NONE;
					switch(encrInfo->wtype) {
						case WT_Symmetric:
							expectFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7;
							break;
						case WT_Asymmetric:
							if(privEncrAlg == ALG_FEEDEXP) {
								expectFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM;
							}
							else {
								expectFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8;
							}
							break;
						default:
							/* NULL encr not done */
							printf("**GAK! Internal error\n");
					}
				}
				else {
					/* pick a good explicit one - this encapsulates the 
					 * range of legal wrap formats per wrap/encrypt alg */
					int die = loop & 2;
					switch(encrInfo->wtype) {
						case WT_Symmetric:
							if(privWrapAlg == ALG_AES) {
								/* can't do APPLE_CUSTOM - 16 byte IV */
								wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7;
							}
							else if(die) {
								wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7;
							}
							else {
								wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM;
							}
							break;
						case WT_Asymmetric:
							/* Can't wrap FEE key with AES no way, no how -
							 * this is detected at the top of the privWrapAlg
							 * loop 
							 */ 
							if(privEncrAlg == ALG_FEEDEXP) {
								/* FEE doesn't do PKCS8 private key format */
								wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM;
							}
							else if(die) {
								wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8;
							}
							else if(privWrapAlg == ALG_AES) {
								/* AES can't do APPLE_CUSTOM - 16 byte IV */
								wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8;
							}
							else {
								wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM;
							}
							break;
						default:
							/* NULL encr not done */
							printf("***GAK! Internal error\n");
							exit(1);
					}
					expectFormat = wrapFormat;
				}
				
				/*
				 * If wrapping with apple custom - either by default or
				 * explicitly - generate some descriptive data. 
				 */
				if(expectFormat == CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM) {
					simpleGenData(&descData, 1, MAX_DESC_DATA_SIZE);
					descDataP = &descData;
				}
				else {
					descDataP = NULL;
				}

				if(verbose) {
					printf("      ...wrapAlg = %s size %u bits format %s expect %s\n",
						wrapInfo->algName, (unsigned)wrapKeySizeBits, formatString(wrapFormat),
						formatString(expectFormat));
				}
				/* OK, here we go! */
				if(doTest(cspHand,
						encrKeyPtr,
						wrapEncrKey,
						decrKeyPtr,
						wrapKeyPtr,
						unwrapKeyPtr,
						wrapInfo->encrAlg,
						wrapInfo->encrMode,
						wrapFormat,
						expectFormat,
						wrapInfo->encrPad,
						wrapInfo->ivSize,
						encrInfo->encrAlg,
						encrInfo->encrMode,
						encrInfo->encrPad,
						encrInfo->ivSize,
						effectiveKeySizeInBits,
						&ptext,
						descDataP,
						quiet,
						bareCsp)) {
					rtn = 1;
					goto testDone;
				}
				/* end of wrap alg loop - free/delete wrap key(s) */
				switch(wrapInfo->wtype) {
					case WT_Symmetric:
						cspFreeKey(cspHand, wrapKeyPtr);
						/* mallocd by cspGenSymKey */
						CSSM_FREE(wrapKeyPtr);
						break;
					case WT_Asymmetric:
						cspFreeKey(cspHand, wrapKeyPtr);
						cspFreeKey(cspHand, unwrapKeyPtr);
						break;
					default:
						break;
				}
			}	/* for wrapAlg */
			/* end of encr alg loop - free encr key(s) */
			cspFreeKey(cspHand, encrKeyPtr);
			if(encrInfo->wtype == WT_Symmetric) {
				/* mallocd by cspGenSymKey */
				CSSM_FREE(decrKeyPtr);
			}
			else {
				cspFreeKey(cspHand, decrKeyPtr);
			}
		}
	}
testDone:
	cspShutdown(cspHand, bareCsp);
	if(pause) {
		fpurge(stdin);
		printf("ModuleDetach/Unload complete; hit CR to exit: ");
		getchar();
	}
	if((rtn == 0) && !quiet) {
		printf("%s test complete\n", argv[0]);
	}
	return rtn;
}
