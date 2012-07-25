/*
 * atomTime.c - measure performance of mulg, gsquare, feemod, 
 * gshift{left,right}, elliptic, ellMulProj
 */
 
#include "ckconfig.h"
#include "ckutilsPlatform.h"
#include "CryptKitSA.h"
#include "ckutilities.h"		/* needs private headers */
#include "curveParams.h"		/* ditto */
#include "falloc.h"				/* ditto */
#include "elliptic.h"
#include "ellipticProj.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* default loops for "fast" and "slow" ops respecitively */
#define LOOPS_DEF_FAST	10000
#define LOOPS_DEF_SLOW	100

#define NUM_BORROW	100

/* individual enables - normally all on, disable to zero in on one test */
#define MULG_ENABLE	    1
#define GSQUARE_ENABLE	    1
#define FEEMOD_ENABLE	    1
#define BORROW_ENABLE	    1
#define	SHIFT_ENABLE	    1
#define BINVAUX_ENABLE	    1
#define MAKE_RECIP_ENABLE   1
#define MODGRECIP_ENABLE    1
#define KEYGEN_ENABLE	    1
#define ELLIPTIC_ENABLE	    1
#define ELL_SIMPLE_ENABLE   1

static void usage(char **argv)
{
	printf("Usage: %s [l=loops_fast] [L=loops_slow] [q(uick)] [D=depth]\n", argv[0]);
	exit(1);
}

/*
 * Fill numGiants with random data of length bits. 
 */
static void genRandGiants(giant *giants, 
	unsigned numGiants,
	unsigned bits,
	feeRand rand)
{
	int i;
	giant g;
	unsigned char *rdata;
	unsigned bytes = (bits + 7) / 8;
	giantDigit mask = 0;
	unsigned giantMsd = 0;	// index of MSD
	unsigned log2BitsPerDigit;
	unsigned bitsPerDigit;
	
	/* just to satisfy compiler - make sure it's always called */
	if(giants == NULL) {
	    return;
	}
	
	log2BitsPerDigit = GIANT_LOG2_BITS_PER_DIGIT;
	bitsPerDigit = GIANT_BITS_PER_DIGIT;

	if((bits & 7) != 0) {
		/* 
		 * deserializeGiant() has a resolution of one byte. We
		 * need more resolution - that is, we'll be creating
		 * giants a little larger than we need, and we'll mask off 
		 * some bits in the giants' m.s. digit.
		 * This assumes that data fills the giantDigits such 
		 * that if bytes mod GIANT_BYTES_PER_DIGIT != 0, the
		 * empty byte(s) in the MSD are in the m.s. byte(s).
		 */
		giantMsd = bits >> log2BitsPerDigit;
		mask = (1 << (bits & (bitsPerDigit - 1))) - 1;
	}
	rdata = fmalloc(bytes);
	for(i=0; i<numGiants; i++) {
		g = giants[i];
		feeRandBytes(rand, rdata, bytes);
		deserializeGiant(rdata, g, bytes);
		if(mask != 0) {
			int j;
			
		        g->n[giantMsd] &= mask;
			
			/*
			 * We've zeroed out some bits; we might have to 
			 * adjust the sign of the giant as well. Note that
			 * deserializeGiant always yields positive 
			 * giants....
			 */
			for(j=(g->sign - 1); j!=0; j--) {	
			    if(g->n[j] == 0) {
				(g->sign)--;
			    }
			    else {
				break;
			    }
			}
		}
	}
	ffree(rdata);
	return;
}

#if	CRYPTKIT_ELL_PROJ_ENABLE
/*
 * Assumes the presence of numEllPoints items in *points, and that the
 * x coordinate in each point is init'd to a random giant. Uses the x
 * coords as seeds to make normalized points of the entire *points array.
 */
static void makePoints(pointProjStruct *points, 
	unsigned numEllPoints,
	curveParams *cp)
{
	int i;
	giant seed = newGiant(cp->maxDigits);
	
	for(i=0; i<numEllPoints; i++) {
		gtog(points[i].x, seed);
		findPointProj(&points[i], seed, cp);
	}
	freeGiant(seed);
}

