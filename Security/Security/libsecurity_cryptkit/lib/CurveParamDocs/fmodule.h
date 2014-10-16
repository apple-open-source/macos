/**************************************************************
 *
 *	fmodule.h
 *
 *	Header file for fmodule.c.
 *
 *	Updates:
 *		13 Apr 98   REC - creation
 *
 *	c. 1998 Perfectly Scientific, Inc.
 *	All Rights Reserved.
 *
 *
 *************************************************************/

#define GENERAL_MOD 0
#define FERMAT_MOD 1
#define MERSENNE_MOD (-1)

int
init_fmodule(int shorts);

void
s_modg(
	giant		N,
	giant		t
);

unsigned short *
prime_list();

unsigned short *
exponent_list();

int
sieve(giant N, int sievelim);
