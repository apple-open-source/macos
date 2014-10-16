/* Copyright (c) 2006,2008 Apple Inc.
 *
 * ccSymTest.c - test CommonCrypto symmetric encrypt/decrypt.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <CommonCrypto/CommonCryptor.h>
#include "common.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

/*
 * Defaults.
 */
#define LOOPS_DEF		500
#define MIN_DATA_SIZE	8
#define MAX_DATA_SIZE	10000						/* bytes */
#define MAX_KEY_SIZE	kCCKeySizeMaxRC4			/* bytes */
#define MAX_BLOCK_SIZE	kCCBlockSizeAES128			/* bytes */
#define LOOP_NOTIFY		250

/*
 * Enumerate algs our own way to allow iteration.
 */
typedef enum {
	ALG_AES_128 = 1,	/* 128 bit block, 128 bit key */
	ALG_AES_192,		/* 128 bit block, 192 bit key */
	ALG_AES_256,		/* 128 bit block, 256 bit key */
	ALG_DES,
	ALG_3DES,
	ALG_CAST,
	ALG_RC4,
	/* these aren't in CommonCrypto (yet?) */
	ALG_RC2,
	ALG_RC5,
	ALG_BFISH,
	ALG_ASC,
	ALG_NULL					/* normally not used */
} SymAlg;
#define ALG_FIRST			ALG_AES_128
#define ALG_LAST			ALG_RC4


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
	printf("   a=algorithm (d=DES; 3=3DES; a=AES128; n=AES192; A=AES256; \n");
	printf("     c=CAST; 4=RC4; default=all)\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   m=maxPtextSize (default=%d)\n", MAX_DATA_SIZE);
	printf("   n=minPtextSize (default=%d)\n", MIN_DATA_SIZE);
	printf("   k=keySizeInBytes\n");
	printf("   p=pauseInterval (default=0, no pause)\n");
	printf("   o (no padding, well-aligned plaintext)\n");
	printf("   e (ECB only)\n");
	printf("   E (CBC only, no ECB)\n");
	printf("   u (no multi-update ops)\n");
	printf("   U (only multi-update ops)\n");
	printf("   x (always allocate context)\n");
	printf("   X (never allocate context)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

static void printCCError(const char *str, CCCryptorStatus crtn)
{
	const char *errStr;
	char unknownStr[200];
	
	switch(crtn) {
		case kCCSuccess: errStr = "kCCSuccess"; break;
		case kCCParamError: errStr = "kCCParamError"; break;
		case kCCBufferTooSmall: errStr = "kCCBufferTooSmall"; break;
		case kCCMemoryFailure: errStr = "kCCMemoryFailure"; break;
		case kCCAlignmentError: errStr = "kCCAlignmentError"; break;
		case kCCDecodeError: errStr = "kCCDecodeError"; break;
		case kCCUnimplemented: errStr = "kCCUnimplemented"; break;
		default:
			sprintf(unknownStr, "Unknown(%ld)\n", (long)crtn);
			errStr = unknownStr;
			break;
	}
	printf("***%s returned %s\n", str, errStr);
}

/* max context size */
#define CC_MAX_CTX_SIZE	kCCContextSizeRC4

/* 
 * We write a marker at end of expected output and at end of caller-allocated 
 * CCCryptorRef, and check at the end to make sure they weren't written 
 */
#define MARKER_LENGTH	8
#define MARKER_BYTE		0x7e

/* 
 * Test harness for CCCryptor with lots of options. 
 */
CCCryptorStatus doCCCrypt(
	bool forEncrypt,
	CCAlgorithm encrAlg,			
	bool doCbc,
	bool doPadding,
	const void *keyBytes, size_t keyLen,
	const void *iv,
	bool randUpdates,
	bool inPlace,								/* !doPadding only */
	size_t ctxSize,								/* if nonzero, we allocate ctx */
	bool askOutSize,
	const uint8_t *inText, size_t inTextLen,
	uint8_t **outText, size_t *outTextLen)		/* both returned, WE malloc */
{
	CCCryptorRef	cryptor = NULL;
	CCCryptorStatus crtn;
	CCOperation		op = forEncrypt ? kCCEncrypt : kCCDecrypt;
	CCOptions		options = 0;
	uint8_t			*outBuf = NULL;			/* mallocd output buffer */
	uint8_t			*outp;					/* running ptr into outBuf */
	const uint8		*inp;					/* running ptr into inText */
	size_t			outLen;					/* bytes remaining in outBuf */
	size_t			toMove;					/* bytes remaining in inText */
	size_t			thisMoveOut;			/* output from CCCryptUpdate()/CCCryptFinal() */
	size_t			outBytes;				/* total bytes actually produced in outBuf */
	char			ctx[CC_MAX_CTX_SIZE];	/* for CCCryptorCreateFromData() */
	uint8_t			*textMarker = NULL;		/* 8 bytes of marker here after expected end of 
											 * output */
	char			*ctxMarker = NULL;		/* ditto for caller-provided context */
	unsigned		dex;
	size_t			askedOutSize;			/* from the lib */
	size_t			thisOutLen;				/* dataOutAvailable we use */
	
	if(ctxSize > CC_MAX_CTX_SIZE) {
		printf("***HEY! Adjust CC_MAX_CTX_SIZE!\n");
		exit(1);
	}
	if(!doCbc) {
		options |= kCCOptionECBMode;
	}
	if(doPadding) {
		options |= kCCOptionPKCS7Padding;
	}
	
	/* just hack this one */
	outLen = inTextLen;
	if(forEncrypt) {
		outLen += MAX_BLOCK_SIZE;
	}
	
	outBuf = (uint8_t *)malloc(outLen + MARKER_LENGTH);
	
	/* library should not touch this memory */
	textMarker = outBuf + outLen;
	memset(textMarker, MARKER_BYTE, MARKER_LENGTH);
	
	/* subsequent errors to errOut: */

	if(inPlace) {
		memmove(outBuf, inText, inTextLen);
		inp = outBuf;
	}
	else {
		inp = inText;
	}

	if(!randUpdates) {
		/* one shot */
		if(askOutSize) {
			crtn = CCCrypt(op, encrAlg, options,
				keyBytes, keyLen, iv,
				inp, inTextLen,
				outBuf, 0, &askedOutSize);
			if(crtn != kCCBufferTooSmall) {
				printf("***Did not get kCCBufferTooSmall as expected\n");
				printf("   alg %d inTextLen %lu cbc %d padding %d keyLen %lu\n",
					(int)encrAlg, (unsigned long)inTextLen, (int)doCbc, (int)doPadding,
					(unsigned long)keyLen);
				printCCError("CCCrypt", crtn);
				crtn = -1;
				goto errOut;
			}
			outLen = askedOutSize;
		}
		crtn = CCCrypt(op, encrAlg, options,
			keyBytes, keyLen, iv,
			inp, inTextLen,
			outBuf, outLen, &outLen);
		if(crtn) {
			printCCError("CCCrypt", crtn);
			goto errOut;
		}
		*outText = outBuf;
		*outTextLen = outLen;
		goto errOut;
	}
	
	/* random multi updates */
	if(ctxSize) {
		size_t ctxSizeCreated;
		
		if(askOutSize) {
			crtn = CCCryptorCreateFromData(op, encrAlg, options,
				keyBytes, keyLen, iv,
				ctx, 0 /* ctxSize */,
				&cryptor, &askedOutSize);
			if(crtn != kCCBufferTooSmall) {
				printf("***Did not get kCCBufferTooSmall as expected\n");
				printCCError("CCCryptorCreateFromData", crtn);
				crtn = -1;
				goto errOut;
			}
			ctxSize = askedOutSize;
		}
		crtn = CCCryptorCreateFromData(op, encrAlg, options,
			keyBytes, keyLen, iv,
			ctx, ctxSize, &cryptor, &ctxSizeCreated);
		if(crtn) {
			printCCError("CCCryptorCreateFromData", crtn);
			return crtn;
		}
		ctxMarker = ctx + ctxSizeCreated;
		memset(ctxMarker, MARKER_BYTE, MARKER_LENGTH);
	}
	else {
		crtn = CCCryptorCreate(op, encrAlg, options,
			keyBytes, keyLen, iv,
			&cryptor);
		if(crtn) {
			printCCError("CCCryptorCreate", crtn);
			return crtn;
		}
	}
	
	toMove = inTextLen;		/* total to go */
	outp = outBuf;
	outBytes = 0;			/* bytes actually produced in outBuf */
	
	while(toMove) {
		uint32 thisMoveIn;			/* input to CCryptUpdate() */
		
		thisMoveIn = genRand(1, toMove);
		logSize(("###ptext segment len %lu\n", (unsigned long)thisMoveIn)); 
		if(askOutSize) {
			thisOutLen = CCCryptorGetOutputLength(cryptor, thisMoveIn, false);
		}
		else {
			thisOutLen = outLen;
		}
		crtn = CCCryptorUpdate(cryptor, inp, thisMoveIn,
			outp, thisOutLen, &thisMoveOut);
		if(crtn) {
			printCCError("CCCryptorUpdate", crtn);
			goto errOut;
		}
		inp			+= thisMoveIn;
		toMove		-= thisMoveIn;
		outp		+= thisMoveOut;
		outLen   	-= thisMoveOut;
		outBytes	+= thisMoveOut;
	}
	
	if(doPadding) {
		/* Final is not needed if padding is disabled */
		if(askOutSize) {
			thisOutLen = CCCryptorGetOutputLength(cryptor, 0, true);
		}
		else {
			thisOutLen = outLen;
		}
		crtn = CCCryptorFinal(cryptor, outp, thisOutLen, &thisMoveOut);
	}
	else {
		thisMoveOut = 0;
		crtn = kCCSuccess;
	}
	
	if(crtn) {
		printCCError("CCCryptorFinal", crtn);
		goto errOut;
	}
	
	outBytes += thisMoveOut;
	*outText = outBuf;
	*outTextLen = outBytes;
	crtn = kCCSuccess;

	for(dex=0; dex<MARKER_LENGTH; dex++) {
		if(textMarker[dex] != MARKER_BYTE) {
			printf("***lib scribbled on our textMarker memory (op=%s)!\n",
				forEncrypt ? "encrypt" : "decrypt");
			crtn = (CCCryptorStatus)-1;
		}
	}
	if(ctxSize) {
		for(dex=0; dex<MARKER_LENGTH; dex++) {
			if(ctxMarker[dex] != MARKER_BYTE) {
				printf("***lib scribbled on our ctxMarker memory (op=%s)!\n",
					forEncrypt ? "encrypt" : "decrypt");
				crtn = (CCCryptorStatus)-1;
			}
		}
	}
	
errOut:
	if(crtn) {
		if(outBuf) {
			free(outBuf);
		}
	}
	if(cryptor) {
		CCCryptorRelease(cryptor);
	}
	return crtn;
}

static int doTest(const uint8_t *ptext,
	size_t ptextLen,
	CCAlgorithm encrAlg,			
	bool doCbc,
	bool doPadding,
	bool nullIV,			/* if CBC, use NULL IV */
	uint32 keySizeInBytes,
	bool stagedEncr,
	bool stagedDecr,
	bool inPlace,	
	size_t ctxSize,		
	bool askOutSize,
	bool quiet)
{
	uint8_t			keyBytes[MAX_KEY_SIZE];
	uint8_t			iv[MAX_BLOCK_SIZE];
	uint8_t			*ivPtrEncrypt;
	uint8_t			*ivPtrDecrypt;
	uint8_t			*ctext = NULL;		/* mallocd by doCCCrypt */
	size_t			ctextLen = 0;
	uint8_t			*rptext = NULL;		/* mallocd by doCCCrypt */
	size_t			rptextLen;
	CCCryptorStatus	crtn;
	int				rtn = 0;
	
	/* random key */
	appGetRandomBytes(keyBytes, keySizeInBytes);
	
	/* random IV if needed */
	if(doCbc) {
		if(nullIV) {
			memset(iv, 0, MAX_BLOCK_SIZE);
			
			/* flip a coin, give one side NULL, the other size zeroes */
			if(genRand(1,2) == 1) {
				ivPtrEncrypt = NULL;
				ivPtrDecrypt = iv;
			}
			else {
				ivPtrEncrypt = iv;
				ivPtrDecrypt = NULL;
			}
		}
		else {
			appGetRandomBytes(iv, MAX_BLOCK_SIZE);
			ivPtrEncrypt = iv;
			ivPtrDecrypt = iv;
		}
	}	
	else {
		ivPtrEncrypt = NULL;
		ivPtrDecrypt = NULL;
	}

	crtn = doCCCrypt(true, encrAlg, doCbc, doPadding,
		keyBytes, keySizeInBytes, ivPtrEncrypt,
		stagedEncr, inPlace, ctxSize, askOutSize,
		ptext, ptextLen,
		&ctext, &ctextLen);
	if(crtn) {
		rtn = testError(quiet);
		if(rtn) {
			goto abort;
		}
	}
	
	logSize(("###ctext len %lu\n", ctextLen)); 
	
	crtn = doCCCrypt(false, encrAlg, doCbc, doPadding,
		keyBytes, keySizeInBytes, ivPtrDecrypt,
		stagedDecr, inPlace, ctxSize, askOutSize,
		ctext, ctextLen,
		&rptext, &rptextLen);
	if(crtn) {
		rtn = testError(quiet);
		if(rtn) {
			goto abort;
		}
	}

	logSize(("###rptext len %lu\n", rptextLen)); 
	
	/* compare ptext, rptext */
	if(ptextLen != rptextLen) {
		printf("Ptext length mismatch: expect %lu, got %lu\n", ptextLen, rptextLen);
		rtn = testError(quiet);
		if(rtn) {
			goto abort;
		}
	}
	if(memcmp(ptext, rptext, ptextLen)) {
		printf("***data miscompare\n");
		rtn = testError(quiet);
	}
abort:
	if(ctext) {
		free(ctext);
	}
	if(rptext) {
		free(rptext);
	}
	return rtn;
}

bool isBitSet(unsigned bit, unsigned word) 
{
	if(bit > 31) {
		printf("We don't have that many bits\n");
		exit(1);
	}
	unsigned mask = 1 << bit;
	return (word & mask) ? true : false;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	uint8				*ptext;
	size_t				ptextLen;
	bool				stagedEncr;
	bool				stagedDecr;
	bool				doPadding;
	bool				doCbc;
	bool				nullIV;
	const char			*algStr;
	CCAlgorithm			encrAlg;	
	int					i;
	int					currAlg;		// ALG_xxx
	uint32				minKeySizeInBytes;
	uint32				maxKeySizeInBytes;
	uint32				keySizeInBytes;
	int					rtn = 0;
	uint32				blockSize;		// for noPadding case
	size_t				ctxSize;		// always set per alg
	size_t				ctxSizeUsed;	// passed to doTest
	bool				askOutSize;		// inquire output size each op
	
	/*
	 * User-spec'd params
	 */
	bool		keySizeSpec = false;		// false: use rand key size
	SymAlg		minAlg = ALG_FIRST;
	SymAlg		maxAlg = ALG_LAST;
	unsigned	loops = LOOPS_DEF;
	bool		verbose = false;
	size_t		minPtextSize = MIN_DATA_SIZE;
	size_t		maxPtextSize = MAX_DATA_SIZE;
	bool		quiet = false;
	unsigned	pauseInterval = 0;
	bool		paddingSpec = false;		// true: user calls doPadding, const
	bool		cbcSpec = false;			// ditto for doCbc
	bool		stagedSpec = false;			// ditto for stagedEncr and stagedDecr
	bool		inPlace = false;			// en/decrypt in place for ECB
	bool		allocCtxSpec = false;		// use allocCtx
	bool		allocCtx = false;			// allocate context ourself
	
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
						minAlg = maxAlg = ALG_AES_128;
						break;
					case 'n':
						minAlg = maxAlg = ALG_AES_192;
						break;
					case 'A':
						minAlg = maxAlg = ALG_AES_256;
						break;
					case 'b':
						minAlg = maxAlg = ALG_BFISH;
						break;
					case 'c':
						minAlg = maxAlg = ALG_CAST;
						break;
					default:
						usage(argv);
				}
				if(maxAlg > ALG_LAST) {
					/* we left them in the switch but we can't use them */
					usage(argv);
				}
				break;
		    case 'l':
				loops = atoi(&argp[2]);
				break;
		    case 'n':
				minPtextSize = atoi(&argp[2]);
				break;
		    case 'm':
				maxPtextSize = atoi(&argp[2]);
				break;
		    case 'k':
		    	minKeySizeInBytes = maxKeySizeInBytes = atoi(&argp[2]);
		    	keySizeSpec = true;
				break;
			case 'x':
				allocCtxSpec = true;
				allocCtx = true;
				break;
			case 'X':
				allocCtxSpec = true;
				allocCtx = false;
				break;
		    case 'v':
		    	verbose = true;
				break;
		    case 'q':
		    	quiet = true;
				break;
		    case 'p':
		    	pauseInterval = atoi(&argp[2]);;
				break;
			case 'o':
				doPadding = false;
				paddingSpec = true;
				break;
			case 'e':
				doCbc = false;
				cbcSpec = true;
				break;
			case 'E':
				doCbc = true;
				cbcSpec = true;
				break;
		    case 'u':
		    	stagedEncr = false;
		    	stagedDecr = false;
				stagedSpec = true;
				break;
		    case 'U':
		    	stagedEncr = true;
		    	stagedDecr = true;
				stagedSpec = true;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	ptext = (uint8 *)malloc(maxPtextSize);
	if(ptext == NULL) {
		printf("Insufficient heap space\n");
		exit(1);
	}
	/* ptext length set in test loop */
	
	printf("Starting ccSymTest; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	
	if(pauseInterval) {
		fpurge(stdin);
		printf("Top of test; hit CR to proceed: ");
		getchar();
	}

	for(currAlg=minAlg; currAlg<=maxAlg; currAlg++) {
		switch(currAlg) {
			case ALG_DES:
				encrAlg = kCCAlgorithmDES;
				blockSize = kCCBlockSizeDES;
				minKeySizeInBytes = kCCKeySizeDES;
				maxKeySizeInBytes = minKeySizeInBytes;
				ctxSize = kCCContextSizeDES;
				algStr = "DES";
				break;
			case ALG_3DES:
				encrAlg = kCCAlgorithm3DES;
				blockSize = kCCBlockSize3DES;
				minKeySizeInBytes = kCCKeySize3DES;
				maxKeySizeInBytes = minKeySizeInBytes;
				ctxSize = kCCContextSize3DES;
				algStr = "3DES";
				break;
			case ALG_AES_128:
				encrAlg = kCCAlgorithmAES128;
				blockSize = kCCBlockSizeAES128;
				minKeySizeInBytes = kCCKeySizeAES128;
				maxKeySizeInBytes = minKeySizeInBytes;
				ctxSize = kCCContextSizeAES128;
				algStr = "AES128";
				break;
			case ALG_AES_192:
				encrAlg = kCCAlgorithmAES128;
				blockSize = kCCBlockSizeAES128;
				minKeySizeInBytes = kCCKeySizeAES192;
				maxKeySizeInBytes = minKeySizeInBytes;
				ctxSize = kCCContextSizeAES128;
				algStr = "AES192";
				break;
			case ALG_AES_256:
				encrAlg = kCCAlgorithmAES128;
				blockSize = kCCBlockSizeAES128;
				minKeySizeInBytes = kCCKeySizeAES256;
				maxKeySizeInBytes = minKeySizeInBytes;
				ctxSize = kCCContextSizeAES128;
				algStr = "AES256";
				break;
			case ALG_CAST:
				encrAlg = kCCAlgorithmCAST;
				blockSize = kCCBlockSizeCAST;
				minKeySizeInBytes = kCCKeySizeMinCAST;
				maxKeySizeInBytes = kCCKeySizeMaxCAST;
				ctxSize = kCCContextSizeCAST;
				algStr = "CAST";
				break;
			case ALG_RC4:
				encrAlg = kCCAlgorithmRC4;
				blockSize = 0;
				minKeySizeInBytes = kCCKeySizeMinRC4;
				maxKeySizeInBytes = kCCKeySizeMaxRC4;
				ctxSize = kCCContextSizeRC4;
				algStr = "RC4";
				break;
			default:
				printf("***BRRZAP!\n");
				exit(1);
		}
		if(!quiet || verbose) {
			printf("Testing alg %s\n", algStr);
		}
		for(loop=1; ; loop++) {
			ptextLen = genRand(minPtextSize, maxPtextSize);
			appGetRandomBytes(ptext, ptextLen);
			
			/* per-loop settings */
			if(!keySizeSpec) {
				if(minKeySizeInBytes == maxKeySizeInBytes) {
					keySizeInBytes = minKeySizeInBytes;
				}
				else {
					keySizeInBytes = genRand(minKeySizeInBytes, maxKeySizeInBytes);
				}
			}
			if(blockSize == 0) {
				/* stream cipher */
				doCbc = false;
				doPadding = false;
			}
			else {
				if(!cbcSpec) {
					doCbc = isBitSet(0, loop);
				}
				if(!paddingSpec) {
					doPadding = isBitSet(1, loop);
				}
			}
			if(!doPadding && (blockSize != 0)) {
				/* align plaintext */
				ptextLen = (ptextLen / blockSize) * blockSize;
				if(ptextLen == 0) {
					ptextLen = blockSize;
				}
			}
			if(!stagedSpec) {
				stagedEncr = isBitSet(2, loop);
				stagedDecr = isBitSet(3, loop);
			}
			if(doCbc) {
				nullIV = isBitSet(4, loop);
			}
			else {
				nullIV = false;
			}
			inPlace = isBitSet(5, loop);
			if(allocCtxSpec) {
				ctxSizeUsed = allocCtx ? ctxSize : 0;
			}
			else if(isBitSet(6, loop)) {
				ctxSizeUsed = ctxSize;
			}
			else {
				ctxSizeUsed = 0;
			}
			askOutSize = isBitSet(7, loop);
			if(!quiet) {
			   	if(verbose || ((loop % LOOP_NOTIFY) == 0)) {
					printf("..loop %3d ptextLen %lu keyLen %d cbc=%d padding=%d stagedEncr=%d "
							"stagedDecr=%d\n",
						loop, (unsigned long)ptextLen, (int)keySizeInBytes, 
						(int)doCbc, (int)doPadding,
					 	(int)stagedEncr, (int)stagedDecr);
					printf("           nullIV %d inPlace %d ctxSize %d askOutSize %d\n",
						(int)nullIV, (int)inPlace, (int)ctxSizeUsed, (int)askOutSize);
				}
			}
			
			if(doTest(ptext, ptextLen,
					encrAlg, doCbc, doPadding, nullIV,
					keySizeInBytes,
					stagedEncr,	stagedDecr, inPlace, ctxSizeUsed, askOutSize,
					quiet)) {
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
	if(pauseInterval) {
		fpurge(stdin);
		printf("ModuleDetach/Unload complete; hit CR to exit: ");
		getchar();
	}
	if((rtn == 0) && !quiet) {
		printf("%s test complete\n", argv[0]);
	}
	free(ptext);
	return rtn;
}
