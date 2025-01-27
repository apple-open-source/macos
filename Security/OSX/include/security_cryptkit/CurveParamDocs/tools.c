/**************************************************************
 *
 *	tools.c
 *
 *	Number-theoretical algorithm implementations
 *
 *	Updates:
 *     30 Apr 99    REC  Modified init_tools type to void.
 *		3 Apr 98    REC  Creation
 *
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
#include "tools.h"

/* definitions */

#define STACK_COUNT 20

/* global variables */

int 	pr[NUM_PRIMES];   /* External use allowed. */
static giant tmp[STACK_COUNT];
static int stack = 0;
static giant popg();
static void pushg();

/**************************************************************
 *
 *	Maintenance functions
 *
 **************************************************************/


void
init_tools(int shorts) 
{	
	int j;

	for(j = 0; j < STACK_COUNT; j++) {
		tmp[j] = newgiant(shorts);
	}
	make_primes();  /* Create table of all primes < 2^16,
					   to be used by other programs as array 
					   pr[0..NUM_PRIMES]. */
}

static giant
popg() {
	return(tmp[stack++]);
}

static void
pushg(int n) {
	stack -= n;
}

/**************************************************************
 *
 *	Number-theoretical functions
 *
 **************************************************************/

int
prime_literal(
	unsigned int	p
)
/* Primality test via small, literal sieve. 
   After init, one should use primeq() instead.
 */
{
	unsigned  int	j=3;

	if ((p & 1)==0)
		return ((p == 2)?1:0);
	if (j >= p)
		return (1);
	while ((p%j)!=0)
	{
		j += 2;
		if (j*j > p)
			return(1);
	}
	return(0);
}

int
primeq(
	unsigned int odd
)
/* Faster primality test, using preset array pr[] of primes. 
   This test is valid for all unsigned, 32-bit integers odd.
 */
{
	unsigned int p;
	unsigned int j;

    if(odd < 2) return (0);
	if ((odd & 1)==0)
		return ((odd == 2)?1:0);
	for (j=1; ;j++)
	{
		p = pr[j];
		if (p*p > odd)
			return(1);
		if (odd % p == 0)
			return(0);
	}
}

void
make_primes()
{   int k, npr;
	pr[0] = 2;
	for (k=0, npr=1;; k++)
	{
		if (prime_literal(3+2*k))
		{
			pr[npr++] = 3+2*k;
			if (npr >= NUM_PRIMES)
				break;
		}
	}
}

int
prime_probable(giant p)
/* Invoke Miller-Rabin test of given security depth. 
   For MILLER_RABIN_DEPTH == 8, this is an ironclad primality
   test for suspected primes p < 3.4 x 10^{14}.
*/
{
	giant t1 = popg(), t2 = popg(), t3 = popg();
    int j, ct, s;

    if((p->n[0] & 1) == 0) {  /* Evenness test. */
		pushg(3); return(0);
	}
    if(bitlen(p) < 32) {  /* Single-word case. */
		pushg(3);
		return(primeq((unsigned int)gtoi(p)));
	}
	itog(-1, t1);
	addg(p, t1);  /* t1 := p-1. */
	gtog(t1, t2);
	s = 1;
	gshiftright(1, t2);
	while(t2->n[0] & 1 == 0) {
		gshiftright(1, t2);
		++s;
	}
	/* Now, p-1 = 2^s * t2. */
	for(j = 0; j < MILLER_RABIN_DEPTH; j++) {
		itog(pr[j+1], t3);	
		powermodg(t3, t2, p);
		ct = 1;
		if(isone(t3)) continue;
		if(gcompg(t3, t1) == 0) continue;
		while((ct < s) && (gcompg(t1, t3) != 0)) {
			squareg(t3); modg(p, t3);
			if(isone(t3)) {
				goto composite;
			}
			++ct;
		}
		if(gcompg(t1, t3) != 0) goto composite;
	}
	goto prime;

composite:
		pushg(3); return(0);
prime:  pushg(3); return(1);
}