#endif	/* CRYPTKIT_ELL_PROJ_ENABLE */

int main(int argc, char **argv)
{
	int 		arg;
	char 		*argp;
	unsigned 	loopsFast = LOOPS_DEF_FAST;
	unsigned	loopsSlow = LOOPS_DEF_SLOW;
	unsigned	maxLoops;
	unsigned	depth;
	feeRand 	rand;
	giant 		*giants;
	unsigned 	seed;
	int		i;
	int		j;
	PLAT_TIME	startTime;
	PLAT_TIME	endTime;
	double		elapsed;
	unsigned	numGiants;
	#if CRYPTKIT_ELL_PROJ_ENABLE
	unsigned	numEllGiants;		// for elliptic ops
	unsigned	numEllPoints;		// for elliptic ops
	#endif	
	curveParams	*cp;
	unsigned	quick = 0;
	unsigned	minDepth = 0;
	unsigned	maxDepth = FEE_DEPTH_MAX;
	unsigned	basePrimeLen;
	int		*shiftCnt;
	char		*curveType;
	#if CRYPTKIT_ELL_PROJ_ENABLE
	pointProjStruct	*points;
	#endif
	giant		z = NULL;
	giant		modGiant = NULL;
	giant		recip = NULL;
	feePubKey	*keyArray = NULL;
	giant		gborrow[NUM_BORROW];
	
    	/* just to satisfy compiler - make sure it's always called */
	genRandGiants(NULL, 0, 0, 0);

	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'l':
		    	loopsFast = atoi(&argp[2]);
			break;
		    case 'L':
		    	loopsSlow = atoi(&argp[2]);
			break;
		    case 'q':
		    	quick = 1;
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
	time((unsigned long *)&seed);
	rand = feeRandAllocWithSeed(seed);

	maxLoops = loopsFast;
	if(loopsSlow > maxLoops) {
	    maxLoops = loopsSlow;
	}
	
	/*
	 * Alloc array of giants big enough for squaring at the largest
	 * key size, enough of them for 'loops' mulgs
	 */
	cp = curveParamsForDepth(FEE_DEPTH_LARGEST);
	numGiants = maxLoops * 2;			// 2 giants per loop
	if(loopsSlow > (maxLoops / 4)) {
	    /* findPointProj needs 4 giants per loop */
	    numGiants *= 4;
	}
	#if CRYPTKIT_ELL_PROJ_ENABLE
	numEllGiants = loopsSlow * 2;
	numEllPoints = loopsSlow * 2;
	#endif
	giants = fmalloc(numGiants * sizeof(giant));
	if(giants == NULL) {
		printf("malloc failure\n");
		exit(1);
	}
	for(i=0; i<numGiants; i++) {
		giants[i] = newGiant(cp->maxDigits);
		if(giants[i] == NULL) {
			printf("malloc failure\n");
			exit(1);
		}
	}
	freeCurveParams(cp);
	
	#if CRYPTKIT_ELL_PROJ_ENABLE
	/*
	 * Projective points - two per ellLoop. The giants come from 
	 * giants[]. We reserve an extra giant per point.
	 * We're assuming that numEllPoints < (4 * numGiants).
	 */
	points = fmalloc(numEllPoints * sizeof(pointProjStruct));
	if(points == NULL) {
		printf("malloc failure\n");
		exit(1);
	}
	j=0;
	for(i=0; i<numEllPoints; i++) {
		points[i].x = giants[j++];
		points[i].y = giants[j++];
		points[i].z = giants[j++];
		j++;				// skip a giant
	}
	#endif
	
	/* feePubKey array */
	keyArray = fmalloc(sizeof(feePubKey) * loopsSlow);
	if(keyArray == NULL) {
		printf("malloc failure\n");
		exit(1);
	}
	
	/*
	 * Alloc an array of shiftCnt ints
	 */
	shiftCnt = fmalloc(maxLoops * sizeof(int));
	if(shiftCnt == NULL) {
		printf("malloc failure\n");
		exit(1);
	}
	
	for(depth=minDepth; depth<=maxDepth; depth++) {
	
		if(quick) {
		    if((depth != FEE_DEPTH_127M) &&
		       (depth != FEE_DEPTH_161W)) {
		       	continue;
		    }
		}
		
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
		
		/*
		 * mulg test
		 * bitlen(giant) <= bitlen(basePrime);
		 * giants[n+1] *= giants[n]
		 */
		#if	MULG_ENABLE
		genRandGiants(giants, numGiants, basePrimeLen, rand);
		PLAT_GET_TIME(startTime);
		for(i=0; i<numGiants; i+=2) {
			mulg(giants[i], giants[i+1]);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_NS(startTime, endTime);
		printf("   mulg:             %12.2f ns per op\n", 
			elapsed / (numGiants / 2));
		#endif	/* MULG_ENABLE */
		
		/*
		 * gsquare test
		 * bitlen(giant) <= bitlen(basePrime);
		 * gsquare(giants[n])
		 */
		#if	GSQUARE_ENABLE
		genRandGiants(giants, numGiants, basePrimeLen, rand);
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsFast; i++) {
			gsquare(giants[i]);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_NS(startTime, endTime);
		printf("   gsquare:          %12.2f ns per op\n", elapsed / loopsFast);
		#endif	/* GSQUARE_ENABLE */
		
		/*
		 * feemod test
		 * bitlen(giant) <= ((2 * bitlen(basePrime) - 2);
		 * feemod(giants[n])
		 */
		#if	FEEMOD_ENABLE
		genRandGiants(giants, numGiants, (basePrimeLen * 2) - 2, 
			rand);
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsFast; i++) {
			feemod(cp, giants[i]);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_NS(startTime, endTime);
		printf("   feemod:           %12.2f ns per op\n", elapsed / loopsFast);
		#endif	/* FEEMOD_ENABLE */
		
		
		/*
		 * borrowGiant test
		 */
		#if	BORROW_ENABLE
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsFast; i++) {
		    for(j=0; j<NUM_BORROW; j++) {
		    	gborrow[j] = borrowGiant(cp->maxDigits);
		    }
		    for(j=0; j<NUM_BORROW; j++) {
		    	returnGiant(gborrow[j]);
		    }
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_NS(startTime, endTime);
		printf("   borrow/return:    %12.2f ns per op\n", 
			elapsed / (loopsFast * NUM_BORROW));		
		#endif	/* BORROW_ENABLE */
		
		/*
		 * shiftright test
		 * bitlen(giant) <= bitlen(basePrime)
		 * 0 <= shiftCnt <= bitlen(basePrime)
		 * gshiftright(giants[i])
		 */
		#if	SHIFT_ENABLE
		genRandGiants(giants, numGiants, basePrimeLen, rand);
		for(i=0; i<loopsFast; i++) {
			shiftCnt[i] = feeRandNextNum(rand) % basePrimeLen;
		}
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsFast; i++) {
			gshiftright(shiftCnt[i], giants[i]);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_NS(startTime, endTime);
		printf("   gshiftright:      %12.2f ns per op\n", elapsed / loopsFast);
		 
		/*
		 * shiftleft test
		 * bitlen(giant) <= bitlen(basePrime)
		 * 1 <= shiftCnt <= bitlen(basePrime)
		 * gshiftleft(giants[i]
		 */
		genRandGiants(giants, numGiants, basePrimeLen, rand);
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsFast; i++) {
			gshiftright(shiftCnt[i], giants[i]);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_NS(startTime, endTime);
		printf("   gshiftleft:       %12.2f ns per op\n", elapsed / loopsFast);
		#endif	/* SHIFT_ENABLE */
		
		/*
		 * binvaux test
		 * bitlen(giant) <= bitlen(basePrime);
		 * binvaux(basePrime, giants[n+1])
		 */
		#if	BINVAUX_ENABLE
		genRandGiants(giants, loopsSlow, basePrimeLen, rand);
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsSlow; i++) {
			binvaux(cp->basePrime, giants[i]);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_US(startTime, endTime);
		printf("   binvaux:          %12.2f us per op\n", 
			elapsed / loopsSlow);
		#endif	/* BINVAUX_ENABLE */
		
		/*
		 * make_recip test
		 * bitlen(giant) <= bitlen(basePrime);
		 * make_recip(giants[n], giants[n+1]
		 */
		#if	MAKE_RECIP_ENABLE
		genRandGiants(giants, numGiants, basePrimeLen, rand);
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsSlow*2; i+=2) {
			make_recip(giants[i], giants[i+1]);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_US(startTime, endTime);
		printf("   make_recip:       %12.2f us per op\n", 
			elapsed / loopsSlow);
		#endif	/* MAKE_RECIP_ENABLE */
		
		/*
		 * modg_via_recip test
		 * bitlen(giant) <= ((2 * bitlen(basePrime) - 2);
		 * bitlen(modGiant) <= bitlen(basePrime)
		 * calc recip of modGiant
		 * modg_via_recip(giants[i])
		 */
		#if	MODGRECIP_ENABLE
		genRandGiants(giants, numGiants, (basePrimeLen * 2) - 2, 
			rand);
		modGiant = borrowGiant(cp->maxDigits);
		recip = borrowGiant(cp->maxDigits);
		genRandGiants(&modGiant, 1, basePrimeLen, rand);
		make_recip(modGiant, recip);
		
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsSlow; i++) {
			modg_via_recip(modGiant, recip, giants[i]);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_NS(startTime, endTime);
		printf("   modg_via_recip:   %12.2f ns per op\n", 
			elapsed / loopsSlow);
		returnGiant(modGiant);
		modGiant = NULL;
		returnGiant(recip);
		recip = NULL;
		#endif	/* MODGRECIP_ENABLE */
		
		/*
		 * key generate test
		 * keyArray[n] = feePubKeyAlloc(); 
		 * feePubKeyInitFromPrivData(keyArray[n] );
		 */
		#if KEYGEN_ENABLE
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsSlow; i++) {
		 	keyArray[i] = feePubKeyAlloc(); 
			if(keyArray[i] == NULL) {
				printf("malloc failure\n");
				exit(1);
			}
			/* fixme how about some better seed data */
		 	feePubKeyInitFromPrivDataDepth(keyArray[i],
				(unsigned char *)"somePrivData",
				12,
				depth,
				1);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_US(startTime, endTime);
		printf("   keygen:           %12.2f us per op\n", 	
			elapsed / loopsSlow);
		for(i=0; i<loopsSlow; i++) {
			feePubKeyFree(keyArray[i]);
		}
		#endif	/* KEYGEN_ENABLE*/
		
		/*
		 * elliptic test
		 * bitlen(giant) <= bitlen(basePrime);
		 * {giants[n], 1} *=  giants[n+1]   (elliptic mult)
		 */
		#if ELLIPTIC_ENABLE
		genRandGiants(giants, numGiants, basePrimeLen, rand);
		z = borrowGiant(cp->maxDigits);
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsSlow; i+=2) {
			/* superoptimized int_to_giant(1) */
			z->n[0] = 1;
			z->sign = 1;
			elliptic(giants[i], z, giants[i+1], cp);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_US(startTime, endTime);
		printf("   elliptic:         %12.2f us per op\n", 
			elapsed / (loopsSlow / 2));
		#endif	/* ELLIPTIC_ENABLE*/
		
		/*
		 * elliptic_simple test
		 * bitlen(giant) <= bitlen(basePrime);
		 * giants[n] *= giants[n+1] (elliptic mult)
		 */
		#if ELL_SIMPLE_ENABLE
		genRandGiants(giants, numGiants, basePrimeLen, rand);
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsSlow*2; i+=2) {
			elliptic_simple(giants[i], giants[i+1], cp);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_US(startTime, endTime);
		printf("   elliptic_simple:  %12.2f us per op\n", 
			elapsed / loopsSlow);
		#endif	/* ELL_SIMPLE_ENABLE */
		
		if(cp->curveType != FCT_Weierstrass) {
			goto loopEnd;
		}
		
		#if CRYPTKIT_ELL_PROJ_ENABLE
		/*
		 * ellMulProj test
		 * bitlen(giant) <= bitlen(basePrime);
		 * point[n+1] = point[n] * giants[4n+3] 
		 * 
		 * note we're cooking up way more giants than we have to;
		 * we really only need the x's and k's. But what the heck.
		 */
		genRandGiants(giants, 4 * numEllPoints, basePrimeLen, rand);
		makePoints(points, numEllPoints, cp);
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsSlow; i+=2) {
			ellMulProj(&points[i], &points[i+1], 
				giants[4*i+3], cp);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_US(startTime, endTime);
		printf("   ellMulProj:       %12.2f us per op\n", 
			elapsed / (loopsSlow / 2));
	
		/*
		 * ellMulProjSimple test
		 * bitlen(giant) <= bitlen(basePrime);
		 * point[n] *= giants[4n+3] (projective elliptic mult)
		 */
		genRandGiants(giants, 4 * numEllPoints, basePrimeLen, rand);
		makePoints(points, numEllPoints, cp);
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsSlow; i++) {
			ellMulProjSimple(&points[i], giants[4*i+3], cp);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_US(startTime, endTime);
		printf("   ellMulProjSimple: %12.2f us per op\n", 
			elapsed / loopsSlow);
	
		/*
		 * ellAddProj test
		 * bitlen(giant) <= bitlen(basePrime);
		 * point[n] += point[n+1] (projective elliptic add)
		 */
		genRandGiants(giants, 4 * numEllPoints, basePrimeLen, rand);
		makePoints(points, numEllPoints, cp);
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsSlow; i+=2) {
			ellAddProj(&points[i], &points[i+1], cp);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_NS(startTime, endTime);
		printf("   ellAddProj:       %12.2f ns per op\n",
			elapsed / loopsSlow);
	
		/*
		 * ellDoubleProj test
		 * bitlen(giant) <= bitlen(basePrime);
		 * ellDoubleProj(point[n])
		 */
		genRandGiants(giants, 4 * numEllPoints, basePrimeLen, rand);
		makePoints(points, numEllPoints, cp);
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsSlow; i++) {
			ellDoubleProj(&points[i], cp);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_NS(startTime, endTime);
		printf("   ellDoubleProj:    %12.2f ns per op\n", 
			elapsed / loopsSlow);
	
		/*
		 * findPointProj test
		 * bitlen(giant) <= bitlen(basePrime);
		 * findPointProj(point[n], giants[4n+3])
		 */
		genRandGiants(giants, 4 * loopsSlow, basePrimeLen, rand);
		PLAT_GET_TIME(startTime);
		for(i=0; i<loopsSlow; i++) {
			findPointProj(&points[i], giants[4*i + 3], cp);
		}
		PLAT_GET_TIME(endTime);
		elapsed = PLAT_GET_US(startTime, endTime);
		printf("   findPointProj:    %12.2f us per op\n", 
			elapsed / loopsSlow);
			
		#endif	/* CRYPTKIT_ELL_PROJ_ENABLE */

loopEnd:
		freeCurveParams(cp);
	}
	
	feeRandFree(rand);
	for(i=0; i<numGiants; i++) {
		freeGiant(giants[i]);
	}
	ffree(giants);
	if(z) {
	    freeGiant(z);
	}
	if(recip) {
	    freeGiant(recip);
	}
	if(modGiant) {
	    freeGiant(modGiant);
	}
	return 0;
}
 
