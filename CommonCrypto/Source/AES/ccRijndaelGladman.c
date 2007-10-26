/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
 * rijndaelGladman.c - Gladman AES/Rijndael implementation.
 *					   Based on rijndael.c written by Dr. Brian Gladman.
 */



/* This is an independent implementation of the encryption algorithm:   */
/*                                                                      */
/*         RIJNDAEL by Joan Daemen and Vincent Rijmen                   */
/*                                                                      */
/* which is a candidate algorithm in the Advanced Encryption Standard   */
/* programme of the US National Institute of Standards and Technology.  */
/*                                                                      */
/* Copyright in this implementation is held by Dr B R Gladman but I     */
/* hereby give permission for its free direct or derivative use subject */
/* to acknowledgment of its origin and compliance with any conditions   */
/* that the originators of the algorithm place on its exploitation.     */
/*                                                                      */
/* Dr Brian Gladman (gladman@seven77.demon.co.uk) 14th January 1999     */

#include "ccRijndaelGladman.h"
#include "ccGladmanTables.h"

/* enable of block/word/byte swapping macros */
#define USE_SWAP_MACROS	1

#if		AES_DYNAMIC_TABLES
u1byte  *sbx_tab;		/* [SBX_TAB_SIZE] */
u1byte  *isb_tab;		/* [ISB_TAB_SIZE] */
u4byte  *rco_tab;		/* [RCO_TAB_SIZE] */
u4byte  (*ft_tab)[FT_TAB_SIZE_LS];
u4byte  (*it_tab)[IT_TAB_SIZE_LS];
#if		AES_LARGE_TABLES
u4byte  (*fl_tab)[FL_TAB_SIZE_LS];
u4byte  (*il_tab)[IL_TAB_SIZE_LS];
#endif	/* AES_LARGE_TABLES */
#endif	/* AES_DYNAMIC_TABLES */

#define ff_mult(a,b)    (a && b ? pow_tab[(log_tab[a] + log_tab[b]) % 255] : 0)

#define f_rn(bo, bi, n, k)                          \
    bo[n] =  ft_tab[0][byte(bi[n],0)] ^             \
             ft_tab[1][byte(bi[(n + 1) & 3],1)] ^   \
             ft_tab[2][byte(bi[(n + 2) & 3],2)] ^   \
             ft_tab[3][byte(bi[(n + 3) & 3],3)] ^ *(k + n)

#define i_rn(bo, bi, n, k)                          \
    bo[n] =  it_tab[0][byte(bi[n],0)] ^             \
             it_tab[1][byte(bi[(n + 3) & 3],1)] ^   \
             it_tab[2][byte(bi[(n + 2) & 3],2)] ^   \
             it_tab[3][byte(bi[(n + 1) & 3],3)] ^ *(k + n)

#if		AES_LARGE_TABLES

#define ls_box(x)                \
    ( fl_tab[0][byte(x, 0)] ^    \
      fl_tab[1][byte(x, 1)] ^    \
      fl_tab[2][byte(x, 2)] ^    \
      fl_tab[3][byte(x, 3)] )

#define f_rl(bo, bi, n, k)                          \
    bo[n] =  fl_tab[0][byte(bi[n],0)] ^             \
             fl_tab[1][byte(bi[(n + 1) & 3],1)] ^   \
             fl_tab[2][byte(bi[(n + 2) & 3],2)] ^   \
             fl_tab[3][byte(bi[(n + 3) & 3],3)] ^ *(k + n)

#define i_rl(bo, bi, n, k)                          \
    bo[n] =  il_tab[0][byte(bi[n],0)] ^             \
             il_tab[1][byte(bi[(n + 3) & 3],1)] ^   \
             il_tab[2][byte(bi[(n + 2) & 3],2)] ^   \
             il_tab[3][byte(bi[(n + 1) & 3],3)] ^ *(k + n)

#else

#define ls_box(x)                            \
    ((u4byte)sbx_tab[byte(x, 0)] <<  0) ^    \
    ((u4byte)sbx_tab[byte(x, 1)] <<  8) ^    \
    ((u4byte)sbx_tab[byte(x, 2)] << 16) ^    \
    ((u4byte)sbx_tab[byte(x, 3)] << 24)

#define f_rl(bo, bi, n, k)                                      \
    bo[n] = (u4byte)sbx_tab[byte(bi[n],0)] ^                    \
        rotl(((u4byte)sbx_tab[byte(bi[(n + 1) & 3],1)]),  8) ^  \
        rotl(((u4byte)sbx_tab[byte(bi[(n + 2) & 3],2)]), 16) ^  \
        rotl(((u4byte)sbx_tab[byte(bi[(n + 3) & 3],3)]), 24) ^ *(k + n)

