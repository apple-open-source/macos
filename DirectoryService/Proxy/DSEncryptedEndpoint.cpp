/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header DSEncryptedEndpoint
 * Layered endpoint that enciphers data.
 */

/*
	Note: all network addresses in method parameters and return values
	are in host byte order - they are converted to network byte order
	inside the methods for socket calls.

	Note2: need to be aware of which routines are FW or Server exclusive
	for what type of logging
 */


#include <string.h>	// for memset(), memcpy() and strcpy()

#include <new>					// for bad_alloc exceptions
#include <stdexcept>			// for standard exceptions

#include <machine/byte_order.h>

#ifdef DSSERVERTCP
#include "CLog.h"
#endif
#include "DSEncryptedEndpoint.h"

uint8	paramBlob[]	= { \
0x30, 0x52, 0x06, 0x08, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x03, 0x30, 0x46, 0x02, 0x41,\
0x00, 0xa0, 0xd4, 0x42, 0xd5, 0x68, 0x08, 0x94, 0xc9, 0xef, 0xb7, 0x18, 0x9c, 0x0b, 0x72, 0x53,\
0xac, 0x8a, 0x7b, 0xc2, 0x40, 0x17, 0x96, 0x29, 0xd1, 0xf2, 0x96, 0xe8, 0x2b, 0x4e, 0x48, 0xaf,\
0x59, 0xbe, 0x29, 0xc4, 0x9b, 0x52, 0xda, 0x05, 0x18, 0x29, 0x73, 0xff, 0xd5, 0x26, 0x47, 0x53,\
0x54, 0x79, 0xf4, 0x39, 0x96, 0x6f, 0x61, 0x5e, 0xe6, 0xfc, 0x92, 0x7d, 0xf4, 0x20, 0x6e, 0xa9,\
0xa3, 0x02, 0x01, 0x02 };

// ----------------------------------------------------------------------------
//	¥ DSEncryptedEndpoint Instance Methods
// ----------------------------------------------------------------------------
#pragma mark **** Instance Methods ****

// ----------------------------------------------------------------------------
// ¥ Constructor / Destructor
// ----------------------------------------------------------------------------

DSEncryptedEndpoint::DSEncryptedEndpoint (
	int		inConnectFD,
	UInt32	inOpenTimeout,
	UInt32	inRWTimeout)
	: inherited (inConnectFD, inOpenTimeout, inRWTimeout)
{
	CSSM_RETURN 	crtn;
	
	crtn = cdsaCspAttach(&fcspHandle);
	if (crtn)
	{
		//attaching to security FW failed
	}
	else
	{
		fOurDerivedKey.KeyData.Data		= nil;
		fOurDerivedKey.KeyData.Length	= 0;
		
		//set the param block
		fOurParamBlock.Data		= (uint8 *)paramBlob;
		fOurParamBlock.Length	= 84;
	}
}

DSEncryptedEndpoint::DSEncryptedEndpoint (
	const DSTCPEndpoint	*inEndpoint,
	const uInt32 		inSessionID)
	: inherited (inEndpoint, inSessionID)
{
	CSSM_RETURN		crtn;

	// Clear the listener FD; this socket should not listen for new connections.
	mListenFD = -1 ;
	
	crtn = cdsaCspAttach(&fcspHandle);
	if (crtn)
	{
		//attaching to security FW failed
	}
	else
	{
		fOurDerivedKey.KeyData.Data		= nil;
		fOurDerivedKey.KeyData.Length	= 0;
		
		//set the param block
		fOurParamBlock.Data		= (uint8 *)paramBlob;
		fOurParamBlock.Length	= 84;
	}
}

DSEncryptedEndpoint::~DSEncryptedEndpoint (void)
{
	cdsaFreeKey(fcspHandle, &fOurDerivedKey);
	cdsaCspDetach(fcspHandle);
}


// ----------------------------------------------------------------------------
// ¥ ClientNegotiateKey
//	Client side of key negotiation exchange
//  returns eDSNoErr for success
// ----------------------------------------------------------------------------

