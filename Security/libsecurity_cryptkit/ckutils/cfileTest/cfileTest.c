/* Copyright 1997 Apple Computer, Inc.
*
 * cfileTest.c - General standalone CipherFile test
 *
 * Revision History
 * ----------------
 *  28 May 98	Doug Mitchell at Apple
 *	Checged to use fmalloc(), ffree(), ccommand()
 *  24 Jun 97	Doug Mitchell at Apple
 *	Mods for Mac CodeWarrior build - standalone; no feeLib
 *   7 Mar 97	Doug Mitchell at Apple
 *	Created.
 */

#include "ckutilsPlatform.h"
#include "Crypt.h"
#include "feeCipherFile.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

static unsigned char *dataPool;		/* plaintext comes from here */

#undef	BOOL
#undef	YES
#undef	NO
#define BOOL	int
#define YES	1
#define NO	0

#define LOOPS_DEF	100
#define MIN_EXP		2		/* for data size 10**exp */
#define MAX_EXP		3		/* FEED is very slow with ptext larger than this... */		
#define DEPTH_DEFAULT	FEE_DEPTH_DEFAULT
#define MIN_OFFSET	0
#define MAX_OFFSET	99

#define PASSWD_LENGTH	10

static void usage(char **argv) 
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("   l==loops (default=%d)\n", LOOPS_DEF);
	printf("   n=minExp (default=%d)\n", MIN_EXP);
	printf("   x=maxExp (default=max=%d)\n", MAX_EXP);
	printf("   D=depth (default=%d)\n", DEPTH_DEFAULT);
	printf("   N=minOffset (default=%d)\n", MIN_OFFSET);
	printf("   q(uiet)           v(erbose)\n");
	printf("   h(elp)            I(ncrementing offset)\n");
	exit(1);
}

/*
 * ...min <= return <= max 
 */
static int genRand(int min, int max) 
{	

    /* note random() only yields a 31-bit number... */
	
    if(max == min)			/* avoid % 1 ! */
	return(max);
    else
	return(min + (RAND() % (max-min+1)));
}

/* end of feeLib routines */

#define MIN_ASCII	' '
#define MAX_ASCII	'~'

static void genPasswd(unsigned char *passwd,
	unsigned passwdLen, BOOL ascii) 
{
	unsigned *ip = (unsigned *)passwd;
	unsigned intCount = passwdLen / 4;
	int i;
	unsigned char *cp;
	unsigned residue = passwdLen & 0x3;
	char ac;
	
	if(ascii) {
		cp = passwd;
		ac = MIN_ASCII;
		for(i=0; i<passwdLen; i++) {
			*cp++ = ac++;
			if(ac > MAX_ASCII) {
				ac = MIN_ASCII;
			}
		}
	}
	else {
		for (i=0; i<intCount; i++) {
			*ip++ = RAND();
		}
		cp = (unsigned char *)ip;
		for(i=0; i<residue; i++) {
			*cp = (unsigned char)RAND();
		}
	}
}

/*
 * Calculate random data size, fill dataPool with that many random bytes. 
 */
typedef enum {
	DT_Random,
	DT_Zero,
	DT_ASCII,
	DT_None			/* data irrelevant; use existing pool */
} dataType;

static void fillDataPool(unsigned size, dataType type)
{
	#ifdef	__LITTLE_ENDIAN__
	unsigned 	*ip;
	unsigned 	intCount;
	unsigned 	residue;
	#endif
	unsigned char	*cp;
	int 		i;
	unsigned char	ac;
	
	switch(type) {	
	    case DT_Zero:
		bzero(dataPool, size);
		break;
	    case DT_ASCII:
	    	ac = MIN_ASCII;
		cp = dataPool;
	    	for(i=0; i<size; i++) {
		 	*cp++ = ac++;
			if(ac > MAX_ASCII) {
				ac = MIN_ASCII;
			}
		}
		break;
	    case DT_Random:
	    	#ifdef	__LITTLE_ENDIAN__
		    intCount = size >> 2;
		    ip = (unsigned *)dataPool;
		    for(i=0; i<intCount; i++) {
			    *ip++ = RAND();
		    }
		    
		    residue = size & 0x3;
		    cp = (unsigned char *)ip;
		    for(i=0; i<residue; i++) {
			    *cp++ = (unsigned char)RAND();
		    }
	    	#else	__LITTLE_ENDIAN__
		    cp = dataPool;
		    for(i=0; i<size; i++) {
			    *cp++ = (char)RAND();
		    }
		#endif	__LITTLE_ENDIAN__
		break;
	    case DT_None:
	    	printf("fillDataPool(DT_None)\n");
		exit(1);
	}
}

