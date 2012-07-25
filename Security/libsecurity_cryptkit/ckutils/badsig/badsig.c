/* Copyright 1996-1997 Apple Computer, Inc.
 * 
 * badsig.c - Verify bad signature detect
 *
 * Revision History
 * ----------------
 *  26 Aug 1996	Doug Mitchell at NeXT
 *	Created.
 */
 
/* 
 * text size =       {random, from 100 bytes to 1 megabyte, in
 *                   geometrical steps, i.e. the number of
 *                   bytes would be 10^r, where r is random out of 
 *                   {2,3,4,5,6}, plus a random integer in {0,..99}};
 *
 * password size = constant;
 *
 * for loop_count
 *     text contents = {random data, random size as specified above};
 *     passsword data = random;
 *
       Alternate between ECDSA and ElGamal on sucessive loops:
 *     generate signature, validate;
 *     for each byte of signature {
 *        corrupt text byte;
 *        verify bad signature;
 *        restore corrupted byte;
 *     }
 *  }
 */
 
#import "Crypt.h"
#include "ckconfig.h"

#if !CRYPTKIT_HIGH_LEVEL_SIG
#error Can not build this program against a lib with !CRYPTKIT_HIGH_LEVEL_SIG.
#endif

#import <sys/param.h>
#import <libc.h>

static unsigned char *passwdPool;	/* all passwords come from here */
static unsigned char *dataPool;		/* plaintext comes from here */

#define MAX_DATA_SIZE		((1024 * 1024) + 100)	/* bytes */

/*
 * Defaults.
 */
#define LOOPS_DEF	1
#define MIN_EXP		2		/* for data size 10**exp */
#define MAX_EXP		4		
#define PWD_LENGTH	15		/* bytes */
#define DEPTH_DEFAULT	FEE_DEPTH_DEFAULT
#define INCR_DEFAULT	1		/* munge every incr bytes */

///#define DEPTH_DEFAULT	FEE_DEPTH_5 

static void usage(char **argv) 
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   n=minExp (default=%d)\n", MIN_EXP);
	printf("   x=maxExp (default=max=%d)\n", MAX_EXP);
	printf("   p=passwdLength (default=%d)\n", PWD_LENGTH);
	printf("   D=depth (default=%d)\n", DEPTH_DEFAULT);
	printf("   i=increment (default=%d)\n", INCR_DEFAULT);
	#if CRYPTKIT_ECDSA_ENABLE
	printf("   e (ElGamal only, no ECDSA)\n");
	#endif
	printf("   s=seed\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
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
	return(min + (random() % (max-min+1)));
}

static unsigned char *genPasswd(unsigned passwdLength) 
{
	unsigned *ip = (unsigned *)passwdPool;
	unsigned intCount = passwdLength / 4;
	int i;
	unsigned char *cp;
	unsigned residue = passwdLength & 0x3;
	
	for (i=0; i<intCount; i++) {
		*ip++ = random();
	}
	cp = (unsigned char *)ip;
	for(i=0; i<residue; i++) {
		*cp = (unsigned char)random();
	}
	return passwdPool;
}

/*
 * Calculate random data size, fill dataPool with that many random bytes. 
 */
typedef enum {
	DT_Random,
	DT_Zero,
	DT_ASCII
} dataType;

#define MIN_OFFSET	0
#define MAX_OFFSET	99

#define MIN_ASCII	' '
#define MAX_ASCII	'~'

static unsigned char *genData(unsigned minExp, 
	unsigned maxExp, 
	dataType type,
	unsigned *length)		// RETURNED
{
	int 		exp;
	int 		offset;
	int 		size;
	unsigned 	*ip;
	unsigned 	intCount;
	unsigned 	residue;
	char 		*cp;
	int 		i;
	char		ac;
	
	/*
	 * Calculate "random" size : (10 ** (random exponent)) + random offset
	 */
	exp = genRand(minExp, maxExp);
	offset = genRand(MIN_OFFSET, MAX_OFFSET);
	size = 1;
	while(exp--) {			// size = 10 ** exp
		size *= 10;
	}
	size += offset;

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
		intCount = size >> 2;
		ip = (unsigned *)dataPool;
		for(i=0; i<intCount; i++) {
			*ip++ = random();
		}
		
		residue = size & 0x3;
		cp = (unsigned char *)ip;
		for(i=0; i<residue; i++) {
			*cp++ = (unsigned char)random();
		}
		break;
	}
	*length = size;
	return dataPool;
}


static int sigError()
{
	char resp[100];
	
	printf("Attach via debugger for more info.\n");
	printf("a to abort, c to continue: ");
	gets(resp);
	return (resp[0] != 'c');
}

#define LOG_FREQ	200

