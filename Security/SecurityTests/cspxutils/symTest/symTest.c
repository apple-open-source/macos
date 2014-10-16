/* Copyright (c) 1998,2003-2006,2008 Apple Inc.
 *
 * symTest.c - test CSP symmetric encrypt/decrypt.
 *
 * Revision History
 * ----------------
 *   4 May 2000 Doug Mitchell
 *		Ported to X/CDSA2. 
 *  20 May 1998 Doug Mitchell at Apple
 *		Ported to CDSA1.2, new Apple CSP
 *  15 Aug 1997	Doug Mitchell at Apple
 *		Ported from CryptKit ObjC version
 *  26 Aug 1996	Doug Mitchell at NeXT
 *		Created.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include "cspwrap.h"
#include "common.h"
#include "cspdlTesting.h"

/*
 * Defaults.
 */
#define LOOPS_DEF		50
#define MIN_PTEXT_SIZE	8
#define MAX_PTEXT_SIZE	0x10000

/*
 * Enumerate algs our own way to allow iteration.
 */
typedef enum {
	ALG_ASC = 1,
	ALG_DES,
	ALG_RC2,
	ALG_RC4,
	ALG_RC5,
	ALG_3DES,
	ALG_AES,
	ALG_BFISH,
	ALG_CAST,
	ALG_NULL					/* normally not used */
} SymAlg;
#define ALG_FIRST			ALG_ASC
#define ALG_LAST			ALG_CAST

#define PBE_ENABLE			0
#define PWD_LENGTH_MAX		64
#define MAX_DATA_SIZE		(100000 + 100)	/* bytes */
#define LOOP_NOTIFY			20