static unsigned dataSizeFromExp(unsigned maxExp)
{
	int size = 1;
	while(maxExp--) {			// size = 10 ** exp
		size *= 10;
	}
	return size;
}

static int	sizeOffset = MIN_OFFSET;

static unsigned char *genData(unsigned minExp, 
	unsigned maxExp, 
	dataType type,
	BOOL incrOffset,
	unsigned minOffset,
	unsigned *dataLen)		// RETURNED
{
	int 		exp;
	int 		offset;
	int 		size;
	
	/*
	 * Calculate "random" size : (10 ** (random exponent)) + random offset
	 */
	exp = genRand(minExp, maxExp);
	if(incrOffset) {
		offset = sizeOffset++;
		if(sizeOffset == MAX_OFFSET) {
			sizeOffset = minOffset;
		}
	}
	else {
		offset = genRand(minOffset, MAX_OFFSET);
	}
	size = dataSizeFromExp(exp) + offset;
	if(type != DT_None) {
		fillDataPool(size, type);
	}
	*dataLen = size;
	return dataPool;
}

static feePubKey genPrivKey(const unsigned char *privKeyData, 
	unsigned privDataLen, 
	int depth)
{
	feePubKey 	privKey;		// generic key object
	feeReturn	frtn;
	
	privKey = feePubKeyAlloc();
	frtn = feePubKeyInitFromPrivDataDepth(privKey,
		(unsigned char *)privKeyData,
		privDataLen,
		depth,
		1);
	if(frtn) {
		printf("pubKeyFromPrivDataDepth: Can't create new key (%s)\n",
			feeReturnString(frtn));
		exit(1);
	}
	return privKey;
}

static feePubKey genPubKey(feePubKey privKey)
{
	feePubKey 	pubKey;			// generic key object
	feeReturn	frtn;
	char		*pubString;
	unsigned	pubStringLen;
	
	frtn = feePubKeyCreateKeyString(privKey, &pubString, &pubStringLen);
	if(frtn) {
		printf("feePubKeyCreateKeyString: Can't get key string (%s)\n",
			feeReturnString(frtn));
		exit(1);
	}
	pubKey = feePubKeyAlloc();
	frtn = feePubKeyInitFromKeyString(pubKey, pubString, pubStringLen);
	if(frtn) {
		printf("feePubKeyInitFromKeyString: Can't create new key "
			"(%s)\n",
			feeReturnString(frtn));
		feePubKeyFree(pubKey);
		exit(1);
	}
	ffree(pubString);
	return pubKey;
}

static char *stringFromEncrType(cipherFileEncrType encrType) 
{
	switch(encrType) {
	    case CFE_PublicDES: return "CFE_PublicDES";
	    case CFE_RandDES: 	return "CFE_RandDES";
	    case CFE_FEED: 	return "CFE_FEED";
	    case CFE_FEEDExp: 	return "CFE_FEEDExp";
	    default:		return "Bogus encrType";
	}
}

#define SIG_NO		0
#define SIG_YES		1
#define EXPLICIT_NO	0
#define EXPLICIT_YES	1
#define EXPLICIT_ERR	2