int
jacobi_symbol(giant a, giant n)
/* Standard Jacobi symbol (a/n).  Parameter n must be odd, positive. */
{	int t = 1, u;
	giant t5 = popg(), t6 = popg(), t7 = popg();

	gtog(a, t5); modg(n, t5);
	gtog(n, t6);
	while(!isZero(t5)) {
	    u = (t6->n[0]) & 7;
		while((t5->n[0] & 1) == 0) {
			gshiftright(1, t5);
			if((u==3) || (u==5)) t = -t;
		}
		gtog(t5, t7); gtog(t6, t5); gtog(t7, t6);
		u = (t6->n[0]) & 3;
		if(((t5->n[0] & 3) == 3) && ((u & 3) == 3)) t = -t;
		modg(t6, t5);
	}
	if(isone(t6)) {
		pushg(3);
		return(t);
	}
	pushg(3);
	return(0);
}

int
pseudoq(giant a, giant p)
/* Query whether a^(p-1) = 1 (mod p). */
{
	int x;
	giant t1 = popg(), t2 = popg();

	gtog(p, t1); itog(1, t2); subg(t2, t1);
	gtog(a, t2);
	powermodg(t2, t1, p);
	x = isone(t2);
	pushg(2);
	return(x);
}

int
pseudointq(int a, giant p)
/* Query whether a^(p-1) = 1 (mod p). */
{
	int x;
	giant t4 = popg();

	itog(a, t4);
	x = pseudoq(t4, p);
	pushg(1);
	return(x);
}


void
powFp2(giant a, giant b, giant w2, giant n, giant p)
/* Perform powering in the field F_p^2:
   a + b w := (a + b w)^n (mod p), where parameter w2 is a quadratic
   nonresidue (formally equal to w^2).
 */
{   int j;
    giant t6 = popg(), t7 = popg(), t8 = popg(), t9 = popg();

	if(isZero(n)) {
		itog(1,a);
		itog(0,b);
		pushg(4);
		return;
	}
	gtog(a, t8); gtog(b, t9);
	for(j = bitlen(n)-2; j >= 0; j--) {
		gtog(b, t6);
		mulg(a, b); addg(b,b); modg(p, b);  /* b := 2 a b. */
		squareg(t6); modg(p, t6);
		mulg(w2, t6); modg(p, t6);
		squareg(a); addg(t6, a); modg(p, a); /* a := a^2 + b^2 w2. */
		if(bitval(n, j)) {
			gtog(b, t6); mulg(t8, b); modg(p, b);
			gtog(a, t7); mulg(t9, a); addg(a, b); modg(p, b);
			mulg(t9, t6); modg(p, t6); mulg(w2, t6); modg(p, t6);
			mulg(t8, a); addg(t6, a); modg(p, a);
		}
	}
	pushg(4);
	return;
}

int
sqrtmod(giant p, giant x)
/* If Sqrt[x] (mod p) exists, function returns 1, else 0.
   In either case x is modified, but if 1 is returned,
   x:= Sqrt[x] (mod p). 
 */
{   giant t0 = popg(), t1 = popg(), t2 = popg(), t3 = popg(),
		  t4 = popg();

	modg(p, x);   /* Justify the argument. */
    gtog(x, t0);  /* Store x for eventual validity check on square root. */
    if((p->n[0] & 3) == 3) {  /* The case p = 3 (mod 4). */
		gtog(p, t1);
		iaddg(1, t1); gshiftright(2, t1);
		powermodg(x, t1, p);
		goto resolve;
	}
/* Next, handle case p = 5 (mod 8). */
    if((p->n[0] & 7) == 5) {
		gtog(p, t1); itog(1, t2);
		subg(t2, t1); gshiftright(2, t1);
		gtog(x, t2);
		powermodg(t2, t1, p);  /* t2 := x^((p-1)/4) % p. */
		iaddg(1, t1);  
		gshiftright(1, t1); /* t1 := (p+3)/8. */
		if(isone(t2)) {
			powermodg(x, t1, p);  /* x^((p+3)/8) is root. */
			goto resolve;
		} else {
			itog(1, t2); subg(t2, t1);  /* t1 := (p-5)/8. */
			gshiftleft(2,x);
			powermodg(x, t1, p);
			mulg(t0, x); addg(x, x); modg(p, x); /* 2x (4x)^((p-5)/8. */
			goto resolve;
		}
	}	

/* Next, handle tougher case: p = 1 (mod 8). */
	itog(2, t1);
	while(1) {  /* Find appropriate nonresidue. */
		gtog(t1, t2);
		squareg(t2); subg(x, t2); modg(p, t2);
		if(jacobi_symbol(t2, p) == -1) break;
		iaddg(1, t1);
	}  /* t2 is now w^2 in F_p^2. */
    itog(1, t3);
    gtog(p, t4); iaddg(1, t4); gshiftright(1, t4);
	powFp2(t1, t3, t2, t4, p);
	gtog(t1, x);

resolve:
    gtog(x,t1); squareg(t1); modg(p, t1);
    if(gcompg(t0, t1) == 0) {
		pushg(5);
		return(1);  /* Success. */
	}
	pushg(5);
	return(0);  /* No square root. */
}

