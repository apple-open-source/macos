#ifndef __APPLE_MANIFEST__
#define __APPLE_MANIFEST__



/*
 * Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
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



#include "ManifestSigner.h"
#include <Security/SecCmsBase.h>
#include <vector>


typedef std::vector<SecIdentityRef> SignerList;

class AppleManifest : public ManifestSigner
{
protected:
	void AddDataBlobToManifest (CFMutableDataRef manifest, ManifestDataBlobItem* db);
	void AddDirectoryToManifest (CFMutableDataRef manifest, ManifestDirectoryItem* directory);
	void AddFileToManifest (CFMutableDataRef manifest, ManifestFileItem* file);
	void AddSymLinkToManifest (CFMutableDataRef manifest, ManifestSymLinkItem* file);
	void AddOtherToManifest (CFMutableDataRef manifest, ManifestOtherItem* other);
	void AddManifestItemListToManifest (CFMutableDataRef manifest, ManifestItemList &itemList);
	void CreateManifest (CFMutableDataRef manifest, ManifestInternal& internalManifest);
	
	void AddSignersToCmsMessage (SecCmsMessageRef cmsMessage, SecCmsSignedDataRef signedData);
	
	void ReconstructDataBlob (uint32& finger, const uint8* data, ManifestDataBlobItem*& db);
	void ReconstructDirectory (uint32& finger, const uint8* data, ManifestDirectoryItem*& directory);
	void ReconstructFile (uint32& finger, const uint8* data, ManifestFileItem *& file);
	void ReconstructSymLink (uint32& finger, const uint8* data, ManifestSymLinkItem*& file);
	void ReconstructOther (uint32& finger, const uint8* data, ManifestOtherItem*& other);
	void ReconstructManifestItemList (uint32 &finger, const uint8* data, ManifestItemList &itemList);
	void ReconstructManifest (uint8* data, uint32 length, ManifestInternal& manifest);
	
	SignerList mSignerList;

	SecCmsMessageRef GetCmsMessageFromData (CFDataRef data);

public:
	AppleManifest ();
	virtual ~AppleManifest ();

	virtual CFDataRef Export (ManifestInternal& manifest);
	void Verify (CFDataRef data, SecManifestTrustSetupCallback setupCallback, void* setupContext,
				 SecManifestTrustEvaluateCallback evaluateCallback, void* evaluateContext,
				 SecPolicyRef policyRef, ManifestInternal *manifest);
	virtual void AddSigner (SecIdentityRef identityRef);
};



#endif
