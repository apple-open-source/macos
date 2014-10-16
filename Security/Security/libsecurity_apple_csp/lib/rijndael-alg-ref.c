/*
 * Copyright (c) 2000-2001,2011-2012,2014 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/* rijndael-alg-ref.c   v2.0   August '99
 * Reference ANSI C code
 * authors: Paulo Barreto
 *          Vincent Rijmen
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rijndael-alg-ref.h"
#include <cspdebugging.h>

#define SC	((BC - 4) >> 1)

#include "boxes-ref.h"

static const word8 shifts[3][4][2] = {
 { { 0, 0 },
   { 1, 3 },
   { 2, 2 },
   { 3, 1 }
 },
 { { 0, 0 },
   { 1, 5 },
   { 2, 4 },
   { 3, 3 }
 },
 { { 0, 0 },
   { 1, 7 },
   { 3, 5 },
   { 4, 4 }
 }
}; 

#if 	!GLADMAN_AES_128_ENABLE

/* 128 bit key/word shift table in bits */
static const word8 shifts128[4][2] = {
 { 0,  0 },
 { 8,  24 },
 { 16, 16 },
 { 24, 8 }
};

#endif	/* GLADMAN_AES_128_ENABLE */

#if		!AES_MUL_BY_LOOKUP
/*
 * Profiling measurements showed that the mul routine is where a large propertion of 
 * the time is spent. Since the first argument to  mul is always one of six 
 * constants (2, 3, 0xe, etc.), we implement six 256x256 byte lookup tables to 
 * do the multiplies. This eliminates the need for the log/antilog tables, so
 * it's only adding one kilobyte of const data. Throughput improvement for this
 * mod is a factor of 3.3 for encrypt and 4.1 for decrypt in the 128-bit optimized
 * case. Improvement for the general case (with a 256 bit key) is 1.46 for encrypt
 * and 1.88 for decrypt. (Decrypt wins more for this enhancement because the 
 * InvMixColumn does four muls, vs. 2 muls for MixColumn). Measurements taken
 * on a 500 MHz G4 with 1 MB of L2 cache. 
 */

/*
 * The mod 255 op in mul is really expensive...
 *
 * We know that b <= (254 * 2), so there are only two cases. Either return b, 
 * or return b-255. 
 *
 * On a G4 this single optimization results in a 24% speedup for encrypt and 
 * a 25% speedup for decrypt. 
 */
static inline word8 mod255(word32 b)
{
	if(b >= 255) {
		b -= 255;
	}
	return b;
}

word8 mul(word8 a, word8 b) {
   /* multiply two elements of GF(2^m)
    * needed for MixColumn and InvMixColumn
    */
	if (a && b) return Alogtable[mod255(Logtable[a] + Logtable[b])];
	else return 0;
}
#endif	/* !AES_MUL_BY_LOOKUP */

static
void KeyAddition(word8 a[4][MAXBC], word8 rk[4][MAXBC], word8 BC) {
	/* Exor corresponding text input and round key input bytes
	 */
	int i, j;
	
	for(i = 0; i < 4; i++)
   		for(j = 0; j < BC; j++) a[i][j] ^= rk[i][j];
}

static
void ShiftRow(word8 a[4][MAXBC], word8 d, word8 BC) {
	/* Row 0 remains unchanged
	 * The other three rows are shifted a variable amount
	 */
	word8 tmp[MAXBC];
	int i, j;
	
	for(i = 1; i < 4; i++) {
		for(j = 0; j < BC; j++) tmp[j] = a[i][(j + shifts[SC][i][d]) % BC];
		for(j = 0; j < BC; j++) a[i][j] = tmp[j];
	}
}

static
void Substitution(word8 a[4][MAXBC], const word8 box[256], word8 BC) {
	/* Replace every byte of the input by the byte at that place
	 * in the nonlinear S-box
	 */
	int i, j;
	
	for(i = 0; i < 4; i++)
		for(j = 0; j < BC; j++) a[i][j] = box[a[i][j]] ;
}
   
