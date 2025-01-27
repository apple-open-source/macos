/**************************************************************
 *
 *	fmodule.c
 *
 *	Factoring utilities.
 *
 *	Updates:
 *		13 Apr 98   REC - creation
 *
 *	c. 1998 Perfectly Scientific, Inc.
 *	All Rights Reserved.
 *
 *
 *************************************************************/

/* include files */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32 

#include <process.h>

#endif

#include <string.h>
#include "giants.h"
#include "fmodule.h"
#include "ellmont.h"

#define NUM_PRIMES 6542 /* PrimePi[2^16]. */
#define GENERAL_MOD 0
#define FERMAT_MOD 1
#define MERSENNE_MOD (-1)
#define D 100  /* A decimation parameter for stage-2 ECM factoring. */

/* compiler options */

#ifdef _WIN32
#pragma warning( disable : 4127 4706 ) /* disable conditional is constant warning */
#endif


/* global variables */

extern int pr[NUM_PRIMES];  /* Primes array from tools.c. */

unsigned short factors[NUM_PRIMES], exponents[NUM_PRIMES];
int		modmode = GENERAL_MOD;
int		curshorts = 0;
static giant t1 = NULL, t2 = NULL, t3 = NULL, t4 = NULL;
static giant An = NULL, Ad = NULL;
static point_mont pt1, pt2;
point_mont pb[D+2];
giant xzb[D+2];

static int verbosity = 0;

/**************************************************************
 *
 *	Functions
 *
 **************************************************************/

int
init_fmodule(int shorts) {
	curshorts = shorts;
	pb[0] = NULL;  /* To guarantee proper ECM initialization. */
	t1 = newgiant(shorts);
	t2 = newgiant(shorts);
	t3 = newgiant(shorts);
	t4 = newgiant(shorts);
	An = newgiant(shorts);
    Ad = newgiant(shorts);
	pt1 = new_point_mont(shorts);
	pt2 = new_point_mont(shorts);
}

void
verbose(int state)
/* Call verbose(1) for output during factoring processes, 
   call verbose(0) to silence all that.
 */ 
{
	verbosity = state;
}

void
dot(void)
{
	printf(".");
	fflush(stdout);
}

void
set_mod_mode(int mode) 
/* Call this with mode := 1, 0, -1, for Fermat-mod, general mod, and Mersenne mod, 
   respectively; the point being that the special cases of
   Fermat- and Mersenne-mod are much faster than
   general mod.  If all mods will be with respect to a number-to-be-factored,
   of the form N = 2^m + 1, use Fermat mod; while if N = 2^m-1, use Mersenne mod.
 */
{
	modmode = mode;
}

void
special_modg(
	giant		N,
	giant		t
)
{
	switch (modmode)
	{
		case MERSENNE_MOD:
			mersennemod(bitlen(N), t);
			break;
		case FERMAT_MOD:
			fermatmod(bitlen(N)-1, t);
			break;
	    default:
			modg(N, t);
			break;
	}
}

unsigned short *
prime_list() {
	return(&factors[0]);
}

unsigned short *
exponent_list() {
	return(&exponents[0]);
}

int
sieve(giant N, int sievelim)
/* Returns number of N's prime factors < min(sievelim, 2^16),
   with N reduced accordingly by said factors.
   The n-th entry of factors[] becomes the n-th prime
   factor of N, with corresponding exponent
   becoming the n-th element of exponents[]. 
 */
{	int j, pcount, rem;
	unsigned short pri;

		pcount = 0;
		exponents[0] = 0;
		for (j=0; j < NUM_PRIMES; j++)
		{
			pri = pr[j];
			if(pri > sievelim) break;
			do {
				gtog(N, t1);
				rem = idivg(pri, t1);
				if(rem == 0) {
					++exponents[pcount];
					gtog(t1, N);
				}
			} while(rem == 0);
			if(exponents[pcount] > 0) {
				factors[pcount] = pr[j];
				++pcount;
				exponents[pcount] = 0;
			}
		}
		return(pcount);
}

