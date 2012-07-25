/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * NSFEEPublicKeyPrivate.h
 *
 * Revision History
 * ----------------
 * 21 Aug 96	Doug Mitchell at NeXT
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
