/* Copyright (c) 1997,2003-2006,2008 Apple Inc.
 *
 * badsig.c - Verify bad signature detect, CDSA version.
 *
 * Revision History
 * ----------------
 *   4 May 2000 Doug Mitchell
 *		Ported to X/CDSA2. 
 *  28 Apr 1998	Doug Mitchell at Apple
 *		Ported to CDSA 1.2, new Apple CSP.
 *  15 Aug 1997	Doug Mitchell at Apple
 *		Ported from CryptKit ObjC version
 *  26 Aug 1996	Doug Mitchell at NeXT
 *		Created.
 */
/*
 * text size =       {random, from 100 bytes to 1 megabyte, in
 *                   geometrical steps, i.e. the number of
 *                   bytes would be 10^r, where r is random out of
 *                   {2,3,4,5,6}, plus a random integer in {0,..99}};
 *
 * for loop_count
 *     text contents = {random data, random size as specified above};
 *     generate key pair;
 *     generate signature, validate;
 *     for various bytes of ptext {
 *        corrupt text byte;
 *        verify bad signature;
 *        restore corrupted byte;
 *     }
 *  }
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <Security/cssm.h>
#include "cspwrap.h"
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"
#include "cspdlTesting.h"

#define USAGE_NAME		"noUsage"
#define USAGE_NAME_LEN	(strlen(USAGE_NAME))

/*
 * Defaults.
 */
#define LOOPS_DEF			10
#define MIN_EXP				2		/* for data size 10**exp */
#define DEFAULT_MAX_EXP		2
#define MAX_EXP				5
#define INCR_DEFAULT		0		/* munge every incr bytes - zero means
									 * "adjust per ptext size" */
#define FEE_PASSWD_LEN		32		/* private data length in bytes, FEE only */


/*
 * Enumerate algs our own way to allow iteration.
 */
#define ALG_FEE_MD5			1
#define ALG_FEE_SHA1		2
#define ALG_ECDSA			3
#define ALG_ANSI_ECDSA		4
#define ALG_RSA				5
#define ALG_DSA				6
#define ALG_RAW_RSA_SHA1	7		
#define ALG_RAW_DSA_SHA1	8	
#define ALG_RSA_SHA224		9
#define ALG_RSA_SHA256		10
#define ALG_RSA_SHA384		11
#define ALG_RSA_SHA512		12
#define ALG_ECDSA_SHA256	13
#define ALG_ECDSA_SHA384	14
#define ALG_ECDSA_SHA512	15


