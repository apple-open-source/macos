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
 * CipherFileDES.c - DES-related cipherfile support
 *
 * Revision History
 * ----------------
 * 24 Jun 97 at Apple
 *	Fixed memory leaks via sigData
 * 18 Feb 97 at Apple
 *	Split off from feeCipherFile.c
 */

#include "ckconfig.h"

#if	CRYPTKIT_CIPHERFILE_ENABLE

#include "Crypt.h"
#include "CipherFileDES.h"
#include "falloc.h"
#include "feeDebug.h"
#include <string.h>

/*
 * These functions are only called from feeCipherFile.c.
 */
feeReturn createRandDES(feePubKey sendPrivKey,	// for sig only
	feePubKey recvPubKey,
	const unsigned char *plainText,
	unsigned plainTextLen,
	int genSig,				// 1 ==> generate signature
	unsigned userData,			// for caller's convenience
	feeCipherFile *cipherFile)		// RETURNED if successful
{
	feeRand		frand = NULL;
	feeReturn	frtn;
	unsigned char	desKey[FEE_DES_MIN_STATE_SIZE];
	unsigned char	*encrDesKey = NULL;	// FEED encrypted desKey
	unsigned	encrDesKeyLen;
	feeDES		des = NULL;
	feeFEEDExp	feed = NULL;
	unsigned char	*cipherText = NULL;
	unsigned	cipherTextLen;
	unsigned char	*sigData = NULL;
	unsigned	sigDataLen = 0;
	feeCipherFile	cfile = NULL;
	unsigned char	*pubKeyString = NULL;	// of sendPrivKey
	unsigned	pubKeyStringLen = 0;

	if(recvPubKey == NULL) {
		return FR_BadPubKey;
	}

	/*
	 * Cons up random DES key and a feeDES object with it
	 */
	frand = feeRandAlloc();
	if(frand == NULL) {
		frtn = FR_Internal;
		goto out;
	}
	feeRandBytes(frand, desKey, FEE_DES_MIN_STATE_SIZE);
	des = feeDESNewWithState(desKey, FEE_DES_MIN_STATE_SIZE);
	if(des == NULL) {
		frtn = FR_Internal;
		goto out;
	}

	/*
	 * Encrypt the DES key via FEEDExp
	 */
	feed = feeFEEDExpNewWithPubKey(recvPubKey, NULL, NULL);
	if(feed == NULL) {
		frtn = FR_BadPubKey;
		goto out;
	}
	frtn = feeFEEDExpEncrypt(feed,
		desKey,
		FEE_DES_MIN_STATE_SIZE,
		&encrDesKey,
		&encrDesKeyLen);
	if(frtn) {
		goto out;
	}

	/*
	 * Encrypt the plaintext via DES
	 */
	frtn = feeDESEncrypt(des,
		plainText,
		plainTextLen,
		&cipherText,
		&cipherTextLen);
	if(frtn) {
		goto out;
	}

	if(genSig) {
		/*
		 * We generate signature on ciphertext by convention.
		 */
		if(sendPrivKey == NULL) {
			frtn = FR_BadPubKey;
			goto out;
		}
		frtn = feePubKeyCreateSignature(sendPrivKey,
			cipherText,
			cipherTextLen,
			&sigData,
			&sigDataLen);
		if(frtn) {
			goto out;
		}
		/*
		 * Sender's public key string
		 */
		frtn = feePubKeyCreateKeyString(sendPrivKey,
			(char **)&pubKeyString,
			&pubKeyStringLen);
		if(frtn) {
			/*
			 * Huh?
			 */
			frtn = FR_BadPubKey;
			goto out;
		}
	}

	/*
	 * Cons up a cipherfile
	 */
	cfile = feeCFileNewFromCipherText(CFE_RandDES,
		cipherText,
		cipherTextLen,
		pubKeyString,
		pubKeyStringLen,
		encrDesKey,
		encrDesKeyLen,
		sigData,
		sigDataLen,
		userData);
	if(cfile == NULL) {
		frtn = FR_Internal;
		goto out;
	}

out:
	/* free alloc'd stuff */

	if(cipherText) {
		ffree(cipherText);
	}
	if(feed) {
		feeFEEDExpFree(feed);
	}
	if(frand) {
		feeRandFree(frand);
	}
	if(des) {
		feeDESFree(des);
	}
	if(sigData) {
		ffree(sigData);
	}
	if(encrDesKey) {
		ffree(encrDesKey);
	}
	if(pubKeyString) {
		ffree(pubKeyString);
	}
	memset(desKey, 0, FEE_DES_MIN_STATE_SIZE);
	*cipherFile = cfile;
	return frtn;

}

