/* MD4.H - header file for MD4C.C
 * $FreeBSD: src/lib/libmd/md4.h,v 1.9 1999/08/28 00:05:05 peter Exp $
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
   rights reserved.

   License to copy and use this software is granted provided that it
   is identified as the "RSA Data Security, Inc. MD4 Message-Digest
   Algorithm" in all material mentioning or referencing this software
   or this function.
   License is also granted to make and use derivative works provided
   that such works are identified as "derived from the RSA Data
   Security, Inc. MD4 Message-Digest Algorithm" in all material
   mentioning or referencing the derived work.

   RSA Data Security, Inc. makes no representations concerning either
   the merchantability of this software or the suitability of this
   software for any particular purpose. It is provided "as is"
   without express or implied warranty of any kind.

   These notices must be retained in any copies of any part of this
   documentation and/or software.
 */

#ifndef _MD4_H_
#define _MD4_H_
/* MD4 context. */
typedef struct MD4Context {
  uint32_t state[4];	/* state (ABCD) */
  uint32_t count[2];	/* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];	/* input buffer */
} MD4_CTX;

#include <sys/cdefs.h>

__BEGIN_DECLS
extern void MD4Init(MD4_CTX * /* context */);
extern void MD4Update (MD4_CTX * /*context */, const unsigned char * /*input */, unsigned int /* inputLen */);
extern void MD4Pad(MD4_CTX * /* context */);
extern void MD4Final(unsigned char [16] /* digest */, MD4_CTX * /* context */);
__END_DECLS

#endif /* _MD4_H_ */