static void doTest(unsigned char *ptext, 
	unsigned ptextLen, 
	feePubKey myPrivKey, 
	feePubKey myPubKey, 
	feePubKey theirPrivKey, 
	feePubKey theirPubKey,
	cipherFileEncrType encrType, 
	int doEnc64, 
	int doSig,
	int doExplicitKey)	/* EXPLICIT_ERR means do one with 
				 * bad verify key */
{
	feeReturn 		frtn;
	unsigned char 		*ctext;
	unsigned 		ctextLen;
	unsigned char 		*dectext;
	unsigned 		dectextLen;
	unsigned 		outUserData = 0x1234567;
	unsigned 		inUserData;
	cipherFileEncrType 	inEncrType;
	feeSigStatus		sigStatus;
	int 			abort = 0;
	char			instr[100];
	feeSigStatus		expSigStatus = SS_PresentValid;
	int			valid64;
	
	/*
	 * These are tailored to specific encrTypes and doExplicitKeys
	 */
	feePubKey sendPrivKey = myPrivKey;
	feePubKey sendPubKey = myPubKey;
	feePubKey recvPrivKey = theirPrivKey;
	feePubKey recvPubKey = theirPubKey;
	
	switch(encrType) {
	    case CFE_RandDES:
	    case CFE_FEEDExp:
	    	if(!doSig) {
		    sendPrivKey = NULL;		// not needed
		}
		break;
	    case CFE_PublicDES:
	    case CFE_FEED:
		break;
	    default:
	    	printf("Hey bozo! Give me a real encrType!\n");
		exit(1);
	}
	if(!doSig) {
	    sendPubKey = NULL;			// never needed
	    expSigStatus = SS_NotPresent;
	}
	else switch(doExplicitKey) {
	    case EXPLICIT_NO:
	        sendPubKey = NULL;		// get it from cipherfile
		break;
	    case EXPLICIT_YES:
	    	break;				// use myPubKey
	    case EXPLICIT_ERR:		
	    	if(feePubKeyIsEqual(myPubKey, theirPubKey)) {
			printf("myPubKey = theirPubKey!\n");
			goto errOut;
		}
	    	sendPubKey = theirPubKey;	// hopefully != myPubKey!
		expSigStatus = SS_PresentInvalid;
		break;
	    default:
	    	printf("BOGUS doExplicitKey\n");
		exit(1);
	}

	frtn = createCipherFile(sendPrivKey,
		recvPubKey,
		encrType,
		ptext,
		ptextLen,
		doSig,
		doEnc64,
		outUserData,
		&ctext,
		&ctextLen);
	if(frtn) {
		printf("createCipherFile: %s\n", feeReturnString(frtn));
		goto errOut;
	}
	
	valid64 = isValidEnc64(ctext, ctextLen);
	if(valid64 != doEnc64) {
		printf("valid64 mismatch! exp %d got %d\n", doEnc64, valid64);
		abort = 1;
	}
	frtn = parseCipherFile(recvPrivKey,
		sendPubKey,
		ctext,
		ctextLen,
		doEnc64,
		&inEncrType,
		&dectext,
		&dectextLen,
		&sigStatus,
		&inUserData);
	if(frtn) {
		printf("parseCipherFile: %s\n", feeReturnString(frtn));
		goto errOut;
	}
	if(inEncrType != encrType) {
		printf("encrType mismatch exp %d got %d\n", 
			encrType, inEncrType);
		abort = 1;
	}
	if(inUserData != outUserData) {
		printf("userData mismatch exp %d got %d\n", 
			outUserData, inUserData);
		abort = 1;
	}
	if(sigStatus != expSigStatus) {
	        printf("Bad sigStatus exp %d got %d\n", 
			expSigStatus, sigStatus);
		abort = 1;
	}
	if(ptextLen != dectextLen) {
		printf("ptextLen mismatch exp %d got %d\n", 
			ptextLen, dectextLen);
		abort = 1;
	}
	if(bcmp(ptext, dectext, ptextLen)) {
		printf("Data Miscompare\n");
		abort = 1;
	}
	ffree(dectext);
	ffree(ctext);
	if(!abort) {
		return;
	}
errOut:
	/* dump params */
	printf("attach with debugger for more info; enter CR to quit: ");
	gets(instr);
	exit(1);
		
}

