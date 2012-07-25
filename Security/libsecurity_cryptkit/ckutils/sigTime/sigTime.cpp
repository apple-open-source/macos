/*
 * atomTime.c - measure performance of digital signature primitives (not incluing 
 * digest)
 */
 
#include "ckconfig.h"
#include "ckutilsPlatform.h"
#include "CryptKitSA.h"
#include "curveParams.h"
#include "falloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define SIGN_LOOPS_DEF		    100
#define VFY_LOOPS_DEF		    100
#define PRIV_KEY_SIZE_BYTES	    32
#define DIGEST_SIZE_BYTES	    20	    /* e.g., SHA1 */
#define NUM_KEYS		    10	    

static void usage(char **argv)
{
	printf("Usage: %s [option...]\n", argv[0]);
	printf("Options:\n");
	printf("  s=signLoops      -- default %d\n", SIGN_LOOPS_DEF);
	printf("  v=verifyLoops    -- default %d\n", VFY_LOOPS_DEF);
	printf("  D=depth          -- default is ALL\n");
	exit(1);
}

typedef struct {
    unsigned char *data;
    unsigned length;
} FeeData;

/*
 * Fill numDatas with random data of length bits. Caller has mallocd referents. 
 */
static void genRandData(FeeData *datas, 
	unsigned numDatas,
	unsigned numBytes,
	feeRand rand)
{
	unsigned i;
	FeeData *fd;
	for(i=0; i<numDatas; i++) {
		fd = &datas[i];
		fd->length = numBytes;
		feeRandBytes(rand, fd->data, numBytes);
	}
	return;
}

static void mallocData(
    FeeData *fd,
    unsigned numBytes)
{
    fd->data = (unsigned char *)fmalloc(numBytes);
    fd->length = numBytes;
}

/* common random callback */
feeReturn randCallback(
    void *ref,
    unsigned char *bytes,
    unsigned numBytes)
{
    feeRand frand = (feeRand)ref;
    feeRandBytes(frand, bytes, numBytes);
    return FR_Success;
}