#define i_rl(bo, bi, n, k)                                      \
    bo[n] = (u4byte)isb_tab[byte(bi[n],0)] ^                    \
        rotl(((u4byte)isb_tab[byte(bi[(n + 3) & 3],1)]),  8) ^  \
        rotl(((u4byte)isb_tab[byte(bi[(n + 2) & 3],2)]), 16) ^  \
        rotl(((u4byte)isb_tab[byte(bi[(n + 1) & 3],3)]), 24) ^ *(k + n)

#endif

#if		AES_DYNAMIC_TABLES

void gen_tabs(void)
{   u4byte  i, t;
    u1byte  p, q;

	/* 
	 * These used to be in externally mallocd lookup tables, but
	 * they are not needed subsequent to this routine. 
	 */
	u1byte pow_tab[POW_TAB_SIZE];
	u1byte log_tab[LOG_TAB_SIZE];
	
    /* log and power tables for GF(2**8) finite field with  */
    /* 0x11b as modular polynomial - the simplest prmitive  */
    /* root is 0x11, used here to generate the tables       */

    for(i = 0,p = 1; i < 256; ++i)
    {
        pow_tab[i] = (u1byte)p; log_tab[p] = (u1byte)i;

        p = p ^ (p << 1) ^ (p & 0x80 ? 0x01b : 0);
    }

    log_tab[1] = 0; p = 1;

    for(i = 0; i < 10; ++i)
    {
        rco_tab[i] = p; 

        p = (p << 1) ^ (p & 0x80 ? 0x1b : 0);
    }

    /* note that the affine byte transformation matrix in   */
    /* rijndael specification is in big endian format with  */
    /* bit 0 as the most significant bit. In the remainder  */
    /* of the specification the bits are numbered from the  */
    /* least significant end of a byte.                     */

    for(i = 0; i < 256; ++i)
    {   
        p = (i ? pow_tab[255 - log_tab[i]] : 0); q = p; 
        q = (q >> 7) | (q << 1); p ^= q; 
        q = (q >> 7) | (q << 1); p ^= q; 
        q = (q >> 7) | (q << 1); p ^= q; 
        q = (q >> 7) | (q << 1); p ^= q ^ 0x63; 
        sbx_tab[i] = (u1byte)p; isb_tab[p] = (u1byte)i;
    }

    for(i = 0; i < 256; ++i)
    {
        p = sbx_tab[i]; 

#if		AES_LARGE_TABLES        
        
        t = p; fl_tab[0][i] = t;
        fl_tab[1][i] = rotl(t,  8);
        fl_tab[2][i] = rotl(t, 16);
        fl_tab[3][i] = rotl(t, 24);
#endif
        t = ((u4byte)ff_mult(2, p)) |
            ((u4byte)p <<  8) |
            ((u4byte)p << 16) |
            ((u4byte)ff_mult(3, p) << 24);
        
        ft_tab[0][i] = t;
        ft_tab[1][i] = rotl(t,  8);
        ft_tab[2][i] = rotl(t, 16);
        ft_tab[3][i] = rotl(t, 24);

        p = isb_tab[i]; 

#if		AES_LARGE_TABLES        
        
        t = p; il_tab[0][i] = t; 
        il_tab[1][i] = rotl(t,  8); 
        il_tab[2][i] = rotl(t, 16); 
        il_tab[3][i] = rotl(t, 24);
#endif 
        t = ((u4byte)ff_mult(14, p)) |
            ((u4byte)ff_mult( 9, p) <<  8) |
            ((u4byte)ff_mult(13, p) << 16) |
            ((u4byte)ff_mult(11, p) << 24);
        
        it_tab[0][i] = t; 
        it_tab[1][i] = rotl(t,  8); 
        it_tab[2][i] = rotl(t, 16); 
        it_tab[3][i] = rotl(t, 24); 
    }
};

#endif	/* AES_DYNAMIC_TABLES */

#define star_x(x) (((x) & 0x7f7f7f7f) << 1) ^ ((((x) & 0x80808080) >> 7) * 0x1b)

#define imix_col(y,x)       \
    u   = star_x(x);        \
    v   = star_x(u);        \
    w   = star_x(v);        \
    t   = w ^ (x);          \
   (y)  = u ^ v ^ w;        \
   (y) ^= rotr(u ^ t,  8) ^ \
          rotr(v ^ t, 16) ^ \
          rotr(t,24)