int doTest(unsigned char *ptext, 
	unsigned ptextLen,
	unsigned char *passwd, 
	unsigned passwdLen,
	int verbose, 
	int quiet,
	unsigned depth, 
	unsigned incr,
	int doECDSA,
	int doECDSAVfy)			// ignored if doECDSASig == 0
{
	feePubKey	*pubKey;
	unsigned char	*sig;
	unsigned	sigLen;
	unsigned	byte;	
	unsigned char	origData;
	unsigned char	bits;
	feeReturn	frtn;
	
	pubKey = feePubKeyAlloc();
	frtn = feePubKeyInitFromPrivDataDepth(pubKey,
		passwd,
		passwdLen,
		depth,
		1);
	if(frtn) {
		printf("feePubKeyInitFromPrivData returned %s\n",
			feeReturnString(frtn));
		return sigError();	
	}
	#if CRYPTKIT_ECDSA_ENABLE
	if(doECDSA) {
	    frtn = feePubKeyCreateECDSASignature(pubKey,
		    ptext,
		    ptextLen,
		    &sig,
		    &sigLen);
	    if(frtn) {
		    printf("feePubKeyCreateECDSASignature returned %s\n",
			    feeReturnString(frtn));
		    return sigError();	
	    }
	    if(doECDSAVfy) {
		frtn = feePubKeyVerifyECDSASignature(pubKey,
		    ptext,
		    ptextLen,
		    sig,
		    sigLen);	    
	    }
	    else {
		frtn = feePubKeyVerifySignature(pubKey,
		    ptext,
		    ptextLen,
		    sig,
		    sigLen);	    
	    }
	}
	else {
	#else
	{
	#endif	/* CRYPTKIT_ECDSA_ENABLE */
	    frtn = feePubKeyCreateSignature(pubKey,
		    ptext,
		    ptextLen,
		    &sig,
		    &sigLen);
	    if(frtn) {
		    printf("feePubKeyCreateSignature returned %s\n",
			    feeReturnString(frtn));
		    return sigError();	
	    }
	    frtn = feePubKeyVerifySignature(pubKey,
	    	ptext,
		ptextLen,
		sig,
		sigLen);
	}
	if(frtn) {
	    printf("**Unexpected BAD signature\n");
	    return sigError();	
	}
	for(byte=0; byte<ptextLen; byte += incr) {
	    if(!quiet && (verbose || ((byte % LOG_FREQ) == 0))) {
		    printf("....byte %d\n", byte);
	    }
	    origData = ptext[byte];
	    
	    /*
	     * Generate random non-zero byte
	     */
	    do {
		    bits = random() & 0xff;
	    } while(bits == 0);
	    
	    ptext[byte] ^= bits;
	    #if CRYPTKIT_ECDSA_ENABLE
	    if(doECDSA && doECDSAVfy) {
	    	frtn = feePubKeyVerifyECDSASignature(pubKey,
		    ptext,
		    ptextLen,
		    sig,
		    sigLen);
	    }
	    else {
	    #else
	    {
	    #endif  /* CRYPTKIT_ECDSA_ENABLE */
	    	frtn = feePubKeyVerifySignature(pubKey,
		    ptext,
		    ptextLen,
		    sig,
		    sigLen);
	    }
	    if(frtn == FR_Success) {
		printf("**Unexpected GOOD signature\n");
		return sigError();	
	    }
	    ptext[byte] = origData;
	}
	feePubKeyFree(pubKey);
	return 0;
}

int main(int argc, char **argv)
{
	int		arg;
	char		*argp;
	int		loop;
	unsigned char	*ptext;
	unsigned	ptextLen;
	unsigned char	*passwd;
	int		doECDSA;
	int		doECDSAVfy;
	
	/*
	 * User-spec'd params
	 */
	unsigned	passwdLen = PWD_LENGTH;
	unsigned	loops = LOOPS_DEF;
	int		seedSpec = 0;
	unsigned	seed;
	int		verbose = 0;
	unsigned	minExp = MIN_EXP;
	unsigned	maxExp = MAX_EXP;
	int		quiet = 0;
	unsigned	depth = DEPTH_DEFAULT;
	unsigned	incr = INCR_DEFAULT;
	#if CRYPTKIT_ECDSA_ENABLE
	int		elGamalOnly = 0;
	#else
	int		elGamalOnly = 1;
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
		    case 'x':
			maxExp = atoi(&argp[2]);
			if(maxExp > MAX_EXP) {
				usage(argv);
			}
			break;
		    case 'D':
			depth = atoi(&argp[2]);
			break;
		    case 'i':
			incr = atoi(&argp[2]);
			break;
		    case 's':
			seed = atoi(&argp[2]);
			seedSpec = 1;
			break;
		    case 'p':
			passwdLen = atoi(&argp[2]);
			if(passwdLen == 0) {
				usage(argv);
			}
			break;
		    case 'e':
		    	elGamalOnly = 1;
			break;
		    case 'v':
		    	verbose = 1;
			break;
		    case 'q':
		    	quiet = 1;
			break;
		    case 'h':
		    default:
			usage(argv);
		}
	}
		
	if(seedSpec == 0) {
		time((long *)(&seed));
	}
	srandom(seed);
	passwdPool = malloc(passwdLen);
	dataPool = malloc(MAX_DATA_SIZE);

	printf("Starting %s test: loops %d seed %d elGamalOnly %d depth %d\n", 
		argv[0], loops, seed, elGamalOnly, depth);
		
	#if	0
	/* debug only */
	{
		char s[20];
		printf("attach, then CR to continue: ");
		gets(s);
	}	
	#endif	0
			
	for(loop=1; ; loop++) {
	
		ptext = genData(minExp, maxExp, DT_Random, &ptextLen);
		passwd = genPasswd(passwdLen);
		
		/*
		 * Alternate between ECDSA and ElGamal
		 */
		if(elGamalOnly) {
		    doECDSA = 0;
		    doECDSAVfy = 0;
		}
		else {
		    if(loop & 1) {
			doECDSA = 1;
			if(loop & 2) {
			    doECDSAVfy = 1;
			}
			else {
			    doECDSAVfy = 0;
			}
		    }
		    else {
			doECDSA = 0;
			doECDSAVfy = 0;
		    }
		 }
		if(!quiet) {
		    printf("..loop %d text size %d  ECDSA %d ECDSAVfy %d\n",
			loop, ptextLen, doECDSA, doECDSAVfy);
		}
		if(doTest(ptext, ptextLen, passwd, passwdLen, 
				verbose, quiet, depth, incr,
				doECDSA, doECDSAVfy)) {
		    exit(1);
		}

		if(loops && (loop == loops)) {
			break;
		}
	}
	if(!quiet) {
		printf("%s test complete\n", argv[0]);
	}
	return 0;
}
