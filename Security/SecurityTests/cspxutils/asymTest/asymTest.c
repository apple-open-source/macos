/* Copyright (c) 1998,2003-2006,2008 Apple Inc.
 *
 * asymTest.c - test CSP asymmetric encrypt/decrypt.
 *
 * Revision History
 * ----------------
 *  10 May 2000 Doug Mitchell
 *		Ported to X/CDSA2. 
 *  14 May 1998	Doug Mitchell at Apple
 *		Created.
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"
#include "cspdlTesting.h"

#define USAGE_NAME			"noUsage"
#define USAGE_NAME_LEN		(strlen(USAGE_NAME))
#define USAGE2_NAME			"noUsage2"
#define USAGE2_NAME_LEN		(strlen(USAGE2_NAME))
#define LOOPS_DEF			10
#define MIN_EXP				2		/* for data size 10**exp */
#define DEFAULT_MAX_EXP		2
#define MAX_EXP				4

/*
 * Enumerate algs our own way to allow iteration.
 */
#define ALG_RSA				1
#define ALG_FEED			2
#define ALG_FEEDEXP			3
#define ALG_FEE_CFILE		4
#define ALG_FIRST			ALG_RSA
#define ALG_LAST			ALG_FEEDEXP
#define MAX_DATA_SIZE		(10000 + 100)	/* bytes */
#define FEE_PASSWD_LEN		32		/* private data length in bytes, FEE only */

#define DUMP_KEY_DATA		0

/* 
 * RSA encryption now allows arbitrary plaintext size. BSAFE was limited to 
 * primeSize - 11.
 */