feeReturn decryptRandDES(feeCipherFile cipherFile,
	feePubKey recvPrivKey,
	feePubKey sendPubKey,				// optional
	unsigned char **plainText,			// RETURNED
	unsigned *plainTextLen,				// RETURNED
	feeSigStatus *sigStatus)			// RETURNED
{
	feeReturn 	frtn = FR_Success;
	unsigned char	*cipherText = NULL;
	unsigned	cipherTextLen;
	feeFEEDExp	feed = NULL;		// to decrypt desKey
	feeDES		des = NULL;		// to decrypt cipherText
	unsigned char	*desKey;
	unsigned	desKeyLen;
	unsigned char	*encrDesKey = NULL;	// FEED encrypted desKey
	unsigned	encrDesKeyLen;
	unsigned char	*sigData = NULL;
	unsigned	sigDataLen;
	unsigned char	*sendPubKeyStr = NULL;
	unsigned	sendPubKeyStrLen = 0;
	feePubKey 	parsedSendPubKey = NULL;

	if(feeCFileEncrType(cipherFile) != CFE_RandDES) {
		frtn = FR_Internal;
		goto out;
	}

	/*
	 * Get ciphertext and encrypted DES key from cipherFile
	 */
	cipherText = feeCFileCipherText(cipherFile, &cipherTextLen);
	if(cipherText == NULL) {
		frtn = FR_BadCipherFile;
		goto out;
	}
	encrDesKey = feeCFileOtherKeyData(cipherFile, &encrDesKeyLen);
	if(encrDesKey == NULL) {
		frtn = FR_BadCipherFile;
		goto out;
	}

	/*
	 * FEED decrypt to get DES key
	 */
	feed = feeFEEDExpNewWithPubKey(recvPrivKey, NULL, NULL);
	if(feed == NULL) {
		frtn = FR_BadPubKey;
		goto out;
	}
	frtn = feeFEEDExpDecrypt(feed,
		encrDesKey,
		encrDesKeyLen,
		&desKey,
		&desKeyLen);
	if(frtn) {
		goto out;
	}

	/*
	 * Now DES decrypt the ciphertext
	 */
	if(desKeyLen != FEE_DES_MIN_STATE_SIZE) {
		frtn = FR_BadCipherFile;
		goto out;
	}
	des = feeDESNewWithState(desKey, desKeyLen);
	if(des == NULL) {
		frtn = FR_Internal;
		goto out;
	}
	frtn = feeDESDecrypt(des,
		cipherText,
		cipherTextLen,
		plainText,
		plainTextLen);
	if(frtn) {
		goto out;
	}

	sigData = feeCFileSigData(cipherFile, &sigDataLen);
	if(sigData) {
		feeReturn sigFrtn;

		if(sendPubKey == NULL) {
			/*
			 * Obtain sender's public key from cipherfile
			 */
			sendPubKeyStr = feeCFileSendPubKeyData(cipherFile,
				&sendPubKeyStrLen);
			if(sendPubKeyStr == NULL) {
			    /*
			     * Hmm..shouldn't really happen, but let's
			     * press on.
			     */
			    *sigStatus = SS_PresentNoKey;
			    goto out;
			}
			parsedSendPubKey = feePubKeyAlloc();
			frtn = feePubKeyInitFromKeyString(parsedSendPubKey,
				(char *)sendPubKeyStr, sendPubKeyStrLen);
			if(frtn) {
			    dbgLog(("parseRandDES: bad sendPubKeyStr\n"));
			    *sigStatus = SS_PresentNoKey;
			    goto out;
			}
			sendPubKey = parsedSendPubKey;
		}
		sigFrtn = feePubKeyVerifySignature(sendPubKey,
			cipherText,
			cipherTextLen,
			sigData,
			sigDataLen);
		switch(sigFrtn) {
		    case FR_Success:
		    	*sigStatus = SS_PresentValid;
			break;
		    default:
		    	*sigStatus = SS_PresentInvalid;
			break;
		}
	}
	else {
		*sigStatus = SS_NotPresent;
	}
out:
	if(cipherText) {
		ffree(cipherText);
	}
	if(feed) {
		feeFEEDExpFree(feed);
	}
	if(des) {
		feeDESFree(des);
	}
	if(desKey) {
		memset(desKey, 0, desKeyLen);
		ffree(desKey);
	}
	if(encrDesKey) {
		ffree(encrDesKey);
	}
	if(sigData) {
		ffree(sigData);
	}
	if(parsedSendPubKey) {
		feePubKeyFree(parsedSendPubKey);
	}
	if(sendPubKeyStr) {
		ffree(sendPubKeyStr);
	}
	return frtn;
}