int
pollard_rho(giant N, giant fact, int steps, int abort)
/* Perform Pollard-rho x:= 3; loop(x:= x^2 + 2), a total of steps times.
   Parameter fact will be a nontrivial factor found, in which case
   N is also modified as: N := N/fact.
   The function returns 0 if no nontrivial factor found, else returns 1.
   The abort parameter, when set, causes the factorer to exit on the
   first nontrivial factor found (the requisite GCD is checked
   every 1000 steps).  If abort := 0, the full number
   of steps are always performed, then one solitary GCD is taken,
   before exit.
 */
{	
	int j, found = 0;

	itog(3, t1);
	gtog(t1, t2);
	itog(1, fact);
	for(j=0; j < steps; j++) {
		squareg(t1); iaddg(2, t1); special_modg(N, t1);
		squareg(t2); iaddg(2, t2); special_modg(N, t2);
		squareg(t2); iaddg(2, t2); special_modg(N, t2);
		gtog(t2, t3); subg(t1,t3); mulg(t3, fact); special_modg(N, fact);
		if(((j % 1000 == 999) && abort) || (j == steps-1)) {
			if(verbosity) dot();
			gcdg(N, fact);
			if(!isone(fact)) {
				found = (gcompg(N, fact) == 0) ? 0 : 1;
				break;
			}
		}			
	}
    if(verbosity) { printf("\n"); fflush(stdout); }
	if(found) {
		divg(fact, N);
		return(1);
	}
	itog(1, fact);
	return(0);		
}

int
pollard_pminus1(giant N, giant fact, int lim, int abort)
/* Perform Pollard-(p-1); where we test
	
		GCD[N, 3^P - 1],

   where P is an accumulation of primes <= min(lim, 2^16), 
   to appropriate powers.
   Parameter fact will be a nontrivial factor found, in which case
   N is also modified as: N := N/fact.
   The function returns 0 if no nontrivial factor found, else returns 1.
   The abort parameter, when set, causes the factorer to exit on the
   first nontrivial factor found (the requisite GCD is checked
   every 100 steps).  If abort := 0, the full number
   of steps are always performed, then one solitary GCD is taken,
   before exit.  
 */
{  int cnt, j, k, pri, found = 0;

   itog(3, fact);
   for (j=0; j< NUM_PRIMES; j++)
	{
			pri = pr[j];
			if((pri > lim) || (j == NUM_PRIMES-1) || (abort && (j % 100 == 99))) {
				if(verbosity) dot();
				gtog(fact, t1);
				itog(1, t2);
				subg(t2, t1);
				special_modg(N, t1);
				gcdg(N, t1);
				if(!isone(t1)) {
					found = (gcompg(N, t1) == 0) ? 0 : 1;
					break;
				}
				if(pri > lim) break;
            }
			if(pri < 19) { cnt = 20-pri;  /* Smaller primes get higher powers. */
			} else if(pri < 100) {
						cnt = 2; 
				   } else cnt = 1;
			for (k=0; k< cnt; k++)
			{
				powermod(fact, pri, N);
			}
	}
    if(verbosity) { printf("\n"); fflush(stdout); }
	if(found) {
		gtog(t1, fact);
		divg(fact, N);
		return(1);
	}
	itog(1, fact);
	return(0);		
}