#define ALG_FIRST			ALG_FEE_MD5
#define ALG_LAST			ALG_ECDSA_SHA512
#define MAX_DATA_SIZE		(100000 + 100)	/* bytes */

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=algorithm (f=FEE/MD5; F=FEE/SHA1; e=ECDSA; r=RSA; d=DSA; R=raw RSA; \n");
	printf("       D=raw DSA; 2=RSA/SHA224; 6=RSA/SHA256; 3=RSA/SHA384; 5=RSA/SHA512; default=all)\n");
	printf("       E=ECDSA/ANSI; 7=ECDSA/SHA256; 8=ECDSA/SHA384; 9=ECDSA/512; default=all\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   n=minExp (default=%d)\n", MIN_EXP);
	printf("   x=maxExp (default=%d, max=%d)\n", DEFAULT_MAX_EXP, MAX_EXP);
	printf("   k=keySize (r=random; default fixed per algorithm)\n");
	printf("   i=increment (default=%d)\n", INCR_DEFAULT);
	printf("   R(ef keys only)\n");
	printf("   p=pauseInterval (default=0, no pause)\n");
	printf("   P=primeType (m=Mersenne, f=FEE, g=general; FEE only)\n");
	printf("   C=curveType (m=Montgomery, w=Weierstrass, g=general; FEE only)\n");
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

#define LOG_FREQ	20

static int doTest(CSSM_CSP_HANDLE cspHand,
	CSSM_ALGORITHMS sigAlg,		// CSSM_ALGID_xxx signature algorithm
	CSSM_ALGORITHMS keyGenAlg,
	CSSM_DATA_PTR ptext,
	CSSM_BOOL verbose,
	CSSM_BOOL quiet,
	CSSM_BOOL randKeySize,
	uint32 keySize,
	unsigned incr,
	CSSM_BOOL pubIsRef,
	CSSM_KEYBLOB_FORMAT pubFormat,	
	CSSM_BOOL privIsRef,
	CSSM_KEYBLOB_FORMAT privFormat,
	CSSM_BOOL stagedSign,
	CSSM_BOOL stagedVerify,
	CSSM_BOOL genSeed,
	uint32 primeType,		// CSSM_FEE_PRIME_TYPE_xxx, FEE only
	uint32 curveType,		// CSSM_FEE_CURVE_TYPE_xxx, FEE only
	CSSM_BOOL genParams)	// DSA only
{
	CSSM_KEY		pubKey;
	CSSM_KEY		privKey;
	CSSM_DATA 		sig = {0, NULL};
	unsigned		length;
	unsigned		byte;
	unsigned char	*data;
	unsigned char	origData;
	unsigned char	bits;
	int				rtn = 0;
	CSSM_RETURN		crtn;
	unsigned		loop = 0;
	
	if(keyGenAlg == CSSM_ALGID_FEE) {
		uint8 			passwd[FEE_PASSWD_LEN];
		CSSM_DATA		pwdData = {FEE_PASSWD_LEN, passwd};
		CSSM_DATA_PTR	pwdDataPtr;

		if(randKeySize) {
			/* random params for this op */
			randFeeKeyParams(sigAlg, &keySize, &primeType, &curveType);
		}
		/* else use caller's size, primeType, curveType */

		if(genSeed) {
			simpleGenData(&pwdData, FEE_PASSWD_LEN, FEE_PASSWD_LEN);
			pwdDataPtr = &pwdData;
		}
		else {
			pwdDataPtr = NULL;
		}
		if(verbose) {
			printf("  key size %u  primeType %s  curveType %s\n", 
				(unsigned)keySize, primeTypeStr(primeType), curveTypeStr(curveType));
		}	/* verbose */
		rtn = cspGenFEEKeyPair(cspHand,
				USAGE_NAME,
				USAGE_NAME_LEN,
				keySize,
				primeType,
				curveType,
				&pubKey,
				pubIsRef,
				CSSM_KEYUSE_VERIFY,
				pubFormat,
				&privKey,
				privIsRef,
				CSSM_KEYUSE_SIGN,
				privFormat,
				pwdDataPtr);
	}	/* FEE */
	else {
		if(randKeySize) {
			keySize = randKeySizeBits(keyGenAlg, OT_Sign);
		}
		if(verbose) {
			printf("  key size %u\n", (unsigned)keySize);
		}
		if(keyGenAlg == CSSM_ALGID_DSA) {
			rtn = cspGenDSAKeyPair(cspHand,
				USAGE_NAME,
				USAGE_NAME_LEN,
				keySize,
				&pubKey,
				pubIsRef,
				CSSM_KEYUSE_VERIFY,
				pubFormat,
				&privKey,
				privIsRef,
				CSSM_KEYUSE_SIGN,
				privFormat,
				genParams,
				NULL);
		}
		else {
			rtn = cspGenKeyPair(cspHand,
				keyGenAlg,
				USAGE_NAME,
				USAGE_NAME_LEN,
				keySize,
				&pubKey,
				pubIsRef,
				CSSM_KEYUSE_VERIFY,
				pubFormat,
				&privKey,
				privIsRef,
				CSSM_KEYUSE_SIGN,
				privFormat,
				genSeed);
		}
	}
	if(rtn) {
		rtn = testError(quiet);
		goto abort;
	}
	if(stagedSign) {
		crtn = cspStagedSign(cspHand,
			sigAlg,
			&privKey,
			ptext,
			CSSM_TRUE,			// multi
			&sig);
	}
	else {
		crtn = cspSign(cspHand,
			sigAlg,
			&privKey,
			ptext,
			&sig);
	}
	if(crtn) {
		rtn = 1;
		goto abort;
	}
	if(stagedVerify) {
		crtn = cspStagedSigVerify(cspHand,
			sigAlg,
			&pubKey,
			ptext,
			&sig,
			CSSM_TRUE,			// multi
			CSSM_OK);
	}
	else {
		crtn = cspSigVerify(cspHand,
			sigAlg,
			&pubKey,
			ptext,
			&sig,
			CSSM_OK);
	}
	if(crtn) {
		printf("**Unexpected BAD signature\n");
		return testError(quiet);
	}
	data = (unsigned char *)ptext->Data;
	length = ptext->Length;
	for(byte=0; byte<length; byte += incr) {
		if(verbose && ((loop++ % LOG_FREQ) == 0)) {
			printf("  ..byte %d\n", byte);
		}
		origData = data[byte];
		/*
		 * Generate random non-zero byte
		 */
		do {
			bits = genRand(1, 0xff) & 0xff;
		} while(bits == 0);
		data[byte] ^= bits;
		if(stagedVerify) {
			crtn = cspStagedSigVerify(cspHand,
				sigAlg,
				&pubKey,
				ptext,
				&sig,
				CSSM_TRUE,			// multi
				CSSMERR_CSP_VERIFY_FAILED);		// expect failure
		}
		else {
			crtn = cspSigVerify(cspHand,
				sigAlg,
				&pubKey,
				ptext,
				&sig,
				CSSMERR_CSP_VERIFY_FAILED);
		}
		if(crtn) {
			return testError(quiet);
		}
		data[byte] = origData;
	}
abort:
	/* free/delete keys */
	if(cspFreeKey(cspHand, &privKey)) {
		printf("Error freeing privKey\n");
		rtn = 1;
	}
	if(cspFreeKey(cspHand, &pubKey)) {
		printf("Error freeing pubKey\n");
		rtn = 1;
	}
	CSSM_FREE(sig.Data);
	return rtn;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	CSSM_DATA			ptext;
	CSSM_CSP_HANDLE 	CSPHandle;
	CSSM_BOOL			pubIsRef = CSSM_TRUE;
	CSSM_BOOL			privIsRef = CSSM_TRUE;
	CSSM_BOOL			stagedSign;
	CSSM_BOOL 			stagedVfy;
	const char 			*algStr;
	unsigned			actualIncr;
	CSSM_ALGORITHMS		sigAlg;			// CSSM_ALGID_xxx
	CSSM_ALGORITHMS 	keyGenAlg;
	unsigned			currAlg;		// ALG_xxx
	int					i;
	int					rtn = 0;
	CSSM_BOOL			genSeed;		// for FEE
	CSSM_BOOL			genParams;		// for DSA
	CSSM_KEYBLOB_FORMAT	pubFormat = 0;
	CSSM_KEYBLOB_FORMAT privFormat = 0;
	const char			*pubFormStr = "none";
	const char			*privFormStr = "none";
	
	/*
	 * User-spec'd params
	 */
	unsigned	loops = LOOPS_DEF;
	CSSM_BOOL	verbose = CSSM_FALSE;
	unsigned	minExp = MIN_EXP;
	unsigned	maxExp = DEFAULT_MAX_EXP;
	CSSM_BOOL	quiet = CSSM_FALSE;
	CSSM_BOOL	randKeySize = CSSM_FALSE;
	uint32		keySizeInBits = CSP_KEY_SIZE_DEFAULT;
	unsigned	incr = INCR_DEFAULT;
	unsigned	minAlg = ALG_FIRST;
	uint32		maxAlg = ALG_LAST;
	unsigned	pauseInterval = 0;
	CSSM_BOOL	bareCsp = CSSM_TRUE;
	uint32		primeType = CSSM_FEE_PRIME_TYPE_DEFAULT;	// FEE only
	uint32		curveType = CSSM_FEE_CURVE_TYPE_DEFAULT;	// FEE only
	CSSM_BOOL	refKeysOnly = CSSM_FALSE;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case 'f':
						minAlg = maxAlg = ALG_FEE_MD5;
						break;
					case 'F':
						minAlg = maxAlg = ALG_FEE_SHA1;
						break;
					case 'e':
						minAlg = maxAlg = ALG_ECDSA;
						break;
					case 'E':
						minAlg = maxAlg = ALG_ANSI_ECDSA;
						break;
					case '7':
						minAlg = maxAlg = ALG_ECDSA_SHA256;
						break;
					case '8':
						minAlg = maxAlg = ALG_ECDSA_SHA384;
						break;
					case '9':
						minAlg = maxAlg = ALG_ECDSA_SHA512;
						break;
					case 'r':
						minAlg = maxAlg = ALG_RSA;
						break;
					case 'd':
						minAlg = maxAlg = ALG_DSA;
						break;
					case 'R':
						minAlg = maxAlg = ALG_RAW_RSA_SHA1;
						break;
					case 'D':
						minAlg = maxAlg = ALG_RAW_DSA_SHA1;
						break;
					case '2':
						minAlg = maxAlg = ALG_RSA_SHA224;
						break;
					case '6':
						minAlg = maxAlg = ALG_RSA_SHA256;
						break;
					case '3':
						minAlg = maxAlg = ALG_RSA_SHA384;
						break;
					case '5':
						minAlg = maxAlg = ALG_RSA_SHA512;
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
				if(argp[2] == 'r') {
					randKeySize = CSSM_TRUE;
				}
				else {
					keySizeInBits = atoi(&argp[2]);
				}
				break;
		    case 'i':
				incr = atoi(&argp[2]);
				break;
		    case 'p':
				pauseInterval = atoi(&argp[2]);
				break;
		    case 'R':
				refKeysOnly = CSSM_TRUE;
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
			case 'D':
				bareCsp = CSSM_FALSE;
				#if CSPDL_ALL_KEYS_ARE_REF
				refKeysOnly = CSSM_TRUE;
				#endif
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
	ptext.Data = (uint8 *)CSSM_MALLOC(MAX_DATA_SIZE);
	/* length set in test loop */
	if(ptext.Data == NULL) {
		printf("Insufficient heap\n");
		exit(1);
	}
	printf("Starting badsig; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	CSPHandle = cspDlDbStartup(bareCsp, NULL);
	if(CSPHandle == 0) {
		exit(1);
	}
	for(currAlg=minAlg; currAlg<=maxAlg; currAlg++) {
		switch(currAlg) {
			case ALG_FEE_MD5:
				sigAlg = CSSM_ALGID_FEE_MD5;
				algStr = "FEE/MD5";
				keyGenAlg = CSSM_ALGID_FEE;
				break;
			case ALG_FEE_SHA1:
				sigAlg = CSSM_ALGID_FEE_SHA1;
				algStr = "FEE/SHA1";
				keyGenAlg = CSSM_ALGID_FEE;
				break;
			case ALG_ECDSA:
				sigAlg = CSSM_ALGID_SHA1WithECDSA;
				algStr = "ECDSA";
				keyGenAlg = CSSM_ALGID_FEE;
				break;
			case ALG_ANSI_ECDSA:
				sigAlg = CSSM_ALGID_SHA1WithECDSA;
				algStr = "ANSI_ECDSA";
				keyGenAlg = CSSM_ALGID_ECDSA;
				break;
			case ALG_ECDSA_SHA256:
				sigAlg = CSSM_ALGID_SHA256WithECDSA;
				algStr = "ANSI_ECDSA_SHA256";
				keyGenAlg = CSSM_ALGID_ECDSA;
				break;
			case ALG_ECDSA_SHA384:
				sigAlg = CSSM_ALGID_SHA384WithECDSA;
				algStr = "ANSI_ECDSA_SHA384";
				keyGenAlg = CSSM_ALGID_ECDSA;
				break;
			case ALG_ECDSA_SHA512:
				sigAlg = CSSM_ALGID_SHA512WithECDSA;
				algStr = "ANSI_ECDSA_SHA512";
				keyGenAlg = CSSM_ALGID_ECDSA;
				break;
			case ALG_RSA:
				sigAlg = CSSM_ALGID_SHA1WithRSA;
				algStr = "RSA/SHA1";
				keyGenAlg = CSSM_ALGID_RSA;
				break;
			case ALG_DSA:
				sigAlg = CSSM_ALGID_SHA1WithDSA;
				algStr = "DSA";
				keyGenAlg = CSSM_ALGID_DSA;
				break;
			case ALG_RAW_RSA_SHA1:
				sigAlg = CSSM_ALGID_SHA1;
				algStr = "Raw RSA/SHA1";
				keyGenAlg = CSSM_ALGID_RSA;
				break;
			case ALG_RAW_DSA_SHA1:
				sigAlg = CSSM_ALGID_DSA;
				algStr = "Raw DSA/SHA1";
				keyGenAlg = CSSM_ALGID_DSA;
				break;
			case ALG_RSA_SHA224:
				sigAlg = CSSM_ALGID_SHA224WithRSA;
				algStr = "RSA/SHA224";
				keyGenAlg = CSSM_ALGID_RSA;
				break;
			case ALG_RSA_SHA256:
				sigAlg = CSSM_ALGID_SHA256WithRSA;
				algStr = "RSA/SHA256";
				keyGenAlg = CSSM_ALGID_RSA;
				break;
			case ALG_RSA_SHA384:
				sigAlg = CSSM_ALGID_SHA384WithRSA;
				algStr = "RSA/SHA384";
				keyGenAlg = CSSM_ALGID_RSA;
				break;
			case ALG_RSA_SHA512:
				sigAlg = CSSM_ALGID_SHA512WithRSA;
				algStr = "RSA/SHA512";
				keyGenAlg = CSSM_ALGID_RSA;
				break;
			default:
				printf("***BRRZAP! alg parsing needs work.\n");
				exit(1);
		}
		if(!quiet) {
			printf("Testing alg %s\n", algStr);
		}
		for(loop=1; ; loop++) {
			ptext.Length = genData(ptext.Data, minExp, maxExp, DT_Random);
			if(!quiet) {
				printf("..loop %d text size %lu\n", loop, ptext.Length);
			}
			if(incr == 0) {
				/* adjust increment as appropriate */
				actualIncr = (ptext.Length / 50) + 1;
			}
			else {
				actualIncr = incr;
			}
			/* mix up some ref and data keys, as well as staging */
			if(!refKeysOnly) {
				pubIsRef   = (loop & 1) ? CSSM_TRUE : CSSM_FALSE;
				privIsRef  = (loop & 2) ? CSSM_TRUE : CSSM_FALSE;
			}
			
			/* variable alg-specific params */
			switch(currAlg) {
				case ALG_RAW_RSA_SHA1:
				case ALG_RAW_DSA_SHA1:
					/* staging not supported */
					stagedSign = 0;
					stagedVfy = 0;
					break;
				default:
					stagedSign = (loop & 4) ? CSSM_TRUE : CSSM_FALSE;
					stagedVfy  = (loop & 8) ? CSSM_TRUE : CSSM_FALSE;
					break;
			}
			genSeed = CSSM_FALSE;
			switch(currAlg) {
				case ALG_FEE_MD5:
				case ALG_FEE_SHA1:
				case ALG_ECDSA:
					/* FEE keys */
					genSeed = (ptext.Data[0] & 1) ? CSSM_TRUE : CSSM_FALSE;
					break;
				case ALG_DSA:
				case ALG_RAW_DSA_SHA1:
					/* DSA */
					if(bareCsp || CSPDL_DSA_GEN_PARAMS) {
						/* alternate this one */
						genParams = (ptext.Data[0] & 2) ? CSSM_TRUE : CSSM_FALSE;;
					}
					else {
						/* CSPDL - no gen params */
						genParams = CSSM_FALSE;
					}
					break;
				default:
					break;
			}
			
			/* random raw key formats */
			pubFormat = CSSM_KEYBLOB_RAW_FORMAT_NONE;
			pubFormStr = "None";
			if(!pubIsRef) {
				unsigned die;
				switch(keyGenAlg) {
					case CSSM_ALGID_FEE:
						/* NONE, OCTET_STRING */
						die = genRand(1,2);
						if(die == 2) {
							pubFormat = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
							pubFormStr = "OctetString";
						}
						break;
					case CSSM_ALGID_RSA:
						/* NONE, PKCS1, X509, OPENSSH, OPENSSHv2 */
						die = genRand(1, 5);
						switch(die) {
							case 1:
								break;	// none/default, set above */
							case 2:
								pubFormat = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
								pubFormStr = "PKCS1";
								break;
							case 3:
								pubFormat = CSSM_KEYBLOB_RAW_FORMAT_X509;
								pubFormStr = "X509";
								break;
							case 4:
								pubFormat = CSSM_KEYBLOB_RAW_FORMAT_OPENSSH;
								pubFormStr = "SSH1";
								break;
							case 5:
								pubFormat = CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2;
								pubFormStr = "SSH2";
								break;
						}
						break;
					case CSSM_ALGID_DSA:
						/* NONE, X509, FIPS186, OPENSSHv2 */
						die = genRand(1, 4);
						switch(die) {
							case 1:
								break;	// none/default, set above */
							case 2:
								pubFormat = CSSM_KEYBLOB_RAW_FORMAT_X509;
								pubFormStr = "X509";
								break;
							case 3:
								pubFormat = CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
								pubFormStr = "FIPS186";
								break;
							case 4:
								pubFormat = CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2;
								pubFormStr = "SSH2";
								break;
						}
						break;
					case CSSM_ALGID_ECDSA:
						/* no options - default is CSSM_KEYBLOB_RAW_FORMAT_NONE, X509 */
						pubFormStr = "X509";
						break;
					default:
						printf("***BRRRZAP! Key alg processing needed\n");
						exit(1);
				}
			}
			privFormat = CSSM_KEYBLOB_RAW_FORMAT_NONE;
			privFormStr = "None";
			if(!privIsRef) {
				unsigned die;
				switch(keyGenAlg) {
					case CSSM_ALGID_FEE:
						/* NONE, OCTET_STRING */
						die = genRand(1,2);
						if(die == 2) {
							privFormat = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
							privFormStr = "OctetString";
						}
						break;
					case CSSM_ALGID_RSA:
						/* NONE, PKCS1, PKCS8, OPENSSH */
						die = genRand(1, 4);
						switch(die) {
							case 1:
								break;	// none/default, set above */
							case 2:
								privFormat = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
								privFormStr = "PKCS1";
								break;
							case 3:
								privFormat = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
								privFormStr = "PKCS8";
								break;
							case 4:
								privFormat = CSSM_KEYBLOB_RAW_FORMAT_OPENSSH;
								privFormStr = "SSH1";
								break;
						}
						break;
					case CSSM_ALGID_DSA:
						/* NONE, FIPS186, PKCS8 */
						die = genRand(1, 3);
						switch(die) {
							case 1:
								break;	// none/default, set above */
							case 2:
								privFormat = CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
								privFormStr = "FIPS186";
								break;
							case 3:
								privFormat = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
								privFormStr = "PKCS8";
								break;
						}
						break;
					case CSSM_ALGID_ECDSA:
						/* no options */
						privFormStr = "PKCS8";
						break;
					default:
						printf("***BRRRZAP! Key alg processing needed\n");
						exit(1);
				}
			}

			if(!quiet) {
				printf("  pubIsRef %d  pubForm %s  privIsRef %d  privForm %s  stagedSign %d  stagedVfy %d"
					" genSeed %d\n",
					 (int)pubIsRef, pubFormStr, (int)privIsRef, privFormStr, 
					 (int)stagedSign, (int)stagedVfy, (int)genSeed);
			}
			
			if(doTest(CSPHandle,
					sigAlg,
					keyGenAlg,
					&ptext,
					verbose,
					quiet,
					randKeySize,
					keySizeInBits,
					actualIncr,
					pubIsRef,
					pubFormat,
					privIsRef,
					privFormat,
					stagedSign,
					stagedVfy,
					genSeed,
					primeType,
					curveType,
					genParams)) {
				rtn = 1;
				goto testDone;
			}
			if(loops && (loop == loops)) {
				break;
			}
			if(pauseInterval && ((loop % pauseInterval) == 0)) {
				char inch;
				fpurge(stdin);
				printf("Hit CR to proceed or q to quit: ");
				inch = getchar();
				if(inch == 'q') {
					goto testDone;
				}
			}
		}	/* for loop */
	}		/* for alg */
testDone:
	CSSM_ModuleDetach(CSPHandle);
	if((rtn == 0) && !quiet) {
		printf("%s test complete\n", argv[0]);
	}
	return rtn;
}
