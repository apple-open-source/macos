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

/*
 * In this case we use the MD5 implementation in libSystem.
 */
#include <CommonCrypto/CommonDigest.h>

typedef CC_MD5_CTX MD5Context;

#define MD5Init(c)		CC_MD5_Init(c)
#define MD5Update(c, d, l)	CC_MD5_Update(c, d, l)
#define MD5Final(c, d)		CC_MD5_Final(d, c)

#define MD5_DIGEST_SIZE		CC_MD5_DIGEST_LENGTH

#endif	/*_CK_MD5_H_*/
