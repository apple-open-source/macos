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
 * feeCipherFile.c - general cipherfile support
 *
 * Revision History
 * ----------------
 * 05 Feb 97 at Apple
 *	Added CFE_FEED and CFE_FEEDExp types.
 * 24 Oct 96 at NeXT
 *	Created.
 */

#include "feeCipherFile.h"
#include "falloc.h"
#include "feeFEEDExp.h"
#include "feeFEED.h"
#include "feeDebug.h"
#include "CipherFileFEED.h"
#include "CipherFileDES.h"


/*
 * Create a cipherfile of specified cipherFileEncrType.
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
	unsigned *cipherFileDataLen)		// RETURNED
{
	feeReturn 		frtn = FR_Success;
	feeCipherFile		cipherFile = NULL;
	unsigned char 		*cipherData = NULL;
	unsigned 		cipherDataLen;

	/*
	 * Dispatch to encrType-specific code.
	 */
	switch(encrType) {
	    case CFE_RandDES:
	    	frtn = createRandDES(sendPrivKey,
			recvPubKey,
			plainText,
			plainTextLen,
			genSig,
			userData,
			&cipherFile);
		break;
	    case CFE_PublicDES:
	    	frtn = createPubDES(sendPrivKey,
			recvPubKey,
			plainText,
			plainTextLen,
			genSig,
			userData,
			&cipherFile);
		break;
	    case CFE_FEED:
	    	frtn = createFEED(sendPrivKey,
			recvPubKey,
			plainText,
			plainTextLen,
			genSig,
			userData,
			&cipherFile);
		break;
	    case CFE_FEEDExp:
	    	frtn = createFEEDExp(sendPrivKey,
			recvPubKey,
			plainText,
			plainTextLen,
			genSig,
			userData,
			&cipherFile);
		break;
	    default:
	    	frtn = FR_Unimplemented;
		break;
	}

	if(frtn) {
		goto out;
	}

	/*
	 * Common logic for all encrTypes
	 */

	/*
	 * Get the cipherfile's raw data
	 */
	frtn = feeCFileDataRepresentation(cipherFile,
		(const unsigned char **)&cipherData,
		&cipherDataLen);
	if(frtn) {
		goto out;
	}

	/*
	 * Optionally encode in 64-char ASCII
	 */
	if(doEnc64) {
		*cipherFileData = enc64(cipherData,
			cipherDataLen,
			cipherFileDataLen);
		ffree(cipherData);
		if(*cipherFileData == NULL) {
			frtn = FR_Internal;
			ffree(cipherData);
			goto out;
		}
	}
	else {
		*cipherFileData = cipherData;
		*cipherFileDataLen = cipherDataLen;
	}
out:
	/* free stuff */
	if(cipherFile) {
		feeCFileFree(cipherFile);
	}
	return frtn;
}

/*
 * Parse a cipherfile.
 *
 * sendPubKey only needed for cipherFileEncrType CFE_RandDES if signature
 * is present. If sendPubKey is present, it will be used for signature
 * validation rather than the embedded sender's public key.
 */
feeReturn parseCipherFile(feePubKey recvPrivKey,
	feePubKey sendPubKey,
	const unsigned char *cipherFileData,
	unsigned cipherFileDataLen,
	int doDec64,					// 1 ==> perform dec64
	cipherFileEncrType *encrType,	// RETURNED
	unsigned char **plainText,		// RETURNED
	unsigned *plainTextLen,			// RETURNED
	feeSigStatus *sigStatus,		// RETURNED
	unsigned *userData)				// RETURNED
{
	feeReturn 		frtn;
	unsigned char		*cipherData = NULL;
	unsigned		cipherDataLen;
	int			freeCipherData = 0;
	feeCipherFile		cipherFile = NULL;

	*plainText = NULL;
	*plainTextLen = 0;

	if(recvPrivKey == NULL) {	// always required
		frtn = FR_BadPubKey;
		goto out;
	}

	/*
	 * First, optional dec64()
	 */
	if(doDec64) {
		cipherData = dec64(cipherFileData,
			cipherFileDataLen,
			&cipherDataLen);
		if(cipherData == NULL) {
			frtn = FR_BadEnc64;
			goto out;
		}
		else {
			freeCipherData = 1;
		}
	}
	else {
		cipherData = (unsigned char *)cipherFileData;
		cipherDataLen = cipherFileDataLen;
	}

	/*
	 * Cons up a feeCipherFile object.
	 */
	frtn = feeCFileNewFromDataRep(cipherData,
		cipherDataLen,
		&cipherFile);
	if(frtn) {
		goto out;
	}
	*encrType = feeCFileEncrType(cipherFile);
	*userData = feeCFileUserData(cipherFile);
	frtn = decryptCipherFile(cipherFile,
		recvPrivKey,
		sendPubKey,
		plainText,
		plainTextLen,
		sigStatus);

out:
	/* free stuff */

	if(cipherData && freeCipherData) {
		ffree(cipherData);
	}
	if(cipherFile) {
		feeCFileFree(cipherFile);
	}
	return frtn;
}

/*
 * Decrypt a feeCipherFile obtained via feeCFileNewFromDataRep().
 * recvPrivKey is required in all cases. If sendPubKey is present,
 * sendPubKey - rather than the embedded sender's public key - will be
 * used for signature validation.
 */
feeReturn decryptCipherFile(feeCipherFile cipherFile,
	feePubKey recvPrivKey,			// required
	feePubKey sendPubKey,			// optional, for signature
	unsigned char **plainText,		// malloc'd & RETURNED
	unsigned *plainTextLen,			// RETURNED
	feeSigStatus *sigStatus)		// RETURNED
{
	cipherFileEncrType 	encrType = feeCFileEncrType(cipherFile);
	feeReturn 		frtn;

	*plainText = NULL;
	*plainTextLen = 0;

	/*
	 * Dispatch to encrType-specific code.
	 */
	switch(encrType) {
	    case CFE_RandDES:
	    	frtn = decryptRandDES(cipherFile,
			recvPrivKey,
			sendPubKey,
			plainText,
			plainTextLen,
			sigStatus);
		break;
	    case CFE_PublicDES:
	    	frtn = decryptPubDES(cipherFile,
			recvPrivKey,
			sendPubKey,
			plainText,
			plainTextLen,
			sigStatus);
		break;
	    case CFE_FEED:
	    	frtn = decryptFEED(cipherFile,
			recvPrivKey,
			sendPubKey,
			plainText,
			plainTextLen,
			sigStatus);
		break;
	    case CFE_FEEDExp:
	    	frtn = decryptFEEDExp(cipherFile,
			recvPrivKey,
			sendPubKey,
			plainText,
			plainTextLen,
			sigStatus);
		break;
	    default:
	    	frtn = FR_Unimplemented;
		break;
	}
	return frtn;
}
