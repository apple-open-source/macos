/**************************************************************
 *
 *	tools.h
 *
 *	Header file for tools.c.
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

#define NUM_PRIMES 6542 /* PrimePi[2^16]. */
#define MILLER_RABIN_DEPTH (8)

void
init_tools(int shorts);

void
make_primes();

int
prime_literal(
	unsigned int	p
);

int
primeq(
	unsigned int 	odd
);

void
make_primes();

int
prime_probable(giant p);

int
jacobi_symbol(giant a, giant n);

int
pseudoq(giant a, giant p);

int
pseudointq(int a, giant p);


void
powFp2(giant a, giant b, giant w2, giant n, giant p);

int
sqrtmod(giant p, giant x);

void
sqrtg(giant n);

int
cornacchia4(giant n, int d, giant u, giant v);


