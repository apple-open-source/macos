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
 * CipherFileFEED.c - FEED and FEEDExp related cipherfile support
 *
 * Revision History
 * ----------------
 * 24 Jun 97	Doug Mitchell at Apple
 *	Fixed memory leaks via sigData
 * 18 Feb 97	Doug Mitchell at Apple
 *	Split off from feeCipherFile.c
 */

#include "ckconfig.h"

#if	CRYPTKIT_CIPHERFILE_ENABLE

#include "Crypt.h"
#include "CipherFileFEED.h"
#include "falloc.h"
#include "feeDebug.h"

feeReturn createFEED(feePubKey sendPrivKey,	// required
	feePubKey recvPubKey,
	const unsigned char *plainText,
	unsigned plainTextLen,
	int genSig,				// 1 ==> generate signature
	unsigned userData,			// for caller's convenience
	feeCipherFile *cipherFile)		// RETURNED if successful
{
	feeReturn	frtn;
	feeFEED		feed = NULL;
	unsigned char	*cipherText = NULL;
	unsigned	cipherTextLen;
	unsigned char	*sigData = NULL;
	unsigned	sigDataLen = 0;
	feeCipherFile	cfile = NULL;
	unsigned char	*pubKeyString = NULL;	// of sendPrivKey
	unsigned	pubKeyStringLen = 0;

	if((sendPrivKey == NULL) || (recvPubKey == NULL)) {
		return FR_BadPubKey;
	}

	/*
	 * FEED encrypt plaintext
	 */
	feed = feeFEEDNewWithPubKey(sendPrivKey, recvPubKey, FF_ENCRYPT, NULL, NULL);
	if(feed == NULL) {
		frtn = FR_BadPubKey;
		goto out;
	}
	frtn = feeFEEDEncrypt(feed,
		plainText,
		plainTextLen,
		&cipherText,
		&cipherTextLen);
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
	cfile = feeCFileNewFromCipherText(CFE_FEED,
		cipherText,
		cipherTextLen,
		pubKeyString,
		pubKeyStringLen,
		NULL,
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
	if(feed) {
		feeFEEDFree(feed);
	}
	if(pubKeyString) {
		ffree(pubKeyString);
	}
	if(sigData) {
		ffree(sigData);
	}
	*cipherFile = cfile;
	return frtn;

}

feeReturn decryptFEED(feeCipherFile cipherFile,
	feePubKey recvPrivKey,
	feePubKey sendPubKey,				// optional
	unsigned char **plainText,			// RETURNED
	unsigned *plainTextLen,				// RETURNED
	feeSigStatus *sigStatus)			// RETURNED
{
	feeReturn 	frtn = FR_Success;
	unsigned char	*cipherText = NULL;
	unsigned	cipherTextLen;
	feeFEED		feed = NULL;
	unsigned char	*sigData = NULL;
	unsigned	sigDataLen;
	unsigned char	*sendPubKeyStr = NULL;
	unsigned	sendPubKeyStrLen = 0;
	feePubKey 	parsedSendPubKey = NULL;

	if(feeCFileEncrType(cipherFile) != CFE_FEED) {
		frtn = FR_Internal;
		goto out;
	}
//printf("decryptFEED\n");
//printf("privKey:\n"); printPubKey(recvPrivKey);
//printf("pubKey:\n");  printPubKey(sendPubKey);
	/*
	 * Get ciphertext and sender's public key from cipherFile
	 */
	cipherText = feeCFileCipherText(cipherFile, &cipherTextLen);
	if(cipherText == NULL) {
		frtn = FR_BadCipherFile;
		goto out;
	}
	sendPubKeyStr = feeCFileSendPubKeyData(cipherFile, &sendPubKeyStrLen);
	if(sendPubKeyStr == NULL) {
		frtn = FR_BadCipherFile;
		goto out;
	}
	parsedSendPubKey = feePubKeyAlloc();
	frtn = feePubKeyInitFromKeyString(parsedSendPubKey,
		(char *)sendPubKeyStr,
		sendPubKeyStrLen);
	if(frtn) {
		frtn = FR_BadCipherFile;
		goto out;
	}
//printf("parsedSendPubKey:\n");  printPubKey(parsedSendPubKey);

	/*
	 * FEED decrypt
	 */
	feed = feeFEEDNewWithPubKey(recvPrivKey, parsedSendPubKey, FF_DECRYPT, NULL, NULL);
	if(feed == NULL) {
		frtn = FR_BadPubKey;
		goto out;
	}
	frtn = feeFEEDDecrypt(feed,
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
			 * use embedded sender's public key
			 */
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
		feeFEEDFree(feed);
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

feeReturn createFEEDExp(feePubKey sendPrivKey,	// for sig only
	feePubKey recvPubKey,
	const unsigned char *plainText,
	unsigned plainTextLen,
	int genSig,				// 1 ==> generate signature
	unsigned userData,			// for caller's convenience
	feeCipherFile *cipherFile)		// RETURNED if successful
{
	feeReturn	frtn;
	feeFEEDExp	feed = NULL;
	unsigned char	*cipherText = NULL;
	unsigned	cipherTextLen;
	unsigned char	*sigData = NULL;
	unsigned	sigDataLen = 0;
	feeCipherFile	cfile = NULL;
	unsigned char	*pubKeyString = NULL;	// of sendPrivKey, for sig
	unsigned	pubKeyStringLen = 0;

	if(recvPubKey == NULL) {
		return FR_BadPubKey;
	}

	/*
	 * FEEDExp encrypt plaintext
	 */
	feed = feeFEEDExpNewWithPubKey(recvPubKey, NULL, NULL);
	if(feed == NULL) {
		frtn = FR_BadPubKey;
		goto out;
	}
	frtn = feeFEEDExpEncrypt(feed,
		plainText,
		plainTextLen,
		&cipherText,
		&cipherTextLen);
	if(frtn) {
		goto out;
	}

	if(genSig) {
		if(sendPrivKey == NULL) {
			frtn = FR_IllegalArg;
			goto out;
		}
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
	cfile = feeCFileNewFromCipherText(CFE_FEEDExp,
		cipherText,
		cipherTextLen,
		pubKeyString,
		pubKeyStringLen,
		NULL,
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
	if(feed) {
		feeFEEDExpFree(feed);
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

feeReturn decryptFEEDExp(feeCipherFile cipherFile,
	feePubKey recvPrivKey,
	feePubKey sendPubKey,				// optional
	unsigned char **plainText,			// RETURNED
	unsigned *plainTextLen,				// RETURNED
	feeSigStatus *sigStatus)			// RETURNED
{
	feeReturn 	frtn = FR_Success;
	unsigned char	*cipherText = NULL;
	unsigned	cipherTextLen;
	feeFEEDExp	feed = NULL;
	unsigned char	*sigData = NULL;
	unsigned	sigDataLen;
	unsigned char	*sendPubKeyStr = NULL;
	unsigned	sendPubKeyStrLen = 0;
	feePubKey 	parsedSendPubKey = NULL;

	if(feeCFileEncrType(cipherFile) != CFE_FEEDExp) {
		frtn = FR_Internal;
		goto out;
	}

	/*
	 * Get ciphertext from cipherFile
	 */
	cipherText = feeCFileCipherText(cipherFile, &cipherTextLen);
	if(cipherText == NULL) {
		frtn = FR_BadCipherFile;
		goto out;
	}

	/*
	 * FEEDExp decrypt
	 */
	feed = feeFEEDExpNewWithPubKey(recvPrivKey, NULL, NULL);
	if(feed == NULL) {
		frtn = FR_BadPubKey;
		goto out;
	}
	frtn = feeFEEDExpDecrypt(feed,
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
			 * use embedded sender's public key
			 */
			sendPubKeyStr = feeCFileSendPubKeyData(cipherFile,
				&sendPubKeyStrLen);
			if(sendPubKeyStr == NULL) {
				frtn = FR_BadCipherFile;
				goto out;
			}
			parsedSendPubKey = feePubKeyAlloc();
			frtn = feePubKeyInitFromKeyString(parsedSendPubKey,
				(char *)sendPubKeyStr, sendPubKeyStrLen);
			if(frtn) {
				frtn = FR_BadCipherFile;
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

#endif	/* CRYPTKIT_CIPHERFILE_ENABLE */
