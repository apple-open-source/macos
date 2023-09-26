/* $FreeBSD$ */

#ifndef _MD5_H_
#define _MD5_H_

/*-
 SPDX-License-Identifier: RSA-MD

 Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
 */

#include <CommonCrypto/CommonDigest.h>

#define MD5_BLOCK_LENGTH		CC_MD5_BLOCK_BYTES
#define MD5_DIGEST_LENGTH		CC_MD5_DIGEST_LENGTH
#define MD5_DIGEST_STRING_LENGTH	(MD5_DIGEST_LENGTH * 2 + 1)

#define MD5_CTX	CC_MD5_CTX

#include <sys/cdefs.h>
#include <sys/types.h>

#define MD5Init		CC_MD5_Init
#define MD5Update	CC_MD5_Update
#define MD5Final	CC_MD5_Final

__BEGIN_DECLS
char * MD5End(MD5_CTX *, char *);
char * MD5Fd(int, char *);
char * MD5FdChunk(int, char *, off_t, off_t);
char * MD5File(const char *, char *);
char * MD5FileChunk(const char *, char *, off_t, off_t);
char * MD5Data(const void *, unsigned int, char *);
#ifndef __APPLE__
/* Omitted from upstream; same situation as MD4Pad in md4.h */
void   MD5Pad (MD5_CTX *context);
#endif
__END_DECLS

#endif /* _MD5_H_ */