int
ecm(giant N, giant fact, int S, unsigned int B, unsigned int C)
/* Perform elliptic curve method (ECM), with:
   Brent seed parameter = S
   Stage-one limit = B
   Stage-two limit = C
   This function:
		returns 1 if nontrivial factor is found in stage 1 of ECM;
		returns 2 if nontrivial factor is found in stage 2 of ECM;
		returns 0 otherwise.
   In the positive return, parameter fact is the factor and N := N/fact.
 */
{	unsigned int pri, q;
	int j, cnt, count, k;

    if(verbosity) {
	  	printf("Finding curve and point, B = %u, C = %u, seed = %d...", B, C, S);
	  	fflush(stdout);
    }
	find_curve_point_brent(pt1, S, An, Ad, N);
    if(verbosity) {
	  	printf("\n"); 
		printf("Commencing stage 1 of ECM...\n");
		fflush(stdout);
    }

	q = pr[NUM_PRIMES-1];
	count = 0;
	for(j=0; ; j++) {
		if(j < NUM_PRIMES) {
			pri = pr[j];
		} else {
			q += 2;
			if(primeq(q)) pri = q; 
				else continue;
		}
		if(verbosity) if((++count) % 100 == 0) dot();
		if(pri > B) break;
		if(pri < 19) { cnt = 20-pri; 
			} else if(pri < 100) {
						cnt = 2; 
				   } else cnt = 1;
		for(k = 0; k < cnt; k++)	
			ell_mul_int_brent(pt1, pri, An, Ad, N); 
	}
    k = 19;
	while (k<B)
	{
		if (primeq(k))
		{
			ell_mul_int_brent(pt1, k, An, Ad, N);
			if (k<100)
					ell_mul_int_brent(pt1, k, An, Ad, N);
			if (cnt++ %100==0)
					dot();
		}
		k += 2;
	}
    if(verbosity) { printf("\n"); fflush(stdout); }

/* Next, test stage-1 attempt. */	
    gtog(pt1->z, fact);
	gcdg(N, fact);
    if((!isone(fact)) && (gcompg(N, fact) != 0)) {
		divg(fact, N);
		return(1);
	}
	if(B >= C) {  /* No stage 2 planned. */
		itog(1, fact);
		return(0);
	}

/* Commence second stage of ECM. */
    if(verbosity) {
	  	printf("\n"); 
		printf("Commencing stage 2 of ECM...\n");
		fflush(stdout);
    }
	if(pb[0] == NULL) {
		for(k=0; k < D+2; k++) {
				pb[k] = new_point_mont(curshorts);
				xzb[k] = newgiant(curshorts);

		}
	}
	k = ((int)B)/D;
	ptop_mont(pt1, pb[0]);
	ell_mul_int_brent(pb[0], k*D+1 , An, Ad, N);
	ptop_mont(pt1, pb[D+1]);
	ell_mul_int_brent(pb[D+1], (k+2)*D+1 , An, Ad, N);

	for (j=1; j <= D; j++)
	{
		ptop_mont(pt1, pb[j]);
		ell_mul_int_brent(pb[j], 2*j , An, Ad, N);
		gtog(pb[j]->z, xzb[j]);
		mulg(pb[j]->x, xzb[j]);
		special_modg(N, xzb[j]);
	}
	itog(1, fact);
	count = 0;
	while (1) {
		if(verbosity) if((++count) % 10 == 0) dot();
		gtog(pb[0]->z, xzb[0]);
		mulg(pb[0]->x, xzb[0]);
		special_modg(N, xzb[0]);
		mulg(pb[0]->z, fact);
		special_modg(N, fact); /* Accumulate. */
		for (j = 1; j < D; j++) {
				if (!primeq(k*D+1+2*j)) continue;
/* Next, accumulate (xa - xb)(za + zb) - xa za + xb zb. */
				gtog(pb[0]->x, t1);
				subg(pb[j]->x, t1);
				gtog(pb[0]->z, t2);
				addg(pb[j]->z, t2);
				mulg(t1, t2);
				special_modg(N, t1);
				subg(xzb[0], t2);
				addg(xzb[j], t2);
				special_modg(N, t2);
				mulg(t2, fact);
				special_modg(N, fact);
		}
		k += 2;
		if(k*D > C)
		break;
		ptop_mont(pb[D+1], pt2);
		ell_odd_brent(pb[D], pb[D+1], pb[0], N);
		ptop_mont(pt2, pb[0]);
	}
    if(verbosity) { printf("\n"); fflush(stdout); }

	gcdg(N, fact);
    if((!isone(fact)) && (gcompg(N, fact) != 0)) {
		divg(fact, N);
		return(2);  /* Return value of 2 for stage-2 success! */
	}
	itog(1, fact);
	return(0);	
}		

	
