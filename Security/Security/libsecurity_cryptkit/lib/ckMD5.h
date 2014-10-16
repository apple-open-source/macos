/*
	File:		MD5.h

	Written by:	Colin Plumb

	Copyright:	Copyright (c) 1998,2011,2014 Apple Inc. All Rights Reserved.

	Change History (most recent first):

		 <8>	10/06/98	ap		Changed to compile with C++.

	To Do:
*/

/* Copyright (c) 1998,2011,2014 Apple Inc.  All Rights Reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * MD5.h
 * derived and used without need for permission from public domain source
 */

#ifndef	_CK_MD5_H_
#define _CK_MD5_H_

#include "ckconfig.h"

#if	CRYPTKIT_MD5_ENABLE
#if	CRYPTKIT_LIBMD_DIGEST

/*
 * In this case we use the MD5 implementation in libSystem.
 */
#include <CommonCrypto/CommonDigest.h>

typedef CC_MD5_CTX MD5Context;

#define MD5Init(c)		CC_MD5_Init(c)
#define MD5Update(c, d, l)	CC_MD5_Update(c, d, l)
#define MD5Final(c, d)		CC_MD5_Final(d, c)

#define MD5_DIGEST_SIZE		CC_MD5_DIGEST_LENGTH

#else	/* ! CRYPTKIT_LIBMD_DIGEST */

/* Our own private implementation */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __alpha
typedef unsigned int UINT32;
#elif defined (macintosh) || defined (__ppc__)
typedef unsigned int UINT32;
#else
typedef unsigned long UINT32;
#endif

typedef struct {
	UINT32 buf[4];
	UINT32 bits[2];			// bits[0] is low 32 bits of bit count
	unsigned char in[64];
} MD5Context;

#define MD5_DIGEST_SIZE		16	/* in bytes */

void MD5Init(MD5Context *context);
void MD5Update(MD5Context *context, unsigned char const *buf,
	       unsigned len);
void MD5Final(MD5Context *context, unsigned char *digest);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef MD5Context MD5_CTX;

#ifdef __cplusplus
}
#endif

#endif  /* CRYPTKIT_LIBMD_DIGEST */
#endif	/* CRYPTKIT_MD5_ENABLE */
#endif	/*_CK_MD5_H_*/