sInt32 DSEncryptedEndpoint::ClientNegotiateKey ( void )
{
	bool			bFirstPass	= true;
	sInt32			result		= eDSContinue;
	CSSM_RETURN		crtn;
	uInt8		   *outBuff		= nil;
	uInt32			outLen		= 0;
	uInt8		   *inBuff		= nil;
	uInt32 			inLen		= 0;
	uInt32 			readBytes	= 0;
	CSSM_KEY		myPriv;
	CSSM_KEY		myPub;
	CSSM_KEY		localDerived;
	uInt32			theTestBlob	= 0;
	uInt32			theTestBlobBE	= 0;
	CSSM_DATA		plainText	= {0, NULL};
	CSSM_DATA		cipherText	= {0, NULL};
	sInt32			syncRet		= eDSNoErr;

	bzero(&myPriv,sizeof(CSSM_KEY));
	bzero(&myPub,sizeof(CSSM_KEY));
	bzero(&localDerived,sizeof(CSSM_KEY));
	
	do {
		if (bFirstPass)
		{
			crtn = cdsaDhGenerateKeyPair(
				fcspHandle,
				&myPub,
				&myPriv,
				DH_KEY_SIZE,
				&fOurParamBlock,
				NULL);
			if (crtn)
			{
				//printf("failed to generate the key pair\n");
				return eDSCorruptBuffer; //TODO need an eDSEncryptError
			}
			// build the send buffer with the auth tag
			outBuff = (uInt8*)calloc(1,4 + myPub.KeyData.Length);
			*((FourCharCode *) outBuff) = NXSwapHostLongToBig(DSTCPAuthTag);
			outLen = myPub.KeyData.Length;
			memcpy(outBuff+4, (uInt8 *)myPub.KeyData.Data, outLen);
			outLen += 4; //for the tag
			bFirstPass = false;
		}
		else //secondPass
		{
			crtn = cdsaDhKeyExchange(
				fcspHandle,
				&myPriv,
				(void *)inBuff,
				inLen,
				&localDerived,
				DERIVE_KEY_SIZE,
				DERIVE_KEY_ALG);
			if (crtn)
			{
				if (inBuff != nil)
				{
					free(inBuff);
					inBuff = nil;
				}
				//printf("failed to generate the key pair exchange\n");
				return eDSCorruptBuffer; //TODO need an eDSEncryptError
			}
			//now need to send server a blob to confirm keys work
			::srandom(getpid() + time(NULL));
			theTestBlob			= random();
            theTestBlobBE = NXSwapHostLongToBig(theTestBlob);
			plainText.Data		= (uInt8 *)&theTestBlobBE;
			plainText.Length	= 4;

			crtn = cdsaEncrypt(
				fcspHandle,
				&localDerived,
				&plainText,
				&cipherText);
			if (crtn)
			{
				//cssmPerror("cdsaEncrypt", crtn);
				//("failed to encrypt the test blob\n");
				return eDSCorruptBuffer; //TODO need an eDSEncryptError
			}

			outBuff	= cipherText.Data;
			outLen	= cipherText.Length;

			result = eDSNoErr;
		}
		// send messge to network
		if (outLen != 0)
		{
			SendBuffer(outBuff,outLen) ;
			if (outBuff != nil)
			{
				free(outBuff);
				outBuff = nil;
			}
			if (inBuff != nil)
			{
				free(inBuff);
				inBuff = nil;
			}
			// read message from network
			syncRet = SyncToMessageBody(true, &inLen);
			if (syncRet != eDSNoErr) return syncRet;
			inBuff = (uInt8*)calloc(1, inLen);
			readBytes = DoTCPRecvFrom(inBuff, inLen);
			if (readBytes != inLen)
			{
				if (inBuff != nil)
				{
					free(inBuff);
					inBuff = nil;
				}
				//printf("failed to read all the data\n");
				return eDSCorruptBuffer; //TODO need an eDSEncryptError
			}
		}
	} while ( result == eDSContinue );
	
	if (inLen != 0)
	{
		cipherText.Data		= inBuff;
		cipherText.Length	= inLen;
		plainText.Data		= nil;
		plainText.Length	= 0;
		crtn = cdsaDecrypt(
			fcspHandle,
			&localDerived,
			&cipherText,
			&plainText);
		if (crtn)
		{
			//cssmPerror("cdsaDecrypt", crtn);
			//printf("failed to decrypt the test blob plus one\n");
			result = eDSCorruptBuffer; //TODO need an eDSEncryptError
		}
		else
		{
			if (	(plainText.Data == nil) ||
					(plainText.Length != 4) ||
					(theTestBlob+1 != NXSwapBigLongToHost(*((uInt32*)plainText.Data))) )
			{
				//printf("failed to compare the updated test blob plus one\n");
				result = eDSCorruptBuffer; //TODO need an eDSEncryptError
			}
			if (plainText.Data != nil)
			{
				free(plainText.Data);
			}
		}
	}
	else
	{
		//printf("failed to get an inLen\n");
		return eDSCorruptBuffer; //TODO need an eDSEncryptError
	}
	
	if (inBuff != nil)
	{
		free(inBuff);
		inBuff = nil;
	}
	
	//cdsaFreeKey(fcspHandle, &myPub);
	//cdsaFreeKey(fcspHandle, &myPriv);
	if (result == eDSNoErr)
	{
		fOurDerivedKey = localDerived;
	}
	return result;
} // ClientNegotiateKey