static
void MixColumn(word8 a[4][MAXBC], word8 BC) {
	/* Mix the four bytes of every column in a linear way
	 */
	word8 b[4][MAXBC];
	int i, j;
		
	for(j = 0; j < BC; j++) {
		for(i = 0; i < 4; i++) {
			#if		AES_MUL_BY_LOOKUP
			b[i][j] = mulBy0x02[a[i][j]]
				^ mulBy0x03[a[(i + 1) % 4][j]]
				^ a[(i + 2) % 4][j]
				^ a[(i + 3) % 4][j];
			#else
			b[i][j] = mul(2,a[i][j])
				^ mul(3,a[(i + 1) % 4][j])
				^ a[(i + 2) % 4][j]
				^ a[(i + 3) % 4][j];
			#endif
		}
	}
	for(i = 0; i < 4; i++) {
		for(j = 0; j < BC; j++) a[i][j] = b[i][j];
	}
}

static
void InvMixColumn(word8 a[4][MAXBC], word8 BC) {
	/* Mix the four bytes of every column in a linear way
	 * This is the opposite operation of Mixcolumn
	 */
	word8 b[4][MAXBC];
	int i, j;
	
	for(j = 0; j < BC; j++) {
		for(i = 0; i < 4; i++) {         
			#if		AES_MUL_BY_LOOKUP
			b[i][j] = mulBy0x0e[a[i][j]]
				^ mulBy0x0b[a[(i + 1) % 4][j]]
				^ mulBy0x0d[a[(i + 2) % 4][j]]
				^ mulBy0x09[a[(i + 3) % 4][j]];     
			#else
			b[i][j] = mul(0xe,a[i][j])
				^ mul(0xb,a[(i + 1) % 4][j])                 
				^ mul(0xd,a[(i + 2) % 4][j])
				^ mul(0x9,a[(i + 3) % 4][j]);     
			#endif
		}
	}
	for(i = 0; i < 4; i++) {
		for(j = 0; j < BC; j++) a[i][j] = b[i][j];
	}
}

int rijndaelKeySched (
	word8 k[4][MAXKC], 
	int keyBits, 
	int blockBits, 
	word8 W[MAXROUNDS+1][4][MAXBC]) {
	
	/* Calculate the necessary round keys
	 * The number of calculations depends on keyBits and blockBits
	 */
	int KC, BC, ROUNDS;
	int i, j, t, rconpointer = 0;
	word8 tk[4][MAXKC];   

	switch (keyBits) {
	case 128: KC = 4; break;
	case 192: KC = 6; break;
	case 256: KC = 8; break;
	default : return (-1);
	}

	switch (blockBits) {
	case 128: BC = 4; break;
	case 192: BC = 6; break;
	case 256: BC = 8; break;
	default : return (-2);
	}

	switch (keyBits >= blockBits ? keyBits : blockBits) {
	case 128: ROUNDS = 10; break;
	case 192: ROUNDS = 12; break;
	case 256: ROUNDS = 14; break;
	default : return (-3); /* this cannot happen */
	}

	
	for(j = 0; j < KC; j++)
		for(i = 0; i < 4; i++)
			tk[i][j] = k[i][j];
	t = 0;
	/* copy values into round key array */
	for(j = 0; (j < KC) && (t < (ROUNDS+1)*BC); j++, t++)
		for(i = 0; i < 4; i++) W[t / BC][i][t % BC] = tk[i][j];
		
	while (t < (ROUNDS+1)*BC) { /* while not enough round key material calculated */
		/* calculate new values */
		for(i = 0; i < 4; i++)
			tk[i][0] ^= S[tk[(i+1)%4][KC-1]];
		tk[0][0] ^= rcon[rconpointer++];

		if (KC != 8)
			for(j = 1; j < KC; j++)
				for(i = 0; i < 4; i++) tk[i][j] ^= tk[i][j-1];
		else {
			for(j = 1; j < KC/2; j++)
				for(i = 0; i < 4; i++) tk[i][j] ^= tk[i][j-1];
			for(i = 0; i < 4; i++) tk[i][KC/2] ^= S[tk[i][KC/2 - 1]];
			for(j = KC/2 + 1; j < KC; j++)
				for(i = 0; i < 4; i++) tk[i][j] ^= tk[i][j-1];
	}
	/* copy values into round key array */
	for(j = 0; (j < KC) && (t < (ROUNDS+1)*BC); j++, t++)
		for(i = 0; i < 4; i++) W[t / BC][i][t % BC] = tk[i][j];
	}		

	return 0;
}
      
