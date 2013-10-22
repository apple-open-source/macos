/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
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

#include <CoreFoundation/CoreFoundation.h>
#include <CommonCrypto/CommonDigest.h>

#include <Security/Security.h>
#include <security_utilities/security_utilities.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <Security/cssmapplePriv.h>

#include "SecureDownload.h"
#include "SecureDownloadInternal.h"
#include "Download.h"



static void CheckCFThingForNULL (CFTypeRef theType)
{
	if (theType == NULL)
	{
		CFError::throwMe ();
	}
}



Download::Download () : mDict (NULL), mURLs (NULL), mName (NULL), mDate (NULL), mHashes (NULL), mNumHashes (0), mCurrentHash (0), mBytesInCurrentDigest (0)
{
}



static void ReleaseIfNotNull (CFTypeRef theThing)
{
	if (theThing != NULL)
	{
		CFRelease (theThing);
	}
}



Download::~Download ()
{
	ReleaseIfNotNull (mDict);
}



CFArrayRef Download::CopyURLs ()
{
	CFRetain (mURLs);
	return mURLs;
}



CFStringRef Download::CopyName ()
{
	CFRetain (mName);
	return mName;
}



CFDateRef Download::CopyDate ()
{
	CFRetain (mDate);
	return mDate;
}



void Download::GoOrNoGo (SecTrustResultType result)
{
	switch (result)
	{
		case kSecTrustResultInvalid:
		case kSecTrustResultDeny:
		case kSecTrustResultFatalTrustFailure:
		case kSecTrustResultOtherError:
			MacOSError::throwMe (errSecureDownloadInvalidTicket);
			
		case kSecTrustResultProceed:
			return;
		
		// we would normally ask for the user's permission in these cases.
		// we don't in this case, as the Apple signing root had better be
		// in X509 anchors.  I'm leaving this broken out for ease of use
		// in case we change our minds...
		case kSecTrustResultConfirm:
		case kSecTrustResultRecoverableTrustFailure:
		case kSecTrustResultUnspecified:
		{
			MacOSError::throwMe (errSecureDownloadInvalidTicket);
		}
		
		default:
			break;
	}
}

	

SecPolicyRef Download::GetPolicy ()
{
	SecPolicySearchRef search;
	SecPolicyRef policy;
	OSStatus result;

	// get the policy for resource signing
	result = SecPolicySearchCreate (CSSM_CERT_X_509v3, &CSSMOID_APPLE_TP_RESOURCE_SIGN, NULL, &search);
	if (result != errSecSuccess)
	{
		MacOSError::throwMe (result);
	}
	
	result = SecPolicySearchCopyNext (search, &policy);
	if (result != errSecSuccess)
	{
		MacOSError::throwMe (result);
	}

	CFRelease (search);
	
	return policy;
}



#define SHA256_NAME CFSTR("SHA-256")

void Download::ParseTicket (CFDataRef ticket)
{
	// make a propertylist from the ticket
	CFDictionaryRef mDict = (CFDictionaryRef) _SecureDownloadParseTicketXML (ticket);
	CheckCFThingForNULL (mDict);
	CFRetain (mDict);
	
	mURLs = (CFArrayRef) CFDictionaryGetValue (mDict, SD_XML_URL);
	CheckCFThingForNULL (mURLs);
	
	// get the download name
	mName = (CFStringRef) CFDictionaryGetValue (mDict, SD_XML_NAME);
	CheckCFThingForNULL (mName);

	// get the download date
	mDate = (CFDateRef) CFDictionaryGetValue (mDict, SD_XML_CREATED);
	CheckCFThingForNULL (mDate);
	
	// get the download size
	CFNumberRef number = (CFNumberRef) CFDictionaryGetValue (mDict, SD_XML_SIZE);
	CFNumberGetValue (number, kCFNumberSInt64Type, &mDownloadSize);

	// get the verifications dictionary
	CFDictionaryRef verifications = (CFDictionaryRef) CFDictionaryGetValue (mDict, SD_XML_VERIFICATIONS);
	
	// from the verifications dictionary, get the hashing dictionary that we support
	CFDictionaryRef hashInfo = (CFDictionaryRef) CFDictionaryGetValue (verifications, SHA256_NAME);
	
	// from the hashing dictionary, get the sector size
	number = (CFNumberRef) CFDictionaryGetValue (hashInfo, SD_XML_SECTOR_SIZE);
	CFNumberGetValue (number, kCFNumberSInt32Type, &mSectorSize);
	
	// get the hashes
	mHashes = (CFDataRef) CFDictionaryGetValue (hashInfo, SD_XML_DIGESTS);
	CFIndex hashSize = CFDataGetLength (mHashes);
	mNumHashes = hashSize / CC_SHA256_DIGEST_LENGTH;
	mDigests = (Sha256Digest*) CFDataGetBytePtr (mHashes);
	mCurrentHash = 0;
	mBytesInCurrentDigest = 0;
}