#define LOG_SIZE			0
#if		LOG_SIZE
#define logSize(s)	printf s
#else
#define logSize(s)
#endif

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   a=algorithm (s=ASC; d=DES; 3=3DES; 2=RC2; 4=RC4; 5=RC5; a=AES;\n"); 
	printf("                b=Blowfish; c=CAST; n=Null; default=all)\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   n=minPtextSize (default=%d)\n", MIN_PTEXT_SIZE);
	printf("   x=maxPtextSize (default=%d)\n", MAX_PTEXT_SIZE);
	printf("   k=keySizeInBits\n");
	printf("   r(eference keys only)\n");
	printf("   e(xport)\n");
	printf("   d (no DB open)\n");
	printf("   p=pauseInterval (default=0, no pause)\n");
	printf("   o (no padding, well-aligned plaintext)\n");
	printf("   u (no multi-update ops)\n");
	printf("   U (only multi-update ops)\n");
	printf("   m (CSP mallocs out bufs)\n");
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   K (key gen only)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

/* constant seed data */
static CSSM_DATA seedData = {8, (uint8 *)"12345678"};

/* alternate between two derivation algs, with different digest sizes */
#define PBE_DERIVE_ALG_ODD	CSSM_ALGID_PKCS5_PBKDF1_MD5
#define PBE_DERIVE_ALG_EVEN	CSSM_ALGID_PKCS5_PBKDF1_SHA1

/*
 * When expectEqualText is true, encrypt/decrypt in place. 
 */
#define EQUAL_TEXT_IN_PLACE		1

static int doTest(CSSM_CSP_HANDLE cspHand,
	CSSM_DATA_PTR ptext,
	uint32 keyAlg,						// CSSM_ALGID_xxx of the key
	uint32 encrAlg,						// encrypt/decrypt
	uint32 mode,
	uint32 padding,
	uint32 effectiveKeySizeInBits,
	CSSM_BOOL refKey,
	CSSM_DATA_PTR pwd,				// password- NULL means use a random key data
	CSSM_BOOL stagedEncr,
	CSSM_BOOL stagedDecr,
	CSSM_BOOL mallocPtext,			// only meaningful if !stagedDecr
	CSSM_BOOL mallocCtext,			// only meaningful if !stagedEncr
	CSSM_BOOL quiet,
	CSSM_BOOL keyGenOnly,
	CSSM_BOOL expectEqualText)		// ptext size must == ctext size
{
	CSSM_KEY_PTR	symKey = NULL;
	CSSM_DATA 		ctext = {0, NULL};
	CSSM_DATA		rptext = {0, NULL};
	CSSM_RETURN		crtn;
	int				rtn = 0;
	uint32			keySizeInBits;
	CSSM_DATA 		initVector;
	uint32			rounds = 0;
	
	/* generate keys with well aligned sizes; effectiveKeySize specified in encrypt
	 * only if not well aligned */
	keySizeInBits = (effectiveKeySizeInBits + 7) & ~7;
	if(keySizeInBits == effectiveKeySizeInBits) {
		effectiveKeySizeInBits = 0;
	}
	
	if(encrAlg == CSSM_ALGID_RC5) {
		/* roll the dice, pick one of three values for rounds */
		unsigned die = genRand(1,3);
		switch(die) {
			case 1:
				rounds = 8;
				break;
			case 2:
				rounds = 12;
				break;
			case 3:
				rounds = 16;
				break;
		}
	}

	if(pwd == NULL) {
		/* random key */
		symKey = cspGenSymKey(cspHand,
				keyAlg,
				"noLabel",
				7,
				CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
				keySizeInBits,
				refKey);
	}
	else {
		/* this code isn't tested */
		uint32	  pbeAlg;
		initVector.Data = NULL;		// we're going to ignore this
		initVector.Length = 0;
		/* one of two random PBE algs */
		if(ptext->Data[0] & 1) {
			pbeAlg = PBE_DERIVE_ALG_ODD;
		}
		else {
			pbeAlg = PBE_DERIVE_ALG_EVEN;
		}
		symKey = cspDeriveKey(cspHand,
			pbeAlg,
			keyAlg,
			"noLabel",
			7,
			CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
			keySizeInBits,
			refKey, 
			pwd,
			&seedData,
			1,			// iteration count
			&initVector);
		if(initVector.Data != NULL) {
			CSSM_FREE(initVector.Data);
		}
	}
	if(symKey == NULL) {
		rtn = testError(quiet);
		goto abort;
	}
	if(keyGenOnly) {
		rtn = 0;
		goto abort;
	}
	
	/* not all algs need this, pass it in anyway */
	initVector.Data = (uint8 *)"someStrangeInitVect";
	switch(encrAlg) {
		case CSSM_ALGID_AES:
		case CSSM_ALGID_NONE:
			initVector.Length = 16;
			break;
		default:
			initVector.Length = 8;
			break;
	}
	if(stagedEncr) {
		crtn = cspStagedEncrypt(cspHand,
			encrAlg,
			mode,
			padding,
			symKey,
			NULL,		// second key unused
			effectiveKeySizeInBits,
			0,			// cipherBlockSize
			rounds,
			&initVector,
			ptext,
			&ctext,
			CSSM_TRUE);	// multi
	}
	else {
		const CSSM_DATA *ptextPtr = ptext;
		if(expectEqualText && mallocCtext && CSPDL_NOPAD_ENFORCE_SIZE) {
			/* 
			 * !pad test: ensure this works when ctextlen == ptextlen by 
			 * mallocing ourself right now (instead of cspEncrypt doing it 
			 * after doing a CSSM_QuerySize())
			 */
			ctext.Data = (uint8 *)appMalloc(ptext->Length, NULL);
			if(ctext.Data == NULL) {
				printf("memmory failure\n");
				rtn = testError(quiet);
				goto abort;
			}
			ctext.Length = ptext->Length;
			#if	EQUAL_TEXT_IN_PLACE
			/* encrypt in place */
			memmove(ctext.Data, ptext->Data, ptext->Length);
			ptextPtr = &ctext;
			#endif
		}
		crtn = cspEncrypt(cspHand,
			encrAlg,
			mode,
			padding,
			symKey,
			NULL,		// second key unused
			effectiveKeySizeInBits,
			rounds,
			&initVector,
			ptextPtr,
			&ctext,
			mallocCtext);
	}
	if(crtn) {
		rtn = testError(quiet);
		goto abort;
	}
	if(expectEqualText && (ptext->Length != ctext.Length)) {
		printf("***ctext/ptext length mismatch: ptextLen %lu  ctextLen %lu\n",
			ptext->Length, ctext.Length);
		rtn = testError(quiet);
		if(rtn) {
			goto abort;
		}
	}
	logSize(("###ctext len %lu\n", ctext.Length)); 
	if(stagedDecr) {
		crtn = cspStagedDecrypt(cspHand,
			encrAlg,
			mode,
			padding,
			symKey,
			NULL,		// second key unused
			effectiveKeySizeInBits,
			0,			// cipherBlockSize
			rounds,
			&initVector,
			&ctext,
			&rptext,
			CSSM_TRUE);	// multi
	}
	else {
		const CSSM_DATA *ctextPtr = &ctext;
		if(expectEqualText && mallocPtext && CSPDL_NOPAD_ENFORCE_SIZE) {
			/* 
			 * !pad test: ensure this works when ctextlen == ptextlen by 
			 * mallocing ourself right now (instead of cspDecrypt doing it 
			 * after doing a CSSM_QuerySize())
			 */
			rptext.Data = (uint8 *)appMalloc(ctext.Length, NULL);
			if(rptext.Data == NULL) {
				printf("memmory failure\n");
				rtn = testError(quiet);
				goto abort;
			}
			rptext.Length = ctext.Length;
			#if	EQUAL_TEXT_IN_PLACE
			/* decrypt in place */
			memmove(rptext.Data, ctext.Data, ctext.Length);
			ctextPtr = &rptext;
			#endif
		}
		crtn = cspDecrypt(cspHand,
			encrAlg,
			mode,
			padding,
			symKey,
			NULL,		// second key unused
			effectiveKeySizeInBits,
			rounds,
			&initVector,
			ctextPtr,
			&rptext,
			mallocPtext);
	}
	if(crtn) {
		rtn = testError(quiet);
		goto abort;
	}
	logSize(("###rptext len %lu\n", rptext.Length)); 
	/* compare ptext, rptext */
	if(ptext->Length != rptext.Length) {
		printf("Ptext length mismatch: expect %lu, got %lu\n", ptext->Length, rptext.Length);
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
	/* free key if we have it*/
	if(symKey != NULL) {
		if(cspFreeKey(cspHand, symKey)) {
			printf("Error freeing privKey\n");
			rtn = 1;
		}
		CSSM_FREE(symKey);
	}
	/* free rptext, ctext */
	appFreeCssmData(&rptext, CSSM_FALSE);
	appFreeCssmData(&ctext, CSSM_FALSE);
	return rtn;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	CSSM_DATA			ptext;
	CSSM_CSP_HANDLE 	cspHand;
	CSSM_BOOL			stagedEncr;
	CSSM_BOOL 			stagedDecr;
	CSSM_BOOL			mallocCtext;
	CSSM_BOOL			mallocPtext;
	CSSM_BOOL			refKey;
	const char			*algStr;
	uint32				keyAlg;			// CSSM_ALGID_xxx of the key
	uint32				encrAlg;		// CSSM_ALGID_xxx of the encrypt/decrypt/sign
	int					i;
	int					currAlg;		// ALG_xxx
	CSSM_DATA_PTR		pPwd;
	CSSM_DATA			pwd;
	uint32				actKeySizeInBits;
	int					rtn = 0;
	uint32				blockSize;		// for noPadding case
	CSSM_BOOL 			expectEqualText;
	
	/*
	 * User-spec'd params
	 */
	CSSM_BOOL	keySizeSpec = CSSM_FALSE;		// false: use rand key size
	SymAlg		minAlg = ALG_FIRST;
	SymAlg		maxAlg = ALG_LAST;
	unsigned	loops = LOOPS_DEF;
	CSSM_BOOL	verbose = CSSM_FALSE;
	unsigned	minPtextSize = MIN_PTEXT_SIZE;
	unsigned	maxPtextSize = MAX_PTEXT_SIZE;
	CSSM_BOOL	quiet = CSSM_FALSE;
	unsigned	pauseInterval = 0;
	uint32		mode;
	uint32		padding;
	CSSM_BOOL	noDbOpen = CSSM_FALSE;
	CSSM_BOOL	bareCsp = CSSM_TRUE;
	CSSM_BOOL 	keyGenOnly = CSSM_FALSE;
	CSSM_BOOL	noPadding = CSSM_FALSE;
	CSSM_BOOL	multiEnable = CSSM_TRUE;
	CSSM_BOOL	multiOnly = CSSM_FALSE;
	CSSM_BOOL	refKeysOnly = CSSM_FALSE;
	CSSM_BOOL	cspMallocs = CSSM_FALSE;
	
	#if	macintosh
	argc = ccommand(&argv);
	#endif
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case 's':
						minAlg = maxAlg = ALG_ASC;
						break;
					case 'd':
						minAlg = maxAlg = ALG_DES;
						break;
					case '3':
						minAlg = maxAlg = ALG_3DES;
						break;
					case '2':
						minAlg = maxAlg = ALG_RC2;
						break;
					case '4':
						minAlg = maxAlg = ALG_RC4;
						break;
					case '5':
						minAlg = maxAlg = ALG_RC5;
						break;
					case 'a':
						minAlg = maxAlg = ALG_AES;
						break;
					case 'b':
						minAlg = maxAlg = ALG_BFISH;
						break;
					case 'c':
						minAlg = maxAlg = ALG_CAST;
						break;
					case 'n':
						minAlg = maxAlg = ALG_NULL;
						break;
					default:
						usage(argv);
				}
				break;
		    case 'l':
				loops = atoi(&argp[2]);
				break;
		    case 'n':
				minPtextSize = atoi(&argp[2]);
				break;
		    case 'x':
				maxPtextSize = atoi(&argp[2]);
				break;
			case 'r':
				refKeysOnly = CSSM_TRUE;
				break;
		    case 'k':
		    	actKeySizeInBits = atoi(&argp[2]);
		    	keySizeSpec = CSSM_TRUE;
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
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
		    case 'p':
		    	pauseInterval = atoi(&argp[2]);;
				break;
			case 'o':
				noPadding = CSSM_TRUE;
				break;
		    case 'd':
		    	noDbOpen = CSSM_TRUE;
				break;
		    case 'K':
		    	keyGenOnly = CSSM_TRUE;
				break;
		    case 'u':
		    	multiEnable = CSSM_FALSE;
				break;
		    case 'U':
		    	multiOnly = CSSM_TRUE;
				break;
		    case 'm':
		    	cspMallocs = CSSM_TRUE;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	if(multiOnly && !multiEnable) {
		printf("***can't specify multi disable and multi only\n");
		exit(1);
	}
	if(minPtextSize > maxPtextSize) {
		printf("***minPtextSize must be <= maxPtextSize\n");
		usage(argv);
	}
	pwd.Data = (uint8 *)CSSM_MALLOC(PWD_LENGTH_MAX);
	ptext.Data = (uint8 *)CSSM_MALLOC(maxPtextSize);
	if(ptext.Data == NULL) {
		printf("Insufficient heap space\n");
		exit(1);
	}
	/* ptext length set in test loop */
	printf("Starting symTest; args: ");
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
	for(currAlg=minAlg; currAlg<=maxAlg; currAlg++) {
		/* some default values... */
		mode = CSSM_ALGMODE_NONE;
		padding = CSSM_PADDING_NONE;
		blockSize = 0;			// i.e., don't align
		expectEqualText = CSSM_FALSE;
		switch(currAlg) {
			case ALG_ASC:
				encrAlg = keyAlg = CSSM_ALGID_ASC;
				algStr = "ASC";
				break;
			case ALG_DES:
				encrAlg = keyAlg = CSSM_ALGID_DES;
				algStr = "DES";
				if(noPadding) {
					mode = CSSM_ALGMODE_CBC_IV8;
					blockSize = 8;
					expectEqualText = CSSM_TRUE;
				}
				else {
					mode = CSSM_ALGMODE_CBCPadIV8;
					padding = CSSM_PADDING_PKCS1;
				}
				break;
			case ALG_3DES:
				/* currently the only one with different key and encr algs */
				/* Though actually these two consts are equivalent...for now... */
				keyAlg  = CSSM_ALGID_3DES_3KEY;
				encrAlg = CSSM_ALGID_3DES_3KEY_EDE;
				algStr = "3DES";
				if(noPadding) {
					mode = CSSM_ALGMODE_CBC_IV8;
					blockSize = 8;
					expectEqualText = CSSM_TRUE;
				}
				else {
					mode = CSSM_ALGMODE_CBCPadIV8;
					padding = CSSM_PADDING_PKCS1;
				}
				break;
			case ALG_RC2:
				encrAlg = keyAlg = CSSM_ALGID_RC2;
				algStr = "RC2";
				if(noPadding) {
					mode = CSSM_ALGMODE_CBC_IV8;
					blockSize = 8;
					expectEqualText = CSSM_TRUE;
				}
				else {
					mode = CSSM_ALGMODE_CBCPadIV8;
					padding = CSSM_PADDING_PKCS1;		// what does padding do here?
				}
				break;
			case ALG_RC4:
				encrAlg = keyAlg = CSSM_ALGID_RC4;
				algStr = "RC4";
				mode = CSSM_ALGMODE_NONE;
				expectEqualText = CSSM_TRUE;			// always for RC4
				break;
			case ALG_RC5:
				encrAlg = keyAlg = CSSM_ALGID_RC5;
				algStr = "RC5";
				if(noPadding) {
					mode = CSSM_ALGMODE_CBC_IV8;
					blockSize = 8;
					expectEqualText = CSSM_TRUE;
				}
				else {
					mode = CSSM_ALGMODE_CBCPadIV8;
					padding = CSSM_PADDING_PKCS1;		// eh?
				}
				break;
			case ALG_AES:
				encrAlg = keyAlg = CSSM_ALGID_AES;
				algStr = "AES";
				if(noPadding) {
					mode = CSSM_ALGMODE_CBC_IV8;
					blockSize = 16;
					expectEqualText = CSSM_TRUE;
				}
				else {
					mode = CSSM_ALGMODE_CBCPadIV8;
					padding = CSSM_PADDING_PKCS5;
				}
				break;
			case ALG_BFISH:
				encrAlg = keyAlg = CSSM_ALGID_BLOWFISH;
				algStr = "Blowfish";
				if(noPadding) {
					mode = CSSM_ALGMODE_CBC_IV8;
					blockSize = 8;
					expectEqualText = CSSM_TRUE;
				}
				else {
					mode = CSSM_ALGMODE_CBCPadIV8;
					padding = CSSM_PADDING_PKCS5;
				}
				break;
			case ALG_CAST:
				encrAlg = keyAlg = CSSM_ALGID_CAST;
				algStr = "CAST";
				if(noPadding) {
					mode = CSSM_ALGMODE_CBC_IV8;
					blockSize = 8;
					expectEqualText = CSSM_TRUE;
				}
				else {
					mode = CSSM_ALGMODE_CBCPadIV8;
					padding = CSSM_PADDING_PKCS5;
				}
				break;
			case ALG_NULL:
				encrAlg = keyAlg = CSSM_ALGID_NONE;
				algStr = "NULL";
				if(noPadding) {
					mode = CSSM_ALGMODE_CBC_IV8;
					blockSize = 16;
					expectEqualText = CSSM_TRUE;
				}
				else {
					mode = CSSM_ALGMODE_CBCPadIV8;
					padding = CSSM_PADDING_PKCS5;
				}
				break;
		}
		if(!quiet || verbose) {
			printf("Testing alg %s\n", algStr);
		}
		for(loop=1; ; loop++) {
			simpleGenData(&ptext, minPtextSize, maxPtextSize);
			if(blockSize) {
				/* i.e., no padding --> align ptext */
				ptext.Length = ((ptext.Length + blockSize - 1) / blockSize) * blockSize;
			}
			if(!keySizeSpec) {
				actKeySizeInBits = randKeySizeBits(keyAlg, OT_Encrypt);
			}
			/* else constant, spec'd by user, may be 0 (default per alg) */
			/* mix up some random and derived keys, as well as staging and "who does
			 * the malloc?" */
			pPwd       = (loop & 1) ? &pwd : NULL;
			if(multiEnable) {
				if(multiOnly) {
					stagedEncr = stagedDecr = CSSM_TRUE;
				}
				else {
					stagedEncr = (loop & 2) ? CSSM_TRUE : CSSM_FALSE;
					stagedDecr = (loop & 4) ? CSSM_TRUE : CSSM_FALSE;
				}
			}
			else {
				stagedEncr = CSSM_FALSE;
				stagedDecr = CSSM_FALSE;
			}
			if(!stagedEncr && !cspMallocs) {
				mallocCtext = (ptext.Data[0] & 1) ? CSSM_TRUE : CSSM_FALSE;
			}
			else {
				mallocCtext = CSSM_FALSE;
			}
			if(!stagedDecr && !cspMallocs) {
				mallocPtext = (ptext.Data[0] & 2) ? CSSM_TRUE : CSSM_FALSE;
			}
			else {
				mallocPtext = CSSM_FALSE;
			}
			if(refKeysOnly) {
				refKey = CSSM_TRUE;
			}
			else {
				refKey = (ptext.Data[0] & 4) ? CSSM_TRUE : CSSM_FALSE;
			}
			#if !PBE_ENABLE
			pPwd = NULL;
		 	#endif
			if(!quiet) {
			   	if(verbose || ((loop % LOOP_NOTIFY) == 0)) {
					printf("..loop %d text size %lu keySizeBits %u\n",
						loop, (unsigned long)ptext.Length, (unsigned)actKeySizeInBits);
					if(verbose) {
						printf("  refKey %d derive %d stagedEncr %d  stagedDecr %d mallocCtext %d "
							"mallocPtext %d\n",
					 	(int)refKey, (pPwd == NULL) ? 0 : 1, (int)stagedEncr, (int)stagedDecr,
					 	(int)mallocCtext, (int)mallocPtext);
					 }
				}
			}
			#if		PBE_ENABLE
			if(pPwd != NULL) {
				/* PBE - cook up random password */
				simpleGenData(pPwd, APPLE_PBE_MIN_PASSWORD, PWD_LENGTH_MAX);
			}
			#endif
			
			if(doTest(cspHand,
					&ptext,
					keyAlg,
					encrAlg,
					mode,
					padding,
					actKeySizeInBits,
					refKey,
					pPwd,
					stagedEncr,
					stagedDecr,
					mallocPtext,
					mallocCtext,
					quiet,
					keyGenOnly,
					expectEqualText)) {
				rtn = 1;
				break;
			}
			if(pauseInterval && ((loop % pauseInterval) == 0)) {
				char c;
				fpurge(stdin);
				printf("Hit CR to proceed, q to abort: ");
				c = getchar();
				if(c == 'q') {
					goto testDone;
				}
			}
			if(loops && (loop == loops)) {
				break;
			}
		}	/* main loop */
		if(rtn) {
			break;
		}
		
	}	/* for algs */
	
testDone:
	cspShutdown(cspHand, bareCsp);
	if(pauseInterval) {
		fpurge(stdin);
		printf("ModuleDetach/Unload complete; hit CR to exit: ");
		getchar();
	}
	if((rtn == 0) && !quiet) {
		printf("%s test complete\n", argv[0]);
	}
	CSSM_FREE(pwd.Data);
	CSSM_FREE(ptext.Data);
	return rtn;
}
