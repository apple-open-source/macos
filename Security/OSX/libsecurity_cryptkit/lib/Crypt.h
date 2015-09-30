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
 * Crypt.h - top-level header for FEE library.
 *
 * Revision History
 * ----------------
 * 8/24/98		ap
 *	Added tags around #endif comment.
 * 28 May 1996 at Apple
 *	Added falloc.h, newly exported API.
 * 27 Aug 1996 at NeXT
 *	Created.
 */

#ifndef	_CK_CRYPT_H_
#define _CK_CRYPT_H_

#ifdef	macintosh

#include <feePublicKey.h>
#include <feeDES.h>
#include <feeDigitalSignature.h>
#include <feeECDSA.h>
#include <feeHash.h>
#include <ckSHA1.h>
#include <feeRandom.h>
#include <feeTypes.h>
#include <feeFunctions.h>
#include <feeFEED.h>
#include <feeFEEDExp.h>
#include <enc64.h>
#include <falloc.h>

#else

#include <security_cryptkit/feePublicKey.h>
#include <security_cryptkit/feeDES.h>
#include <security_cryptkit/feeDigitalSignature.h>
#include <security_cryptkit/feeECDSA.h>
#include <security_cryptkit/feeHash.h>
#include <security_cryptkit/ckSHA1.h>
#include <security_cryptkit/feeRandom.h>
#include <security_cryptkit/feeTypes.h>
#include <security_cryptkit/feeFunctions.h>
#include <security_cryptkit/feeFEED.h>
#include <security_cryptkit/feeFEEDExp.h>
#include <security_cryptkit/enc64.h>
#include <security_cryptkit/falloc.h>

#endif

#endif /* _CK_CRYPT_H_ */