int rijndaelEncrypt (
	word8 a[4][MAXBC], 
	int keyBits, 
	int blockBits, 
	word8 rk[MAXROUNDS+1][4][MAXBC])
{
	/* Encryption of one block, general case. 
	 */
	int r, BC, ROUNDS;

	switch (blockBits) {
	case 128: BC = 4; break;
	case 192: BC = 6; break;
	case 256: BC = 8; break;
	default : return (-2);
	}

	switch (keyBits >= blockBits ? keyBits : blockBits) {
	case 128: ROUNDS = 10; break;
	case 192: ROUNDS = 12; break;
	case 256: ROUNDS = 14; break;
	default : return (-3); /* this cannot happen */
	}

	/* begin with a key addition
	 */
	KeyAddition(a,rk[0],BC); 

	/* ROUNDS-1 ordinary rounds
	 */
	for(r = 1; r < ROUNDS; r++) {
		Substitution(a,S,BC);
		ShiftRow(a,0,BC);
		MixColumn(a,BC);
		KeyAddition(a,rk[r],BC);
	}
	
	/* Last round is special: there is no MixColumn
	 */
	Substitution(a,S,BC);
	ShiftRow(a,0,BC);
	KeyAddition(a,rk[ROUNDS],BC);

	return 0;
}   

int rijndaelDecrypt (
	word8 a[4][MAXBC], 
	int keyBits, 
	int blockBits, 
	word8 rk[MAXROUNDS+1][4][MAXBC])
{
	int r, BC, ROUNDS;
	
	switch (blockBits) {
	case 128: BC = 4; break;
	case 192: BC = 6; break;
	case 256: BC = 8; break;
	default : return (-2);
	}

	switch (keyBits >= blockBits ? keyBits : blockBits) {
	case 128: ROUNDS = 10; break;
	case 192: ROUNDS = 12; break;
	case 256: ROUNDS = 14; break;
	default : return (-3); /* this cannot happen */
	}

	/* To decrypt: apply the inverse operations of the encrypt routine,
	 *             in opposite order
	 * 
	 * (KeyAddition is an involution: it 's equal to its inverse)
	 * (the inverse of Substitution with table S is Substitution with the 
	 *  inverse table of S)
	 * (the inverse of Shiftrow is Shiftrow over a suitable distance)
	 */

	/* First the special round:
	 *   without InvMixColumn
	 *   with extra KeyAddition
	 */
	KeyAddition(a,rk[ROUNDS],BC);
	Substitution(a,Si,BC);
	ShiftRow(a,1,BC);              
	
	/* ROUNDS-1 ordinary rounds
	 */
	for(r = ROUNDS-1; r > 0; r--) {
		KeyAddition(a,rk[r],BC);
		InvMixColumn(a,BC);      
		Substitution(a,Si,BC);
		ShiftRow(a,1,BC);                
	}
	
	/* End with the extra key addition
	 */
	
	KeyAddition(a,rk[0],BC);    

	return 0;
}

#if		!GLADMAN_AES_128_ENABLE

/*
 * All of these 128-bit-key-and-block routines require 32-bit word-aligned 
 * char array pointers.ÊThe key schedule arrays are easy; they come from
 * keyInstance which has a 4-byte-aligned element preceeding the key schedule.
 * Others require manual alignment of a local variable by the caller.
 */