/* initialise the key schedule from the user supplied key   */

#define loop4(i)                                    \
{   t = ls_box(rotr(t,  8)) ^ rco_tab[i];           \
    t ^= e_key[4 * i];     e_key[4 * i + 4] = t;    \
    t ^= e_key[4 * i + 1]; e_key[4 * i + 5] = t;    \
    t ^= e_key[4 * i + 2]; e_key[4 * i + 6] = t;    \
    t ^= e_key[4 * i + 3]; e_key[4 * i + 7] = t;    \
}

#define loop6(i)                                    \
{   t = ls_box(rotr(t,  8)) ^ rco_tab[i];           \
    t ^= e_key[6 * i];     e_key[6 * i + 6] = t;    \
    t ^= e_key[6 * i + 1]; e_key[6 * i + 7] = t;    \
    t ^= e_key[6 * i + 2]; e_key[6 * i + 8] = t;    \
    t ^= e_key[6 * i + 3]; e_key[6 * i + 9] = t;    \
    t ^= e_key[6 * i + 4]; e_key[6 * i + 10] = t;   \
    t ^= e_key[6 * i + 5]; e_key[6 * i + 11] = t;   \
}

#define loop8(i)                                    \
{   t = ls_box(rotr(t,  8)) ^ rco_tab[i];           \
    t ^= e_key[8 * i];     e_key[8 * i + 8] = t;    \
    t ^= e_key[8 * i + 1]; e_key[8 * i + 9] = t;    \
    t ^= e_key[8 * i + 2]; e_key[8 * i + 10] = t;   \
    t ^= e_key[8 * i + 3]; e_key[8 * i + 11] = t;   \
    t  = e_key[8 * i + 4] ^ ls_box(t);              \
    e_key[8 * i + 12] = t;                          \
    t ^= e_key[8 * i + 5]; e_key[8 * i + 13] = t;   \
    t ^= e_key[8 * i + 6]; e_key[8 * i + 14] = t;   \
    t ^= e_key[8 * i + 7]; e_key[8 * i + 15] = t;   \
}

#define AES_128BIT_KEY_ONLY		0

int ccAesSetKey(
	GAesKey *aesKey,
	const u4byte in_key[], 
	u4byte key_len)				/* in bytes */
{   u4byte  i, t, u, v, w;
	u4byte  *e_key = aesKey->e_key;
	u4byte  *d_key = aesKey->d_key;

	key_len *= 8;
    aesKey->k_len = (key_len + 31) / 32;
	
	#if		USE_SWAP_MACROS
	get_key(e_key, key_len);
	#else
    e_key[0] = in_key[0]; e_key[1] = in_key[1];
    e_key[2] = in_key[2]; e_key[3] = in_key[3];
	#endif
	
    switch(aesKey->k_len)
    {
        case 4: t = e_key[3];
                for(i = 0; i < 10; ++i) 
                    loop4(i);
                break;
		#if		!AES_128BIT_KEY_ONLY
		
        case 6: 
				#if	USE_SWAP_MACROS
				t = e_key[5];
				#else
				/* done in get_key macros in USE_SWAP_MACROS case */
				e_key[4] = in_key[4]; t = e_key[5] = in_key[5];
				#endif
                for(i = 0; i < 8; ++i) 
                    loop6(i);
                break;

        case 8: 
				#if	USE_SWAP_MACROS
				t = e_key[7];
				#else
				e_key[4] = in_key[4]; e_key[5] = in_key[5];
                e_key[6] = in_key[6]; t = e_key[7] = in_key[7];
				#endif
                for(i = 0; i < 7; ++i) 
                    loop8(i);
                break;
		#endif	/* !AES_128BIT_KEY_ONLY */
		default:
			return -1;
    }

    d_key[0] = e_key[0]; d_key[1] = e_key[1];
    d_key[2] = e_key[2]; d_key[3] = e_key[3];

    for(i = 4; i < 4 * aesKey->k_len + 24; ++i)
    {
        imix_col(d_key[i], e_key[i]);
    }

    return 0;
};

/* encrypt a block of text  */

#define f_nround(bo, bi, k) \
    f_rn(bo, bi, 0, k);     \
    f_rn(bo, bi, 1, k);     \
    f_rn(bo, bi, 2, k);     \
    f_rn(bo, bi, 3, k);     \
    k += 4

