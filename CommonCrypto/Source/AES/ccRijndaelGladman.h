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
 * ccRijndaelGladman.h - constants and macros for Gladman AES/Rijndael implementation.
 *					  Based on std_defs.h written by Dr. Brian Gladman.
 */



/* 1. Standard types for AES cryptography source code               */

typedef unsigned char   u1byte; /* an 8 bit unsigned character type */
typedef unsigned short  u2byte; /* a 16 bit unsigned integer type   */
typedef unsigned int   u4byte; /* a 32 bit unsigned integer type   */

typedef signed char     s1byte; /* an 8 bit signed character type   */
typedef signed short    s2byte; /* a 16 bit signed integer type     */
typedef signed int     s4byte; /* a 32 bit signed integer type     */

/* 2. Standard interface for AES cryptographic routines             */

/* These are all based on 32 bit unsigned values and will therefore */
/* require endian conversions for big-endian architectures          */

#ifdef  __cplusplus
    extern "C"
    {
#endif

	/* 
	 * Lookup tables. Dynamically allocated (by client) and generated (by 
	 * gen_tabs()) if AES_DYNAMIC_TABLES is nonzero; else statically 
	 * declared in gladmanTables.h.
	 */
	#define AES_LARGE_TABLES	0
	#define AES_DYNAMIC_TABLES	0
	
	#define POW_TAB_SIZE	256
	#define LOG_TAB_SIZE	256
	#define SBX_TAB_SIZE	256
	#define ISB_TAB_SIZE	256
	#define RCO_TAB_SIZE	10
	#define FT_TAB_SIZE_MS	4
	#define FT_TAB_SIZE_LS	256
	#define IT_TAB_SIZE_MS	4
	#define IT_TAB_SIZE_LS	256

	#if AES_DYNAMIC_TABLES
	extern u1byte  *sbx_tab;		/* [SBX_TAB_SIZE] */
	extern u1byte  *isb_tab;		/* [ISB_TAB_SIZE] */
	extern u4byte  *rco_tab;		/* [RCO_TAB_SIZE] */
	extern u4byte  (*ft_tab)[FT_TAB_SIZE_LS];
	extern u4byte  (*it_tab)[IT_TAB_SIZE_LS];
	/* else declared in gladmanTables.h */
	#endif	/* AES_DYNAMIC_TABLES */
	
	#if		AES_LARGE_TABLES
	#define FL_TAB_SIZE_MS	4
	#define FL_TAB_SIZE_LS	256
	#define IL_TAB_SIZE_MS	4
	#define IL_TAB_SIZE_LS	256
	#if AES_DYNAMIC_TABLES
	extern u4byte  (*fl_tab)[FL_TAB_SIZE_LS];
	extern u4byte  (*il_tab)[IL_TAB_SIZE_LS];
	/* else declared in gladmanTables.h */
	#endif	/* AES_DYNAMIC_TABLES */
	#endif	/* AES_LARGE_TABLES */

	typedef struct {
		u4byte  k_len;
		u4byte  e_key[64];
		u4byte  d_key[64];
	} GAesKey;
	
	#if		AES_DYNAMIC_TABLES
	void gen_tabs(void);			// one-time-only table generate
	#endif	/* AES_DYNAMIC_TABLES */
	
	/* key size in bytes here */
    int ccAesSetKey(GAesKey *aesKey, const u4byte in_key[], u4byte key_len);
	
	/* these pointers for i/o look scary but the implementation does the right 
	 * thing and assumes that you're casting a char * to them. */
    void ccAesEncrypt(const GAesKey *aesKey, const u4byte in_blk[4], u4byte out_blk[4]);
    void ccAesDecrypt(const GAesKey *aesKey, const u4byte in_blk[4], u4byte out_blk[4]);

#ifdef  __cplusplus
    };
#endif

/* 3. Basic macros for speeding up generic operations               */

/* Circular rotate of 32 bit values                                 */

#ifdef _MSC_VER

#  include <stdlib.h>
#  pragma intrinsic(_lrotr,_lrotl)
#  define rotr(x,n) _lrotr(x,n)
#  define rotl(x,n) _lrotl(x,n)

#else

#define rotr(x,n)   (((x) >> ((int)(n))) | ((x) << (32 - (int)(n))))
#define rotl(x,n)   (((x) << ((int)(n))) | ((x) >> (32 - (int)(n))))

#endif

/* Invert byte order in a 32 bit variable                           */

#define bswap(x)    ((rotl(x, 8) & 0x00ff00ff) | (rotr(x, 8) & 0xff00ff00))

/* Extract byte from a 32 bit quantity (little endian notation)     */ 

#define byte(x,n)   ((u1byte)((x) >> (8 * n)))