feeReturn createPubDES(feePubKey sendPrivKey,	// required
	feePubKey recvPubKey,
	const unsigned char *plainText,
	unsigned plainTextLen,
	int genSig,				// 1 ==> generate signature
	unsigned userData,			// for caller's convenience
	feeCipherFile *cipherFile)		// RETURNED if successful
{
	feeRand		frand = NULL;
	feeReturn	frtn;
	unsigned char	*desKey;
	unsigned	desKeyLen;
	feeDES		des = NULL;
	unsigned char	*cipherText = NULL;
	unsigned	cipherTextLen;
	unsigned char	*sigData = NULL;
	unsigned	sigDataLen = 0;
	feeCipherFile	cfile = NULL;
	unsigned char	*pubKeyString = NULL;
	unsigned	pubKeyStringLen;

	if((sendPrivKey == NULL) || (recvPubKey == NULL)) {
		return FR_BadPubKey;
	}

	/*
	 * Get the public string version of sendPrivKey for embedding in
	 * cipherfile
	 */
	frtn = feePubKeyCreateKeyString(sendPrivKey,
		(char **)&pubKeyString,
		&pubKeyStringLen);
	if(frtn) {
		goto out;
	}

	/*
	 * Obtain DES key via key exchange and get a feeDES object with it
	 */
	frtn = feePubKeyCreatePad(sendPrivKey,
		recvPubKey,
		&desKey,
		&desKeyLen);
	if(frtn) {
		goto out;
	}
	des = feeDESNewWithState(desKey, desKeyLen);
	if(des == NULL) {
		frtn = FR_Internal;
		goto out;
	}

	/*
	 * Encrypt the plaintext via DES
	 */
	frtn = feeDESEncrypt(des,
		plainText,
		plainTextLen,
		&cipherText,
		&cipherTextLen);
	if(frtn) {
		goto out;
	}

	if(genSig) {
		/*
		 * We generate signature on ciphertext by convention.
		 */
		frtn = feePubKeyCreateSignature(sendPrivKey,
			cipherText,
			cipherTextLen,
			&sigData,
			&sigDataLen);
		if(frtn) {
			goto out;
		}
	}

	/*
	 * Cons up a cipherfile
	 */
	cfile = feeCFileNewFromCipherText(CFE_PublicDES,
		cipherText,
		cipherTextLen,
		pubKeyString,
		pubKeyStringLen,
		NULL,			// otherKey
		0,
		sigData,
		sigDataLen,
		userData);
	if(cfile == NULL) {
		frtn = FR_Internal;
		goto out;
	}

out:
	/* free alloc'd stuff */

	if(cipherText) {
		ffree(cipherText);
	}
	if(frand) {
		feeRandFree(frand);
	}
	if(des) {
		feeDESFree(des);
	}
	if(desKey) {
		ffree(desKey);
	}
	if(sigData) {
		ffree(sigData);
	}
	if(pubKeyString) {
		ffree(pubKeyString);
	}
	*cipherFile = cfile;
	return frtn;

}