static inline void KeyAddition128(
	word8 a[4][BC_128_OPT], 
	word8 rk[4][MAXBC]) {
	
	/* these casts are endian-independent */
	((word32 *)a)[0] ^= *((word32 *)(&rk[0]));
	((word32 *)a)[1] ^= *((word32 *)(&rk[1]));
	((word32 *)a)[2] ^= *((word32 *)(&rk[2]));
	((word32 *)a)[3] ^= *((word32 *)(&rk[3]));
}

static void Substitution128(
	word8 a[4][BC_128_OPT], 
	const word8 box[256]) {
	/* Replace every byte of the input by the byte at that place
	 * in the nonlinear S-box
	 */
	int i, j;
	
	/* still to be optimized - larger S boxes? */
	for(i = 0; i < 4; i++) {
		for(j = 0; j < BC_128_OPT; j++) {
			a[i][j] = box[a[i][j]];
		}
	}
}

#if	defined(__ppc__) && defined(__GNUC__)

static inline void rotateWordLeft(
	word8 *word,			// known to be word aligned
	unsigned rotCount)		// in bits
{
	word32 lword = *((word32 *)word);
	asm("rlwnm %0,%1,%2,0,31" : "=r"(lword) : "0"(lword), "r"(rotCount));
	*((word32 *)word) = lword;
}

#else

/* 
 * Insert your machine/compiler dependent code here,
 * or just use this, which works on any platform and compiler
 * which supports the __attribute__((aligned(4))) directive. 
 */
static void rotateWordLeft(
	word8 *word,			// known to be word aligned
	unsigned rotCount)		// in bits
{
	word8 tmp[BC_128_OPT] __attribute__((aligned(4)));
	unsigned bytes = rotCount / 8;
	
	tmp[0] = word[bytes     & (BC_128_OPT-1)];
	tmp[1] = word[(1+bytes) & (BC_128_OPT-1)];
	tmp[2] = word[(2+bytes) & (BC_128_OPT-1)];
	tmp[3] = word[(3+bytes) & (BC_128_OPT-1)];
	*((word32 *)word) = *((word32 *)tmp);
}
#endif

static inline void ShiftRow128(
	word8 a[4][BC_128_OPT], 
	word8 d) {
	/* Row 0 remains unchanged
	 * The other three rows are shifted (actually rotated) a variable amount
	 */
	int i;
	
	for(i = 1; i < 4; i++) {
		rotateWordLeft(a[i], shifts128[i][d]);
	}
}

/*
 * The following two routines are where most of the time is spent in this
 * module. Further optimization would have to focus here. 
 */
static void MixColumn128(word8 a[4][BC_128_OPT]) {
	/* Mix the four bytes of every column in a linear way
	 */
	word8 b[4][BC_128_OPT];
	int i, j;
	
	for(j = 0; j < BC_128_OPT; j++) {
		for(i = 0; i < 4; i++) {
			#if		AES_MUL_BY_LOOKUP
			b[i][j] = mulBy0x02[a[i][j]]
				^ mulBy0x03[a[(i + 1) % 4][j]]
				^ a[(i + 2) % 4][j]
				^ a[(i + 3) % 4][j];
			#else
			b[i][j] = mul(2,a[i][j])
				^ mul(3,a[(i + 1) % 4][j])
				^ a[(i + 2) % 4][j]
				^ a[(i + 3) % 4][j];
			#endif
		}
	}
	memmove(a, b, 4 * BC_128_OPT);
}

static void InvMixColumn128(word8 a[4][BC_128_OPT]) {
	/* Mix the four bytes of every column in a linear way
	 * This is the opposite operation of Mixcolumn
	 */
	word8 b[4][BC_128_OPT];
	int i, j;
	
	for(j = 0; j < BC_128_OPT; j++) {
		for(i = 0; i < 4; i++) {  
			#if		AES_MUL_BY_LOOKUP
			b[i][j] = mulBy0x0e[a[i][j]]
				^ mulBy0x0b[a[(i + 1) % 4][j]]
				^ mulBy0x0d[a[(i + 2) % 4][j]]
				^ mulBy0x09[a[(i + 3) % 4][j]];     
			#else
			b[i][j] = mul(0xe,a[i][j])
				^ mul(0xb,a[(i + 1) % 4][j])                 
				^ mul(0xd,a[(i + 2) % 4][j])
				^ mul(0x9,a[(i + 3) % 4][j]);     
			#endif
		}
	}
	memmove(a, b, 4 * BC_128_OPT);
}

