#include "AppleManifest.h"
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsEncoder.h>
#include <Security/SecCmsMessage.h>
#include <Security/SecCmsSignedData.h>
#include <Security/SecCmsSignerInfo.h>


/*
 * Copyright (c) 2003-2004,2011-2014 Apple Inc. All Rights Reserved.
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



const int kLengthLength = 8;



static void ConvertUInt64ToBytes (UInt64 length, UInt8* bytes)
{
	int i;
	for (i = kLengthLength - 1; i >= 0; i--)
	{
		bytes[i] = length & 0xFF;
		length >>= 8;
	}
}



static void WriteLengthAndUpdate (CFMutableDataRef data, UInt64 length, CFIndex location)
{
	// back patch the length of the list
	secinfo ("manifest", "Length was %lld, patched at location %lld", length, (UInt64) location);
	
	UInt8 lengthBytes[kLengthLength];
	ConvertUInt64ToBytes (length, lengthBytes);
	
	CFRange range = {location, kLengthLength};
	CFDataReplaceBytes (data, range, lengthBytes, kLengthLength);
}



static CFIndex GetCurrentLengthAndExtend (CFMutableDataRef data)
{
	CFIndex currentIndex = CFDataGetLength (data);
	CFDataIncreaseLength (data, kLengthLength);
	return currentIndex;
}



static void AppendUInt16 (CFMutableDataRef data, UInt16 num)
{
	UInt8 n[2];
	n[0] = num >> 8;
	n[1] = num & 0xFF;
	CFDataAppendBytes (data, n, sizeof (n));
}



static void AppendUInt32 (CFMutableDataRef data, UInt32 num)
{
	UInt8 n[4];
	n[0] = (num >> 24) & 0xFF;
	n[1] = (num >> 16) & 0xFF;
	n[2] = (num >> 8) & 0xFF;
	n[3] = num & 0xFF;
	CFDataAppendBytes (data, n, sizeof (n));
}



static void AppendUInt64 (CFMutableDataRef data, UInt64 num)
{
	UInt8 n[8];
	n[0] = (num >> 56) & 0xFF;
	n[1] = (num >> 48) & 0xFF;
	n[2] = (num >> 40) & 0xFF;
	n[3] = (num >> 32) & 0xFF;
	n[4] = (num >> 24) & 0xFF;
	n[5] = (num >> 16) & 0xFF;
	n[6] = (num >> 8) & 0xFF;
	n[7] = num & 0xFF;
	
	CFDataAppendBytes (data, n, sizeof (n));
}



static void WriteFileSystemItemHeader (CFMutableDataRef data, const FileSystemEntryItem *fsi)
{
	// write the name
	const char* name = fsi->GetName ();
	secinfo ("manifest", "\tAdding header for %s", name);
	uint16_t len = (uint16_t)strlen (name);
	AppendUInt16 (data, len);
	CFDataAppendBytes (data, (UInt8*) name, len);
	AppendUInt32 (data, fsi->GetUID ());
	AppendUInt32 (data, fsi->GetGID ());
	AppendUInt32 (data, fsi->GetMode ());
}



AppleManifest::AppleManifest ()
{
}



AppleManifest::~AppleManifest ()
{
	// release our interest in the signers
	int signerCount = (int)mSignerList.size ();

	int i;
	for (i = 0; i < signerCount; ++i)
	{
		CFRelease (mSignerList[i]);
	}
}



void AppleManifest::AddDirectoryToManifest (CFMutableDataRef manifest, ManifestDirectoryItem* directory)
{
	secinfo ("manifest", "Adding directory %s to manifest", directory->GetName ());
	
	CFIndex currentIndex = GetCurrentLengthAndExtend (manifest);
	AppendUInt16 (manifest, (UInt16) kManifestDirectoryItemType);
	
	WriteFileSystemItemHeader (manifest, directory);
	
	AddManifestItemListToManifest (manifest, directory->GetItemList ());
	
	WriteLengthAndUpdate (manifest, CFDataGetLength (manifest) - currentIndex, currentIndex);
}



void AppleManifest::AddFileToManifest (CFMutableDataRef manifest, ManifestFileItem* file)
{
	CFIndex currentIndex = GetCurrentLengthAndExtend (manifest);
	AppendUInt16 (manifest, (UInt16) kManifestFileItemType);
	
	WriteFileSystemItemHeader (manifest, file);
	
	int numForks = file->GetNumberOfForks ();
	AppendUInt16 (manifest, (UInt16) numForks);
	
	int i;
	// write the file lengths
	for (i = 0; i < numForks; ++i)
	{
		size_t length;
		length = file->GetForkLength (i);
		AppendUInt64 (manifest, length);
	}

	// write the digests
	for (i = 0; i < numForks; ++i)
	{
		void* sha1Digest;
		size_t size;
		file->GetItemRepresentation (i, sha1Digest, size);
		CFDataAppendBytes (manifest, (UInt8*) sha1Digest, size);
	}
	
	WriteLengthAndUpdate (manifest, CFDataGetLength (manifest) - currentIndex, currentIndex);
}



void AppleManifest::AddSymLinkToManifest (CFMutableDataRef manifest, ManifestSymLinkItem* file)
{
	CFIndex currentIndex = GetCurrentLengthAndExtend (manifest);
	AppendUInt16 (manifest, (UInt16) kManifestSymLinkItemType);
	
	WriteFileSystemItemHeader (manifest, file);
	
	const SHA1Digest* digest = file->GetDigest ();
	CFDataAppendBytes (manifest, (const UInt8*) digest, kSHA1DigestSize);
	
	WriteLengthAndUpdate (manifest, CFDataGetLength (manifest) - currentIndex, currentIndex);
}



void AppleManifest::AddOtherToManifest (CFMutableDataRef manifest, ManifestOtherItem* other)
{
	CFIndex currentIndex = GetCurrentLengthAndExtend (manifest);
	AppendUInt16 (manifest, (UInt16) kManifestSymLinkItemType);
	
	WriteFileSystemItemHeader (manifest, other);
	
	WriteLengthAndUpdate (manifest, CFDataGetLength (manifest) - currentIndex, currentIndex);
}



void AppleManifest::AddDataBlobToManifest (CFMutableDataRef manifest, ManifestDataBlobItem* item)
{
	CFIndex currentIndex = GetCurrentLengthAndExtend (manifest);
	AppendUInt16 (manifest, (UInt16) kManifestDataBlobItemType);
	
	AppendUInt64 (manifest, (UInt64) item->GetLength ());
	const SHA1Digest* sha1Digest = item->GetDigest ();
	CFDataAppendBytes (manifest, (UInt8*) sha1Digest, sizeof (SHA1Digest));
	
	WriteLengthAndUpdate (manifest, CFDataGetLength (manifest) - currentIndex, currentIndex);
}



void AppleManifest::AddManifestItemListToManifest (CFMutableDataRef data, ManifestItemList &itemList)
{
	// save the current position
	CFIndex currentIndex = GetCurrentLengthAndExtend (data);
	
	unsigned i;
	for (i = 0; i < itemList.size (); ++i)
	{
		ManifestItem* item = itemList[i];
		
		switch (item->GetItemType ())
		{
			case kManifestDataBlobItemType:
			{
				AddDataBlobToManifest (data, static_cast<ManifestDataBlobItem*>(item));
				break;
			}
			
			case kManifestFileItemType:
			{
				AddFileToManifest (data, static_cast<ManifestFileItem*>(item));
				break;
			}
			
			case kManifestDirectoryItemType:
			{
				AddDirectoryToManifest (data, static_cast<ManifestDirectoryItem*>(item));
				break;
			}
			
			case kManifestSymLinkItemType:
			{
				AddSymLinkToManifest (data, static_cast<ManifestSymLinkItem*>(item));
				break;
			}
			
			case kManifestOtherType:
			{
				AddOtherToManifest (data, static_cast<ManifestOtherItem*>(item));
				break;
			}
		}
	}
	
	WriteLengthAndUpdate (data, CFDataGetLength (data) - currentIndex, currentIndex);
}



static const unsigned char gManifestHeader[] = {0x2F, 0xAA, 0x05, 0xB3, 0x64, 0x0E, 0x9D, 0x27}; // why these numbers?  These were picked at random
static const unsigned char gManifestVersion[] = {0x01, 0x00, 0x00, 0x00};



void AppleManifest::CreateManifest (CFMutableDataRef manifest, ManifestInternal& internalManifest)
{
	// create the manifest header
	CFDataAppendBytes (manifest, (UInt8*) gManifestHeader, sizeof (gManifestHeader));
	CFDataAppendBytes (manifest, (UInt8*) gManifestVersion, sizeof (gManifestVersion));
	AddManifestItemListToManifest (manifest, internalManifest.GetItemList ());
}



void AppleManifest::AddSignersToCmsMessage (SecCmsMessageRef cmsMessage, SecCmsSignedDataRef signedData)
{
	// add signers for each of our signers
	int numSigners = (int)mSignerList.size ();
	
	int i;
	for (i = 0; i < numSigners; ++i)
	{
		SecIdentityRef id = mSignerList[i];
		SecCmsSignerInfoRef signerInfo = SecCmsSignerInfoCreate (cmsMessage, id, SEC_OID_SHA1);
		if (signerInfo == NULL)
		{
			SecCmsMessageDestroy (cmsMessage);
			MacOSError::throwMe (errSecManifestCMSFailure);
		}
		
		int result = SecCmsSignerInfoIncludeCerts (signerInfo, SecCmsCMCertChain, certUsageObjectSigner);
		if (result != 0)
		{
			SecCmsMessageDestroy (cmsMessage);
			MacOSError::throwMe (errSecManifestCMSFailure);
		}
		
		SecCmsSignedDataAddSignerInfo (signedData, signerInfo);
	}
}



CFDataRef AppleManifest::Export (ManifestInternal& manifest)
{
	// there had better be at least one signer
	if (mSignerList.size () == 0)
	{
		secinfo ("manifest", "No signers found");
		MacOSError::throwMe (errSecManifestNoSigners);
	}
	
	// create a CFMutableDataRef to hold the manifest object
	CFMutableDataRef data = CFDataCreateMutable (kCFAllocatorDefault, 0);

	// make the manifest
	CreateManifest (data, manifest);

	// make the PKCS #7 wrapper
	SecCmsMessageRef cmsMessage;
	cmsMessage = SecCmsMessageCreate (NULL);
	if (cmsMessage == NULL) // did something go wrong?
	{
		MacOSError::throwMe (errSecManifestCMSFailure);
	}
	
	// create a signed data holder
	SecCmsSignedDataRef signedData;
	signedData = SecCmsSignedDataCreate (cmsMessage);
	if (signedData == NULL)
	{
		SecCmsMessageDestroy (cmsMessage);
		MacOSError::throwMe (errSecManifestCMSFailure);
	}
	
	// link the signed data and the CMS message
	SecCmsContentInfoRef contentInfo = SecCmsMessageGetContentInfo (cmsMessage);
	
	int result = SecCmsContentInfoSetContentSignedData (cmsMessage, contentInfo, signedData);
	if (result != 0)
	{
		SecCmsMessageDestroy (cmsMessage);
		MacOSError::throwMe (errSecManifestCMSFailure);
	}
	
	// attach the content information from the signature to the data
	contentInfo = SecCmsSignedDataGetContentInfo (signedData);
	result = SecCmsContentInfoSetContentData (cmsMessage, contentInfo, NULL, false);
	if (result != 0)
	{
		SecCmsMessageDestroy (cmsMessage);
		MacOSError::throwMe (errSecManifestCMSFailure);
	}
		
	AddSignersToCmsMessage (cmsMessage, signedData);
	
	// make an encoder context
	SecArenaPoolRef arena;
    result = SecArenaPoolCreate(1024, &arena);
    if (result)
	{
		MacOSError::throwMe (errSecManifestCMSFailure);
	}
	
	CSSM_DATA finalMessage = {0, NULL};
    SecCmsEncoderRef encoderContext;
	result = SecCmsEncoderCreate (cmsMessage, NULL, NULL, &finalMessage, arena, NULL, NULL, NULL, NULL, NULL, NULL, &encoderContext);
	if (result)
	{
		MacOSError::throwMe (errSecManifestCMSFailure);
	}

	result = SecCmsEncoderUpdate (encoderContext, CFDataGetBytePtr (data), CFDataGetLength (data));
	if (result != 0)
	{
		SecCmsMessageDestroy (cmsMessage);
		MacOSError::throwMe (errSecManifestCMSFailure);
	}

	result = SecCmsEncoderFinish (encoderContext);
	if (result != 0)
	{
		MacOSError::throwMe (errSecManifestCMSFailure);
	}
	
	// create a CFData from the results
	CFDataRef retData = CFDataCreate (kCFAllocatorDefault, (UInt8*) finalMessage.Data, finalMessage.Length);
	
	SecArenaPoolFree(arena, false);
	SecCmsMessageDestroy (cmsMessage);

	CFRelease (data);

	return retData;
}



static u_int64_t ReconstructUInt64 (uint32& finger, const uint8* data)
{
	unsigned i;
	u_int64_t r = 0;
	
	for (i = 0; i < sizeof (u_int64_t); ++i)
	{
		r = (r << 8) | data[finger++];
	}
	
	return r;
}



static u_int32_t ReconstructUInt32 (uint32& finger, const uint8* data)
{
	unsigned i;
	u_int32_t r = 0;
	
	for (i = 0; i < sizeof (u_int32_t); ++i)
	{
		r = (r << 8) | data[finger++];
	}
	
	return r;
}



static u_int16_t ReconstructUInt16 (uint32& finger, const uint8* data)
{
	unsigned i;
	u_int16_t r = 0;

	for (i = 0; i < sizeof (u_int16_t); ++i)
	{
		r = (r << 8) | data[finger++];
	}
	
	return r;
}



static void ReconstructFileSystemHeader (uint32& finger, const uint8* data, FileSystemEntryItem* item)
{
	// get the number of bytes for the name
	u_int16_t length = ReconstructUInt16 (finger, data);
	char name[length + 1];
	
	// make a c-string for the name
	memcpy (name, data + finger, length);
	name[length] = 0;
	item->SetName (name);
	
	secinfo ("manifest", "    File item name is %s", name);

	finger += length;
	
	uid_t uid = (uid_t) ReconstructUInt32 (finger, data);
	gid_t gid = (gid_t) ReconstructUInt32 (finger, data);
	mode_t mode = (mode_t) ReconstructUInt32 (finger, data);
	
	secinfo ("manifest", "    File item uid is %d", uid);
	secinfo ("manifest", "    File item gid is %d", gid);
	secinfo ("manifest", "    File item mode is %d", mode);

	item->SetUID (uid);
	item->SetGID (gid);
	item->SetMode (mode);
}



static void ParseItemHeader (uint32 &finger, const uint8* data, ManifestItemType &itemType, u_int64_t &end)
{
	u_int64_t start = finger;
	u_int64_t length = ReconstructUInt64 (finger, data);
	itemType = (ManifestItemType) ReconstructUInt16 (finger, data);
	end = start + length;
}



void AppleManifest::ReconstructDataBlob (uint32 &finger, const uint8* data, ManifestDataBlobItem*& item)
{
	secinfo ("manifest", "Reconstructing data blob.");
	item = new ManifestDataBlobItem ();
	u_int64_t length = ReconstructUInt64 (finger, data);
	item->SetLength ((size_t)length);
	item->SetDigest ((SHA1Digest*) (data + finger));
	finger += kSHA1DigestSize;
}



void AppleManifest::ReconstructDirectory (uint32 &finger, const uint8* data, ManifestDirectoryItem*& directory)
{
	// make the directory
	secinfo ("manifest", "Reconstructing directory.");
	directory = new ManifestDirectoryItem ();
	ReconstructFileSystemHeader (finger, data, directory);
	
	ReconstructManifestItemList (finger, data, directory->GetItemList ());
}



void AppleManifest::ReconstructFile (uint32& finger, const uint8* data, ManifestFileItem *& file)
{
	secinfo ("manifest", "Reconstructing file.");
	// make the file
	file = new ManifestFileItem ();
	ReconstructFileSystemHeader (finger, data, file);
	
	u_int16_t numForks = ReconstructUInt16 (finger, data);
	file->SetNumberOfForks (numForks);

	// reconstruct the lengths
	u_int16_t n;
	for (n = 0; n < numForks; ++n)
	{
		u_int64_t length = ReconstructUInt64 (finger, data);
		file->SetForkLength (n, (size_t) length);
	}

	// reconstruct the digests
	for (n = 0; n < numForks; ++n)
	{
		file->SetItemRepresentation (n, data + finger, kSHA1DigestSize);
		finger += kSHA1DigestSize;
	}
}



void AppleManifest::ReconstructSymLink (uint32& finger, const uint8* data, ManifestSymLinkItem*& file)
{
	secinfo ("manifest", "Reconstructing symlink.");
	file = new ManifestSymLinkItem ();
	ReconstructFileSystemHeader (finger, data, file);

	file->SetDigest ((const SHA1Digest*) (data + finger));
	finger += kSHA1DigestSize;
}



void AppleManifest::ReconstructOther (uint32& finger, const uint8* data, ManifestOtherItem*& other)
{
	secinfo ("manifest", "Reconstructing other.");
	other = new ManifestOtherItem ();
	ReconstructFileSystemHeader (finger, data, other);
}



void AppleManifest::ReconstructManifestItemList (uint32 &finger, const uint8* data, ManifestItemList &itemList)
{
	uint32 start = finger;
	uint64_t length = ReconstructUInt64 (finger, data);
    uint32 end = (uint32)(start + length);

    if (length > UINT32_MAX || (length + (uint64_t)start) > (uint64_t)UINT32_MAX)
        MacOSError::throwMe (errSecManifestDamaged);

	while (finger < end)
	{
		u_int64_t itemEnd;
		ManifestItemType itemType;
		ParseItemHeader (finger, data, itemType, itemEnd);
		
		switch (itemType)
		{
			case kManifestFileItemType:
			{
				ManifestFileItem* file;
				ReconstructFile (finger, data, file);
				itemList.push_back (file);
			}
			break;
			
			case kManifestDirectoryItemType:
			{
				ManifestDirectoryItem* directory;
				ReconstructDirectory (finger, data, directory);
				itemList.push_back (directory);
			}
			break;
			
			case kManifestSymLinkItemType:
			{
				ManifestSymLinkItem* symLink;
				ReconstructSymLink (finger, data, symLink);
				itemList.push_back (symLink);
			}
			break;
			
			case kManifestOtherType:
			{
				ManifestOtherItem* other;
				ReconstructOther (finger, data, other);
				itemList.push_back (other);
			}
			break;
			
			case kManifestDataBlobItemType:
			{
				ManifestDataBlobItem* item;
				ReconstructDataBlob (finger, data, item);
				itemList.push_back (item);
			}
			break;
		}
		
		if (finger != itemEnd)
		{
			MacOSError::throwMe (errSecManifestDamaged);
		}
	}
}



void AppleManifest::ReconstructManifest (uint8* data, uint32 length, ManifestInternal& manifest)
{
	uint32 finger = 0;

	// make sure the passed-in header starts with our magic number
	if (memcmp (data, gManifestHeader, sizeof (gManifestHeader)) != 0)
	{
		MacOSError::throwMe (errSecManifestDamaged);
	}
	
	finger += sizeof (gManifestHeader);

	// for now, the version had better be 0x01000000
	if (memcmp (data + finger, gManifestVersion, sizeof (gManifestVersion)) != 0)
	{
		MacOSError::throwMe (errSecManifestDamaged);
	}
	
	finger += sizeof (gManifestVersion);
	
	ReconstructManifestItemList (finger, data, manifest.GetItemList ());
}



SecCmsMessageRef AppleManifest::GetCmsMessageFromData (CFDataRef data)
{
	// setup decoding
	SecCmsDecoderRef decoderContext;
	int result = SecCmsDecoderCreate (NULL, NULL, NULL, NULL, NULL, NULL, NULL, &decoderContext);
    if (result)
    {
		MacOSError::throwMe (errSecManifestCMSFailure);
    }

	result = SecCmsDecoderUpdate (decoderContext, CFDataGetBytePtr (data), CFDataGetLength (data));
	if (result)
	{
        SecCmsDecoderDestroy(decoderContext);
		MacOSError::throwMe (errSecManifestCMSFailure);
	}

    SecCmsMessageRef message;
	result = SecCmsDecoderFinish (decoderContext, &message);
    if (result)
    {
		MacOSError::throwMe (errSecManifestCMSFailure);
    }

    return message;
}



void AppleManifest::Verify (CFDataRef data, SecManifestTrustSetupCallback setupCallback, void* setupContext,
						    SecManifestTrustEvaluateCallback evaluateCallback, void* evaluateContext,
						    SecPolicyRef policy, ManifestInternal *manifest)
{
	SecCmsMessageRef cmsMessage = NULL;

	try
	{
		cmsMessage = GetCmsMessageFromData (data);

		SecPolicySearchRef search;
		OSStatus result;
		
		SecPolicyRef originalPolicy = policy;
		
		if (policy == NULL)
		{
			// get a basic SecPolicy
			result = SecPolicySearchCreate (CSSM_CERT_X_509v3, &CSSMOID_APPLE_X509_BASIC, NULL, &search);
			MacOSError::check (result);
			
			result = SecPolicySearchCopyNext (search, &policy);
			if (result != errSecSuccess)
			{
				MacOSError::throwMe (errSecManifestNoPolicy);
			}
			
			CFRelease (search);
		}
		
		// process the results
		int contentLevelCount = SecCmsMessageContentLevelCount (cmsMessage);
		SecCmsSignedDataRef signedData;

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
				MacOSError::throwMe (errSecManifestDidNotVerify);
			}
			
			// import the certificates found in the cms message
			result = SecCmsSignedDataImportCerts (signedData, NULL, certUsageObjectSigner, true);
			if (result != 0)
			{
				MacOSError::throwMe (result);
			}
			
			int numberOfSigners = SecCmsSignedDataSignerInfoCount (signedData);
			int j;
			
			if (numberOfSigners == 0) // no signers?  This is a possible attack
			{
				MacOSError::throwMe (errSecManifestNoSignersFound);
			}
			
			for (j = 0; j < numberOfSigners; ++j)
			{
				SecTrustResultType resultType;
				SecTrustRef trustRef = NULL;
				
				try
				{
					result = SecCmsSignedDataVerifySignerInfo (signedData, j, NULL, policy, &trustRef);
					
					if (result != 0)
					{
						MacOSError::throwMe (result);
					}
					
					SecManifestTrustCallbackResult tcResult = setupCallback (trustRef, setupContext);
					switch (tcResult)
					{
						case kSecManifestDoNotVerify:
							continue;
						
						case kSecManifestSignerVerified:
							continue;
						
						case kSecManifestFailed:
							MacOSError::throwMe (errSecManifestDidNotVerify);
						
						case kSecManifestContinue:
						break;
					}
					
					result = SecTrustEvaluate (trustRef, &resultType);
					if (result != errSecSuccess)
					{
						MacOSError::throwMe (result);
					}
					
					if (resultType != kSecTrustResultProceed)
					{
						if (evaluateCallback (trustRef, resultType, evaluateContext) != kSecManifestSignerVerified)
						{
							MacOSError::throwMe (errSecManifestDidNotVerify);
						}
					}
					
					CFRelease (trustRef);
				}
				catch (...)
				{
					if (trustRef != NULL)
					{
						CFRelease (trustRef);
					}
					
					throw;
				}
			}
		}
		
		if (manifest != NULL)
		{
			CSSM_DATA_PTR message = SecCmsMessageGetContent (cmsMessage);
			ReconstructManifest (message->Data, (uint32)message->Length, *manifest);
		}
		
		SecCmsMessageDestroy (cmsMessage);
		if (originalPolicy == NULL)
		{
			CFRelease (policy);
		}
	}
	catch (...)
	{
		if (cmsMessage != NULL)
		{
			SecCmsMessageDestroy (cmsMessage);
		}
		
		throw;
	}
}



void AppleManifest::AddSigner (SecIdentityRef identityRef)
{
	CFRetain (identityRef);
	mSignerList.push_back (identityRef);
}

