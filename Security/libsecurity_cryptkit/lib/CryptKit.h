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
 * CryptKit.h created by blaine on Thu 22-Feb-1996
 */

// Encryption related protocols and types
#include <security_cryptkit/NSCryptors.h>
#include <security_cryptkit/feeTypes.h>

// Classes
#include <security_cryptkit/NSFEEPublicKey.h>
#include <security_cryptkit/NSRandomNumberGenerator.h>
#include <security_cryptkit/NSDESCryptor.h>
#include <security_cryptkit/NSCipherFile.h>
#include <security_cryptkit/NSMD5Hash.h>

// Misc. Functions
#include <security_cryptkit/enc64.h>
#include <security_cryptkit/falloc.h>
#include <security_cryptkit/feeFunctions.h>
