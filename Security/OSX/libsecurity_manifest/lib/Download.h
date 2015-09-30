/*
 * Copyright (c) 2006,2011,2014 Apple Inc. All Rights Reserved.
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

#ifndef __DOWNLOAD_H__
#define __DOWNLOAD_H__


#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsEncoder.h>
#include <Security/SecCmsMessage.h>
#include <Security/SecCmsSignedData.h>
#include <Security/SecCmsSignerInfo.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecCertificatePriv.h>
#include <CommonCrypto/CommonDigest.h>

#include "SecureDownload.h"

typedef u_int8_t Sha256Digest[CC_SHA256_DIGEST_LENGTH];

struct OpaqueSecureDownload
{
};


class Download : public OpaqueSecureDownload
{
protected:
	CFDictionaryRef mDict;
	CFArrayRef mURLs;
	CFStringRef mName;
	CFDateRef mDate;
	CFDataRef mHashes;
	CFIndex mNumHashes, mCurrentHash;
	Sha256Digest mCurrentDigest, *mDigests; // mDigests points to the data member of mHashes
	CFIndex mBytesInCurrentDigest;
	CC_SHA256_CTX mSHA256Context;
	
	SInt64 mDownloadSize;
	ssize_t mSectorSize;

	SecCmsMessageRef GetCmsMessageFromData (CFDataRef data);
	void ParseTicket (CFDataRef ticket);
	SecPolicyRef GetPolicy ();
	void FinalizeDigestAndCompare ();
	void GoOrNoGo (SecTrustResultType result);
	
public:
	
	Download ();
	virtual ~Download ();
	void Initialize (CFDataRef data,
					 SecureDownloadTrustSetupCallback setup,
					 void* setupContext,
					 SecureDownloadTrustEvaluateCallback evaluate,
					 void* evaluateContext);
	CFArrayRef CopyURLs ();
	CFStringRef CopyName ();
	CFDateRef CopyDate ();
	SInt64 GetDownloadSize () {return mDownloadSize;}
	void UpdateWithData (CFDataRef data);
	void Finalize ();
};



#endif
