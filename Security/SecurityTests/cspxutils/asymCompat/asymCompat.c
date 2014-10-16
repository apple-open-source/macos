/* 
 * asymCompat.c - test compatibilty of two different implementations of a
 * RSA and DSA - one in the standard AppleCSP, one in BSAFE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include "cspwrap.h"
#include "common.h"
#include "bsafeUtils.h"
#include <string.h>
#include "cspdlTesting.h"

/*
 * Defaults.
 */
#define OLOOPS_DEF		10		/* outer loops, one set of keys per loop */
#define SIG_LOOPS_DEF	100		/* sig loops */
#define ENC_LOOPS_DEF	100		/* encrypt/decrypt loops */
#define MAX_TEXT_SIZE	1025

#define LOOP_NOTIFY		20

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=algorithm (r=RSA; d=DSA; default=both)\n");
	printf("   l=outerloops (default=%d; 0=forever)\n", OLOOPS_DEF);
	printf("   s=sigLoops (default=%d)\n", SIG_LOOPS_DEF);
	printf("   e=encryptLoops (default=%d)\n", ENC_LOOPS_DEF);
	printf("   k=keySizeInBits; default is random\n");
	printf("   S (sign/verify only)\n");
	printf("   E (encrypt/decrypt only)\n");
	printf("   r (generate ref keys)\n");
	printf("   R (generate public ref keys)\n");
	printf("   p=pauseInterval (default=0, no pause)\n");
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

static const char *algToStr(CSSM_ALGORITHMS sigAlg)
{
	switch(sigAlg) {
		case CSSM_ALGID_RSA:			return "RSA";
		case CSSM_ALGID_DSA:			return "DSA";
		case CSSM_ALGID_SHA1WithRSA:	return "SHA1WithRSA";
		case CSSM_ALGID_MD5WithRSA:		return "MD5WithRSA";
		case CSSM_ALGID_SHA1WithDSA:	return "SHA1WithDSA";
		default:
			printf("***Unknown sigAlg\n");
			exit(1);
	}
	/* NOT REACHED */
	return "";
}

/*
 * CDSA private key decrypt with blinding option.
 */