int rijndaelKeySched128 (
	word8 k[4][KC_128_OPT], 
	word8 W[MAXROUNDS+1][4][MAXBC]) {
	
	/* Calculate the necessary round keys
	 * The number of calculations depends on keyBits and blockBits
	 */
	int i, j, t, rconpointer = 0;
	word8 tk[4][KC_128_OPT];   
	unsigned numSchedRows = (ROUNDS_128_OPT + 1) * BC_128_OPT;
	
	for(j = 0; j < KC_128_OPT; j++)
		for(i = 0; i < 4; i++)
			tk[i][j] = k[i][j];
	t = 0;
	/* copy values into round key array */
	for(j = 0; (j < KC_128_OPT) && (t < numSchedRows); j++, t++) {
		for(i = 0; i < 4; i++) {
			W[t / BC_128_OPT][i][t % BC_128_OPT] = tk[i][j];
		}
	}
		
	while (t < numSchedRows) { 
		/* while not enough round key material calculated */
		/* calculate new values */
		for(i = 0; i < 4; i++) {
			tk[i][0] ^= S[tk[(i+1)%4][KC_128_OPT-1]];
		}
		tk[0][0] ^= rcon[rconpointer++];

		for(j = 1; j < KC_128_OPT; j++) {
			for(i = 0; i < 4; i++) {
				tk[i][j] ^= tk[i][j-1];
			}
		}

		/* copy values into round key array */
		for(j = 0; (j < KC_128_OPT) && (t < numSchedRows); j++, t++) {
			for(i = 0; i < 4; i++) {
				W[t / BC_128_OPT][i][t % BC_128_OPT] = tk[i][j];
			}
		}
	}		

	return 0;
}

int rijndaelEncrypt128 (
	word8 a[4][BC_128_OPT], 
	word8 rk[MAXROUNDS+1][4][MAXBC])
{
	/* Encryption of one block. 
	 */
	int r;

	/* begin with a key addition
	 */
	KeyAddition128(a,rk[0]); 
	
	/* ROUNDS-1 ordinary rounds
	 */
	for(r = 1; r < ROUNDS_128_OPT; r++) {
		Substitution128(a,S);
		ShiftRow128(a,0);
		MixColumn128(a);
		KeyAddition128(a,rk[r]);
	}
	
	/* Last round is special: there is no MixColumn
	 */
	Substitution128(a,S);
	ShiftRow128(a,0);
	KeyAddition128(a,rk[ROUNDS_128_OPT]);

	return 0;
}   

int rijndaelDecrypt128 (
	word8 a[4][BC_128_OPT], 
	word8 rk[MAXROUNDS+1][4][MAXBC])
{
	int r;
	
	/* To decrypt: apply the inverse operations of the encrypt routine,
	 *             in opposite order
	 * 
	 * (KeyAddition is an involution: it 's equal to its inverse)
	 * (the inverse of Substitution with table S is Substitution with the 
	 *  inverse table of S)
	 * (the inverse of Shiftrow is Shiftrow over a suitable distance)
	 */

	/* First the special round:
	 *   without InvMixColumn
	 *   with extra KeyAddition
	 */
	KeyAddition128(a,rk[ROUNDS_128_OPT]);
	Substitution128(a,Si);
	ShiftRow128(a,1);              
	
	/* ROUNDS-1 ordinary rounds
	 */
	for(r = ROUNDS_128_OPT-1; r > 0; r--) {
		KeyAddition128(a,rk[r]);
		InvMixColumn128(a);      
		Substitution128(a,Si);
		ShiftRow128(a,1);                
	}
	
	/* End with the extra key addition
	 */
	
	KeyAddition128(a,rk[0]);    

	return 0;
}

#endif		/* !GLADMAN_AES_128_ENABLE */

