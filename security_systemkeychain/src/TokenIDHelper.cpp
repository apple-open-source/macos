/*
 * Copyright (c) 2003-2007 Apple Computer, Inc. All Rights Reserved.
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
 *
 * TokenIDHelper.cpp
 */

#include "TokenIDHelper.h"

#include <Security/SecKeychain.h>
#include <Security/SecKeychainPriv.h>
#include <Security/SecCertificate.h>
#include <Security/SecKey.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/errors.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

static void extract_certificate_from_identity(const void *value, void *context);
static bool encryptionEnabled(SecKeyRef privateKeyRef);
static OSStatus findCertificatePublicKeyHash(SecCertificateRef certificate, CFDataRef *label);

int findFirstEncryptionPublicKeyOnToken(SecKeyRef *publicKey, SecKeychainRef *keychainRef, CFDataRef *label)
{
	if (!publicKey || !keychainRef)
		return paramErr;

	OSStatus status = noErr;
	CFArrayRef identityArray = NULL;
	SecKeyRef tmpKeyRef = NULL;
	SecCertificateRef certificate = NULL;
	SecKeychainRef tmpKeychainRef = NULL;
		
	try
	{
		status = findEncryptionIdentities((CFTypeRef *)&identityArray);
		if (status)
			MacOSError::throwMe(status);

		if (!identityArray || 
			(CFGetTypeID(identityArray)!=CFArrayGetTypeID()) ||
			(CFArrayGetCount(identityArray)==0))
			MacOSError::throwMe(paramErr);

		CFTypeRef tmpref = CFArrayGetValueAtIndex(identityArray, 0);
		if (CFGetTypeID(tmpref)!=SecIdentityGetTypeID())
			MacOSError::throwMe(paramErr);
			
		status = SecIdentityCopyCertificate(SecIdentityRef(tmpref), &certificate);
		if (status)
			MacOSError::throwMe(status);

		if (!certificate)
			MacOSError::throwMe(errKCItemNotFound);
		
		status = findCertificatePublicKeyHash(certificate, label);
		if (status)
			MacOSError::throwMe(status);

		status = SecKeychainItemCopyKeychain(SecKeychainItemRef(certificate), &tmpKeychainRef);
		if (status)
			MacOSError::throwMe(status);

		status = SecCertificateCopyPublicKey(certificate, &tmpKeyRef);
		if (status)
			MacOSError::throwMe(status);
		
		// Found an encryption key
		*publicKey = tmpKeyRef;
		*keychainRef = tmpKeychainRef;
	}
	catch (const MacOSError &err)
	{
		status = err.osStatus();
		cssmPerror("findFirstEncryptionPublicKeyOnToken", status);
	}
	catch (...)
	{
		fprintf(stderr, "findFirstEncryptionPublicKeyOnToken: unknown exception\n");
		status = errKCItemNotFound;
	}
	
	if (status)
	{
		if (identityArray)
			CFRelease(identityArray);
		if (certificate)
			CFRelease(certificate);
	}

	if (identityArray)
		CFRelease(identityArray);
	if (certificate)
		CFRelease(certificate);
	
	return status;
}

OSStatus findCertificatePublicKeyHash(SecCertificateRef certificate, CFDataRef *label)
{
	UInt32 tag[1] = { kSecPublicKeyHashItemAttr };	// kSecKeyLabel == hash public key	[kSecPublicKeyHashItemAttr ??kSecKeyLabel]
	UInt32 format[1] = { CSSM_DB_ATTRIBUTE_FORMAT_BLOB };
	SecKeychainAttributeInfo info = { 1, tag, format }; // attrs to retrieve

	SecKeychainAttributeList *attrList = NULL;
		
	OSStatus status = SecKeychainItemCopyAttributesAndData(SecKeychainItemRef(certificate), &info, NULL, &attrList, 0, NULL);
	if (status || !attrList || !attrList->count)
		return status;

	const uint32_t index = 0;
	if (attrList->attr[index].tag == kSecPublicKeyHashItemAttr)
		*label = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)attrList->attr[index].data, attrList->attr[index].length);

	SecKeychainItemFreeAttributesAndData(attrList, NULL);
	return noErr;
}