int main(int argc, char **argv)
{
	int		arg;
	char		*argp;
	int		loop;
	unsigned char	*ptext;
	unsigned	ptextLen;
	unsigned char	passwd1[PASSWD_LENGTH];
	unsigned char	passwd2[PASSWD_LENGTH];
	int		encrType;
	int		doEnc64;
	feePubKey	myPrivKey;
	feePubKey	theirPrivKey;
	feePubKey	myPubKey;
	feePubKey	theirPubKey;
	unsigned 	maxSize;
	
	/*
	 * User-spec'd params
	 */
	unsigned	loops = LOOPS_DEF;
	BOOL		seedSpec = NO;
	unsigned	seed;
	BOOL		quiet = NO;
	BOOL		verbose = NO;
	unsigned	minExp = MIN_EXP;
	unsigned	maxExp = MAX_EXP;
	BOOL		incrOffset = NO;
	unsigned	depth = DEPTH_DEFAULT;
	unsigned	minOffset = MIN_OFFSET;
	
	#if	macintosh
	argc = ccommand(&argv);
	#endif

	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'l':
			loops = atoi(&argp[2]);
			break;
		    case 'n':
			minExp = atoi(&argp[2]);
			break;
		    case 'D':
			depth = atoi(&argp[2]);
			break;
		    case 'N':
			minOffset = atoi(&argp[2]);
			if(minOffset > MAX_OFFSET) {
				minOffset = MIN_OFFSET;
			}
			sizeOffset = minOffset;
			break;
		    case 'x':
			maxExp = atoi(&argp[2]);
			if(maxExp > MAX_EXP) {
				usage(argv);
			}
			break;
		    case 's':
			seed = atoi(&argp[2]);
			seedSpec = YES;
			break;
		    case 'I':
		    	incrOffset = YES;
			break;
		    case 'q':
		    	quiet = YES;
			break;
		    case 'v':
		    	verbose = YES;
			break;
		    case 'h':
		    default:
			usage(argv);
		}
	}
	
	if(seedSpec == NO) {
		time((unsigned long *)(&seed));
	}
	SRAND(seed);
	maxSize = dataSizeFromExp(maxExp) + MAX_OFFSET + 8;
	dataPool = fmalloc(maxSize);
	
	printf("Starting cfileTest: loops %d seed %d depth %d\n",
		loops, seed, depth);

	for(loop=1; ; loop++) {
	
	    ptext = genData(minExp, maxExp, DT_Random, incrOffset, 
		    minOffset, &ptextLen);
	    if(!quiet) {
		    printf("..loop %d plaintext size %d\n", loop, ptextLen);
	    }
	    
	    /*
	     * Generate a whole bunch of keys
	     */
	    genPasswd(passwd1, PASSWD_LENGTH, NO);	// not ascii!
	    genPasswd(passwd2, PASSWD_LENGTH, NO);
	    myPrivKey 	 = genPrivKey(passwd1, PASSWD_LENGTH, depth);
	    theirPrivKey = genPrivKey(passwd2, PASSWD_LENGTH, depth);
	    myPubKey 	 = genPubKey(myPrivKey);
	    theirPubKey  = genPubKey(theirPrivKey);
	    
	    for(encrType=CFE_PublicDES; 
		encrType<=CFE_FEEDExp; 
		encrType++) {
		
		if(verbose) {
		    printf("  ..%s\n", stringFromEncrType(encrType));
		}
		for(doEnc64=0; doEnc64<2; doEnc64++) { 
		    if(verbose) {
		        printf("    ..doEnc64 %d\n", doEnc64);
		    }   
		    
		    if(verbose) {
		        printf("      ..no sig\n");
		    }   
	    	    doTest(ptext, ptextLen, myPrivKey, myPubKey, 
		    	theirPrivKey, theirPubKey,
			encrType, doEnc64, SIG_NO, EXPLICIT_NO);
			
		    if(verbose) {
		        printf("      ..sig, implicit sendPubKey\n");
		    }   
		    doTest(ptext, ptextLen, myPrivKey, myPubKey, 
		    	theirPrivKey, theirPubKey,
			encrType, doEnc64, SIG_YES, EXPLICIT_NO);
			
		    if(verbose) {
		        printf("      ..sig, explicit sendPubKey\n");
		    }   
		    doTest(ptext, ptextLen, myPrivKey, myPubKey, 
		    	theirPrivKey, theirPubKey,
			encrType, doEnc64, SIG_YES, EXPLICIT_YES);
			
		    if(verbose) {
		        printf("      ..sig, force error\n");
		    }   
		    doTest(ptext, ptextLen, myPrivKey, myPubKey, 
		    	theirPrivKey, theirPubKey,
			encrType, doEnc64, SIG_YES, EXPLICIT_ERR);
			
		} /* for doEnc64 */
	    }	  /* for encrType */
	    
	    feePubKeyFree(myPrivKey);
	    feePubKeyFree(myPubKey);
	    feePubKeyFree(theirPrivKey);
	    feePubKeyFree(theirPubKey);
	    if(loops) {
		    if(loop == loops) {
			    break;
		    }
	    }
	}	/* main loop */
	
	if(!quiet) {
		printf("cfile test complete\n");
	}
	return 0;
}