#define f_lround(bo, bi, k) \
    f_rl(bo, bi, 0, k);     \
    f_rl(bo, bi, 1, k);     \
    f_rl(bo, bi, 2, k);     \
    f_rl(bo, bi, 3, k)

void ccAesEncrypt(
	const GAesKey *aesKey,
	const u4byte in_blk[4], 
	u4byte out_blk[4])
{   
	u4byte  b0[4], b1[4], *kp;
	u4byte  *e_key = aesKey->e_key;
	
	#if USE_SWAP_MACROS
	u4byte  swap_block[4];
	get_block(swap_block);
    b0[0] = swap_block[0] ^ e_key[0]; b0[1] = swap_block[1] ^ e_key[1];
    b0[2] = swap_block[2] ^ e_key[2]; b0[3] = swap_block[3] ^ e_key[3];
	#else
    b0[0] = in_blk[0] ^ e_key[0]; b0[1] = in_blk[1] ^ e_key[1];
    b0[2] = in_blk[2] ^ e_key[2]; b0[3] = in_blk[3] ^ e_key[3];
	#endif
	
    kp = e_key + 4;
	
    if(aesKey->k_len > 6)
    {
        f_nround(b1, b0, kp); f_nround(b0, b1, kp);
    }

    if(aesKey->k_len > 4)
    {
        f_nround(b1, b0, kp); f_nround(b0, b1, kp);
    }

    f_nround(b1, b0, kp); f_nround(b0, b1, kp);
    f_nround(b1, b0, kp); f_nround(b0, b1, kp);
    f_nround(b1, b0, kp); f_nround(b0, b1, kp);
    f_nround(b1, b0, kp); f_nround(b0, b1, kp);	
    f_nround(b1, b0, kp); f_lround(b0, b1, kp);

	#if USE_SWAP_MACROS
	put_block(b0);
	#else
    out_blk[0] = b0[0]; out_blk[1] = b0[1];
    out_blk[2] = b0[2]; out_blk[3] = b0[3];
	#endif
};

/* decrypt a block of text  */

#define i_nround(bo, bi, k) \
    i_rn(bo, bi, 0, k);     \
    i_rn(bo, bi, 1, k);     \
    i_rn(bo, bi, 2, k);     \
    i_rn(bo, bi, 3, k);     \
    k -= 4

#define i_lround(bo, bi, k) \
    i_rl(bo, bi, 0, k);     \
    i_rl(bo, bi, 1, k);     \
    i_rl(bo, bi, 2, k);     \
    i_rl(bo, bi, 3, k)

void ccAesDecrypt(
	const GAesKey *aesKey,
	const u4byte in_blk[4], 
	u4byte out_blk[4])
{   
	u4byte  b0[4], b1[4], *kp;
	u4byte  *e_key = aesKey->e_key;
	u4byte  *d_key = aesKey->d_key;
	u4byte	k_len = aesKey->k_len;
	
	#if USE_SWAP_MACROS
	u4byte  swap_block[4];
	get_block(swap_block);
    b0[0] = swap_block[0] ^ e_key[4 * k_len + 24]; 
	b0[1] = swap_block[1] ^ e_key[4 * k_len + 25];
    b0[2] = swap_block[2] ^ e_key[4 * k_len + 26]; 
	b0[3] = swap_block[3] ^ e_key[4 * k_len + 27];
	#else
    b0[0] = in_blk[0] ^ e_key[4 * k_len + 24]; 
	b0[1] = in_blk[1] ^ e_key[4 * k_len + 25];
    b0[2] = in_blk[2] ^ e_key[4 * k_len + 26]; 
	b0[3] = in_blk[3] ^ e_key[4 * k_len + 27];
	#endif
	
    kp = d_key + 4 * (k_len + 5);

    if(k_len > 6)
    {
        i_nround(b1, b0, kp); i_nround(b0, b1, kp);
    }

    if(k_len > 4)
    {
        i_nround(b1, b0, kp); i_nround(b0, b1, kp);
    }

    i_nround(b1, b0, kp); i_nround(b0, b1, kp);
    i_nround(b1, b0, kp); i_nround(b0, b1, kp);
    i_nround(b1, b0, kp); i_nround(b0, b1, kp);
    i_nround(b1, b0, kp); i_nround(b0, b1, kp);
    i_nround(b1, b0, kp); i_lround(b0, b1, kp);

	#if USE_SWAP_MACROS
	put_block(b0);
	#else
    out_blk[0] = b0[0]; out_blk[1] = b0[1];
    out_blk[2] = b0[2]; out_blk[3] = b0[3];
	#endif
};
