/********************************************************************\
 *
 *      FILE:     rmd128.h
 *
 *      CONTENTS: Header file for a sample C-implementation of the
 *                RIPEMD-128 hash-function. This function is a
 *                plug-in substitute for RIPEMD. A 160-bit hash
 *                result is obtained using RIPEMD-160.
 *      TARGET:   any computer with an ANSI C compiler
 *
 *      AUTHOR:   Antoon Bosselaers, ESAT-COSIC
 *      DATE:     1 March 1996
 *      VERSION:  1.0
 *
 *      Copyright (c) Katholieke Universiteit Leuven
 *      1996, All Rights Reserved
 *
\********************************************************************/
/* AKU:
 * <> added __alpha,
 * <> unique prefix for function names
 * <> using tcl.h, _ANSI_ARGS_
 */

#ifndef  RMD128H           /* make sure this file is read only once */
#define  RMD128H

/********************************************************************/

#include <tcl.h>

/* typedef 8, 16 and 32 bit types, resp.  */
/* adapt these, if necessary, 
   for your operating system and compiler */
typedef    unsigned char        byte;   /* unsigned 8-bit type */
typedef    unsigned short       word;   /* unsigned 16-bit type */

#if defined(__alpha) || defined(__LP64__)
typedef    unsigned int         dword;  /* unsigned 32-bit integer (AXP) */ 
#else
typedef    unsigned long        dword;  /* unsigned 32-bit type */
#endif

/********************************************************************/

/* macro definitions */

/* collect four bytes into one word: */
#define BYTES_TO_DWORD(strptr)                    \
            (((dword) *((strptr)+3) << 24) | \
             ((dword) *((strptr)+2) << 16) | \
             ((dword) *((strptr)+1) <<  8) | \
             ((dword) *(strptr)))

/* ROL(x, n) cyclically rotates x over n bits to the left */
/* x must be of an unsigned 32 bits type and 0 <= n < 32. */
#define ROL(x, n)        (((x) << (n)) | ((x) >> (32-(n))))

/* the three basic functions F(), G() and H() */
#define F(x, y, z)        ((x) ^ (y) ^ (z)) 
#define G(x, y, z)        (((x) & (y)) | (~(x) & (z))) 
#define H(x, y, z)        (((x) | ~(y)) ^ (z))
#define I(x, y, z)        (((x) & (z)) | ((y) & ~(z))) 
  
/* the eight basic operations FF() through III() */
#define FF(a, b, c, d, x, s)        {\
      (a) += F((b), (c), (d)) + (x);\
      (a) = ROL((a), (s));\
   }
#define GG(a, b, c, d, x, s)        {\
      (a) += G((b), (c), (d)) + (x) + 0x5a827999UL;\
      (a) = ROL((a), (s));\
   }
#define HH(a, b, c, d, x, s)        {\
      (a) += H((b), (c), (d)) + (x) + 0x6ed9eba1UL;\
      (a) = ROL((a), (s));\
   }
#define II(a, b, c, d, x, s)        {\
      (a) += I((b), (c), (d)) + (x) + 0x8f1bbcdcUL;\
      (a) = ROL((a), (s));\
   }
#define FFF(a, b, c, d, x, s)        {\
      (a) += F((b), (c), (d)) + (x);\
      (a) = ROL((a), (s));\
   }
#define GGG(a, b, c, d, x, s)        {\
      (a) += G((b), (c), (d)) + (x) + 0x6d703ef3UL;\
      (a) = ROL((a), (s));\
   }
#define HHH(a, b, c, d, x, s)        {\
      (a) += H((b), (c), (d)) + (x) + 0x5c4dd124UL;\
      (a) = ROL((a), (s));\
   }
#define III(a, b, c, d, x, s)        {\
      (a) += I((b), (c), (d)) + (x) + 0x50a28be6UL;\
      (a) = ROL((a), (s));\
   }

/********************************************************************/

/* function prototypes */

void
ripemd128_MDinit _ANSI_ARGS_ ((dword *MDbuf));
/*
 *  initializes MDbuffer to "magic constants"
 */

void
ripemd128_compress _ANSI_ARGS_ ((dword *MDbuf, dword *X));
/*
 *  the compression function.
 *  transforms MDbuf using message bytes X[0] through X[15]
 */

void
ripemd128_MDfinish _ANSI_ARGS_ ((dword *MDbuf, byte *strptr, dword lswlen, dword mswlen));
/*
 *  puts bytes from strptr into X and pad out; appends length 
 *  and finally, compresses the last block(s)
 *  note: length in bits == 8 * (lswlen + 2^32 mswlen).
 *  note: there are (lswlen mod 64) bytes left in strptr.
 */

#endif  /* RMD128H */

/*********************** end of file rmd128.h ***********************/