static CSSM_RETURN _cspDecrypt(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEED, etc.
		uint32 mode,						// CSSM_ALGMODE_CBC, etc. - only for symmetric algs
		CSSM_PADDING padding,				// CSSM_PADDING_PKCS1, etc. 
		CSSM_BOOL blinding,
		const CSSM_KEY *key,				// public or session key
		const CSSM_DATA *ctext,
		CSSM_DATA_PTR ptext)				// RETURNED
{
	CSSM_CC_HANDLE 	cryptHand;
	CSSM_RETURN		crtn;
	CSSM_RETURN		ocrtn = CSSM_OK;
	CSSM_SIZE		bytesDecrypted;
	CSSM_DATA		remData = {0, NULL};

	cryptHand = genCryptHandle(cspHand, 
		algorithm, 
		mode, 
		padding,
		key, 
		NULL,		// pubKey, 
		NULL,		// iv,
		0,			// effectiveKeySizeInBits,
		0);			// rounds
	if(cryptHand == 0) {
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	if(blinding) {
		CSSM_CONTEXT_ATTRIBUTE	newAttr;	
		newAttr.AttributeType     = CSSM_ATTRIBUTE_RSA_BLINDING;
		newAttr.AttributeLength   = sizeof(uint32);
		newAttr.Attribute.Uint32  = 1;
		crtn = CSSM_UpdateContextAttributes(cryptHand, 1, &newAttr);
		if(crtn) {
			printError("CSSM_UpdateContextAttributes", crtn);
			return crtn;
		}
	}

	crtn = CSSM_DecryptData(cryptHand,
		ctext,
		1,
		ptext,
		1,
		&bytesDecrypted,
		&remData);
	if(crtn == CSSM_OK) {
		// NOTE: We return the proper length in ptext....
		ptext->Length = bytesDecrypted;
		
		// FIXME - sometimes get mallocd RemData here, but never any valid data
		// there...side effect of CSPFullPluginSession's buffer handling logic;
		// but will we ever actually see valid data in RemData? So far we never
		// have....
		if(remData.Data != NULL) {
			appFree(remData.Data, NULL);
		}
	}
	else {
		printError("CSSM_DecryptData", crtn);
		ocrtn = crtn;
	}
	crtn = CSSM_DeleteContext(cryptHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	return ocrtn;
}

/* sign with RSA blinging option */
static CSSM_RETURN _cspSign(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
		CSSM_KEY_PTR key,					// private key
		const CSSM_DATA *text,
		CSSM_BOOL rsaBlinding,
		CSSM_DATA_PTR sig)					// RETURNED
{
	CSSM_CC_HANDLE	sigHand;
	CSSM_RETURN		crtn;
	CSSM_RETURN		ocrtn = CSSM_OK;
	const CSSM_DATA	*ptext;
	CSSM_DATA		digest = {0, NULL};
	CSSM_ALGORITHMS	digestAlg = CSSM_ALGID_NONE;

	/* handle special cases for raw sign */
	switch(algorithm) {
		case CSSM_ALGID_SHA1:
			digestAlg = CSSM_ALGID_SHA1;
			algorithm = CSSM_ALGID_RSA;
			break;
		case CSSM_ALGID_MD5:
			digestAlg = CSSM_ALGID_MD5;
			algorithm = CSSM_ALGID_RSA;
			break;
		case CSSM_ALGID_DSA:
			digestAlg = CSSM_ALGID_SHA1;
			algorithm = CSSM_ALGID_DSA;
			break;
		default:
			break;
	}
	if(digestAlg != CSSM_ALGID_NONE) {
		crtn = cspDigest(cspHand,
			digestAlg,
			CSSM_FALSE,			// mallocDigest
			text,
			&digest);
		if(crtn) {
			return crtn;
		}	
		/* sign digest with raw RSA/DSA */
		ptext = &digest;
	}
	else {
		ptext = text;
	}
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		algorithm,
		NULL,				// passPhrase
		key,
		&sigHand);
	if(crtn) {
		printError("CSSM_CSP_CreateSignatureContext (1)", crtn);
		return crtn;
	}
	if(rsaBlinding) {
		CSSM_CONTEXT_ATTRIBUTE	newAttr;	
		newAttr.AttributeType     = CSSM_ATTRIBUTE_RSA_BLINDING;
		newAttr.AttributeLength   = sizeof(uint32);
		newAttr.Attribute.Uint32  = 1;
		crtn = CSSM_UpdateContextAttributes(sigHand, 1, &newAttr);
		if(crtn) {
			printError("CSSM_UpdateContextAttributes", crtn);
			return crtn;
		}
	}
	crtn = CSSM_SignData(sigHand,
		ptext,
		1,
		digestAlg,
		sig);
	if(crtn) {
		printError("CSSM_SignData", crtn);
		ocrtn = crtn;
	}
	crtn = CSSM_DeleteContext(sigHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	if(digest.Data != NULL) {
		CSSM_FREE(digest.Data);
	}
	return ocrtn;
}


/*
 * Sign/verify test.
 *
 * for specified numLoops {
 *		generate random text;
 *		sign with BSAFE priv key, verify with CDSA pub key;
 *		sign with CDSA priv key, verify with BSAFE pub key;
 * }
 */
static int sigTest(
	CSSM_CSP_HANDLE		cspHand,
	unsigned			numLoops,
	
	/* one matched key pair */
	BU_KEY				bsafePrivKey,
	CSSM_KEY_PTR		cdsaPubKey,
	
	/* another matched key pair */
	CSSM_KEY_PTR		cdsaPrivKey,
	BU_KEY				bsafePubKey,
	
	CSSM_DATA_PTR		ptext,
	unsigned			maxPtextSize,	
	CSSM_ALGORITHMS		sigAlg,
	CSSM_BOOL			rsaBlinding,
	CSSM_BOOL			quiet,
	CSSM_BOOL			verbose)
{
	CSSM_RETURN crtn;
	CSSM_DATA	sig = {0, NULL};
	unsigned	loop;
	uint32		keySizeInBits = cdsaPrivKey->KeyHeader.LogicalKeySizeInBits;
	
	if(!quiet) {
		printf("   ...sig alg %s  keySize %u\n", algToStr(sigAlg), (unsigned)keySizeInBits);
	}
	for(loop=0; loop<numLoops; loop++) {
		simpleGenData(ptext, 1, maxPtextSize);
		if(!quiet) {
			if(verbose || ((loop % LOOP_NOTIFY) == 0)) {
				printf("      ...loop %d keySize %u textSize %lu\n", 
					loop, (unsigned)cdsaPrivKey->KeyHeader.LogicalKeySizeInBits, 
					(unsigned long)ptext->Length);
			}
		}
		
		/* sign with BSAFE, verify with CDSA */
		crtn = buSign(bsafePrivKey,
			sigAlg,
			ptext,
			keySizeInBits,
			&sig);
		if(crtn) {
			return testError(quiet);
		}
		crtn = cspSigVerify(cspHand,
			sigAlg,
			cdsaPubKey,
			ptext,
			&sig,
			CSSM_OK);
		if(crtn) {
			printf("***ERROR: Sign with BSAFE, vfy with CDSA, alg %s\n",
				algToStr(sigAlg));
			if(testError(quiet)) {
				return 1;
			}
		}
		appFreeCssmData(&sig, CSSM_FALSE);
	
		/* sign with CDSA, verify with BSAFE */
		crtn = _cspSign(cspHand,
			sigAlg,
			cdsaPrivKey,
			ptext,
			rsaBlinding,
			&sig);
		if(crtn) {
			return testError(quiet);
		}
		crtn = buVerify(bsafePubKey,
			sigAlg,
			ptext,
			&sig);
		if(crtn) {
			printf("***ERROR: Sign with CDSA, vfy with BSAFE, alg %s\n",
				algToStr(sigAlg));
			if(testError(quiet)) {
				return 1;
			}
		}
		appFreeCssmData(&sig, CSSM_FALSE);
	}
	return CSSM_OK;
}

/*
 * RSA Encrypt/decrypt test.
 *
 * for specified numLoops {
 *		generate random text;
 *		encrypt with BSAFE pub key, decrypt with CDSA priv key, verify;
 *		encrypt with CDSA pub key, decrypt with BSAFE priv key, verify;
 * }
 */
static int encryptTest(
	CSSM_CSP_HANDLE		cspHand,
	unsigned			numLoops,
	
	/* one matched key pair */
	BU_KEY				bsafePrivKey,
	CSSM_KEY_PTR		cdsaPubKey,
	
	/* another matched key pair */
	CSSM_KEY_PTR		cdsaPrivKey,
	BU_KEY				bsafePubKey,
	
	CSSM_DATA_PTR		ptext,
	unsigned			maxPtextSize,	
	CSSM_BOOL			rsaBlinding,
	CSSM_BOOL			quiet,
	CSSM_BOOL			verbose)
{
	CSSM_RETURN crtn;
	CSSM_DATA	ctext = {0, NULL};
	CSSM_DATA	rptext = {0, NULL};
	unsigned	loop;
	unsigned	actKeySizeBytes;
	
	actKeySizeBytes = cdsaPrivKey->KeyHeader.LogicalKeySizeInBits / 8;
	if(actKeySizeBytes < 12) {
		printf("***Key with %u key bits is too small for RSA encrypt\n",
			(unsigned)cdsaPrivKey->KeyHeader.LogicalKeySizeInBits);
		return 1;
	}
	if(maxPtextSize > (actKeySizeBytes - 11)) {
		maxPtextSize = actKeySizeBytes - 11;
	}
	if(!quiet) {
		printf("   ...encr alg RSA\n");
	}
	for(loop=0; loop<numLoops; loop++) {
		simpleGenData(ptext, 1, maxPtextSize);
		if(!quiet) {
			if(verbose || ((loop % LOOP_NOTIFY) == 0)) {
				printf("      ...loop %d keySize %u textSize %lu\n", 
					loop, (unsigned)cdsaPrivKey->KeyHeader.LogicalKeySizeInBits, 
					(unsigned long)ptext->Length);
			}
		}
		
		/* encrypt with BSAFE, decrypt with CDSA */
		crtn = buEncryptDecrypt(bsafePubKey,
			CSSM_TRUE,		// encrypt
			CSSM_ALGID_RSA,
			CSSM_ALGMODE_NONE,
			NULL,			// iv
			cdsaPrivKey->KeyHeader.LogicalKeySizeInBits,	
			0,				// rounds
			ptext,
			&ctext);
		if(crtn) {
			return testError(quiet);
		}
		crtn = _cspDecrypt(cspHand,
			CSSM_ALGID_RSA,
			CSSM_ALGMODE_NONE,
			CSSM_PADDING_PKCS1,
			rsaBlinding,
			cdsaPrivKey,
			&ctext,
			&rptext);
		if(crtn) {
			printf("***ERROR: encrypt with BSAFE, decrypt with CDSA\n");
			return testError(quiet);
		}
		if(!appCompareCssmData(ptext, &rptext)) {
			printf("***DATA MISCOMPARE: encrypt with BSAFE, decrypt with CDSA\n");
			return testError(quiet);
		}
		appFreeCssmData(&ctext, CSSM_FALSE);
		appFreeCssmData(&rptext, CSSM_FALSE);
	
		/* encrypt with CDSA, decrypt with BSAFE */
		crtn = cspEncrypt(cspHand,
			CSSM_ALGID_RSA,
			CSSM_ALGMODE_NONE,
			CSSM_PADDING_PKCS1,
			cdsaPubKey,
			NULL,					// (FEE) pub key
			0,						// effectiveKeyBits
			0,						// rounds
			NULL,					// IV
			ptext,
			&ctext,
			CSSM_FALSE);			// mallocCtext
		if(crtn) {
			return testError(quiet);
		}
		crtn = buEncryptDecrypt(bsafePrivKey,
			CSSM_FALSE,		// encrypt
			CSSM_ALGID_RSA,
			CSSM_ALGMODE_NONE,
			NULL,			// iv
			cdsaPrivKey->KeyHeader.LogicalKeySizeInBits,	
			0,				// rounds
			&ctext,
			&rptext);
		if(crtn) {
			printf("***ERROR: encrypt with CDSA, decrypt with BSAFE\n");
			return testError(quiet);
		}
		if(!appCompareCssmData(ptext, &rptext)) {
			printf("***DATA MISCOMPARE: encrypt with CDSA, decrypt with BSAFE\n");
			return testError(quiet);
		}
		appFreeCssmData(&ctext, CSSM_FALSE);
		appFreeCssmData(&rptext, CSSM_FALSE);
	}
	return CSSM_OK;
}

static int doTest(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_ALGORITHMS		keyAlg,				// RSA/DSA
	CSSM_ALGORITHMS		sigAlg,
	unsigned			sigLoops,			// may be zero
	unsigned			encrLoops,			// ditto; it will be zero for DSA
	CSSM_BOOL			rsaBlinding,
	CSSM_DATA_PTR		ptext,
	unsigned			maxPtextSize,
	uint32				keySizeInBits,		// 0 --> random per alg
	CSSM_BOOL			pubRefKeys,
	CSSM_BOOL			privRefKeys,
	CSSM_BOOL			bareCsp,			// for other workarounds
	CSSM_BOOL			quiet,
	CSSM_BOOL			verbose)
{
	CSSM_KEY			cdsaGenPubKey;
	CSSM_KEY			cdsaGenPrivKey;		// only used to create bsafeDerivePrivKey
	CSSM_KEY			cdsaTempKey;		// raw key if privRefKeys true
	CSSM_KEY			cdsaDerivePrivKey;	// same as bsafeGenPrivKey
	BU_KEY				bsafeGenPubKey;
	BU_KEY				bsafeGenPrivKey;	// only used to create cdsaDerivePrivKey
	BU_KEY				bsafeDerivePrivKey;	// same as cdsaGenPrivKey
	unsigned			actKeySizeBits;
	CSSM_RETURN			crtn;
	int					rtn;
	
	if(!keySizeInBits) {
		/* random key size */
		actKeySizeBits = randKeySizeBits(keyAlg, OT_Encrypt);
	}
	else {
		/* caller/user specified */
		actKeySizeBits = keySizeInBits;
	}
	if(verbose) {
		printf("   ...generating %s key pair, keySize %d bits...\n",
			algToStr(keyAlg), actKeySizeBits);
	}
	
	/* 
     * Generate two keypairs 
	 */
	if(keyAlg == CSSM_ALGID_DSA) {
		CSSM_BOOL doGenParams;
		
		if(bareCsp || CSPDL_DSA_GEN_PARAMS) {
			doGenParams = CSSM_TRUE;
		}
		else {
			/* CSPDL - no gen params */
			doGenParams = CSSM_FALSE;
		}
		crtn = cspGenDSAKeyPair(cspHand,
			"foo",
			3,
			actKeySizeBits,
			&cdsaGenPubKey,
			pubRefKeys,
			CSSM_KEYUSE_ANY,
			CSSM_KEYBLOB_RAW_FORMAT_NONE,
			&cdsaGenPrivKey,
			privRefKeys,
			CSSM_KEYUSE_SIGN,
			CSSM_KEYBLOB_RAW_FORMAT_NONE,
			doGenParams,		// genParams
			NULL);				// params
	}
	else {
		crtn = cspGenKeyPair(cspHand,
			keyAlg,
			"foo",
			3,
			actKeySizeBits,
			&cdsaGenPubKey,
			pubRefKeys,
			CSSM_KEYUSE_ANY,
			CSSM_KEYBLOB_RAW_FORMAT_NONE,
			&cdsaGenPrivKey,
			privRefKeys,
			CSSM_KEYUSE_ANY,
			CSSM_KEYBLOB_RAW_FORMAT_NONE,
			CSSM_FALSE);					// genSeed not used 
	}
	if(crtn) {
		return testError(quiet);
	}
	crtn = buGenKeyPair(actKeySizeBits,
		keyAlg,
		&bsafeGenPubKey,
		&bsafeGenPrivKey);
	if(crtn) {
		return testError(quiet);
	}
	
	/* 
	 * Convert private keys to other library. 
	 * NOTE: the reason we're only converting private keys is solely due to the 
	 * fact that BSAFE does not handle PKCS1 formatted public key blobs. Very odd. 
	 * But it's too much of a pain to re-implement that wheel here, and SSL and 
	 * cert handling in general verify the CSP's PKCS1-style public key handling. 
	 */
	if(privRefKeys) {
		/* first generate a temporary raw CDSA key */
		crtn = buBsafePrivKeyToCdsa(keyAlg, 
			actKeySizeBits,
			bsafeGenPrivKey, 
			&cdsaTempKey);
		if(crtn) {
			return testError(quiet);
		}
		
		/* convert it to the ref key we'll actually use */
		crtn = cspRawKeyToRef(cspHand, &cdsaTempKey, &cdsaDerivePrivKey);
		cspFreeKey(cspHand, &cdsaTempKey);
	}
	else {
		crtn = buBsafePrivKeyToCdsa(keyAlg, 
			actKeySizeBits,
			bsafeGenPrivKey, 
			&cdsaDerivePrivKey);
	}
	if(crtn) {
		return testError(quiet);
	}
	if(privRefKeys) {
		/* we have a CDSA priv ref key; convert it to raw format */
		crtn = cspRefKeyToRaw(cspHand, &cdsaGenPrivKey, &cdsaTempKey);
		if(crtn) {
			return testError(quiet);
		}
		/* now convert it to BSAFE */
		crtn = buCdsaPrivKeyToBsafe(&cdsaTempKey, &bsafeDerivePrivKey);
		cspFreeKey(cspHand, &cdsaTempKey);
	}
	else {
		crtn = buCdsaPrivKeyToBsafe(&cdsaGenPrivKey, &bsafeDerivePrivKey);
	}
	if(crtn) {
		return testError(quiet);
	}
	
	if(sigLoops) {
		rtn = sigTest(cspHand,
			sigLoops,
			bsafeDerivePrivKey,
			&cdsaGenPubKey,
			&cdsaDerivePrivKey,
			bsafeGenPubKey,
			ptext,
			maxPtextSize,
			sigAlg,
			rsaBlinding,
			quiet,
			verbose);
		if(rtn) {
			return rtn;
		}
	}
	
	if(encrLoops) {
		rtn = encryptTest(cspHand,
			encrLoops,
			bsafeDerivePrivKey,
			&cdsaGenPubKey,
			&cdsaDerivePrivKey,
			bsafeGenPubKey,
			ptext,
			maxPtextSize,
			rsaBlinding,
			quiet,
			verbose);
		if(rtn) {
			return rtn;
		}
	}

	/* free all six keys */
	buFreeKey(bsafeGenPubKey);
	buFreeKey(bsafeGenPrivKey);
	buFreeKey(bsafeDerivePrivKey);
	cspFreeKey(cspHand, &cdsaGenPubKey);
	cspFreeKey(cspHand, &cdsaGenPrivKey);
	cspFreeKey(cspHand, &cdsaDerivePrivKey);
	return 0;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	CSSM_DATA			ptext;
	CSSM_CSP_HANDLE 	cspHand;
	int					i;
	int					rtn = 0;
	
	/*
	 * User-spec'd params
	 */
	uint32				keySizeInBits = 0;
	unsigned			oloops = OLOOPS_DEF;
	unsigned			sigLoops = SIG_LOOPS_DEF;
	unsigned			encrLoops = ENC_LOOPS_DEF;
	CSSM_BOOL			verbose = CSSM_FALSE;
	CSSM_BOOL			quiet = CSSM_FALSE;
	unsigned			pauseInterval = 0;
	CSSM_BOOL			bareCsp = CSSM_TRUE;
	CSSM_BOOL			doDSA = CSSM_TRUE;
	CSSM_BOOL			doRSA = CSSM_TRUE;
	CSSM_BOOL			pubRefKeys = CSSM_FALSE;
	CSSM_BOOL			privRefKeys = CSSM_FALSE;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case 'r':
						doDSA = CSSM_FALSE;
						break;
					case 'd':
						doRSA = CSSM_FALSE;
						break;
					default:
						usage(argv);
				}
				break;
		    case 'l':
				oloops = atoi(&argp[2]);
				break;
		    case 's':
				sigLoops = atoi(&argp[2]);
				break;
		    case 'e':
				encrLoops = atoi(&argp[2]);
				break;
		    case 'k':
		    	keySizeInBits = atoi(&argp[2]);
				break;
		    case 'v':
		    	verbose = CSSM_TRUE;
				break;
			case 'r':
				privRefKeys = CSSM_TRUE;
				break;
			case 'R':
				pubRefKeys = CSSM_TRUE;
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				#if 	CSPDL_ALL_KEYS_ARE_REF
				privRefKeys = CSSM_TRUE;
				pubRefKeys = CSSM_TRUE;
				#endif
				break;
		    case 'E':
		    	sigLoops = 0;
				break;
			case 'S':
				encrLoops = 0;
				break;
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
		    case 'p':
		    	pauseInterval = atoi(&argp[2]);;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	ptext.Data = (uint8 *)CSSM_MALLOC(MAX_TEXT_SIZE);
	if(ptext.Data == NULL) {
		printf("Insufficient heap space\n");
		exit(1);
	}
	/* ptext length set in inner test loops */
	
	printf("Starting asymCompat; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	if(pauseInterval) {
		fpurge(stdin);
		printf("Top of test; hit CR to proceed: ");
		getchar();
	}
	for(loop=1; ; loop++) {
		if(!quiet) {
			if(verbose || ((loop % LOOP_NOTIFY) == 0)) {
				printf("...oloop %d\n", loop);
			}
		}
		
		if(doRSA) {
			CSSM_ALGORITHMS	sigAlg;
			if(loop & 1) {
				sigAlg = CSSM_ALGID_SHA1WithRSA;
			}
			else {
				sigAlg = CSSM_ALGID_MD5WithRSA;
			}
			
			/* enable RSA blinding on half the loops for RSA */
			CSSM_BOOL rsaBlinding = CSSM_FALSE;
			if(loop & 2) {
				rsaBlinding = CSSM_TRUE;
			}
			
			rtn = doTest(cspHand,
				CSSM_ALGID_RSA,
				sigAlg,
				sigLoops,
				encrLoops,
				rsaBlinding,
				&ptext,
				MAX_TEXT_SIZE,
				keySizeInBits,
				pubRefKeys,
				privRefKeys,
				bareCsp,
				quiet,
				verbose);
			if(rtn) {
				break;
			}
		}
		if(doDSA) {
			rtn = doTest(cspHand,
				CSSM_ALGID_DSA,
				CSSM_ALGID_SHA1WithDSA,
				sigLoops,
				0,					// encrLoops - none for DSA
				CSSM_FALSE,			// blinding
				&ptext,
				MAX_TEXT_SIZE,
				keySizeInBits,
				pubRefKeys,
				privRefKeys,
				bareCsp,
				quiet,
				verbose);
			if(rtn) {
				break;
			}
		}
		if(oloops && (loop == oloops)) {
			break;
		}
		if(pauseInterval && (loop % pauseInterval) == 0) {
			fpurge(stdin);
			printf("hit CR to proceed: ");
			getchar();
		}
	}
	
	cspShutdown(cspHand, bareCsp);
	if(pauseInterval) {
		fpurge(stdin);
		printf("ModuleDetach/Unload complete; hit CR to exit: ");
		getchar();
	}
	if((rtn == 0) && !quiet) {
		printf("%s test complete\n", argv[0]);
	}
	CSSM_FREE(ptext.Data);
	return rtn;
}