// ----------------------------------------------------------------------------
// ¥ ServerNegotiateKey
//	Server side of key negotiation exchange
//  returns eDSNoErr for success
// ----------------------------------------------------------------------------

sInt32 DSEncryptedEndpoint::ServerNegotiateKey ( void )
{
	bool			bFirstPass	= true;
	sInt32			result		= eDSContinue;
	CSSM_RETURN		crtn;
	uInt8		   *outBuff		= nil;
	uInt32			outLen		= 0;
	uInt8		   *inBuff		= nil;
	uInt32 			inLen		= 0;
	uInt32 			readBytes	= 0;
	CSSM_KEY		myPriv;
	CSSM_KEY		myPub;
	CSSM_KEY		localDerived;
	CSSM_DATA		plainText	= {0, NULL};
	CSSM_DATA		cipherText	= {0, NULL};
	FourCharCode	rxCode		= 0;
	sInt32			syncRet		= eDSNoErr;
	
	bzero(&myPriv,sizeof(CSSM_KEY));
	bzero(&myPub,sizeof(CSSM_KEY));
	bzero(&localDerived,sizeof(CSSM_KEY));

	do {
		if (inBuff != nil)
		{
			free(inBuff);
			inBuff = nil;
		}
		// read message from network
		syncRet = SyncToMessageBody(true, &inLen);
		if (syncRet != eDSNoErr) return syncRet;
		inBuff = (uInt8*)calloc(1, inLen);
		readBytes = DoTCPRecvFrom(inBuff, inLen);
		if (readBytes != inLen)
		{
			//failed to read all the data
			if (inBuff != nil)
			{
				free(inBuff);
				inBuff = nil;
			}
			return eDSCorruptBuffer; //TODO need an eDSEncryptError
		}

		if (bFirstPass)
		{
			rxCode = NXSwapBigLongToHost(*((FourCharCode *) inBuff));
            //first check the auth tag
			if ( (inLen <= 4) || (rxCode != DSTCPAuthTag) )
			{
				if (inBuff != nil)
				{
					free(inBuff);
					inBuff = nil;
				}
				return eDSCorruptBuffer; //TODO need an eDSEncryptError
			}
			crtn = cdsaDhGenerateKeyPair(
				fcspHandle,
				&myPub,
				&myPriv,
				DH_KEY_SIZE,
				&fOurParamBlock,
				NULL);
			if (crtn)
			{
				//failed to generate the key pair
				return eDSCorruptBuffer; //TODO need an eDSEncryptError
			}
			crtn = cdsaDhKeyExchange(
				fcspHandle,
				&myPriv,
				(void *)(inBuff+4),
				(inLen-4),
				&localDerived,
				DERIVE_KEY_SIZE,
				DERIVE_KEY_ALG);
			if (crtn)
			{
				if (inBuff != nil)
				{
					free(inBuff);
					inBuff = nil;
				}
				//failed to generate the key pair exchange
				return eDSCorruptBuffer; //TODO need an eDSEncryptError
			}
			outBuff	= myPub.KeyData.Data;
			outLen	= myPub.KeyData.Length;
			bFirstPass = false;
		}
		else //secondPass
		{
			if (inLen != 0)
			{
				cipherText.Data		= inBuff;
				cipherText.Length	= inLen;
				crtn = cdsaDecrypt(
					fcspHandle,
					&localDerived,
					&cipherText,
					&plainText);
				if (crtn)
				{
					//cssmPerror("cdsaDecrypt", crtn);
					if (inBuff != nil)
					{
						free(inBuff);
						inBuff = nil;
					}
					//failed to decrypt the test blob
					return eDSCorruptBuffer; //TODO need an eDSEncryptError
				}
				if (inBuff != nil)
				{
					free(inBuff);
					inBuff = nil;
				}
			}
			if ( (plainText.Data == nil) || (plainText.Length != 4) )
			{
				//failed to decrypt the test blob
				return eDSCorruptBuffer; //TODO need an eDSEncryptError
			}
			//add one to test blob received
            uInt32 temp = NXSwapBigLongToHost(*(uInt32*)plainText.Data);
            temp++;
			*(uInt32*)plainText.Data = NXSwapHostLongToBig(temp);

			cipherText.Data		= nil;
			cipherText.Length	= 0;

			crtn = cdsaEncrypt(
				fcspHandle,
				&localDerived,
				&plainText,
				&cipherText);
			if (crtn)
			{
				//cssmPerror("cdsaEncrypt", crtn);
				//failed to encrypt the test blob
				return eDSCorruptBuffer; //TODO need an eDSEncryptError
			}

			outBuff	= cipherText.Data;
			outLen	= cipherText.Length;

			result = eDSNoErr;
		}
		// send messge to network
		if (outLen != 0)
		{
			SendBuffer(outBuff,outLen) ;
			if (outBuff != nil)
			{
				free(outBuff);
				outBuff = nil;
			}
		}
	} while ( result == eDSContinue );
	
	//cdsaFreeKey(fcspHandle, &myPub);
	//cdsaFreeKey(fcspHandle, &myPriv);
	if (result == eDSNoErr)
	{
		fOurDerivedKey = localDerived;
	}
	return result;
} // ServerNegotiateKey


