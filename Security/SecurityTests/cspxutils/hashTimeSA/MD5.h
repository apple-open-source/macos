/*
	File:		MD5.h

	Written by:	Colin Plumb

	Copyright:	Copyright (c) 1998,2004 Apple Computer, Inc. All Rights Reserved.

	Change History (most recent first):

		 <8>	10/06/98	ap		Changed to compile with C++.

	To Do:
*/

/* Copyright (c) 1998,2004 Apple Computer, Inc.  All Rights Reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * MD5.h
 * derived and used without need for permission from public domain source
 */

#ifndef	_CK_MD5_H_
#define _CK_MD5_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __alpha
typedef unsigned int uint32;
#elif defined (macintosh)
typedef unsigned int uint32;
#else
#include <Security/cssmconfig.h>
//typedef unsigned long uint32;
#endif

struct MD5Context {
	uint32 buf[4];
	uint32 bits[2];			// bits[0] is low 32 bits of bit count
	unsigned char in[64];
};

#define MD5_DIGEST_SIZE		16	/* in bytes */
#define MD5_BLOCK_SIZE		64	/* in bytes */

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
	       unsigned len);
void MD5Final(struct MD5Context *context, unsigned char *digest);

#ifdef __cplusplus
}
#endif

#endif	/*_CK_MD5_H_*/
