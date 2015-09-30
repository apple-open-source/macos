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
 * feeCipherFile.h
 *
 * Revision History
 * ----------------
 * 24 Oct 96 at NeXT
 *	Created.
 */

#ifndef	_CK_FEECIPHERFILE_H_
#define _CK_FEECIPHERFILE_H_

#if	!defined(__MACH__)
#include <ckconfig.h>
#include <feeTypes.h>
#include <feePublicKey.h>
#include <CipherFileTypes.h>
#else
#include "ckconfig.h"
#include "feeTypes.h"
#include "feePublicKey.h"
#include "CipherFileTypes.h"
#endif

#if	CRYPTKIT_CIPHERFILE_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Opaque cipherfile object.
 */
typedef void *feeCipherFile;

/*
 * Alloc and return a new feeCipherFile object associated with the specified
 * data.
 */
feeCipherFile feeCFileNewFromCipherText(cipherFileEncrType encrType,
	const unsigned char *cipherText,
	unsigned cipherTextLen,
	const unsigned char *sendPubKeyData,
	unsigned sendPubKeyDataLen,
	const unsigned char *otherKeyData,
	unsigned otherKeyDataDataLen,
	const unsigned char *sigData,	// optional; NULL means no signature
	unsigned sigDataLen,		// 0 if sigData is NULL
	unsigned userData);		// for caller's convenience

/*
 * Obtain the contents of a feeCipherFile as a byte stream. Caller must free
 * the returned data.
 */
feeReturn feeCFileDataRepresentation(feeCipherFile cipherFile,
	const unsigned char **dataRep,	// RETURNED
	unsigned *dataRepLen);		// RETURNED

/*
 * Alloc and return a new feeCipherFile object, given a byte stream (originally
 * obtained from feeCFDataRepresentation()).
 */
feeReturn feeCFileNewFromDataRep(const unsigned char *dataRep,
	unsigned dataRepLen,
	feeCipherFile *cipherFile);	// RETURNED if sucessful

/*
 * Free a feeCipherFile object.
 */
void feeCFileFree(feeCipherFile cipherFile);

/*
 * Given a feeCipherFile object (typically obtained from
 * feeCFileNewFromDataRep()), obtain its constituent parts.
 *
 * Data returned must be freed by caller.
 * feeCFileSigData(), feeCFileSendPubKeyData, and feeCFileOtherKeyData()
 * may return NULL, indicating component not present.
 */
cipherFileEncrType feeCFileEncrType(feeCipherFile cipherFile);
unsigned char *feeCFileCipherText(feeCipherFile cipherFile,
	unsigned *cipherTextLen);		// RETURNED
unsigned char *feeCFileSendPubKeyData(feeCipherFile cipherFile,
	unsigned *sendPubKeyDataLen);		// RETURNED
unsigned char *feeCFileOtherKeyData(feeCipherFile cipherFile,
	unsigned *otherKeyDataLen);		// RETURNED
unsigned char *feeCFileSigData(feeCipherFile cipherFile,
	unsigned *sigDataLen);			// RETURNED
unsigned feeCFileUserData(feeCipherFile cipherFile);

/*
 * High-level feeCipherFile support.
 */

/*
 * Obtain the data representation of a feeCipherFile given the specified
 * plainText and cipherFileEncrType.
 * Receiver's public key is required for all encrTypes; sender's private
 * key is required for signature generation and also for encrType
 * CFE_PublicDES and CFE_FEED.
 */
feeReturn createCipherFile(feePubKey sendPrivKey,
	feePubKey recvPubKey,
	cipherFileEncrType encrType,
	const unsigned char *plainText,
	unsigned plainTextLen,
	int genSig,				// 1 ==> generate signature
	int doEnc64,				// 1 ==> perform enc64
	unsigned userData,			// for caller's convenience
	unsigned char **cipherFileData,		// RETURNED
	unsigned *cipherFileDataLen);		// RETURNED

/*
 * Parse and decrypt a cipherfile given its data representation.
 *
 * recvPrivKey is required in all cases. If sendPubKey is present,
 * sendPubKey - rather than the embedded sender's public key - will be
 * used for signature validation.
 */
feeReturn parseCipherFile(feePubKey recvPrivKey,	// required
	feePubKey sendPubKey,			// optional, for signature
	const unsigned char *cipherFileData,
	unsigned cipherFileDataLen,
	int doDec64,				// 1 ==> perform dec64
	cipherFileEncrType *encrType,		// RETURNED
	unsigned char **plainText,		// malloc'd & RETURNED
	unsigned *plainTextLen,			// RETURNED
	feeSigStatus *sigStatus,		// RETURNED
	unsigned *userData);			// RETURNED

/*
 * Decrypt a feeCipherFile object obtained via feeCFileNewFromDataRep().
 * recvPrivKey is required in all cases. If sendPubKey is present,
 * sendPubKey - rather than the embedded sender's public key - will be
 * used for signature validation.
 *
 * Note: this function is used (in conjunction with feeCFileNewFromDataRep())
 * rather than the simpler parseCipherFile(), in case the caller needs
 * access to CipherFile fields not returned in parseCipherFile(). For
 * example, the caller might want to get the sender's public key data
 * via feeCFileSendPubKeyData().
 */
feeReturn decryptCipherFile(feeCipherFile cipherFile,
	feePubKey recvPrivKey,			// required
	feePubKey sendPubKey,			// optional, for signature
	unsigned char **plainText,		// malloc'd & RETURNED
	unsigned *plainTextLen,			// RETURNED
	feeSigStatus *sigStatus);		// RETURNED

#ifdef __cplusplus
}
#endif

#endif	/* CRYPTKIT_CIPHERFILE_ENABLE */
#endif	/*_CK_FEECIPHERFILE_H_*/