/* For inverting byte order in input/output 32 bit words if needed  */
#if defined(__ppc__) || defined(__ppc64__)
#define BYTE_SWAP
#endif

#ifdef  BLOCK_SWAP
#define BYTE_SWAP
#define WORD_SWAP
#endif

#ifdef  BYTE_SWAP
#define io_swap(x)  bswap(x)
#else
#define io_swap(x)  (x)
#endif

/* For inverting the byte order of input/output blocks if needed    */

#ifdef  WORD_SWAP

#define get_block(x)                            \
    ((u4byte*)(x))[0] = io_swap(in_blk[3]);     \
    ((u4byte*)(x))[1] = io_swap(in_blk[2]);     \
    ((u4byte*)(x))[2] = io_swap(in_blk[1]);     \
    ((u4byte*)(x))[3] = io_swap(in_blk[0])

#define put_block(x)                            \
    out_blk[3] = io_swap(((u4byte*)(x))[0]);    \
    out_blk[2] = io_swap(((u4byte*)(x))[1]);    \
    out_blk[1] = io_swap(((u4byte*)(x))[2]);    \
    out_blk[0] = io_swap(((u4byte*)(x))[3])

#define get_key(x,len)                          \
    ((u4byte*)(x))[4] = ((u4byte*)(x))[5] =     \
    ((u4byte*)(x))[6] = ((u4byte*)(x))[7] = 0;  \
    switch((((len) + 63) / 64)) {               \
    case 2:                                     \
    ((u4byte*)(x))[0] = io_swap(in_key[3]);     \
    ((u4byte*)(x))[1] = io_swap(in_key[2]);     \
    ((u4byte*)(x))[2] = io_swap(in_key[1]);     \
    ((u4byte*)(x))[3] = io_swap(in_key[0]);     \
    break;                                      \
    case 3:                                     \
    ((u4byte*)(x))[0] = io_swap(in_key[5]);     \
    ((u4byte*)(x))[1] = io_swap(in_key[4]);     \
    ((u4byte*)(x))[2] = io_swap(in_key[3]);     \
    ((u4byte*)(x))[3] = io_swap(in_key[2]);     \
    ((u4byte*)(x))[4] = io_swap(in_key[1]);     \
    ((u4byte*)(x))[5] = io_swap(in_key[0]);     \
    break;                                      \
    case 4:                                     \
    ((u4byte*)(x))[0] = io_swap(in_key[7]);     \
    ((u4byte*)(x))[1] = io_swap(in_key[6]);     \
    ((u4byte*)(x))[2] = io_swap(in_key[5]);     \
    ((u4byte*)(x))[3] = io_swap(in_key[4]);     \
    ((u4byte*)(x))[4] = io_swap(in_key[3]);     \
    ((u4byte*)(x))[5] = io_swap(in_key[2]);     \
    ((u4byte*)(x))[6] = io_swap(in_key[1]);     \
    ((u4byte*)(x))[7] = io_swap(in_key[0]);     \
    }

#else

#define get_block(x)                            \
    ((u4byte*)(x))[0] = io_swap(in_blk[0]);     \
    ((u4byte*)(x))[1] = io_swap(in_blk[1]);     \
    ((u4byte*)(x))[2] = io_swap(in_blk[2]);     \
    ((u4byte*)(x))[3] = io_swap(in_blk[3])

#define put_block(x)                            \
    out_blk[0] = io_swap(((u4byte*)(x))[0]);    \
    out_blk[1] = io_swap(((u4byte*)(x))[1]);    \
    out_blk[2] = io_swap(((u4byte*)(x))[2]);    \
    out_blk[3] = io_swap(((u4byte*)(x))[3])

#define get_key(x,len)                          \
    ((u4byte*)(x))[4] = ((u4byte*)(x))[5] =     \
    ((u4byte*)(x))[6] = ((u4byte*)(x))[7] = 0;  \
    switch((((len) + 63) / 64)) {               \
    case 4:                                     \
    ((u4byte*)(x))[6] = io_swap(in_key[6]);     \
    ((u4byte*)(x))[7] = io_swap(in_key[7]);     \
    case 3:                                     \
    ((u4byte*)(x))[4] = io_swap(in_key[4]);     \
    ((u4byte*)(x))[5] = io_swap(in_key[5]);     \
    case 2:                                     \
    ((u4byte*)(x))[0] = io_swap(in_key[0]);     \
    ((u4byte*)(x))[1] = io_swap(in_key[1]);     \
    ((u4byte*)(x))[2] = io_swap(in_key[2]);     \
    ((u4byte*)(x))[3] = io_swap(in_key[3]);     \
    }

#endif
