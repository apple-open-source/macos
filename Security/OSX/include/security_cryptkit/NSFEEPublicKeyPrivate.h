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
 * NSFEEPublicKeyPrivate.h
 *
 * Revision History
 * ----------------
 * 21 Aug 96 at NeXT
 *	Created.
 */

#import "NSFEEPublicKey.h"
#import "elliptic.h"
#import "feeDebug.h"
#import "feePublicKey.h"

@interface NSFEEPublicKey(Private)

- (key)minus;
- (key)plus;
#if 0
- (NSData *)privData;
#endif 0
- (feePubKey)feePubKey;

#if 	FEE_DEBUG
- (void)dump;
#endif	FEE_DEBUG
@end