int main(int argc, char **argv)
{
	int 		arg;
	char 		*argp;
	unsigned 	sigLoops = SIGN_LOOPS_DEF;
	unsigned	vfyLoops = VFY_LOOPS_DEF;
	unsigned	numKeys = NUM_KEYS; // might be less for very small loops
	unsigned	depth;
	feeRand 	rand;
	
	feePubKey	keys[NUM_KEYS];
	/* sigLoops copies of each of {digestData, sigData} */
	FeeData		*digestData;
	FeeData		*sigData;
	
	unsigned 	seed;
	unsigned	i;
	PLAT_TIME	startTime;
	PLAT_TIME	endTime;
	double		elapsed;
	curveParams	*cp;
	unsigned	minDepth = 0;
	unsigned	maxDepth = FEE_DEPTH_MAX;
	unsigned	basePrimeLen;
	char		*curveType;
	feeReturn	frtn;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 's':
		    	sigLoops = atoi(&argp[2]);
			break;
		    case 'v':
		    	vfyLoops = atoi(&argp[2]);
			break;
		    case 'D':
		    	minDepth = maxDepth = atoi(&argp[2]);
			break;
		    default:
		    	usage(argv);
			break;
		}
	}
	
	/*
	 * Common random generator
	 */
	time((time_t *)&seed);
	rand = feeRandAllocWithSeed(seed);

	if(numKeys > sigLoops) {
	    numKeys = sigLoops;
	}
	digestData = (FeeData *)fmalloc(sizeof(FeeData) * sigLoops);
	sigData    = (FeeData *)fmalloc(sizeof(FeeData) * sigLoops);
		
	/* alloc the data, once, for largest private key or "digest" we'll use */
	for(i=0; i<sigLoops; i++) {
	    mallocData(&digestData[i], PRIV_KEY_SIZE_BYTES);
	}   
	for(depth=minDepth; depth<=maxDepth; depth++) {
	
		/*
		 * Get curve params for this depth
	 	 */
		cp = curveParamsForDepth(depth);
		if(cp == NULL) {
			printf("malloc failure\n");
			exit(1);
		}
		switch(cp->curveType) {
		    case FCT_Montgomery:
		    	curveType = "FCT_Montgomery";
			break;
		    case FCT_Weierstrass:
			curveType = "FCT_Weierstrass";
			break;
		    case FCT_General:
		    	curveType = "FCT_General";
			break;
		    default:
		    	printf("***Unknown curveType!\n");
			exit(1);
		}
		
		switch(cp->primeType) {
		    case FPT_General:
		    	printf("depth=%d; FPT_General, %s; keysize=%d;\n", 
				depth, curveType, bitlen(cp->basePrime));
			break;
		    case FPT_Mersenne:
			printf("depth=%d; FPT_Mersenne, %s; q=%d\n",
				depth, curveType, cp->q);
			break;
		    default:
			printf("depth=%d; FPT_FEE, %s; q=%d k=%d\n",
				depth, curveType, cp->q, cp->k);
			break;
		}
		basePrimeLen = bitlen(cp->basePrime);
		
		/* one set of random data as private keys */
		unsigned privSize = (basePrimeLen + 8) / 8;
		genRandData(digestData, numKeys, privSize, rand);
	
		/* generate the keys (no hash - we've got that covered) */
		for(i=0; i<numKeys; i++) {
		    keys[i] = feePubKeyAlloc();
		    feePubKeyInitFromPrivDataDepth(keys[i], digestData[i].data, privSize, 
			depth, 0);
		}

		/* now different data to actually sign */
		genRandData(digestData, sigLoops, DIGEST_SIZE_BYTES, rand);
		
		/*
		 * sign 
		 */
		PLAT_GET_TIME(startTime);
		for(i=0; i<sigLoops; i++) {
		    FeeData *digst = &digestData[i];
		    FeeData *sig   = &sigData[i];
		    feePubKey fkey = keys[i % numKeys];
		    
		    feeSig fs = feeSigNewWithKey(fkey, randCallback, rand);
		    frtn = feeSigSign(fs, digst->data, digst->length, fkey);
		    if(frtn) {
			printf("***Error %d on feeSigSign\n", (int)frtn);
			break;
		    }
		    frtn = feeSigData(fs, &sig->data, &sig->length);
		    if(frtn) {
			printf("***Error %d on feeSigData\n", (int)frtn);
			break;
		    }
		    feeSigFree(fs);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_US(startTime, endTime);
		printf("   sign:   %12.2f us per op\n", 
			elapsed / sigLoops);
	
		/*
		 * verify - might be doing more of these than we have
		 * valid signatures.....
		 */
		unsigned dex=0;
		PLAT_GET_TIME(startTime);
		for(i=0; i<vfyLoops; i++) {
		    FeeData *digst = &digestData[dex];
		    FeeData *sig   = &sigData[dex];
		    feePubKey fkey = keys[dex % numKeys];
		    
		    feeSig fs;
		    frtn = feeSigParse(sig->data, sig->length, &fs);
		    if(frtn) {
			printf("***Error %d on feeSigParse\n", (int)frtn);
			break;
		    }
		    frtn = feeSigVerify(fs, digst->data, digst->length, fkey);
		    if(frtn) {
			printf("***Error %d on feeSigVerify\n", (int)frtn);
			break;
		    }
		    feeSigFree(fs);
		    dex++;
		    if(dex == sigLoops) {
			/* that's all the data we have, recycle */
			dex = 0;
		    }
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_US(startTime, endTime);
		printf("   verify: %12.2f us per op\n", 
			elapsed / vfyLoops);

		freeCurveParams(cp);
		/* possibly limited number of signatures.... */
		for(i=0; i<sigLoops; i++) {
		    ffree(sigData[i].data);	// mallocd by feeSigData()
		}
		for(i=0; i<numKeys; i++) {
		    feePubKeyFree(keys[i]); 
		}
	}
	
	feeRandFree(rand);
	for(i=0; i<sigLoops; i++) {
	    ffree(digestData[i].data);
	}
	ffree(digestData);
	ffree(sigData);
	
	return 0;
}
 
