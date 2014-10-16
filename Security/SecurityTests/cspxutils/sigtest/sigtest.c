/*
 * Tool to cook up key pair, sign & verify, any which way
 * with tons of options. 
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include "cspwrap.h"
#include "common.h"
#include <security_cdsa_utils/cuFileIo.h>

#define DATA_SIZE_DEF	100
#define USAGE_DEF		"noUsage"
#define LOOPS_DEF		10
#define FEE_PASSWD_LEN	32		/* private data length in bytes, FEE only */

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  a=algorithm\n");
	printf("     f=FEE/MD5 F=FEE/SHA1 e=ECDSA r=RSA/MD5 2=RSA/MD2 \n");
	printf("     s=RSA/SHA1 d=DSA R=rawRSA (No digest) \n");
	printf("     4=RSA/SHA224 6=RSA/SHA256 3=RSA/SHA384 5=RSA/SHA512;\n");
	printf("     E=ECDSA/ANSI; 7=ECDSA/SHA256; 8=ECDSA/SHA384; 9=ECDSA/512; default=RSA/SHA1\n");
	printf("  d=dataSize (default = %d)\n", DATA_SIZE_DEF);
	printf("  k=keySize\n");
	printf("  b (pub is blob)\n");
	printf("  r (priv is blob)\n");
	printf("  B=[1xboOt] (pub  key in PKCS1/X509/BSAFE/OpenSSH1/OpenSSH2/Octet form)\n");
	printf("          RSA = {PKCS1,X509,OpenSSH1,OpenSSH2}     default = PKCS1\n");
	printf("          DSA = {BSAFE,X509,OpenSSH2}              default = X509\n");
	printf("        ECDSA = {X509, Only!}                      default = X509\n");
	printf("  P=primeType (m=Mersenne, f=FEE, g=general; FEE only)\n");
	printf("  C=curveType (m=Montgomery, w=Weierstrass, a=ANSI; FEE only)\n");
	printf("  l=loops (0=forever)\n");
	printf("  s(ign only)\n");
	printf("  V(erify only)\n");
	printf("  c(ontexts only)\n");
	printf("  S (we generate seed, for FEE only)\n");
	printf("  n(o padding; default is PKCS1)\n");
	printf("  p=pauseInterval (default=0, no pause)\n");
	printf("  D (CSP/DL; default = bare CSP)\n");
	printf("  L (dump key and signature blobs)\n");
	printf("  o (key blobs in OCTET_STRING format)\n");
	printf("  q(uiet)\n");
	printf("  v(erbose))\n");
	exit(1);
}

/* parse public key format character */
static CSSM_KEYBLOB_FORMAT parsePubKeyFormat(char c, char **argv)
{
	switch(c) {
		case '1':
			return CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
		case 'x':
			return CSSM_KEYBLOB_RAW_FORMAT_X509;
		case 'b':
			return CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
		case 'o':
			return CSSM_KEYBLOB_RAW_FORMAT_OPENSSH;
		case 'O':
			return CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2;
		default:
			usage(argv);
	}
	/* not reached */
	return -1;
}

