/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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


/*
 *  vRijndael-alg-ref.c
 *
 *  Copyright (c) 2001,2011,2014 Apple Inc. All Rights Reserved.
 *
 */

#include "rijndaelApi.h"
#include "rijndael-alg-ref.h"
#include "boxes-ref.h"
#include <string.h>

/* debugger seems to have trouble with this code... */
#define VAES_DEBUG	1
#if		VAES_DEBUG
#include <stdio.h>
#define vdprintf(s)		printf s
#else
#define vdprintf(s)
#endif

#define SC	((BC - 4) >> 1)

#if defined(__ppc__) && defined(ALTIVEC_ENABLE)

typedef union {
	unsigned char		s[4][8];
	unsigned long		l[8];
	vector unsigned char 	v[2];
} doubleVec;

typedef union {
	unsigned long		s[4];
	vector unsigned long	v;
} vecLong;

static word8 shifts[3][4][2] = {
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

int vRijndaelKeySched ( vector unsigned char vk[2], int keyBits, int blockBits, 
                unsigned char W[MAXROUNDS+1][4][MAXBC])
{
	/* Calculate the necessary round keys
	 * The number of calculations depends on keyBits and blockBits
	 */
	int KC, BC, ROUNDS;
	int i, j, t, rconpointer = 0;
	doubleVec tk;
	register  vector unsigned char v1, v2, mask;

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

	tk.v[0] = vk[0];
	tk.v[1] = vk[1];
	
	t = 0;
	/* copy values into round key array */
	for(j = 0; (j < KC) && (t < (ROUNDS+1)*BC); j++, t++)
		for(i = 0; i < 4; i++) W[t / BC][i][t % BC] = tk.s[i][j];
		
	while (t < (ROUNDS+1)*BC) { /* while not enough round key material calculated */
		/* calculate new values */
		for(i = 0; i < 4; i++)
			tk.s[i][0] ^= *((word8 *)S + tk.s[(i+1)%4][KC-1]);
		tk.s[0][0] ^= rcon[rconpointer++];

		if (KC != 8) {
			/* xor bytes 1-7 of each row with previous byte */
			mask = (vector unsigned char) ( 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff );
			for ( i = 0; i < 2; i++ ) {
				v1 = vec_sld( tk.v[i], tk.v[i], 15 );
				v2 = vec_and( v1, mask );
				tk.v[i] = vec_xor( tk.v[i], v2 );
			}
		}
		else {
			/* xor bytes 1-3 of each row with previous byte */
			mask = (vector unsigned char) ( 0, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0xff, 0xff, 0xff, 0, 0, 0, 0 );
			for ( i = 0; i < 2; i++ ) {
				v1 = vec_sld( tk.v[i], tk.v[i], 15 );
				v2 = vec_and( v1, mask );
				tk.v[i] = vec_xor( tk.v[i], v2 );
				for(j = 0; j < 4; j++) tk.s[i][KC/2] ^= *((word8 *)S + tk.s[i][KC/2 - 1]);
				/* xor bytes 5-7 of each row with previous byte */
				mask = vec_sld( mask, mask, 4 );
				v2 = vec_and( v1, mask );
				tk.v[i] = vec_xor( tk.v[i], v2 );
				mask = vec_sld( mask, mask, 4 );
			}
		}
		/* copy values into round key array */
		for(j = 0; (j < KC) && (t < (ROUNDS+1)*BC); j++, t++)
			for(i = 0; i < 4; i++) W[t / BC][i][t % BC] = tk.s[i][j];
	}		
	return 0;
}


void vMakeKey(BYTE *keyMaterial, keyInstance *key)
{
        register vector unsigned char v1, v2, v3, mask;
        vector unsigned char	  vk[2];

        /* load and align input */
        v1 = vec_ld( 0, (vector unsigned char *) keyMaterial );
        v2 = vec_ld( 16, (vector unsigned char *) keyMaterial );
        if ( (long) keyMaterial & 0x0fL )
        {	// this is required if keyMaterial is not on a 16-byte boundary
                v3 = vec_ld( 32, (vector unsigned char *) keyMaterial );
                mask = vec_lvsl( 0, keyMaterial );
                v1 = vec_perm( v1, v2, mask );
                v2 = vec_perm( v2, v3, mask );
        }

        /* parse input stream into rectangular array */
        vk[0] = vec_perm( v1, v2, (vector unsigned char) ( 0,  4,  8, 12, 16, 20, 24, 28,  1,  5,  9, 13, 17, 21, 25, 29 ) );
        vk[1] = vec_perm( v1, v2, (vector unsigned char) ( 2,  6, 10, 14, 18, 22, 26, 30,  3,  7, 11, 15, 19, 23, 27, 31 ) );
        vRijndaelKeySched (vk, key->keyLen, key->blockLen, key->keySched);
        memset( (char *) vk, 0, 4 * MAXKC);
}


/*	This routine does 16 simultaneous lookups in a 256-byte table.	*/
vector unsigned char rimskyKorsakov ( vector unsigned char v, vector unsigned char * table )
{
	register vector unsigned char	upperBits000, upperBits001, upperBits010, upperBits011,
                                        upperBits100, upperBits101, upperBits110, upperBits111,
                                        lookupBit00,  lookupBit01, lookupBit10, lookupBit11,
                                        lookupBit0, lookupBit1, lookup,
                                        maskForBit6, maskForBit7, maskForBit8, seven;
	register vector unsigned char	*tabeven, *tabodd;

	seven = vec_splat_u8 ( 7 );
	tabeven = table++;
	tabodd = table;

//	Each variable contains the correct values for the corresponding bits 6, 7 and 8.
	upperBits000 = vec_perm ( *tabeven, *tabodd, v );
	tabeven += 2; tabodd += 2;
	upperBits001 = vec_perm ( *tabeven, *tabodd, v );
	tabeven += 2; tabodd += 2;
	upperBits010 = vec_perm ( *tabeven, *tabodd, v );
	tabeven += 2; tabodd += 2;
	upperBits011 = vec_perm ( *tabeven, *tabodd, v );
	tabeven += 2; tabodd += 2;
	upperBits100 = vec_perm ( *tabeven, *tabodd, v ); 
	tabeven += 2; tabodd += 2;
	upperBits101 = vec_perm ( *tabeven, *tabodd, v );
	tabeven += 2; tabodd += 2;
	upperBits110 = vec_perm ( *tabeven, *tabodd, v );
	tabeven += 2; tabodd += 2;
	upperBits111 = vec_perm ( *tabeven, *tabodd, v );
	
//	Here we extract all the correct values for bit 6.
	maskForBit6  = vec_sl  ( v, vec_splat_u8 ( 2 ) );
	maskForBit6  = vec_sra ( maskForBit6, seven );
	lookupBit00 = vec_sel ( upperBits000, upperBits001, maskForBit6 );
	lookupBit01 = vec_sel ( upperBits010, upperBits011, maskForBit6 );
	lookupBit10 = vec_sel ( upperBits100, upperBits101, maskForBit6 );
	lookupBit11 = vec_sel ( upperBits110, upperBits111, maskForBit6 );

//	Then we get the correct values for bit 7.
	maskForBit7  = vec_sl  ( v, vec_splat_u8 ( 1 ) );
	maskForBit7  = vec_sra ( maskForBit7, seven );
	lookupBit0 = vec_sel ( lookupBit00, lookupBit01, maskForBit7 );
	lookupBit1 = vec_sel ( lookupBit10, lookupBit11, maskForBit7 );

//	Finally, the entire correct result vector.
	maskForBit8 = vec_sra ( v, seven );
	
	lookup = vec_sel ( lookupBit0, lookupBit1, maskForBit8 );
      
    return lookup;
}

vector unsigned char vmul(vector unsigned char a, vector unsigned char b)
{
	register vector unsigned char x, y, zero;
	register vector unsigned short xh, yh, zhi, zlo, two54, two55;
	
	zero = vec_splat_u8( 0 );
	two55 = vec_splat_u16( -1 );
	two55 = (vector unsigned short) vec_mergeh( zero, (vector unsigned char) two55 );
	two54 = vec_sub( two55, vec_splat_u16( 1 ) ); 

	x = rimskyKorsakov( a, (vector unsigned char *)Logtable );	// Logtable[a]
	y = rimskyKorsakov( b, (vector unsigned char *)Logtable );	// Logtable[b]

	//	Convert upper 8 bytes to shorts for addition ond modulo
	xh = (vector unsigned short) vec_mergeh( zero, x );
	yh = (vector unsigned short) vec_mergeh( zero, y );
	xh = vec_add( xh, yh );			// xh = Logtable[a] + Logtable[b]
	yh = vec_sub( xh, two55 );
	zhi = vec_sel( xh, yh, vec_cmpgt( xh, two54 ) );	// xh%255

	//	Convert lower 8 bytes to shorts for addition ond modulo
	xh = (vector unsigned short) vec_mergel( zero, x );
	yh = (vector unsigned short) vec_mergel( zero, y );
	xh = vec_add( xh, yh );
	yh = vec_sub( xh, two55 );
	zlo = vec_sel( xh, yh, vec_cmpgt( xh, two54 ) );

	x = vec_pack( zhi, zlo );			// recombine into single byte vector
	x = rimskyKorsakov( x, (vector unsigned char *)Alogtable );		// Alogtable[x]
	x = vec_sel( x, zero, vec_cmpeq( a, zero ) );	// check a = 0
	x = vec_sel( x, zero, vec_cmpeq( b, zero ) );	// check b = 0
	return x;
}

void vKeyAddition(vector unsigned char v[2], vector unsigned char rk[2])
{
	v[0] = vec_xor( v[0], rk[0] );		// first vector contains rows 0 and 1
	v[1] = vec_xor( v[1], rk[1] );		// second vector contains rows 2 and 3
}


void vShiftRow(vector unsigned char v[2], word8 d, word8 BC)
{
	vecLong			sh;
	register vector unsigned char mask, mask1, t;
	register vector bool char c;
	register int	i, j;
	
	sh.s[0] = 0;
	for (i = 1; i < 4; i++)
		sh.s[i] = shifts[SC][i][d] % BC;	//	contains the number of elements to shift each row
		
	// each vector contains two BC-byte long rows
	j = 0;
	for ( i = 0; i < 2; i++ ) {
		mask = vec_lvsl( 0, (int *) sh.s[j++]);		//	mask for even row
		mask1 = vec_lvsl( 0, (int *) sh.s[j++]);	//	mask for odd row
		if (BC == 4) {
			mask = vec_sld( mask, mask1, 8 );		//	combined rotation mask for both rows
			mask = vec_and( mask, vec_splat_u8( 3 ) );
		} else if (BC == 6) {
			mask = vec_sld( mask, mask, 8 );
			mask = vec_sld( mask, mask1, 8 );		//	combined rotation mask for both rows
			t = vec_sub( mask, vec_splat_u8( 6 ) );
			c = vec_cmpgt( mask, vec_splat_u8( 5 ) );
			mask = vec_sel( mask, t, c );
		} else {
			mask = vec_sld( mask, mask1, 8 );		//	combined rotation mask for both rows
			mask = vec_and( mask, vec_splat_u8( 7 ) );
		}
		mask1 = vec_sld( vec_splat_u8( 0 ), vec_splat_u8( 8 ), 8 );
		mask = vec_add( mask, mask1 );
		v[i] = vec_perm( v[i], v[i], mask );		//	rotate each row as required
	}
}

void vSubstitution( vector unsigned char v[2], vector unsigned char box[16] )
{
	v[0] = rimskyKorsakov( v[0], box );		// first vector contains rows 0 and 1
	v[1] = rimskyKorsakov( v[1], box );		// second vector contains rows 2 and 3
}
   
void vMixColumn(vector unsigned char v[2])
{
	//	vector 0 contains row 0 in bytes 0-7 and row 1 in bytes 8-f
	//	vector 1 contains row 2 in bytes 0-7 and row 3 in bytes 8-f

	register vector unsigned char a0, a1, a2, a3, b0, b1, b2, b3;
	register vector unsigned char two, three;
	
	two = vec_splat_u8( 2 );
	three = vec_splat_u8( 3 );

	a1 = vec_sld( v[0], v[1], 8 );		// equivalent to a[i+1] % 4
	b1 = vec_sld( v[1], v[0], 8 );
	a2 = vec_sld( a1, b1, 8 );		// equivalent to a[i+2] % 4
	b2 = vec_sld( b1, a1, 8 );
	a3 = vec_sld( a2, b2, 8 );		// equivalent to a[i+3] % 4
	b3 = vec_sld( b2, a2, 8 );
	
	//	Calculations for rows 0 and 1
	a0 = vmul( two, v[0] );				// mul(2,a[i][j])
	a0 = vec_xor( a0, vmul( three, a1 ) );		// ^ mul(3,a[(i + 1) % 4][j])
	a0 = vec_xor( a0, a2 );				// ^ a[(i + 2) % 4][j]
	v[0]  = vec_xor( a0, a3 );			// ^ a[(i + 3) % 4][j]

	//	Calculations for rows 2 and 3
	b0 = vmul( two, v[1] );
	b0 = vec_xor( b0, vmul( three, b1 ) );
	b0 = vec_xor( b0, b2 );
	v[1] = vec_xor( b0, b3 );
}

void vInvMixColumn(vector unsigned char v[2])
{
	//	vector 0 contains row 0 in bytes 0-7 and row 1 in bytes 8-f
	//	vector 1 contains row 2 in bytes 0-7 and row 3 in bytes 8-f

	register vector unsigned char a0, a1, a2, a3, b0, b1, b2, b3;
	register vector unsigned char nine, eleven, thirteen, fourteen;;
	
	nine = vec_splat_u8( 0x9 );
	eleven = vec_splat_u8( 0xb );
	thirteen = vec_splat_u8( 0xd );
	fourteen = vec_splat_u8( 0xe );

	a1 = vec_sld( v[0], v[1], 8 );			// equivalent to a[i+1] % 4
	b1 = vec_sld( v[1], v[0], 8 );
	a2 = vec_sld( a1, b1, 8 );			// equivalent to a[i+2] % 4
	b2 = vec_sld( b1, a1, 8 );
	a3 = vec_sld( a2, b2, 8 );			// equivalent to a[i+3] % 4
	b3 = vec_sld( b2, a2, 8 );
	
	//	Calculations for rows 0 and 1
	a0 = vmul( fourteen, v[0] );				// mul(0xe,a[i][j])
	a0 = vec_xor( a0, vmul( eleven, a1 ) );		// ^ mul(0xb,a[(i + 1) % 4][j])
	a0 = vec_xor( a0, vmul( thirteen, a2 ) );	// ^ mul(0xd,a[(i + 2) % 4][j])
	v[0]  = vec_xor( a0, vmul( nine, a3 ) );	// ^ mul(0x9,a[(i + 3) % 4][j])

	//	Calculations for rows 2 and 3
	b0 = vmul( fourteen, v[1] );
	b0 = vec_xor( b0, vmul( eleven, b1 ) );
	b0 = vec_xor( b0, vmul( thirteen, b2 ) );
	v[1]  = vec_xor( b0, vmul( nine, b3 ) );
}

int vRijndaelEncrypt (vector unsigned char a[2], int keyBits, int blockBits, vector unsigned char rk[MAXROUNDS+1][2])
{
	/* Encryption of one block. 
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

        vKeyAddition( a, rk[0] ); 
        for(r = 1; r < ROUNDS; r++) {
                vSubstitution( a, (vector unsigned char *)S);
                vShiftRow( a, 0, BC);
                vMixColumn( a );
                vKeyAddition( a, rk[r] ); 	
        }
        vSubstitution( a, (vector unsigned char *)S);
        vShiftRow( a, 0, BC);
        vKeyAddition( a, rk[ROUNDS] ); 

	return 0;
}   

int vRijndaelDecrypt (vector unsigned char a[2], int keyBits, int blockBits, vector unsigned char rk[MAXROUNDS+1][2])
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
		
        vKeyAddition( a, rk[ROUNDS] ); 
        vSubstitution( a, (vector unsigned char *)Si);
        vShiftRow( a, 1, BC);
        for(r = ROUNDS-1; r > 0; r--) {
                vKeyAddition( a, rk[r] ); 
                vInvMixColumn( a );
                vSubstitution( a, (vector unsigned char *)Si);
                vShiftRow( a, 1, BC);
        }
        vKeyAddition( a, rk[0] ); 

	return 0;
}

#if 0
/* Murley's code, to be deleted */
void vBlockEncrypt(cipherInstance *cipher, keyInstance *key, BYTE *input, int inputLen, BYTE *outBuffer)
{
        register vector unsigned char v1, v2, v3, v4, mask;
        register vector bool char cmp;

        /* load and align input */
        v1 = vec_ld( 0, (vector unsigned char *) input );
        v2 = vec_ld( 16, (vector unsigned char *) input );
        if ( (long) input & 0x0fL )
        {	// this is required if input is not on a 16-byte boundary
                v3 = vec_ld( 32, (vector unsigned char *) input );
                mask = vec_lvsl( 0, input );
                v1 = vec_perm( v1, v2, mask );
                v2 = vec_perm( v2, v3, mask );
        }

        /* parse input stream into rectangular array */
        v3 = vec_perm( v1, v2, (vector unsigned char) ( 0,  4,  8, 12, 16, 20, 24, 28,  1,  5,  9, 13, 17, 21, 25, 29 ) );
        v4 = vec_perm( v1, v2, (vector unsigned char) ( 2,  6, 10, 14, 18, 22, 26, 30,  3,  7, 11, 15, 19, 23, 27, 31 ) );

        /* store into cipher structure */
        if (cipher->mode == MODE_CBC) {
                v3 = vec_xor( v3, *((vector unsigned char *) cipher->chainBlock ) );
                v4 = vec_xor( v4, *((vector unsigned char *) cipher->chainBlock + 1 ) );
        }
        vec_st( v3, 0, (vector unsigned char *) cipher->chainBlock );
        vec_st( v4, 16, (vector unsigned char *) cipher->chainBlock );

        vRijndaelEncrypt((vector unsigned char *) cipher->chainBlock, key->keyLen, cipher->blockLen, (vector unsigned char *) key->keySched);

        v1 = vec_ld( 0, (vector unsigned char *) cipher->chainBlock );
        v2 = vec_ld( 16, (vector unsigned char *) cipher->chainBlock );

        /* parse rectangular array into output ciphertext bytes */
        v3 = vec_perm( v1, v2, (vector unsigned char) ( 0,  8, 16, 24,  1,  9, 17, 25,  2, 10, 18, 26,  3, 11, 19, 27 ) );
        v4 = vec_perm( v1, v2, (vector unsigned char) ( 4, 12, 20, 28,  5, 13, 21, 29,  6, 14, 22, 30,  7, 15, 23, 31 ) );

        if ( (long) outBuffer & 0x0fL )
        {
                /* store output data into a non-aligned buffer */
                mask = vec_lvsr( 0, outBuffer );
                cmp = vec_cmpgt( mask, vec_splat_u8( 0x0f ) );
                v1 = vec_perm( v3, v3, mask );
                v2 = vec_perm( v4, v4, mask );
                v3 = vec_ld( 0, (vector unsigned char *) outBuffer );
                v4 = vec_sel( v3, v1, cmp );
                vec_st( v4, 0, (vector unsigned char *) outBuffer );
                v1 = vec_sel( v1, v2, cmp );
                vec_st( v1, 16, (vector unsigned char *) outBuffer );
                v3 = vec_ld( 32, (vector unsigned char *) outBuffer );
                v2 = vec_sel( v2, v3, cmp );
                vec_st( v2, 32, (vector unsigned char *) outBuffer );
        } else {
                // store output data into an aligned buffer
                vec_st( v3, 0, (vector unsigned char *) outBuffer );
                vec_st( v4, 16, (vector unsigned char *) outBuffer );
        }
        return;
}

void vBlockDecrypt(cipherInstance *cipher, keyInstance *key, BYTE *input, int inputLen, BYTE *outBuffer)
{
        // for vector machines
        register vector unsigned char v1, v2, v3, v4, mask;
        register vector bool char cmp;
        vector unsigned  char	block[2], cblock[2];

        /* load and align input */
        v1 = vec_ld( 0, (vector unsigned char *) input );
        v2 = vec_ld( 16, (vector unsigned char *) input );
        if ( (long) input & 0x0fL )
        {	// this is required if input is not on a 16-byte boundary
                v3 = vec_ld( 32, (vector unsigned char *) input );
                mask = vec_lvsl( 0, input );
                v1 = vec_perm( v1, v2, mask );
                v2 = vec_perm( v2, v3, mask );
        }

        /* parse input stream into rectangular array */
        v3 = vec_perm( v1, v2, (vector unsigned char) ( 0,  4,  8, 12, 16, 20, 24, 28,  1,  5,  9, 13, 17, 21, 25, 29 ) );
        v4 = vec_perm( v1, v2, (vector unsigned char) ( 2,  6, 10, 14, 18, 22, 26, 30,  3,  7, 11, 15, 19, 23, 27, 31 ) );
        block[0] = v3;
        block[1] = v4;

        /* save a copy of incoming ciphertext for later chain */
        if (cipher->mode == MODE_CBC) {
                cblock[0] = v3;
                cblock[1] = v4;
        }
                
        vRijndaelDecrypt ((vector unsigned char *) block, key->keyLen, cipher->blockLen, (vector unsigned char *) key->keySched);
        
        v1 = block[0];
        v2 = block[1];

        /* exor with last ciphertext */
        if (cipher->mode == MODE_CBC) {
                v1 = vec_xor( v1, *((vector unsigned char *) cipher->chainBlock) );
                v2 = vec_xor( v2, *((vector unsigned char *) cipher->chainBlock + 1) );
                vec_st( cblock[0], 0, (vector unsigned char *) cipher->chainBlock );
                vec_st( cblock[1], 16, (vector unsigned char *) cipher->chainBlock );
        }
        
        /* parse rectangular array into output ciphertext bytes */
        v3 = vec_perm( v1, v2, (vector unsigned char) ( 0,  8, 16, 24,  1,  9, 17, 25,  2, 10, 18, 26,  3, 11, 19, 27 ) );
        v4 = vec_perm( v1, v2, (vector unsigned char) ( 4, 12, 20, 28,  5, 13, 21, 29,  6, 14, 22, 30,  7, 15, 23, 31 ) );

        if ( (long) outBuffer & 0x0fL )
        {	/* store output data into a non-aligned buffer */
                mask = vec_lvsr( 0, outBuffer );
                cmp = vec_cmpgt( mask, vec_splat_u8( 0x0f ) );
                v1 = vec_perm( v3, v3, mask );
                v2 = vec_perm( v4, v4, mask );
                v3 = vec_ld( 0, (vector unsigned char *) outBuffer );
                v4 = vec_sel( v3, v1, cmp );
                vec_st( v4, 0, (vector unsigned char *) outBuffer );
                v1 = vec_sel( v1, v2, cmp );
                vec_st( v1, 16, (vector unsigned char *) outBuffer );
                v3 = vec_ld( 32, (vector unsigned char *) outBuffer );
                v2 = vec_sel( v2, v3, cmp );
                vec_st( v2, 32, (vector unsigned char *) outBuffer );
        } else {
                // store output data into an aligned buffer
                vec_st( v3, 0, (vector unsigned char *) outBuffer );
                vec_st( v4, 16, (vector unsigned char *) outBuffer );
        }
}
#endif	/* Murley's code, to be deleted */

/* 
 * dmitch addenda 4/11/2001: 128-bit only encrypt/decrypt with no CBC
 */
void vBlockEncrypt128(
	keyInstance *key, 
	BYTE *input, 
	BYTE *outBuffer)
{
	vector unsigned char block[2];
	register vector unsigned char v1, v2;
	
	if ( (long) input & 0x0fL ) {
		BYTE	localBuf[16];
		vdprintf(("vBlockEncrypt128: unaligned input\n"));
		/* manually re-align - the compiler is supposed to 16-byte align this for us */
		if((unsigned)localBuf & 0xf) {
			vdprintf(("vBlockEncrypt128: unaligned localBuf!\n"));
		}
		memmove(localBuf, input, 16);
		v1 = vec_ld(0, (vector unsigned char *)localBuf);
	}
	else {
		vdprintf(("vBlockEncrypt128: aligned input\n"));
		v1 = vec_ld( 0, (vector unsigned char *) input );
	}

	/* parse input stream into rectangular array */
	/* FIXME - do we need to zero v2 (or something)? */
	block[0] = vec_perm(v1, v2,
		(vector unsigned char) ( 0,  4,  8, 12, 16, 20, 24, 28,  1,  
		5,  9, 13, 17, 21, 25, 29 ) );
	block[1] = vec_perm( v1, v2, 
		(vector unsigned char) ( 2,  6, 10, 14, 18, 22, 26, 30,  3,  
		7, 11, 15, 19, 23, 27, 31 ) );

	vRijndaelEncrypt(block, key->keyLen, 128, (vector unsigned char *) key->keySched);

	/* parse rectangular array into output ciphertext bytes */
	v1 = vec_perm(block[0], block[1], 
		(vector unsigned char) ( 0,  8, 16, 24,  1,  9, 17, 25,  2, 
		10, 18, 26,  3, 11, 19, 27 ) );
	v2 = vec_perm(block[0], block[1], 
		(vector unsigned char) ( 4, 12, 20, 28,  5, 13, 21, 29,  6, 
		14, 22, 30,  7, 15, 23, 31 ) );

	if ( (long) outBuffer & 0x0fL )
	{
		/* store output data into a non-aligned buffer */
		BYTE	localBuf[16];
		vec_st(v1, 0, (vector unsigned char *) localBuf );
		memmove(outBuffer, localBuf, 16);
	} else {
		/* store output data into an aligned buffer */
		vec_st( v1, 0, (vector unsigned char *) outBuffer );
	}
	return;
}

void vBlockDecrypt128(
	keyInstance *key, 
	BYTE *input, 
	BYTE *outBuffer)
{
	vector unsigned char block[2];
	register vector unsigned char v1, v2;
	
	if ( (long) input & 0x0fL ) {
		/* manually re-align - the compiler is supposed to 16-byte align this for us */
		BYTE	localBuf[16];
		vdprintf(("vBlockDecrypt128: unaligned input\n"));
		if((unsigned)localBuf & 0xf) {
			vdprintf(("vBlockDecrypt128: unaligned localBuf!\n"));
		}
		memmove(localBuf, input, 16);
		v1 = vec_ld(0, (vector unsigned char *)localBuf);
	}
	else {
		vdprintf(("vBlockDecrypt128: aligned input\n"));
		v1 = vec_ld( 0, (vector unsigned char *) input );
	}

	/* parse input stream into rectangular array */
	/* FIXME - do we need to zero v2 (or something)? */
	block[0] = vec_perm(v1, v2,
		(vector unsigned char) ( 0,  4,  8, 12, 16, 20, 24, 28,  1,  
		5,  9, 13, 17, 21, 25, 29 ) );
	block[1] = vec_perm( v1, v2, 
		(vector unsigned char) ( 2,  6, 10, 14, 18, 22, 26, 30,  3,  
		7, 11, 15, 19, 23, 27, 31 ) );

	vRijndaelDecrypt(block, key->keyLen, 128, (vector unsigned char *) key->keySched);

	/* parse rectangular array into output ciphertext bytes */
	v1 = vec_perm(block[0], block[1], 
		(vector unsigned char) ( 0,  8, 16, 24,  1,  9, 17, 25,  2, 
		10, 18, 26,  3, 11, 19, 27 ) );
	v2 = vec_perm(block[0], block[1], 
		(vector unsigned char) ( 4, 12, 20, 28,  5, 13, 21, 29,  6, 
		14, 22, 30,  7, 15, 23, 31 ) );

	if ( (long) outBuffer & 0x0fL ) {
		/* store output data into a non-aligned buffer */
		BYTE	localBuf[16];
		vec_st(v1, 0, (vector unsigned char *) localBuf );
		memmove(outBuffer, localBuf, 16);
	} else {
		/* store output data into an aligned buffer */
		vec_st( v1, 0, (vector unsigned char *) outBuffer );
	}
	return;
}

#endif	/* defined(__ppc__) && defined(ALTIVEC_ENABLE) */