feeReturn decryptPubDES(feeCipherFile cipherFile,
	feePubKey recvPrivKey,
	feePubKey sendPubKey,
	unsigned char **plainText,			// RETURNED
	unsigned *plainTextLen,				// RETURNED
	feeSigStatus *sigStatus)			// RETURNED
{
	feeReturn 	frtn = FR_Success;
	unsigned char	*cipherText = NULL;
	unsigned	cipherTextLen;
	feeDES		des = NULL;		// to decrypt cipherText
	unsigned char	*desKey;
	unsigned	desKeyLen;
	unsigned char	*sigData = NULL;
	unsigned	sigDataLen;
	unsigned char	*pubKeyString = NULL;
	unsigned	pubKeyStringLen;
	feePubKey	decryptPubKey = NULL;	// from cipherfile

	if(feeCFileEncrType(cipherFile) != CFE_PublicDES) {
		frtn = FR_Internal;
		goto out;
	}

	/*
	 * Get ciphertext and sender's public key from cipherFile
	 */
	cipherText = feeCFileCipherText(cipherFile, &cipherTextLen);
	if(cipherText == NULL) {
		frtn = FR_BadCipherFile;
		goto out;
	}
	pubKeyString = feeCFileSendPubKeyData(cipherFile, &pubKeyStringLen);
	if(pubKeyString == NULL) {
		frtn = FR_BadCipherFile;
		goto out;
	}
	decryptPubKey = feePubKeyAlloc();
	frtn = feePubKeyInitFromKeyString(decryptPubKey,
		(char *)pubKeyString,
		pubKeyStringLen);
	if(frtn) {
		goto out;
	}

	/*
	 * key exchange to get DES key
	 */
	frtn = feePubKeyCreatePad(recvPrivKey,
		decryptPubKey,
		&desKey,
		&desKeyLen);
	if(frtn) {
		goto out;
	}

	/*
	 * Now DES decrypt the ciphertext
	 */
	if(desKeyLen < FEE_DES_MIN_STATE_SIZE) {
		frtn = FR_BadCipherFile;
		goto out;
	}
	des = feeDESNewWithState(desKey, desKeyLen);
	if(des == NULL) {
		frtn = FR_Internal;
		goto out;
	}
	frtn = feeDESDecrypt(des,
		cipherText,
		cipherTextLen,
		plainText,
		plainTextLen);
	if(frtn) {
		goto out;
	}

	sigData = feeCFileSigData(cipherFile, &sigDataLen);
	if(sigData) {
		feeReturn sigFrtn;

		if(sendPubKey == NULL) {
			/*
			 * Use key embedded in cipherfile
			 */
			sendPubKey = decryptPubKey;
		}
		sigFrtn = feePubKeyVerifySignature(sendPubKey,
			cipherText,
			cipherTextLen,
			sigData,
			sigDataLen);
		switch(sigFrtn) {
		    case FR_Success:
		    	*sigStatus = SS_PresentValid;
			break;
		    default:
		    	*sigStatus = SS_PresentInvalid;
			break;
		}
	}
	else {
		*sigStatus = SS_NotPresent;
	}
out:
	if(cipherText) {
		ffree(cipherText);
	}
	if(des) {
		feeDESFree(des);
	}
	if(desKey) {
		ffree(desKey);
	}
	if(pubKeyString) {
		ffree(pubKeyString);
	}
	if(sigData) {
		ffree(sigData);
	}
	if(decryptPubKey) {
		feePubKeyFree(decryptPubKey);
	}
	return frtn;
}

#endif	/* CRYPTKIT_CIPHERFILE_ENABLE */