int main(int argc, char **argv)
{
	int		 				arg;
	char					*argp;
	CSSM_CSP_HANDLE 		cspHand;
	CSSM_CC_HANDLE			pubSigHand;
	CSSM_CC_HANDLE			privSigHand;
	CSSM_RETURN				crtn;
	CSSM_DATA				randData = {0, NULL};
	CSSM_KEY				privKey;
	CSSM_KEY				pubKey;
	CSSM_DATA				sigData = {0, NULL};
	unsigned				loop;
	int 					i;
	unsigned 				dataSize = DATA_SIZE_DEF;
	unsigned				keySize = CSP_KEY_SIZE_DEFAULT;
	uint32					sigAlg = CSSM_ALGID_SHA1WithRSA;
	uint32					keyGenAlg = CSSM_ALGID_RSA;
	unsigned				pauseInterval = 0;
	unsigned				loops = LOOPS_DEF;
	CSSM_BOOL				quiet = CSSM_FALSE;
	CSSM_BOOL				pubIsRef = CSSM_TRUE;
	CSSM_BOOL				privIsRef = CSSM_TRUE;
	CSSM_BOOL				doSign = CSSM_TRUE;
	CSSM_BOOL				doVerify = CSSM_TRUE;
	CSSM_BOOL				contextsOnly = CSSM_FALSE;
	CSSM_BOOL				noPadding = CSSM_FALSE;
	CSSM_BOOL				verbose = CSSM_FALSE;
	CSSM_BOOL				bareCsp = CSSM_TRUE;
	CSSM_BOOL				genSeed = CSSM_FALSE;
	CSSM_BOOL				dumpBlobs = CSSM_FALSE;
	CSSM_KEYBLOB_FORMAT		privKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_NONE;
	CSSM_KEYBLOB_FORMAT		pubKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_NONE;
	uint32					primeType = CSSM_FEE_PRIME_TYPE_DEFAULT;	// FEE only
	uint32					curveType = CSSM_FEE_CURVE_TYPE_DEFAULT;	// FEE only
	
	for(arg=1; arg<argc; arg++) { 
		argp = argv[arg];
	    switch(argv[arg][0]) {
	    	case 'a':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case 'f':
						sigAlg    = CSSM_ALGID_FEE_MD5;
						keyGenAlg = CSSM_ALGID_FEE;
						break;
					case 'F':
						sigAlg    = CSSM_ALGID_FEE_SHA1;
						keyGenAlg = CSSM_ALGID_FEE;
						break;
					case 'e':
						sigAlg    = CSSM_ALGID_SHA1WithECDSA;
						keyGenAlg = CSSM_ALGID_FEE;
						break;
					case 'E':
						sigAlg    = CSSM_ALGID_SHA1WithECDSA;
						keyGenAlg = CSSM_ALGID_ECDSA;
						break;
					case '7':
						sigAlg    = CSSM_ALGID_SHA256WithECDSA;
						keyGenAlg = CSSM_ALGID_ECDSA;
						break;
					case '8':
						sigAlg    = CSSM_ALGID_SHA384WithECDSA;
						keyGenAlg = CSSM_ALGID_ECDSA;
						break;
					case '9':
						sigAlg    = CSSM_ALGID_SHA512WithECDSA;
						keyGenAlg = CSSM_ALGID_ECDSA;
						break;
					case 'r':
						sigAlg    = CSSM_ALGID_MD5WithRSA;
						keyGenAlg = CSSM_ALGID_RSA;
						break;
					case '2':
						sigAlg    = CSSM_ALGID_MD2WithRSA;
						keyGenAlg = CSSM_ALGID_RSA;
						break;
					case 's':
						sigAlg    = CSSM_ALGID_SHA1WithRSA;
						keyGenAlg = CSSM_ALGID_RSA;
						break;
					case 'd':
						sigAlg    = CSSM_ALGID_SHA1WithDSA;
						keyGenAlg = CSSM_ALGID_DSA;
						break;
					case 'R':
						sigAlg    = CSSM_ALGID_RSA;
						keyGenAlg = CSSM_ALGID_RSA;
						break;
					case '4':
						sigAlg    = CSSM_ALGID_SHA224WithRSA;
						keyGenAlg = CSSM_ALGID_RSA;
						break;
					case '6':
						sigAlg    = CSSM_ALGID_SHA256WithRSA;
						keyGenAlg = CSSM_ALGID_RSA;
						break;
					case '3':
						sigAlg    = CSSM_ALGID_SHA384WithRSA;
						keyGenAlg = CSSM_ALGID_RSA;
						break;
					case '5':
						sigAlg    = CSSM_ALGID_SHA512WithRSA;
						keyGenAlg = CSSM_ALGID_RSA;
						break;
					default:
						usage(argv);
				}
				break;
			case 'd':
				dataSize = atoi(&argv[arg][2]);
				break;
			case 'k':
				keySize = atoi(&argv[arg][2]);
				break;
		    case 'l':
				loops = atoi(&argv[arg][2]);
				break;
		    case 'p':
				pauseInterval = atoi(&argv[arg][2]);
				break;
			case 'b':
				pubIsRef = CSSM_FALSE;
				break;
			case 'r':
				privIsRef = CSSM_FALSE;
				break;
			case 'B':
				if(argp[1] != '=') {
					usage(argv);
				}
				pubKeyFormat = parsePubKeyFormat(argp[2], argv);
				break;
			case 's':
				doVerify = CSSM_FALSE;
				break;
			case 'V':
				doSign = CSSM_FALSE;
				break;
			case 'S':
				genSeed = CSSM_TRUE;
				break;
			case 'n':
				noPadding = CSSM_TRUE;
				break;
			case 'L':
				dumpBlobs = CSSM_TRUE;
				break;
			case 'c':
				contextsOnly = CSSM_TRUE;
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
		    case 'o':
				/* this is for FEE only */
		    	pubKeyFormat = privKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
				break;
			case 'C':
				switch(argp[2]) {
					case 'm':
						curveType = CSSM_FEE_CURVE_TYPE_MONTGOMERY;
						break;
					case 'w':
						curveType = CSSM_FEE_CURVE_TYPE_WEIERSTRASS;
						break;
					case 'a':
						curveType = CSSM_FEE_CURVE_TYPE_ANSI_X9_62;
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
			default:
				usage(argv);
		}
	}
	if(!doSign && !doVerify) {
		printf("s and v mutually exclusive\n");
		exit(1);
	}
	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	printf("Starting sigtest; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
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
		crtn = cspGenFEEKeyPair(cspHand,
				USAGE_DEF,
				strlen(USAGE_DEF),
				keySize,
				primeType,
				curveType,
				&pubKey,
				pubIsRef,
				CSSM_KEYUSE_VERIFY,
				pubKeyFormat,
				&privKey,
				privIsRef,
				CSSM_KEYUSE_SIGN,
				CSSM_KEYBLOB_RAW_FORMAT_NONE,
				pwdDataPtr);
	
	}
	else if((keyGenAlg == CSSM_ALGID_DSA) && !bareCsp) {
		/* CSPDL doesn't do gen alg params */
		crtn = cspGenDSAKeyPair(cspHand,
				USAGE_DEF,
				strlen(USAGE_DEF),
				keySize,
				&pubKey,
				pubIsRef,
				CSSM_KEYUSE_VERIFY,
				pubKeyFormat,
				&privKey,
				privIsRef,
				CSSM_KEYUSE_SIGN,
				CSSM_KEYBLOB_RAW_FORMAT_NONE,
				CSSM_FALSE,
				NULL);
	}
	else {
		crtn = cspGenKeyPair(cspHand,
			keyGenAlg,
			USAGE_DEF,
			strlen(USAGE_DEF),
			keySize,
			&pubKey,
			pubIsRef,	
			CSSM_KEYUSE_VERIFY,
			pubKeyFormat,
			&privKey,
			privIsRef,
			CSSM_KEYUSE_SIGN,
			privKeyFormat,
			genSeed);
	}
	if(crtn) {
		CSSM_ModuleDetach(cspHand);
		exit(1);
	}
	if(dumpBlobs) {
		if(!pubIsRef) {
			writeFile("pubKey.blob", pubKey.KeyData.Data, pubKey.KeyData.Length);
			printf("...wrote %lu bytes to pubKey.blob\n", pubKey.KeyData.Length);
		}
		if(!privIsRef) {
			writeFile("privKey.blob", privKey.KeyData.Data, privKey.KeyData.Length);
			printf("...wrote %lu bytes to privKey.blob\n", privKey.KeyData.Length);
		}
	}
	randData.Data = (uint8 *)CSSM_MALLOC(dataSize);
	randData.Length = dataSize;
	simpleGenData(&randData, dataSize, dataSize);
	printf("\n");
	for(loop=1; ; loop++) {
		if(!quiet) {
			printf("...Loop %d\n", loop);
		}
		if((loop == 1) || doSign) {
			crtn = CSSM_CSP_CreateSignatureContext(cspHand,
				sigAlg,
				NULL,				// passPhrase
				&privKey,
				&privSigHand);
			if(crtn) {
				printError("CSSM_CSP_CreateSignatureContext (1)", crtn);
				break;
			}
			if(noPadding) {
				crtn = AddContextAttribute(privSigHand,
					CSSM_ATTRIBUTE_PADDING,
					sizeof(uint32),
					CAT_Uint32,
					NULL,
					CSSM_PADDING_NONE);
				if(crtn) {
					break;
				}
			}
			if(!contextsOnly) {
				crtn = CSSM_SignData(privSigHand,
					&randData,
					1,
					CSSM_ALGID_NONE,
					&sigData);
				if(crtn) {
					printError("CSSM_SignData error", crtn);
					break;
				}
			}
			crtn = CSSM_DeleteContext(privSigHand);
			if(crtn) {
				printError("CSSM_DeleteContext", crtn);
				break;
			}
			if(dumpBlobs) {
				writeFile("sig.blob", sigData.Data, sigData.Length);
				printf("...wrote %lu bytes to sig.blob\n", sigData.Length);
			}
		}
		if(doVerify) {
			crtn = CSSM_CSP_CreateSignatureContext(cspHand,
				sigAlg,
				NULL,				// passPhrase
				&pubKey,
				&pubSigHand);
			if(crtn) {
				printError("CSSM_CSP_CreateSignatureContext (2)", crtn);
				break;
			}
			if(noPadding) {
				crtn = AddContextAttribute(pubSigHand,
					CSSM_ATTRIBUTE_PADDING,
					sizeof(uint32),
					CAT_Uint32,
					NULL,
					CSSM_PADDING_NONE);
				if(crtn) {
					break;
				}
			}
			if(!contextsOnly) {
				crtn = CSSM_VerifyData(pubSigHand,
					&randData,
					1,
					CSSM_ALGID_NONE,
					&sigData);
				if(crtn) {
					printError("CSSM_VerifyData", crtn);
					break;
				}
			}
			crtn = CSSM_DeleteContext(pubSigHand);
			if(crtn) {
				printError("CSSM_DeleteContext", crtn);
				break;
			}
		}
		if(doSign & !contextsOnly) {
			CSSM_FREE(sigData.Data);
			sigData.Length = 0;
			sigData.Data = NULL;
		}
		/* else keep it around for next verify */
				
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
	if(randData.Data != NULL) {
		CSSM_FREE(randData.Data);
	}
	if(CSSM_ModuleDetach(cspHand)) {
		printError("CSSM_CSP_Detach", crtn);
		exit(1);
	}
	if(crtn == CSSM_OK) {
		if(!quiet) {
			printf("OK\n");
		}
	}
	return crtn;
}