// ----------------------------------------------------------------------------
// ¥ EncryptData
//	Encrypt a block.
// ----------------------------------------------------------------------------

void DSEncryptedEndpoint::EncryptData ( void *inData, const uInt32 inBuffSize, void *&outData, uInt32 &outBuffSize )
{
	CSSM_RETURN		crtn;
	CSSM_DATA		plainText	= {0, NULL};
	CSSM_DATA		cipherText	= {0, NULL};

	// Pass through if the key has not been defined.
    if (fOurDerivedKey.KeyData.Data == nil)
	{
		outBuffSize = 0;
		return;
    }

	plainText.Data		= (uInt8 *)inData;
	plainText.Length	= inBuffSize;
	crtn = cdsaEncrypt(
		fcspHandle,
		&fOurDerivedKey,
		&plainText,
		&cipherText);
	if (crtn)
	{
		//cssmPerror("cdsaEncrypt", crtn);
		//failed to encrypt the data
		return;
	}
	else
	{
		outData		= cipherText.Data;
		outBuffSize	= cipherText.Length;
	}
	return;
}


// ----------------------------------------------------------------------------
// ¥ DecryptData
//	Decrypt a block.
// ----------------------------------------------------------------------------

void DSEncryptedEndpoint::DecryptData ( void *inData, const uInt32 inBuffSize, void *&outData, uInt32 &outBuffSize )
{
	CSSM_RETURN		crtn;
	CSSM_DATA		plainText	= {0, NULL};
	CSSM_DATA		cipherText	= {0, NULL};

	// Pass through if the key has not been defined.
    if (fOurDerivedKey.KeyData.Data == nil)
	{
		outBuffSize = 0;
		return;
    }

	cipherText.Data		= (uInt8 *)inData;
	cipherText.Length	= inBuffSize;
	crtn = cdsaDecrypt(
		fcspHandle,
		&fOurDerivedKey,
		&cipherText,
		&plainText);
	if (crtn)
	{
		//cssmPerror("cdsaDecrypt", crtn);
		//failed to decrypt the data
		return;
	}
	else
	{
		outData		= plainText.Data;
		outBuffSize	= plainText.Length;
	}
	return;
}

