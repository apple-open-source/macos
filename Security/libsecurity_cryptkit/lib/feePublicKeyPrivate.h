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
 * feePublicKeyPrivate.h - feePublicKey private function declarations
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 * 28 Aug 96	Doug Mitchell at NeXT
 *	Created.
 */

#ifndef	_CK_FEEPUBKEYPRIV_H_
#define _CK_FEEPUBKEYPRIV_H_

#include "feeTypes.h"
#include "feePublicKey.h"
#include "feeDebug.h"
#include "elliptic.h"

#ifdef __cplusplus
extern "C" {
#endif

key feePubKeyPlusCurve(feePubKey pubKey);
key feePubKeyMinusCurve(feePubKey pubKey);
curveParams *feePubKeyCurveParams(feePubKey pubKey);
giant feePubKeyPrivData(feePubKey pubKey);
void printPubKey(feePubKey pubKey);

#ifdef __cplusplus
}
#endif

#endif	/*_CK_FEEPUBKEYPRIV_H_*/