void Download::Initialize (CFDataRef ticket,
						   SecureDownloadTrustSetupCallback setup,
						   void* setupContext,
						   SecureDownloadTrustEvaluateCallback evaluate,
						   void* evaluateContext)
{
	// decode the ticket
	SecCmsMessageRef cmsMessage = GetCmsMessageFromData (ticket);
	
	// get a policy
	SecPolicyRef policy = GetPolicy ();

	// parse the CMS message
	int contentLevelCount = SecCmsMessageContentLevelCount (cmsMessage);
	SecCmsSignedDataRef signedData;

	OSStatus result;
	
	int i = 0;
	while (i < contentLevelCount)
	{
		SecCmsContentInfoRef contentInfo = SecCmsMessageContentLevel (cmsMessage, i++);
		SECOidTag contentTypeTag = SecCmsContentInfoGetContentTypeTag (contentInfo);
		
		if (contentTypeTag != SEC_OID_PKCS7_SIGNED_DATA)
		{
			continue;
		}

		signedData = (SecCmsSignedDataRef) SecCmsContentInfoGetContent (contentInfo);
		if (signedData == NULL)
		{
			MacOSError::throwMe (errSecureDownloadInvalidTicket);
		}
		
		// import the certificates found in the cms message
		result = SecCmsSignedDataImportCerts (signedData, NULL, certUsageObjectSigner, true);
		if (result != 0)
		{
			MacOSError::throwMe (errSecureDownloadInvalidTicket);
		}
		
		int numberOfSigners = SecCmsSignedDataSignerInfoCount (signedData);
		int j;
		
		if (numberOfSigners == 0) // no signers?  This is a possible attack
		{
			MacOSError::throwMe (errSecureDownloadInvalidTicket);
		}
		
		for (j = 0; j < numberOfSigners; ++j)
		{
			SecTrustResultType resultType;
			
			// do basic verification of the message
			SecTrustRef trustRef;
			result = SecCmsSignedDataVerifySignerInfo (signedData, j, NULL, policy, &trustRef);
			
			// notify the user of the new trust ref
			if (setup != NULL)
			{
				SecureDownloadTrustCallbackResult tcResult = setup (trustRef, setupContext);
				switch (tcResult)
				{
					case kSecureDownloadDoNotEvaluateSigner:
						continue;
					
					case kSecureDownloadFailEvaluation:
						MacOSError::throwMe (errSecureDownloadInvalidTicket);
					
					case kSecureDownloadEvaluateSigner:
					break;
				}
			}
			
			if (result != 0)
			{
				MacOSError::throwMe (errSecureDownloadInvalidTicket);
			}
			
			result = SecTrustEvaluate (trustRef, &resultType);
			if (result != errSecSuccess)
			{
				MacOSError::throwMe (errSecureDownloadInvalidTicket);
			}
			
			if (evaluate != NULL)
			{
				resultType = evaluate (trustRef, resultType, evaluateContext);
			}
			
			GoOrNoGo (resultType);
		}
	}
	
	// extract the message 
	CSSM_DATA_PTR message = SecCmsMessageGetContent (cmsMessage);
	CFDataRef ticketData = CFDataCreateWithBytesNoCopy (NULL, message->Data, message->Length, kCFAllocatorNull);
	CheckCFThingForNULL (ticketData);
	
	ParseTicket (ticketData);

	// setup for hashing
	CC_SHA256_Init (&mSHA256Context);
	
	// clean up
	CFRelease (ticketData);
	SecCmsMessageDestroy (cmsMessage);
}



SecCmsMessageRef Download::GetCmsMessageFromData (CFDataRef data)
{
	// setup decoding
	SecCmsDecoderRef decoderContext;
	int result = SecCmsDecoderCreate (NULL, NULL, NULL, NULL, NULL, NULL, NULL, &decoderContext);
    if (result)
    {
		MacOSError::throwMe (errSecureDownloadInvalidTicket);
    }

	result = SecCmsDecoderUpdate (decoderContext, CFDataGetBytePtr (data), CFDataGetLength (data));
	if (result)
	{
        SecCmsDecoderDestroy(decoderContext);
		MacOSError::throwMe (errSecureDownloadInvalidTicket);
	}

    SecCmsMessageRef message;
	result = SecCmsDecoderFinish (decoderContext, &message);
    if (result)
    {
		MacOSError::throwMe (errSecureDownloadInvalidTicket);
    }

    return message;
}


static 
size_t MinSizeT (size_t a, size_t b)
{
	// return the smaller of a and b
	return a < b ? a : b;
}



void Download::FinalizeDigestAndCompare ()
{
	Sha256Digest digest;
	CC_SHA256_Final (digest, &mSHA256Context);
	
	// make sure we don't overflow the digest buffer
	if (mCurrentHash >= mNumHashes || memcmp (digest, mDigests[mCurrentHash++], CC_SHA256_DIGEST_LENGTH) != 0)
	{
		// Something's really wrong!
		MacOSError::throwMe (errSecureDownloadInvalidDownload);
	}
	
	// setup for the next receipt of data
	mBytesInCurrentDigest = 0;
	CC_SHA256_Init (&mSHA256Context);
}



void Download::UpdateWithData (CFDataRef data)
{
	// figure out how much data to hash
	CFIndex dataLength = CFDataGetLength (data);
	const UInt8* finger = CFDataGetBytePtr (data);
	
	while (dataLength > 0)
	{
		// figure out how many bytes are left to hash
		size_t bytesLeftToHash = mSectorSize - mBytesInCurrentDigest;
		size_t bytesToHash = MinSizeT (bytesLeftToHash, dataLength);
		
		// hash the data
		CC_SHA256_Update (&mSHA256Context, finger, (CC_LONG)bytesToHash);
		
		// update the pointers
		mBytesInCurrentDigest += bytesToHash;
		bytesLeftToHash -= bytesToHash;
		finger += bytesToHash;
		dataLength -= bytesToHash;
		
		if (bytesLeftToHash == 0) // is our digest "full"?
		{
			FinalizeDigestAndCompare ();
		}
	}
}



void Download::Finalize ()
{
	// are there any bytes left over in the digest?
	if (mBytesInCurrentDigest != 0)
	{
		FinalizeDigestAndCompare ();
	}
	
	if (mCurrentHash != mNumHashes) // check for underflow
	{
		MacOSError::throwMe (errSecureDownloadInvalidDownload);
	}
}