int findEncryptionIdentities(CFTypeRef *identityOrArray)
{
	/*
		Similar code is available in Leopard9A311 and later as "DIHLFVCopyEncryptionIdentities".
		See <rdar://problem/4816811> FV: Add SecTokenBasedEncryptionIdentities call
		We reproduce it here for two reasons:
		1)	The semantics of DIHLFVCopyEncryptionIdentities are different, 
			returning either a CFData or CFArray
		2)	We don' have to introduce a dependence on DiskImages.framework here


		Since CSSM searching for attributes is an AND, not an OR, we need to get all
		identities then check each one for a good key usage. If we built up a search
		using an OR predicate, we would want to specify this for key usage:
		
		uint32_t keyuse = CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP;
	*/
	OSStatus status = noErr;
	CFArrayRef searchList = NULL;
	CFMutableArrayRef idArray = NULL;			// holds all SecIdentityRefs found

	status = SecKeychainCopyDomainSearchList(kSecPreferencesDomainDynamic, &searchList);
	if (status)
		return status;
		
	CFIndex count = searchList ? CFArrayGetCount(searchList) : 0;
	if (!count)
		return errSecNoSuchKeychain;

	// Search for all identities
	uint32_t keyuse = 0;
	SecIdentitySearchRef srchRef = NULL;
	status = SecIdentitySearchCreate(searchList, keyuse, &srchRef);
	if (status)
		return status;

	while (!status)
	{
		SecIdentityRef identity = NULL;
		status = SecIdentitySearchCopyNext(srchRef, &identity);
		if (status == errSecItemNotFound)	// done
			break;
		if (status)
			return status;

		SecKeyRef privateKeyRef = NULL;
		status = SecIdentityCopyPrivateKey(identity, &privateKeyRef);
		if (status)
			continue;
		bool canEncrypt = encryptionEnabled(privateKeyRef);
		CFRelease(privateKeyRef);
		if (!canEncrypt)
			continue;
			
		// add the identity to the array
		if (!idArray)
			idArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(idArray, identity);
	}

	if ((status == noErr || status == errSecItemNotFound) && idArray && CFArrayGetCount(idArray))
	{
		if (idArray)
		{
			*identityOrArray = idArray;
			::CFRetain(*identityOrArray);
		}
		status = noErr;
	}
	else
	if (idArray)
		CFRelease(idArray);

	return status;
}

int unlockToken(const char *password)
{
	OSStatus status = noErr;
	if (!password)
		return paramErr;

	CFArrayRef searchList = NULL;

	status = SecKeychainCopyDomainSearchList(kSecPreferencesDomainDynamic, &searchList);
	if (status)
		return status;
		
	CFIndex count = searchList ? CFArrayGetCount(searchList) : 0;
	if (count)
	{
		SecKeychainRef keychainRef = (SecKeychainRef)CFArrayGetValueAtIndex(searchList, 0);	// only first dynamic keychain!
		status = SecKeychainUnlock(keychainRef, (UInt32)strlen(password), password, 1);
		if (keychainRef)
			CFRelease(keychainRef);
	}
	else
		status = errSecNoSuchKeychain;
	if (searchList)
		CFRelease(searchList);
	return status;
}

void extractCertificatesFromIdentities(CFTypeRef identityOrArray, CFArrayRef *certificateArrayOut)
{
	if (!identityOrArray || !certificateArrayOut)
		return;

	CFIndex cnt = (CFGetTypeID(identityOrArray)==CFArrayGetTypeID())?CFArrayGetCount((CFArrayRef)identityOrArray):1;
	CFMutableArrayRef certificateArray = CFArrayCreateMutable(kCFAllocatorDefault, cnt, &kCFTypeArrayCallBacks);

	if (CFGetTypeID(identityOrArray)==CFArrayGetTypeID())
		CFArrayApplyFunction((CFArrayRef)identityOrArray, CFRangeMake(0, cnt),
			extract_certificate_from_identity, 
			certificateArray);
	else
		extract_certificate_from_identity(identityOrArray, certificateArray);
	*certificateArrayOut = certificateArray;
}

void extract_certificate_from_identity(const void *value, void *context)
{
	if (!context || !value)
		return;
		
	CSSM_DATA certData = {0,};
	SecCertificateRef certificateRef;
	OSStatus status = SecIdentityCopyCertificate((SecIdentityRef)value, &certificateRef);
	if (!status)
	{
		status = SecCertificateGetData(certificateRef, &certData);
			CFRelease(certificateRef);

		if (!status)
		{
			CFDataRef cert = CFDataCreate(kCFAllocatorDefault, (UInt8 *)certData.Data, certData.Length);
			CFArrayAppendValue((CFMutableArrayRef)context, cert);
			CFRelease(cert);
			if (certData.Data)
				free(certData.Data);
		}
	}
}

bool encryptionEnabled(SecKeyRef privateKeyRef)
{
	/*
		Since CSSM searching for attributes is an AND, not an OR, we need to get all
		identities then check each one for a good key usage. Note that for the CAC
		card, the "Email Encryption Private Key" only has the unwrap bit set (0x1A).
		Return true if this identity supports appropriate encryption.
	*/

	UInt32 tag[] = { kSecKeyEncrypt, kSecKeyDecrypt, kSecKeyDerive, kSecKeyWrap, kSecKeyUnwrap };
	UInt32 format[] = { CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_ATTRIBUTE_FORMAT_UINT32, 
		CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_ATTRIBUTE_FORMAT_UINT32, CSSM_DB_ATTRIBUTE_FORMAT_UINT32};
	SecKeychainAttributeInfo info = { 5, tag, format }; // attrs to retrieve

	SecKeychainAttributeList *attrList = NULL;
	OSStatus status = SecKeychainItemCopyAttributesAndData((SecKeychainItemRef)privateKeyRef, &info, NULL, &attrList, 0, NULL);
	if (status || !attrList)
		return false;

	bool canEncrypt = false;
	for (uint32_t index = 0; index < attrList->count; ++index)
	{
		if (attrList->attr[index].length != sizeof(uint32_t) || !attrList->attr[index].data ||
			0 == *(uint32_t*)attrList->attr[index].data)
			continue;
		canEncrypt = true;
		break;
	}

	status = SecKeychainItemFreeAttributesAndData(attrList, NULL);
	return canEncrypt;
}