#define RSA_PLAINTEXT_LIMIT		0

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=algorithm (f=FEED; x=FEEDExp; c=FEE_Cfile; r=RSA; default=all)\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   n=minExp (default=%d)\n", MIN_EXP);
	printf("   x=maxExp (default=%d, max=%d)\n", DEFAULT_MAX_EXP, MAX_EXP);
	printf("   k=keySize\n");
	printf("   P=primeType (m=Mersenne, f=FEE, g=general; FEE only)\n");
	printf("   C=curveType (m=Montgomery, w=Weierstrass, g=general; FEE only)\n");
	printf("   e(xport)\n");
	printf("   r(eference keys only)\n");
	printf("   K (skip decrypt)\n");
	printf("   N(o padding, RSA only)\n");
	printf("   S (no staging)\n");
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   p (pause on each loop)\n");
	printf("   u (quick; small keys)\n");
	printf("   t=plainTextSize; default=random\n");
	printf("   z(ero data)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

static int doTest(CSSM_CSP_HANDLE cspHand,
	CSSM_ALGORITHMS	alg,					// CSSM_ALGID_xxx
	CSSM_PADDING	padding,
	CSSM_DATA_PTR 	ptext,
	CSSM_BOOL 		verbose,
	CSSM_BOOL 		quiet,
	uint32 			keySizeInBits,			// may be 0, i.e., default per alg
	uint32			primeType,				// FEE only
	uint32			curveType,				// ditto
	CSSM_BOOL 		pubIsRef,
	CSSM_KEYBLOB_FORMAT rawPubForm,
	CSSM_BOOL		privIsRef,
	CSSM_KEYBLOB_FORMAT rawPrivForm,
	CSSM_BOOL		secondPubIsRaw,			// FEED via CSPDL: 2nd key must be raw
	CSSM_BOOL 		stagedEncr,
	CSSM_BOOL 		stagedDecr,
	CSSM_BOOL 		mallocPtext,			// only meaningful if !stagedDecr
	CSSM_BOOL 		mallocCtext,			// only meaningful if !stagedEncr
	CSSM_BOOL		skipDecrypt,
	CSSM_BOOL		genSeed)				// FEE keys only
{
	// these two are always generated
	CSSM_KEY		recvPubKey;
	CSSM_KEY		recvPrivKey;
	// these two are for two-key FEE algorithms only
	CSSM_KEY		sendPubKey;		
	CSSM_KEY		sendPrivKey;	
	// these two are optionally created by cspRefKeyToRaw if (FEED && secondPubIsRaw)
	CSSM_KEY		sendPubKeyRaw;	
	CSSM_KEY		recvPubKeyRaw;	
	CSSM_BOOL		rawPubKeysCreated = CSSM_FALSE;
	
	/* two-key FEE, CSP  :  &{send,recv}PubKey
	 * two-key FEE, CSPDL:  &{send,recv}PubKeyRaw
	 * else              :  NULL, &recvPubKey
	 */
	CSSM_KEY_PTR	sendPubKeyPtr = NULL;	
	CSSM_KEY_PTR	recvPubKeyPtr = NULL;
	
	CSSM_DATA 		ctext = {0, NULL};
	CSSM_DATA		rptext = {0, NULL};
	CSSM_RETURN		crtn;
	int				rtn = 0;
	uint32			keyGenAlg;
	uint32			mode = CSSM_ALGMODE_NONE;	// FIXME - does this need testing?
	CSSM_BOOL		twoKeys = CSSM_FALSE;
	
	switch(alg) {
		case CSSM_ALGID_FEED:
		case CSSM_ALGID_FEECFILE:
			twoKeys = CSSM_TRUE;
			/* drop thru */
		case CSSM_ALGID_FEEDEXP:
			keyGenAlg = CSSM_ALGID_FEE;
			break;
		case CSSM_ALGID_RSA:
			keyGenAlg = CSSM_ALGID_RSA;
			break;
		default:
			printf("bogus algorithm\n");
			return 1;
	}
	
	/* one key pair for all algs except CFILE and FEED, which need two */
	if(keyGenAlg == CSSM_ALGID_FEE) {
		uint8 			passwd[FEE_PASSWD_LEN];
		CSSM_DATA		pwdData = {FEE_PASSWD_LEN, passwd};
		CSSM_DATA_PTR	pwdDataPtr;
		if(genSeed) {
			simpleGenData(&pwdData, FEE_PASSWD_LEN, FEE_PASSWD_LEN);
			pwdDataPtr = &pwdData;
		}
		else {
			pwdDataPtr = NULL;
		}
		/*
		 * Note we always generate public keys per the pubIsRef argument, even if 
		 * secondPubIsRaw is true, 'cause the CSPDL can't generate raw keys.
		 */
		rtn = cspGenFEEKeyPair(cspHand,
				USAGE_NAME,
				USAGE_NAME_LEN,
				keySizeInBits,
				primeType,
				curveType,
				&recvPubKey,
				pubIsRef,
				CSSM_KEYUSE_ANY,
				CSSM_KEYBLOB_RAW_FORMAT_NONE,
				&recvPrivKey,
				privIsRef,
				CSSM_KEYUSE_ANY,
				CSSM_KEYBLOB_RAW_FORMAT_NONE,
				pwdDataPtr);
		if(rtn) {
			/* leak */
			return testError(quiet);
		}
		if(twoKeys) {
			rtn = cspGenFEEKeyPair(cspHand,
					USAGE2_NAME,
					USAGE2_NAME_LEN,
					keySizeInBits,
					primeType,
					curveType,
					&sendPubKey,
					pubIsRef,
					CSSM_KEYUSE_ANY,
					CSSM_KEYBLOB_RAW_FORMAT_NONE,
					&sendPrivKey,
					privIsRef,
					CSSM_KEYUSE_ANY,
					CSSM_KEYBLOB_RAW_FORMAT_NONE,
					pwdDataPtr);
			if(rtn) {
				/* leak recv*Key */
				return testError(quiet);
			}
		}
		if(twoKeys) {
			if(secondPubIsRaw && pubIsRef) {
				/* 
				* Convert ref public keys to raw - they're going into a Context; the
				* SecurityServer doesn't deal with ref keys there.
				* Leak all sorts of stuff on any error here. 
				*/
				crtn = cspRefKeyToRaw(cspHand, &sendPubKey, &sendPubKeyRaw);
				if(crtn) {
					return testError(quiet);
				}
				crtn = cspRefKeyToRaw(cspHand, &recvPubKey, &recvPubKeyRaw);
				if(crtn) {
					return testError(quiet);
				}
				/* two keys, CSPDL */
				sendPubKeyPtr = &sendPubKeyRaw;
				recvPubKeyPtr = &recvPubKeyRaw;
				rawPubKeysCreated = CSSM_TRUE;
			}
			else {
				/* two keys, CSP */
				sendPubKeyPtr = &sendPubKey;
				recvPubKeyPtr = &recvPubKey;
			}
		}
		else {
			/* one key pair, standard config */
			sendPubKeyPtr = NULL;
			recvPubKeyPtr =  &recvPubKey;
		}
	}
	else {
		CSSM_KEYBLOB_FORMAT expectPubForm = rawPubForm;
		CSSM_KEYBLOB_FORMAT expectPrivForm = rawPrivForm;
		
		rtn = cspGenKeyPair(cspHand,
				keyGenAlg,
				USAGE_NAME,
				USAGE_NAME_LEN,
				keySizeInBits,
				&recvPubKey,
				pubIsRef,
				twoKeys ? CSSM_KEYUSE_ANY : CSSM_KEYUSE_ENCRYPT,
				rawPubForm,
				&recvPrivKey,
				privIsRef,
				twoKeys ? CSSM_KEYUSE_ANY : CSSM_KEYUSE_DECRYPT,
				rawPrivForm,
				genSeed);
		if(rtn) {
			return testError(quiet);
		}
		/* one key pair, standard config */
		sendPubKeyPtr = NULL;
		recvPubKeyPtr =  &recvPubKey;
		
		/* verify defaults - only for RSA */
		if(!pubIsRef) {
			if(rawPubForm == CSSM_KEYBLOB_RAW_FORMAT_NONE) {
				expectPubForm = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
			}
			if(recvPubKey.KeyHeader.Format != expectPubForm) {
				printf("***Bad raw RSA pub key format - exp %u got %u\n",
					(unsigned)expectPubForm, 
					(unsigned)recvPubKey.KeyHeader.Format);
				if(testError(quiet)) {
					return 1;
				}
			}
		}
		if(!privIsRef) {
			if(rawPrivForm == CSSM_KEYBLOB_RAW_FORMAT_NONE) {
				expectPrivForm = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
			}
			if(recvPrivKey.KeyHeader.Format != expectPrivForm) {
				printf("***Bad raw RSA priv key format - exp %u got %u\n",
					(unsigned)expectPrivForm, 
					(unsigned)recvPrivKey.KeyHeader.Format);
				if(testError(quiet)) {
					return 1;
				}
			}
		}
	}
	#if	DUMP_KEY_DATA
	dumpBuffer("Pub  Key Data", recvPubKey.KeyData.Data, recvPubKey.KeyData.Length);
	dumpBuffer("Priv Key Data", recvPrivKey.KeyData.Data, recvPrivKey.KeyData.Length);
	#endif
	if(stagedEncr) {
		crtn = cspStagedEncrypt(cspHand,
			alg,
			mode,
			padding,
			/* Two keys: second must be pub */
			twoKeys ? &sendPrivKey  : &recvPubKey,
			twoKeys ? recvPubKeyPtr : NULL,
			0,			// effectiveKeySize
			0,			// cipherBlockSize
			0,			// rounds
			NULL,		// initVector
			ptext,
			&ctext,
			CSSM_TRUE);	// multi
	}
	else {
		crtn = cspEncrypt(cspHand,
			alg,
			mode,
			padding,
			/* Two keys: second must be pub */
			twoKeys ? &sendPrivKey  : &recvPubKey,
			twoKeys ? recvPubKeyPtr : NULL,
			0,			// effectiveKeySize
			0,			// rounds
			NULL,		// initVector
			ptext,
			&ctext,
			mallocCtext);
	}
	if(crtn) {
		rtn = testError(quiet);
		goto abort;
	}
	if(verbose) {
		printf("  ..ptext size %lu  ctext size %lu\n",
			(unsigned long)ptext->Length, (unsigned long)ctext.Length);
	}
	if(skipDecrypt) {
		goto abort;
	}
	if(stagedDecr) {
		crtn = cspStagedDecrypt(cspHand,
			alg,
			mode,
			padding,
			/* Two keys: second must be pub */
			&recvPrivKey,
			sendPubKeyPtr,
			0,			// effectiveKeySize
			0,			// cipherBlockSize
			0,			// rounds
			NULL,		// initVector
			&ctext,
			&rptext,
			CSSM_TRUE);	// multi
	}
	else {
		crtn = cspDecrypt(cspHand,
			alg,
			mode,
			padding,
			&recvPrivKey,
			sendPubKeyPtr,
			0,			// effectiveKeySize
			0,			// rounds
			NULL,		// initVector
			&ctext,
			&rptext,
			mallocPtext);
	}
	if(crtn) {
		rtn = testError(quiet);
		goto abort;
	}
	/* compare ptext, rptext */
	if(ptext->Length != rptext.Length) {
		printf("Ptext length mismatch: expect %lu, got %lu\n", 
			(unsigned long)ptext->Length, (unsigned long)rptext.Length);
		rtn = testError(quiet);
		if(rtn) {
			goto abort;
		}
	}
	if(memcmp(ptext->Data, rptext.Data, ptext->Length)) {
		printf("***data miscompare\n");
		rtn = testError(quiet);
	}
abort:
	/* free keys */
	if(cspFreeKey(cspHand, &recvPubKey)) {
		printf("Error freeing recvPubKey\n");
		rtn = 1;
	}
	if(cspFreeKey(cspHand, &recvPrivKey)) {
		printf("Error freeing recvPrivKey\n");
		rtn = 1;
	}
	if(twoKeys) {
		if(cspFreeKey(cspHand, &sendPubKey)) {
			printf("Error freeing sendPubKey\n");
			rtn = 1;
		}
		if(cspFreeKey(cspHand, &sendPrivKey)) {
			printf("Error freeing sendPrivKey\n");
			rtn = 1;
		}
		if(rawPubKeysCreated) {
			if(cspFreeKey(cspHand, &sendPubKeyRaw)) {
				printf("Error freeing sendPubKeyRaw\n");
				rtn = 1;
			}
			if(cspFreeKey(cspHand, &recvPubKeyRaw)) {
				printf("Error freeing recvPubKeyRaw\n");
				rtn = 1;
			}
		}
	}
	/* free rptext, ctext */
	appFreeCssmData(&rptext, CSSM_FALSE);
	appFreeCssmData(&ctext, CSSM_FALSE);
	return rtn;
}

static const char *formStr(
	CSSM_KEYBLOB_FORMAT form)
{
	switch(form) {
		case CSSM_KEYBLOB_RAW_FORMAT_NONE:  return "NONE";
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS1: return "PKCS1";
		case CSSM_KEYBLOB_RAW_FORMAT_PKCS8: return "PKCS8";
		case CSSM_KEYBLOB_RAW_FORMAT_X509:  return "X509";
		case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH:  return "SSH1";
		case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2:  return "SSH2";
		default:
			printf("***BRRRZAP! formStr needs work\n");
			exit(1);
	}
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	CSSM_DATA			ptext;
	CSSM_CSP_HANDLE 	cspHand;
	CSSM_BOOL			pubIsRef = CSSM_TRUE;
	CSSM_BOOL			privIsRef = CSSM_TRUE;
	CSSM_BOOL			stagedEncr;
	CSSM_BOOL 			stagedDecr;
	const char 			*algStr;
	uint32				encAlg;			// CSSM_ALGID_xxx
	unsigned			currAlg;		// ALG_xxx
	int					i;
	CSSM_BOOL			mallocCtext;
	CSSM_BOOL			mallocPtext;
	int					rtn = 0;
	CSSM_BOOL			genSeed;		// for FEE key gen
	CSSM_PADDING		padding;
	CSSM_KEYBLOB_FORMAT	rawPubFormat = CSSM_KEYBLOB_RAW_FORMAT_NONE;
	CSSM_KEYBLOB_FORMAT	rawPrivFormat = CSSM_KEYBLOB_RAW_FORMAT_NONE;
	
	/*
	 * User-spec'd params
	 */
	unsigned			loops = LOOPS_DEF;
	CSSM_BOOL			verbose = CSSM_FALSE;
	unsigned			minExp = MIN_EXP;
	unsigned			maxExp = DEFAULT_MAX_EXP;
	CSSM_BOOL			quiet = CSSM_FALSE;
	uint32				keySizeInBits = CSP_KEY_SIZE_DEFAULT;
	CSSM_BOOL			keySizeSpec = CSSM_FALSE;
	unsigned			minAlg = ALG_FIRST;
	uint32				maxAlg = ALG_LAST;
	CSSM_BOOL			skipDecrypt = CSSM_FALSE;
	CSSM_BOOL			bareCsp = CSSM_TRUE;
	CSSM_BOOL			doPause = CSSM_FALSE;
	CSSM_BOOL			smallKeys = CSSM_FALSE;
	uint32				primeType = CSSM_FEE_PRIME_TYPE_DEFAULT;	// FEE only
	uint32				curveType = CSSM_FEE_CURVE_TYPE_DEFAULT;	// FEE only
	uint32				ptextSize = 0;			// 0 means random
	dataType			dtype = DT_Random;
	CSSM_BOOL			refKeysOnly = CSSM_FALSE;
	CSSM_BOOL			noPadding = CSSM_FALSE;
	CSSM_BOOL			stagingEnabled = CSSM_TRUE;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case 'f':
						minAlg = maxAlg = ALG_FEED;
						break;
					case 'x':
						minAlg = maxAlg = ALG_FEEDEXP;
						break;
					case 'c':
						minAlg = maxAlg = ALG_FEE_CFILE;
						break;
					case 'r':
						minAlg = maxAlg = ALG_RSA;
						break;
					case 'a':
						minAlg = ALG_FIRST;
						maxAlg = ALG_LAST;
						break;
					default:
						usage(argv);
				}
				break;
		    case 'l':
				loops = atoi(&argp[2]);
				break;
		    case 'n':
				minExp = atoi(&argp[2]);
				break;
		    case 'x':
				maxExp = atoi(&argp[2]);
				if(maxExp > MAX_EXP) {
					usage(argv);
				}
				break;
			case 'k':
				keySizeInBits = atoi(&argv[arg][2]);
				keySizeSpec = CSSM_TRUE;
				break;
			case 'K':
				skipDecrypt = CSSM_TRUE;
				break;
		    case 't':
				ptextSize = atoi(&argp[2]);
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				#if CSPDL_ALL_KEYS_ARE_REF
		    	refKeysOnly = CSSM_TRUE;
				#endif
				break;
			case 'u':
				smallKeys = CSSM_TRUE;
				break;
		    case 'N':
		    	noPadding = CSSM_TRUE;
				break;
			case 'z':
				dtype = DT_Zero;
				break;
		    case 'v':
		    	verbose = CSSM_TRUE;
				break;
		    case 'r':
		    	refKeysOnly = CSSM_TRUE;
				break;
			case 'S':
				stagingEnabled = CSSM_FALSE;
				break;
		    case 'p':
		    	doPause = CSSM_TRUE;
				break;
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
			case 'C':
				switch(argp[2]) {
					case 'm':
						curveType = CSSM_FEE_CURVE_TYPE_MONTGOMERY;
						break;
					case 'w':
						curveType = CSSM_FEE_CURVE_TYPE_WEIERSTRASS;
						break;
					default:
						usage(argv);
				}
				break;
			case 'P':
				switch(argp[2]) {
					case 'm':
						primeType = CSSM_FEE_PRIME_TYPE_MERSENNE;
						break;
					case 'f':
						primeType = CSSM_FEE_PRIME_TYPE_FEE;
						break;
					case 'g':
						primeType = CSSM_FEE_PRIME_TYPE_GENERAL;
						break;
					default:
						usage(argv);
				}
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	ptext.Data = (uint8 *)CSSM_MALLOC(MAX_DATA_SIZE);
	
	/* length set in test loop */
	if(ptext.Data == NULL) {
		printf("Insufficient heap\n");
		exit(1);
	}
	if(noPadding) {
		if(ptextSize == 0) {
			printf("**WARNING NoPad mode will fail with random plaintext size\n");
		}
		if(!keySizeSpec) {
			printf("**WARNING NoPad mode will fail with random key size\n");
		}
		else {
			uint32 keyBytes = keySizeInBits / 8;
			if(ptextSize != keyBytes) {
				/*
				 * FIXME: I actually do not understand why this fails, but
				 * doing raw RSA encryption with ptextSize != keySize results
				 * in random-looking failures, probably based on the plaintext
				 * itself (it doesn't fail with zero data).
				 */
				printf("***WARNING NoPad mode requires plaintext size = key size\n");
			}
		}
	}
	printf("Starting asymTest; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	for(currAlg=minAlg; currAlg<=maxAlg; currAlg++) {
		switch(currAlg) {
			case ALG_FEED:
				encAlg = CSSM_ALGID_FEED;
				algStr = "FEED";
				padding = CSSM_PADDING_NONE;
				break;
			case ALG_FEEDEXP:
				encAlg = CSSM_ALGID_FEEDEXP;
				algStr = "FEEDExp";
				padding = CSSM_PADDING_NONE;
				break;
			case ALG_FEE_CFILE:
				encAlg = CSSM_ALGID_FEECFILE;
				algStr = "FEE_CFILE";
				padding = CSSM_PADDING_NONE;
				break;
			case ALG_RSA:
				encAlg = CSSM_ALGID_RSA;
				algStr = "RSA";
				if(noPadding) {
					padding = CSSM_PADDING_NONE;
				}
				else {
					padding = CSSM_PADDING_PKCS1;
				}
				break;
		}
		if(!quiet) {
			printf("Testing alg %s\n", algStr);
		}
		for(loop=1; ; loop++) {
			if(doPause) {
				fpurge(stdin);
				printf("Top of loop; hit CR to proceed: ");
				getchar();
			}
			if(ptextSize) {
				if(dtype == DT_Zero) {
					memset(ptext.Data, 0, ptextSize);
					ptext.Length = ptextSize;
				}
				else {
					simpleGenData(&ptext, ptextSize, ptextSize);
				}
			}
			else {
				ptext.Length = genData(ptext.Data, minExp, maxExp, dtype);
			}
			
			/* raw RSA, no padding, ensure top two bits are zero */
			if((encAlg == CSSM_ALGID_RSA) && (padding == CSSM_PADDING_NONE)) {
				ptext.Data[0] &= 0x3f;
			}

			if(!keySizeSpec) {
				/* random per alg unless user overrides */
				if(encAlg == CSSM_ALGID_RSA) { 
					if(smallKeys) {
						keySizeInBits = CSP_RSA_KEY_SIZE_DEFAULT;
					}
					else {
						keySizeInBits = randKeySizeBits(CSSM_ALGID_RSA, OT_Encrypt);
					}
				}
				else {
					/* FEED, FEEDExp */
					if(smallKeys) {
						keySizeInBits = 127;
						/* default curveType = Weierstrass */
					}
					else {
						randFeeKeyParams(encAlg,
							&keySizeInBits,
							&primeType,
							&curveType);
					}
				}
			}
			#if		RSA_PLAINTEXT_LIMIT
			if(encAlg == CSSM_ALGID_RSA) {
				/* total ptext size can't exceed (modulus size - 11) */
				/* we should probably get this from the CSP, but this
				 * whole thing is such a kludge. What's the point?
				 * Only RSA encrypt/decrypt has a max total size.
				 */
			 	unsigned modSize;
			 	unsigned maxSize;
				if(keySizeInBits == CSP_KEY_SIZE_DEFAULT) {
					modSize = CSP_RSA_KEY_SIZE_DEFAULT / 8;
				}
				else {
					modSize = keySizeInBits / 8;
				}
				maxSize = modSize - 11;
				ptext.Length = genRand(1, maxSize);
			}
			#endif
			if(!quiet) {
				if(encAlg == CSSM_ALGID_RSA) { 
					printf("..loop %d text size %lu keySize %u\n",
						loop, (unsigned long)ptext.Length, (unsigned)keySizeInBits);
				}
				else {
					printf("..loop %d text size %lu keySize %u primeType %s "
						"curveType %s\n",
						loop, (unsigned long)ptext.Length, (unsigned)keySizeInBits,
						primeTypeStr(primeType), curveTypeStr(curveType));
				}
			}
			
			/* mix up some ref and data keys, as well as staging and mallocing */
			if(!refKeysOnly) {
				pubIsRef   = (loop & 1) ? CSSM_TRUE : CSSM_FALSE;
				privIsRef   = (loop & 8) ? CSSM_TRUE : CSSM_FALSE;
			}
			if((currAlg == ALG_FEE_CFILE) || !stagingEnabled) {
				/* staged ops unsupported */
				stagedEncr = CSSM_FALSE;
				stagedDecr = CSSM_FALSE;
			}
			else {
				stagedEncr = (loop & 2) ? CSSM_TRUE : CSSM_FALSE;
				stagedDecr  = (loop & 4) ? CSSM_TRUE : CSSM_FALSE;
			}
			if(!stagedEncr) {
				mallocCtext = (ptext.Data[0] & 1) ? CSSM_TRUE : CSSM_FALSE;
			}
			else {
				mallocCtext = CSSM_FALSE;
			}
			if(!stagedDecr) {
				mallocPtext = (ptext.Data[0] & 2) ? CSSM_TRUE : CSSM_FALSE;
			}
			else {
				mallocPtext = CSSM_FALSE;
			}
			switch(currAlg) {
				case ALG_FEED:
				case ALG_FEEDEXP:
					genSeed = (ptext.Data[0] & 4) ? CSSM_TRUE : CSSM_FALSE;
					break;
				default:
					genSeed = CSSM_FALSE;
			}
			rawPubFormat = rawPrivFormat = CSSM_KEYBLOB_RAW_FORMAT_NONE;
			if(currAlg == ALG_RSA) {
				/* mix up raw key formats */
				unsigned die;
				if(!pubIsRef) {
					/* five formats */
					die = ptext.Data[1] % 5;
					switch(die) {
						case 0:
							rawPubFormat = CSSM_KEYBLOB_RAW_FORMAT_NONE;
							break;
						case 1:
							rawPubFormat = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
							break;
						case 2:
							rawPubFormat = CSSM_KEYBLOB_RAW_FORMAT_OPENSSH;
							break;
						case 3:
							rawPubFormat = CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2;
							break;
						default:
							rawPubFormat = CSSM_KEYBLOB_RAW_FORMAT_X509;
							break;
					}
				}
				if(!privIsRef) {
					/* four formats */
					die = ptext.Data[2] % 4;
					switch(die) {
						case 0:
							rawPrivFormat = CSSM_KEYBLOB_RAW_FORMAT_NONE;
							break;
						case 1:
							rawPrivFormat = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
							break;
						case 2:
							rawPrivFormat = CSSM_KEYBLOB_RAW_FORMAT_OPENSSH;
							break;
						default:
							rawPrivFormat = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
							break;
					}
				}
			}
			#if 0
			if(encAlg == CSSM_ALGID_RSA) {
				/* FIXME - another restriction - the MS bits of each block
				 * of plaintext must be zero, to make the numerical value of the
				 * block less than the modulus!
				 * Aug 7 1998: Now that we're using AI_PKCS_RSA{Public,Private}, the
				 * numerical value of the data no longer has to be less than
				 * the modulus.
				 */
				stagedEncr = stagedDecr = CSSM_TRUE;
			}
			#endif
			
			if(!quiet) {
				printf("  pubRef %d  privRef %d stgEncr %d  stgdDecr %d "
					"malPtext %d malCtext %d genSeed %d pubForm %s privForm %s\n",
					 (int)pubIsRef, (int)privIsRef, (int)stagedEncr, (int)stagedDecr, 
						(int)mallocPtext, (int)mallocCtext, (int)genSeed, 
						formStr(rawPubFormat), formStr(rawPrivFormat));
			}
			if(doTest(cspHand,
					encAlg,
					padding,
					&ptext,
					verbose,
					quiet,
					keySizeInBits,
					primeType,
					curveType,
					pubIsRef,
					rawPubFormat,
					privIsRef,
					rawPrivFormat,
					#if CSPDL_2ND_PUB_KEY_IS_RAW
					/* secondPubIsRaw */
					bareCsp ? CSSM_FALSE : CSSM_TRUE,
					#else
					CSSM_FALSE,
					#endif
					stagedEncr,
					stagedDecr,
					mallocCtext,
					mallocPtext,
					skipDecrypt,
					genSeed)) {
				rtn = 1;
				goto testDone;
			}
			if(loops && (loop == loops)) {
				break;
			}
		}	/* for loop */
	}		/* for alg */
testDone:
	CSSM_ModuleDetach(cspHand);
	if((rtn == 0) && !quiet) {
		printf("%s test complete\n", argv[0]);
	}
	return rtn;
}
