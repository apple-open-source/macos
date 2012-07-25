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
 * CipherFileTypes.h
 *
 * Revision History
 * ----------------
 * 8/24/98		ap
 *	Added tags around #endif comment.
 * 19 Feb 97	Doug Mitchell at NeXT
 *	Created.
 */

#ifndef	_CK_CFILETYPES_H_
#define _CK_CFILETYPES_H_

#include "ckconfig.h"

#if	CRYPTKIT_CIPHERFILE_ENABLE

#include "feeCipherFile.h"

/*
 * Type of encryption used in a CipherFile.
 */
typedef enum {

    /*
     * DES encryption using pad created via public key exchange; sender's
     * public key is embedded.
     */
    CFE_PublicDES = 1,

    /*
     * Random DES key used for encryption. The DES key is encrypted via
     * FEEDExp using recipient's public key; the result is embedded in the
     * CipherFile. Sender's public key is embedded only if
     * signature is generated.
     */
    CFE_RandDES = 2,

    /*
     * 1:1 FEED encryption. Sender's public key is embedded.
     */
    CFE_FEED = 3,

    /*
     * 2:1 FEED encryption. Sender's public key is embedded only if signature
     * is generated.
     */
    CFE_FEEDExp = 4,

    /*
     * User-defined cipherfile.
     */
    CFE_Other = 5

} cipherFileEncrType;


/*
 * Signature status upon decryption of a CipherFile.
 */
typedef enum {

    SS_NotPresent = 0,		// Signature not present.
    SS_PresentValid = 1,	// Signature present and valid.
    SS_PresentNoKey = 2,	// Signature present, but no public key
    						//    available to validate it.
    SS_PresentInvalid = 3	// Signature present and invalid.

} feeSigStatus;

#endif	/* CRYPTKIT_CIPHERFILE_ENABLE */

#endif /* _CK_CFILETYPES_H_ */
