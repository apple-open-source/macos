/* MD4.H - header file for MD4C.C
 * $FreeBSD$
 */

/*-
   SPDX-License-Identifier: RSA-MD

   Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
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

#include <CommonCrypto/CommonDigest.h>

#define MD4_CTX	CC_MD4_CTX

#include <sys/cdefs.h>
#include <sys/types.h>

#define MD4Init		CC_MD4_Init
#define	MD4Update	CC_MD4_Update
#define MD4Final	CC_MD4_Final

__BEGIN_DECLS
#ifndef __APPLE__
/*
 * Pad() is not implemented by CommonCrypto and likely legacy that we don't need
 * to carry forth.  Comment it out for now, we can revisit it if we really find
 * a compelling need.
 */
void   MD4Pad(MD4_CTX *);
#endif
char * MD4End(MD4_CTX *, char *);
char * MD4Fd(int, char *);
char * MD4FdChunk(int, char *, off_t, off_t);
char * MD4File(const char *, char *);
char * MD4FileChunk(const char *, char *, off_t, off_t);
char * MD4Data(const void *, unsigned int, char *);
__END_DECLS

#endif /* _MD4_H_ */