void
sqrtg(giant n)
/* n:= Floor[Sqrt[n]]. */
{   giant t5 = popg(), t6 = popg();

	itog(1, t5); gshiftleft(1 + bitlen(n)/2, t5);
	while(1) {
		gtog(n, t6);
		divg(t5, t6);
		addg(t5, t6); gshiftright(1, t6);
	  	if(gcompg(t6, t5) >= 0) break;
  		gtog(t6, t5);
  	}
    gtog(t5, n);
	pushg(2);
}

int
cornacchia4(giant n, int d, giant u, giant v)
/* Seek representation 4n = u^2 + |d| v^2, 
   for (negative) discriminant d and n > |D|/4.
   Parameter u := 0 and 0 is returned, if no representation is found;
   else 1 is returned and u, v properly set.
 */
{   int r = n->n[0] & 7, sym;
	giant t1 = popg(), t2 = popg(), t3 = popg(), t4 = popg();

	itog(d, t1);
	if((n->n[0]) & 7 == 1) {  /* n = 1 (mod 8). */
		sym = jacobi_symbol(t1,n);
		if(sym != 1) {
			itog(0,u);
			pushg(4);
			return(0);
		}
		gtog(t1, t2);
		sqrtmod(n, t2);  /* t2 := Sqrt[d] (mod n). */
    } else {  /* Avoid separate Jacobi/Legendre test. */
		gtog(t1, t2);
		if(sqrtmod(n, t2) == 0) { 
			itog(0, u);
			pushg(4);
			return(0);
		}
	}
/* t2 is now a valid square root of d (mod n). */
	gtog(t2, t3);
	subg(t1, t3); /* t3 := t2 - d. */
	if((t3->n[0] & 1) == 1) {
		negg(t2);
		addg(n, t2);
	} 
	gtog(n, t3); addg(t3, t3);  /* t3 := 2n. */
	gtog(n, t4); gshiftleft(2, t4); sqrtg(t4); /* t4 = [Sqrt[4 p]]. */
	while(gcompg(t2, t4) > 0) {
		gtog(t3, t1);
		gtog(t2, t3);
		gtog(t1, t2);
		modg(t3, t2);
	}
	gtog(n, t4); gshiftleft(2, t4);
	gtog(t2, t3); squareg(t3);
	subg(t3, t4); /* t4 := 4n - t2^2. */
	gtog(t4, t3);
	itog(d, t1); absg(t1);
	modg(t1, t3);
	if(!isZero(t3)) {
		itog(0,u);
		pushg(4);
		return(0);
	}
	divg(t1, t4); 
	gtog(t4, t1); 
	sqrtg(t4); /* t4 := [Sqrt[t4/Abs[d]]]. */
	gtog(t4, t3);
	squareg(t3);
	if(gcompg(t3, t1) != 0) {
		itog(0, u);
		pushg(4);
		return(0);
	}
	gtog(t2, u);
	gtog(t4, v);
	pushg(4);
	return(1);
}

/*
rep[p_, d_] := Module[{t, x0, a, b, c},
		If[JacobiSymbol[d,p] != 1, Return[{0,0}]];
		x0 = sqrt[d, p];
		If[Mod[x0-d,2] == 1, x0 = p-x0];
		a = 2p; b = x0; c = sqrtint[4 p];
		While[b > c, {a,b} = {b, Mod[a,b]}];
		t = 4p - b^2;
		If[Mod[t,Abs[d]] !=0, Return[{0,0}]];
		v = t/Abs[d];
		u = sqrtint[v]; 
		If[u^2 != v, Return[{0,0}]];
		Return[{b, u}]
		];
*/ 


