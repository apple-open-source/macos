/*
 * Copyright (c) 2002-2010 Apple Inc. All Rights Reserved.
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

#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecKeychainItemPriv.h>
#include <Security/SecItem.h>
#include <Security/SecIdentityPriv.h>

#include "SecBridge.h"
#include <security_keychain/Certificate.h>
#include <security_keychain/Identity.h>
#include <security_keychain/KeyItem.h>
#include <security_keychain/KCCursor.h>
#include <security_cdsa_utilities/Schema.h>
#include <security_utilities/simpleprefs.h>
#include <sys/param.h>
#include <syslog.h>


/* private function declarations */
OSStatus
SecIdentityFindPreferenceItemWithNameAndKeyUsage(
	CFTypeRef keychainOrArray,
	CFStringRef name,
	int32_t keyUsage,
	SecKeychainItemRef *itemRef);

OSStatus SecIdentityDeletePreferenceItemWithNameAndKeyUsage(
	CFTypeRef keychainOrArray,
	CFStringRef name,
	int32_t keyUsage);


CSSM_KEYUSE ConvertArrayToKeyUsage(CFArrayRef usage)
{		
	CFIndex count = 0;
	CSSM_KEYUSE result = (CSSM_KEYUSE) 0;
		
	if ((NULL == usage) || (0 == (count = CFArrayGetCount(usage))))
	{
		return result;
	}
	
	for (CFIndex iCnt = 0; iCnt < count; iCnt++)
	{
		CFStringRef keyUsageStr = NULL;
		keyUsageStr = (CFStringRef)CFArrayGetValueAtIndex(usage,iCnt);
		if (NULL != keyUsageStr)
		{
			if (kCFCompareEqualTo == CFStringCompare((CFStringRef)kSecAttrCanEncrypt, keyUsageStr, 0))
			{
				result |= CSSM_KEYUSE_ENCRYPT;
			}
			else if (kCFCompareEqualTo == CFStringCompare((CFStringRef)kSecAttrCanDecrypt, keyUsageStr, 0))
			{
				result |= CSSM_KEYUSE_DECRYPT;
			}
			else if (kCFCompareEqualTo == CFStringCompare((CFStringRef)kSecAttrCanDerive, keyUsageStr, 0))
			{
				result |= CSSM_KEYUSE_DERIVE;
			}
			else if (kCFCompareEqualTo == CFStringCompare((CFStringRef)kSecAttrCanSign, keyUsageStr, 0))
			{
				result |= CSSM_KEYUSE_SIGN;
			}
			else if (kCFCompareEqualTo == CFStringCompare((CFStringRef)kSecAttrCanVerify, keyUsageStr, 0))
			{
				result |= CSSM_KEYUSE_VERIFY;
			}
			else if (kCFCompareEqualTo == CFStringCompare((CFStringRef)kSecAttrCanWrap, keyUsageStr, 0))
			{
				result |= CSSM_KEYUSE_WRAP;
			}
			else if (kCFCompareEqualTo == CFStringCompare((CFStringRef)kSecAttrCanUnwrap, keyUsageStr, 0))
			{
				result |= CSSM_KEYUSE_UNWRAP;
			}
		}
	}

	return result;
}


CFTypeID
SecIdentityGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().Identity.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus
SecIdentityCopyCertificate(
            SecIdentityRef identityRef, 
            SecCertificateRef *certificateRef)
{
    BEGIN_SECAPI

	SecPointer<Certificate> certificatePtr(Identity::required(identityRef)->certificate());
	Required(certificateRef) = certificatePtr->handle();

    END_SECAPI
}


OSStatus
SecIdentityCopyPrivateKey(
            SecIdentityRef identityRef, 
            SecKeyRef *privateKeyRef)
{
    BEGIN_SECAPI

	SecPointer<KeyItem> keyItemPtr(Identity::required(identityRef)->privateKey());
	Required(privateKeyRef) = keyItemPtr->handle();

    END_SECAPI
}

OSStatus
SecIdentityCreateWithCertificate(
	CFTypeRef keychainOrArray,
	SecCertificateRef certificateRef,
	SecIdentityRef *identityRef)
{
    BEGIN_SECAPI

	SecPointer<Certificate> certificatePtr(Certificate::required(certificateRef));
	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	SecPointer<Identity> identityPtr(new Identity(keychains, certificatePtr));
	Required(identityRef) = identityPtr->handle();

    END_SECAPI
}

SecIdentityRef
SecIdentityCreate(
	CFAllocatorRef allocator,
	SecCertificateRef certificate,
	SecKeyRef privateKey)
{
	SecIdentityRef identityRef = NULL;
    OSStatus __secapiresult;
	try {
		SecPointer<Certificate> certificatePtr(Certificate::required(certificate));
		SecPointer<KeyItem> keyItemPtr(KeyItem::required(privateKey));
		SecPointer<Identity> identityPtr(new Identity(keyItemPtr, certificatePtr));
		identityRef = identityPtr->handle();

		__secapiresult=noErr;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=memFullErr; }
	catch (...) { __secapiresult=internalComponentErr; }
	return identityRef;
}

CFComparisonResult
SecIdentityCompare(
	SecIdentityRef identity1,
	SecIdentityRef identity2,
	CFOptionFlags compareOptions)
{
	if (!identity1 || !identity2)
	{
		if (identity1 == identity2)
			return kCFCompareEqualTo;
		else if (identity1 < identity2)
			return kCFCompareLessThan;
		else
			return kCFCompareGreaterThan;
	}

	BEGIN_SECAPI

	SecPointer<Identity> id1(Identity::required(identity1));
	SecPointer<Identity> id2(Identity::required(identity2));

	if (id1 == id2)
		return kCFCompareEqualTo;
	else if (id1 < id2)
		return kCFCompareLessThan;
	else
		return kCFCompareGreaterThan;

	END_SECAPI1(kCFCompareGreaterThan);
}

CFArrayRef _SecIdentityCopyPossiblePaths(
    CFStringRef name)
{
    // utility function to build and return an array of possible paths for the given name.
    // if name is not a URL, this returns a single-element array.
    // if name is a URL, the array may contain 1..N elements, one for each level of the path hierarchy.
    
    CFMutableArrayRef names = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (!name) {
        return names;
    }
    CFIndex oldLength = CFStringGetLength(name);
    CFArrayAppendValue(names, name);

    CFURLRef url = CFURLCreateWithString(NULL, name, NULL);
    if (url && CFURLCanBeDecomposed(url)) {
        // first, remove the query portion of this URL, if any
        CFStringRef qs = CFURLCopyQueryString(url, NULL);
        if (qs) {
            CFMutableStringRef newName = CFStringCreateMutableCopy(NULL, oldLength, name);
            if (newName) {
                CFIndex qsLength = CFStringGetLength(qs) + 1; // include the '?'
                CFStringDelete(newName, CFRangeMake(oldLength-qsLength, qsLength));
                CFRelease(url);
                url = CFURLCreateWithString(NULL, newName, NULL);
                CFArraySetValueAtIndex(names, 0, newName);
                CFRelease(newName);
            }
            CFRelease(qs);
        }
        // now add an entry for each level of the path
        while (url) {
            CFURLRef parent = CFURLCreateCopyDeletingLastPathComponent(NULL, url);
            if (parent) {
                CFStringRef parentURLString = CFURLGetString(parent);
                if (parentURLString) {
                    CFIndex newLength = CFStringGetLength(parentURLString);
                    // check that string length has decreased as expected; for file URLs,
                    // CFURLCreateCopyDeletingLastPathComponent can insert './' or '../'
                    if ((newLength >= oldLength) || (!CFStringHasPrefix(name, parentURLString))) {
                        CFRelease(parent);
                        CFRelease(url);
                        break;
                    }
                    oldLength = newLength;
                    CFArrayAppendValue(names, parentURLString);
                }
            }
            CFRelease(url);
            url = parent;
        }
		// finally, add wildcard entries for each subdomain
		url = CFURLCreateWithString(NULL, name, NULL);
		if (url && CFURLCanBeDecomposed(url)) {
			CFStringRef netLocString = CFURLCopyNetLocation(url);
			if (netLocString) {
				// first strip off port number, if present
				CFStringRef tmpLocString = netLocString;
				CFArrayRef hostnameArray = CFStringCreateArrayBySeparatingStrings(NULL, netLocString, CFSTR(":"));
				tmpLocString = (CFStringRef)CFRetain((CFStringRef)CFArrayGetValueAtIndex(hostnameArray, 0));
				CFRelease(netLocString);
				CFRelease(hostnameArray);
				netLocString = tmpLocString;
				// split remaining string into domain components
				hostnameArray = CFStringCreateArrayBySeparatingStrings(NULL, netLocString, CFSTR("."));
				CFIndex subdomainCount = CFArrayGetCount(hostnameArray);
				CFIndex i = 0;
				while (++i < subdomainCount) {
					CFIndex j = i;
					CFMutableStringRef wildcardString = CFStringCreateMutable(NULL, 0);
					if (wildcardString) {
						CFStringAppendCString(wildcardString, "*", kCFStringEncodingUTF8);
						while (j < subdomainCount) {
							CFStringRef domainString = (CFStringRef)CFArrayGetValueAtIndex(hostnameArray, j++);
							if (CFStringGetLength(domainString) > 0) {
								CFStringAppendCString(wildcardString, ".", kCFStringEncodingUTF8);
								CFStringAppend(wildcardString, domainString);
							}
						}
						if (CFStringGetLength(wildcardString) > 1) {
							CFArrayAppendValue(names, wildcardString);
						}
						CFRelease(wildcardString);
					}
				}
				CFRelease(hostnameArray);
				CFRelease(netLocString);
			}
			CFRelease(url);
		}
    }
    
    return names;
}

OSStatus _SecIdentityCopyPreferenceMatchingName(
    CFStringRef name,
    CSSM_KEYUSE keyUsage,
    CFArrayRef validIssuers,
    SecIdentityRef *identity)
{
    // this is NOT exported, and called only from SecIdentityCopyPreference (below), so no BEGIN/END macros here;
    // caller must handle exceptions

	StorageManager::KeychainList keychains;
	globals().storageManager.getSearchList(keychains);
	KCCursor cursor(keychains, kSecGenericPasswordItemClass, NULL);

	char idUTF8[MAXPATHLEN];
    Required(name);
    if (!CFStringGetCString(name, idUTF8, sizeof(idUTF8)-1, kCFStringEncodingUTF8))
        idUTF8[0] = (char)'\0';
    CssmData service(const_cast<char *>(idUTF8), strlen(idUTF8));
	FourCharCode itemType = 'iprf';
    cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecServiceItemAttr), service);
	cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecTypeItemAttr), itemType);
    if (keyUsage)
        cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecScriptCodeItemAttr), (sint32)keyUsage);

	Item prefItem;
	if (!cursor->next(prefItem))
		return errSecItemNotFound;

	// get persistent certificate reference
	SecKeychainAttribute itemAttrs[] = { { kSecGenericItemAttr, 0, NULL } };
	SecKeychainAttributeList itemAttrList = { sizeof(itemAttrs) / sizeof(itemAttrs[0]), itemAttrs };
	prefItem->getContent(NULL, &itemAttrList, NULL, NULL);

	// find certificate, given persistent reference data
	CFDataRef pItemRef = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)itemAttrs[0].data, itemAttrs[0].length, kCFAllocatorNull);
	SecKeychainItemRef certItemRef = nil;
	OSStatus status = SecKeychainItemCopyFromPersistentReference(pItemRef, &certItemRef); //%%% need to make this a method of ItemImpl
	prefItem->freeContent(&itemAttrList, NULL);
	if (pItemRef)
		CFRelease(pItemRef);
	if (status)
		return status;
    
    // filter on valid issuers, if provided
    if (validIssuers) {
        //%%%TBI
    }

	// create identity reference, given certificate
	Item certItem = ItemImpl::required(SecKeychainItemRef(certItemRef));
	SecPointer<Certificate> certificate(static_cast<Certificate *>(certItem.get()));
	SecPointer<Identity> identity_ptr(new Identity(keychains, certificate));
	if (certItemRef)
		CFRelease(certItemRef);

	Required(identity) = identity_ptr->handle();

    return status;
}

SecIdentityRef SecIdentityCopyPreferred(CFStringRef name, CFArrayRef keyUsage, CFArrayRef validIssuers)
{
	// This function will look for a matching preference in the following order:
	// - matches the name and the supplied key use
	// - matches the name and the special 'ANY' key use
	// - matches the name with no key usage constraint

	SecIdentityRef identityRef = NULL;
	CSSM_KEYUSE keyUse = ConvertArrayToKeyUsage(keyUsage);
	OSStatus status = SecIdentityCopyPreference(name, keyUse, validIssuers, &identityRef);
	if (status != noErr && keyUse != CSSM_KEYUSE_ANY)
		status = SecIdentityCopyPreference(name, CSSM_KEYUSE_ANY, validIssuers, &identityRef);
	if (status != noErr && keyUse != 0)
		status = SecIdentityCopyPreference(name, 0, validIssuers, &identityRef);

	return identityRef;
}

OSStatus SecIdentityCopyPreference(
    CFStringRef name,
    CSSM_KEYUSE keyUsage,
    CFArrayRef validIssuers,
    SecIdentityRef *identity)
{
    // The original implementation of SecIdentityCopyPreference matches the exact string only.
    // That implementation has been moved to _SecIdentityCopyPreferenceMatchingName (above),
    // and this function is a wrapper which calls it, so that existing clients will get the
    // extended behavior of server domain matching for items that specify URLs.
    // (Note that behavior is unchanged if the specified name is not a URL.)

    BEGIN_SECAPI

    CFTypeRef val = (CFTypeRef)CFPreferencesCopyValue(CFSTR("LogIdentityPreferenceLookup"),
                    CFSTR("com.apple.security"),
                    kCFPreferencesCurrentUser,
                    kCFPreferencesAnyHost);
    Boolean logging = false;
    if (val && CFGetTypeID(val) == CFBooleanGetTypeID()) {
        logging = CFBooleanGetValue((CFBooleanRef)val);
        CFRelease(val);
    }

    OSStatus status = errSecItemNotFound;
    CFArrayRef names = _SecIdentityCopyPossiblePaths(name);
    if (!names) {
        return status;
    }

    CFIndex idx, total = CFArrayGetCount(names);
    for (idx = 0; idx < total; idx++) {
        CFStringRef aName = (CFStringRef)CFArrayGetValueAtIndex(names, idx);
        try {
            status = _SecIdentityCopyPreferenceMatchingName(aName, keyUsage, validIssuers, identity);
        }
        catch (...) { status = errSecItemNotFound; }

        if (logging) {
            // get identity label
            CFStringRef labelString = NULL;
            if (!status && identity && *identity) {
                try {
                    SecPointer<Certificate> cert(Identity::required(*identity)->certificate());
                    cert->inferLabel(false, &labelString);
                }
                catch (...) { labelString = NULL; };
            }
            char *labelBuf = NULL;
            CFIndex labelBufSize = (labelString) ? CFStringGetLength(labelString) * 4 : 4;
            labelBuf = (char *)malloc(labelBufSize);
            if (!labelString || !CFStringGetCString(labelString, labelBuf, labelBufSize, kCFStringEncodingUTF8)) {
                labelBuf[0] = 0;
            }
            if (labelString) {
                CFRelease(labelString);
            }

            // get service name
            char *serviceBuf = NULL;
            CFIndex serviceBufSize = CFStringGetLength(aName) * 4;
            serviceBuf = (char *)malloc(serviceBufSize);
            if (!CFStringGetCString(aName, serviceBuf, serviceBufSize, kCFStringEncodingUTF8)) {
                serviceBuf[0] = 0;
            }

            syslog(LOG_NOTICE, "preferred identity: \"%s\" found for \"%s\"\n", labelBuf, serviceBuf);
            if (!status && name) {
                char *nameBuf = NULL;
                CFIndex nameBufSize = CFStringGetLength(name) * 4;
                nameBuf = (char *)malloc(nameBufSize);
                if (!CFStringGetCString(name, nameBuf, nameBufSize, kCFStringEncodingUTF8)) {
                    nameBuf[0] = 0;
                }
                syslog(LOG_NOTICE, "lookup complete; will use: \"%s\" for \"%s\"\n", labelBuf, nameBuf);
                free(nameBuf);
            }
            
            free(labelBuf);
            free(serviceBuf);
        }

        if (status == noErr) {
            break; // match found
        }
    }

    CFRelease(names);
    return status;

    END_SECAPI
}

OSStatus SecIdentitySetPreference(
    SecIdentityRef identity,
    CFStringRef name,
    CSSM_KEYUSE keyUsage)
{
	if (!name) {
		return paramErr;
	}
	if (!identity) {
		// treat NULL identity as a request to clear the preference
		// (note: if keyUsage is 0, this clears all key usage prefs for name)
		return SecIdentityDeletePreferenceItemWithNameAndKeyUsage(NULL, name, keyUsage);
	}

    BEGIN_SECAPI

	SecPointer<Certificate> certificate(Identity::required(identity)->certificate());

	// determine the account attribute
	//
	// This attribute must be synthesized from certificate label + pref item type + key usage,
	// as only the account and service attributes can make a generic keychain item unique.
	// For 'iprf' type items (but not 'cprf'), we append a trailing space. This insures that
	// we can save a certificate preference if an identity preference already exists for the
	// given service name, and vice-versa.
	// If the key usage is 0 (i.e. the normal case), we omit the appended key usage string.
	//
    CFStringRef labelStr = nil;
	certificate->inferLabel(false, &labelStr);
	if (!labelStr) {
        MacOSError::throwMe(errSecDataTooLarge); // data is "in a format which cannot be displayed"
	}
	CFIndex accountUTF8Len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(labelStr), kCFStringEncodingUTF8) + 1;
	const char *templateStr = "%s [key usage 0x%X]";
	const int keyUsageMaxStrLen = 8;
	accountUTF8Len += strlen(templateStr) + keyUsageMaxStrLen;
	char accountUTF8[accountUTF8Len];
    if (!CFStringGetCString(labelStr, accountUTF8, accountUTF8Len-1, kCFStringEncodingUTF8))
		accountUTF8[0] = (char)'\0';
	if (keyUsage)
		snprintf(accountUTF8, accountUTF8Len-1, templateStr, accountUTF8, keyUsage);
	snprintf(accountUTF8, accountUTF8Len-1, "%s ", accountUTF8);
    CssmData account(const_cast<char *>(accountUTF8), strlen(accountUTF8));
    CFRelease(labelStr);

	// service attribute (name provided by the caller)
	CFIndex serviceUTF8Len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(name), kCFStringEncodingUTF8) + 1;;
	char serviceUTF8[serviceUTF8Len];
    if (!CFStringGetCString(name, serviceUTF8, serviceUTF8Len-1, kCFStringEncodingUTF8))
        serviceUTF8[0] = (char)'\0';
    CssmData service(const_cast<char *>(serviceUTF8), strlen(serviceUTF8));

    // look for existing identity preference item, in case this is an update
	StorageManager::KeychainList keychains;
	globals().storageManager.getSearchList(keychains);
	KCCursor cursor(keychains, kSecGenericPasswordItemClass, NULL);
    FourCharCode itemType = 'iprf';
    cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecServiceItemAttr), service);
	cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecTypeItemAttr), itemType);
    if (keyUsage) {
        cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecScriptCodeItemAttr), (sint32)keyUsage);
	}

	Item item(kSecGenericPasswordItemClass, 'aapl', 0, NULL, false);
    bool add = (!cursor->next(item));
	// at this point, we either have a new item to add or an existing item to update

    // set item attribute values
    item->setAttribute(Schema::attributeInfo(kSecServiceItemAttr), service);
    item->setAttribute(Schema::attributeInfo(kSecTypeItemAttr), itemType);
    item->setAttribute(Schema::attributeInfo(kSecAccountItemAttr), account);
	item->setAttribute(Schema::attributeInfo(kSecScriptCodeItemAttr), (sint32)keyUsage);
    item->setAttribute(Schema::attributeInfo(kSecLabelItemAttr), service);
    
	// generic attribute (store persistent certificate reference)
	CFDataRef pItemRef = nil;
    certificate->copyPersistentReference(pItemRef);
	if (!pItemRef) {
		MacOSError::throwMe(errSecInvalidItemRef);
    }
	const UInt8 *dataPtr = CFDataGetBytePtr(pItemRef);
	CFIndex dataLen = CFDataGetLength(pItemRef);
	CssmData pref(const_cast<void *>(reinterpret_cast<const void *>(dataPtr)), dataLen);
	item->setAttribute(Schema::attributeInfo(kSecGenericItemAttr), pref);
	CFRelease(pItemRef);

    if (add) {
        Keychain keychain = nil;
        try {
            keychain = globals().storageManager.defaultKeychain();
            if (!keychain->exists())
                MacOSError::throwMe(errSecNoSuchKeychain);	// Might be deleted or not available at this time.
        }
        catch(...) {
            keychain = globals().storageManager.defaultKeychainUI(item);
        }

		try {
			keychain->add(item);
		}
		catch (const MacOSError &err) {
			if (err.osStatus() != errSecDuplicateItem)
				throw; // if item already exists, fall through to update
		}
    }
	item->update();

    END_SECAPI
}

OSStatus 
SecIdentitySetPreferred(SecIdentityRef identity, CFStringRef name, CFArrayRef keyUsage)
{
	CSSM_KEYUSE keyUse = ConvertArrayToKeyUsage(keyUsage);
	return SecIdentitySetPreference(identity, name, keyUse);
}

OSStatus
SecIdentityFindPreferenceItem(
	CFTypeRef keychainOrArray,
	CFStringRef idString,
	SecKeychainItemRef *itemRef)
{
    BEGIN_SECAPI

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	KCCursor cursor(keychains, kSecGenericPasswordItemClass, NULL);

	char idUTF8[MAXPATHLEN];
    idUTF8[0] = (char)'\0';
	if (idString)
	{
		if (!CFStringGetCString(idString, idUTF8, sizeof(idUTF8)-1, kCFStringEncodingUTF8))
			idUTF8[0] = (char)'\0';
	}
    size_t idUTF8Len = strlen(idUTF8);
    if (!idUTF8Len)
        MacOSError::throwMe(paramErr);

    CssmData service(const_cast<char *>(idUTF8), idUTF8Len);
    cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecServiceItemAttr), service);
	cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecTypeItemAttr), (FourCharCode)'iprf');

	Item item;
	if (!cursor->next(item))
		MacOSError::throwMe(errSecItemNotFound);

	if (itemRef)
		*itemRef=item->handle();

    END_SECAPI
}

OSStatus
SecIdentityFindPreferenceItemWithNameAndKeyUsage(
	CFTypeRef keychainOrArray,
	CFStringRef name,
	int32_t keyUsage,
	SecKeychainItemRef *itemRef)
{
    BEGIN_SECAPI

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	KCCursor cursor(keychains, kSecGenericPasswordItemClass, NULL);

	char idUTF8[MAXPATHLEN];
    idUTF8[0] = (char)'\0';
	if (name)
	{
		if (!CFStringGetCString(name, idUTF8, sizeof(idUTF8)-1, kCFStringEncodingUTF8))
			idUTF8[0] = (char)'\0';
	}
    size_t idUTF8Len = strlen(idUTF8);
    if (!idUTF8Len)
        MacOSError::throwMe(paramErr);

    CssmData service(const_cast<char *>(idUTF8), idUTF8Len);
    cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecServiceItemAttr), service);
	cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecTypeItemAttr), (FourCharCode)'iprf');
    if (keyUsage)
        cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecScriptCodeItemAttr), (sint32)keyUsage);

	Item item;
	if (!cursor->next(item))
		MacOSError::throwMe(errSecItemNotFound);

	if (itemRef)
		*itemRef=item->handle();

    END_SECAPI
}

OSStatus SecIdentityDeletePreferenceItemWithNameAndKeyUsage(
	CFTypeRef keychainOrArray,
	CFStringRef name,
	int32_t keyUsage)
{
	// when a specific key usage is passed, we'll only match & delete that pref;
	// when a key usage of 0 is passed, all matching prefs should be deleted.
	// maxUsages represents the most matches there could theoretically be, so
	// cut things off at that point if we're still finding items (if they can't
	// be deleted for some reason, we'd never break out of the loop.)
	
	OSStatus status;
	SecKeychainItemRef item = NULL;
	int count = 0, maxUsages = 12;
	while (++count <= maxUsages &&
			(status = SecIdentityFindPreferenceItemWithNameAndKeyUsage(keychainOrArray, name, keyUsage, &item)) == noErr) {
		status = SecKeychainItemDelete(item);
		CFRelease(item);
		item = NULL;
	}
	
	// it's not an error if the item isn't found
	return (status == errSecItemNotFound) ? noErr : status;
}


OSStatus _SecIdentityAddPreferenceItemWithName(
	SecKeychainRef keychainRef,
	SecIdentityRef identityRef,
	CFStringRef idString,
	SecKeychainItemRef *itemRef)
{
    // this is NOT exported, and called only from SecIdentityAddPreferenceItem (below), so no BEGIN/END macros here;
    // caller must handle exceptions

	if (!identityRef || !idString)
		return paramErr;
	SecPointer<Certificate> cert(Identity::required(identityRef)->certificate());
	Item item(kSecGenericPasswordItemClass, 'aapl', 0, NULL, false);
	sint32 keyUsage = 0;

	// determine the account attribute
	//
	// This attribute must be synthesized from certificate label + pref item type + key usage,
	// as only the account and service attributes can make a generic keychain item unique.
	// For 'iprf' type items (but not 'cprf'), we append a trailing space. This insures that
	// we can save a certificate preference if an identity preference already exists for the
	// given service name, and vice-versa.
	// If the key usage is 0 (i.e. the normal case), we omit the appended key usage string.
	//
    CFStringRef labelStr = nil;
	cert->inferLabel(false, &labelStr);
	if (!labelStr) {
        return errSecDataTooLarge; // data is "in a format which cannot be displayed"
	}
	CFIndex accountUTF8Len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(labelStr), kCFStringEncodingUTF8) + 1;
	const char *templateStr = "%s [key usage 0x%X]";
	const int keyUsageMaxStrLen = 8;
	accountUTF8Len += strlen(templateStr) + keyUsageMaxStrLen;
	char accountUTF8[accountUTF8Len];
    if (!CFStringGetCString(labelStr, accountUTF8, accountUTF8Len-1, kCFStringEncodingUTF8))
		accountUTF8[0] = (char)'\0';
	if (keyUsage)
		snprintf(accountUTF8, accountUTF8Len-1, templateStr, accountUTF8, keyUsage);
	snprintf(accountUTF8, accountUTF8Len-1, "%s ", accountUTF8);
    CssmData account(const_cast<char *>(accountUTF8), strlen(accountUTF8));
    CFRelease(labelStr);

	// service attribute (name provided by the caller)
	CFIndex serviceUTF8Len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(idString), kCFStringEncodingUTF8) + 1;;
	char serviceUTF8[serviceUTF8Len];
    if (!CFStringGetCString(idString, serviceUTF8, serviceUTF8Len-1, kCFStringEncodingUTF8))
        serviceUTF8[0] = (char)'\0';
    CssmData service(const_cast<char *>(serviceUTF8), strlen(serviceUTF8));

	// set item attribute values
	item->setAttribute(Schema::attributeInfo(kSecServiceItemAttr), service);
	item->setAttribute(Schema::attributeInfo(kSecLabelItemAttr), service);
	item->setAttribute(Schema::attributeInfo(kSecTypeItemAttr), (FourCharCode)'iprf');
	item->setAttribute(Schema::attributeInfo(kSecAccountItemAttr), account);
	item->setAttribute(Schema::attributeInfo(kSecScriptCodeItemAttr), keyUsage);

	// generic attribute (store persistent certificate reference)
	CFDataRef pItemRef = nil;
	OSStatus status = SecKeychainItemCreatePersistentReference((SecKeychainItemRef)cert->handle(), &pItemRef);
	if (!pItemRef)
		status = errSecInvalidItemRef;
	if (status)
		 return status;
	const UInt8 *dataPtr = CFDataGetBytePtr(pItemRef);
	CFIndex dataLen = CFDataGetLength(pItemRef);
	CssmData pref(const_cast<void *>(reinterpret_cast<const void *>(dataPtr)), dataLen);
	item->setAttribute(Schema::attributeInfo(kSecGenericItemAttr), pref);
	CFRelease(pItemRef);

	Keychain keychain = nil;
	try {
        keychain = Keychain::optional(keychainRef);
        if (!keychain->exists())
            MacOSError::throwMe(errSecNoSuchKeychain);	// Might be deleted or not available at this time.
    }
    catch(...) {
        keychain = globals().storageManager.defaultKeychainUI(item);
    }
	
	try {
		keychain->add(item);
	}
	catch (const MacOSError &err) {
		if (err.osStatus() != errSecDuplicateItem)
			throw; // if item already exists, fall through to update
	}

	item->update();

    if (itemRef)
		*itemRef = item->handle();

    return status;
}

OSStatus SecIdentityAddPreferenceItem(
	SecKeychainRef keychainRef,
	SecIdentityRef identityRef,
	CFStringRef idString,
	SecKeychainItemRef *itemRef)
{
    // The original implementation of SecIdentityAddPreferenceItem adds the exact string only.
    // That implementation has been moved to _SecIdentityAddPreferenceItemWithName (above),
    // and this function is a wrapper which calls it, so that existing clients will get the
    // extended behavior of server domain matching for items that specify URLs.
    // (Note that behavior is unchanged if the specified idString is not a URL.)

    BEGIN_SECAPI

    OSStatus status = internalComponentErr;
    CFArrayRef names = _SecIdentityCopyPossiblePaths(idString);
    if (!names) {
        return status;
    }

    CFIndex total = CFArrayGetCount(names);
    if (total > 0) {
        // add item for name (first element in array)
        CFStringRef aName = (CFStringRef)CFArrayGetValueAtIndex(names, 0);
        try {
            status = _SecIdentityAddPreferenceItemWithName(keychainRef, identityRef, aName, itemRef);
        }
        catch (const MacOSError &err)   { status=err.osStatus(); }
        catch (const CommonError &err)  { status=SecKeychainErrFromOSStatus(err.osStatus()); }
        catch (const std::bad_alloc &)  { status=memFullErr; }
        catch (...)                     { status=internalComponentErr; }
    }
    if (total > 2) {
		Boolean setDomainDefaultIdentity = FALSE;
		CFTypeRef val = (CFTypeRef)CFPreferencesCopyValue(CFSTR("SetDomainDefaultIdentity"),
														  CFSTR("com.apple.security.identities"),
														  kCFPreferencesCurrentUser,
														  kCFPreferencesAnyHost);
		if (val) {
			if (CFGetTypeID(val) == CFBooleanGetTypeID())
				setDomainDefaultIdentity = CFBooleanGetValue((CFBooleanRef)val) ? TRUE : FALSE;
			CFRelease(val);
		}
		if (setDomainDefaultIdentity) {
			// add item for domain (second-to-last element in array, e.g. "*.apple.com")
			OSStatus tmpStatus = noErr;
			CFStringRef aName = (CFStringRef)CFArrayGetValueAtIndex(names, total-2);
			try {
				tmpStatus = _SecIdentityAddPreferenceItemWithName(keychainRef, identityRef, aName, itemRef);
			}
			catch (const MacOSError &err)   { tmpStatus=err.osStatus(); }
			catch (const CommonError &err)  { tmpStatus=SecKeychainErrFromOSStatus(err.osStatus()); }
			catch (const std::bad_alloc &)  { tmpStatus=memFullErr; }
			catch (...)                     { tmpStatus=internalComponentErr; }
		}
    }

    CFRelease(names);
    return status;

    END_SECAPI
}

/* deprecated in 10.5 */
OSStatus SecIdentityUpdatePreferenceItem(
			SecKeychainItemRef itemRef,
			SecIdentityRef identityRef)
{
    BEGIN_SECAPI

	if (!itemRef || !identityRef)
		MacOSError::throwMe(paramErr);
	SecPointer<Certificate> certificate(Identity::required(identityRef)->certificate());
	Item prefItem = ItemImpl::required(itemRef);

	// get the current key usage value for this item
	sint32 keyUsage = 0;
	UInt32 actLen = 0;
	SecKeychainAttribute attr = { kSecScriptCodeItemAttr, sizeof(sint32), &keyUsage };
	try {
		prefItem->getAttribute(attr, &actLen);
	}
	catch(...) {
		keyUsage = 0;
	};

	// set the account attribute
	//
	// This attribute must be synthesized from certificate label + pref item type + key usage,
	// as only the account and service attributes can make a generic keychain item unique.
	// For 'iprf' type items (but not 'cprf'), we append a trailing space. This insures that
	// we can save a certificate preference if an identity preference already exists for the
	// given service name, and vice-versa.
	// If the key usage is 0 (i.e. the normal case), we omit the appended key usage string.
	//
    CFStringRef labelStr = nil;
	certificate->inferLabel(false, &labelStr);
	if (!labelStr) {
        MacOSError::throwMe(errSecDataTooLarge); // data is "in a format which cannot be displayed"
	}
	CFIndex accountUTF8Len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(labelStr), kCFStringEncodingUTF8) + 1;
	const char *templateStr = "%s [key usage 0x%X]";
	const int keyUsageMaxStrLen = 8;
	accountUTF8Len += strlen(templateStr) + keyUsageMaxStrLen;
	char accountUTF8[accountUTF8Len];
    if (!CFStringGetCString(labelStr, accountUTF8, accountUTF8Len-1, kCFStringEncodingUTF8))
		accountUTF8[0] = (char)'\0';
	if (keyUsage)
		snprintf(accountUTF8, accountUTF8Len-1, templateStr, accountUTF8, keyUsage);
	snprintf(accountUTF8, accountUTF8Len-1, "%s ", accountUTF8);
    CssmData account(const_cast<char *>(accountUTF8), strlen(accountUTF8));
	prefItem->setAttribute(Schema::attributeInfo(kSecAccountItemAttr), account);
    CFRelease(labelStr);

	// generic attribute (store persistent certificate reference)
	CFDataRef pItemRef = nil;
	OSStatus status = SecKeychainItemCreatePersistentReference((SecKeychainItemRef)certificate->handle(), &pItemRef);
	if (!pItemRef)
		status = errSecInvalidItemRef;
	if (status)
		MacOSError::throwMe(status);
	const UInt8 *dataPtr = CFDataGetBytePtr(pItemRef);
	CFIndex dataLen = CFDataGetLength(pItemRef);
	CssmData pref(const_cast<void *>(reinterpret_cast<const void *>(dataPtr)), dataLen);
	prefItem->setAttribute(Schema::attributeInfo(kSecGenericItemAttr), pref);
	CFRelease(pItemRef);

	prefItem->update();

    END_SECAPI
}

OSStatus SecIdentityCopyFromPreferenceItem(
			SecKeychainItemRef itemRef,
			SecIdentityRef *identityRef)
{
    BEGIN_SECAPI

	if (!itemRef || !identityRef)
		MacOSError::throwMe(paramErr);
	Item prefItem = ItemImpl::required(itemRef);

	// get persistent certificate reference
	SecKeychainAttribute itemAttrs[] = { { kSecGenericItemAttr, 0, NULL } };
	SecKeychainAttributeList itemAttrList = { sizeof(itemAttrs) / sizeof(itemAttrs[0]), itemAttrs };
	prefItem->getContent(NULL, &itemAttrList, NULL, NULL);

	// find certificate, given persistent reference data
	CFDataRef pItemRef = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)itemAttrs[0].data, itemAttrs[0].length, kCFAllocatorNull);
	SecKeychainItemRef certItemRef = nil;
	OSStatus status = SecKeychainItemCopyFromPersistentReference(pItemRef, &certItemRef); //%%% need to make this a method of ItemImpl
	prefItem->freeContent(&itemAttrList, NULL);
	if (pItemRef)
		CFRelease(pItemRef);
	if (status)
		return status;

	// create identity reference, given certificate
	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList((CFTypeRef)NULL, keychains);
	Item certItem = ItemImpl::required(SecKeychainItemRef(certItemRef));
	SecPointer<Certificate> certificate(static_cast<Certificate *>(certItem.get()));
	SecPointer<Identity> identity(new Identity(keychains, certificate));
	if (certItemRef)
		CFRelease(certItemRef);

	Required(identityRef) = identity->handle();

    END_SECAPI
}

/*
 * System Identity Support.
 */

/* plist domain (in /Library/Preferences) */
#define IDENTITY_DOMAIN		"com.apple.security.systemidentities"

/* 
 * Our plist is a dictionary whose entries have the following format:
 * key   = domain name as CFString
 * value = public key hash as CFData
 */

#define SYSTEM_KEYCHAIN_PATH	kSystemKeychainDir "/" kSystemKeychainName

/* 
 * All accesses to system identities and its associated plist are
 * protected by this lock.
 */
ModuleNexus<Mutex> systemIdentityLock;

OSStatus SecIdentityCopySystemIdentity(
   CFStringRef domain,          
   SecIdentityRef *idRef,
   CFStringRef *actualDomain) /* optional */
{
    BEGIN_SECAPI

	StLock<Mutex> _(systemIdentityLock());
	auto_ptr<Dictionary> identDict;
	
	/* get top-level dictionary - if not present, we're done */
	Dictionary* d = Dictionary::CreateDictionary(IDENTITY_DOMAIN, Dictionary::US_System);
	if (d == NULL)
	{
		return errSecNotAvailable;
	}
	
	identDict.reset(d);
	
	/* see if there's an entry for specified domain */
	CFDataRef entryValue = identDict->getDataValue(domain);
	if(entryValue == NULL) {
		/* try for default entry if we're not already looking for default */
		if(!CFEqual(domain, kSecIdentityDomainDefault)) {
			entryValue = identDict->getDataValue(kSecIdentityDomainDefault);
		}
		if(entryValue == NULL) {
			/* no default identity */
			MacOSError::throwMe(errSecItemNotFound);
		}
		
		/* remember that we're not fetching the requested domain */
		domain = kSecIdentityDomainDefault;
	}
	
	/* open system keychain - error here is fatal */
	Keychain systemKc = globals().storageManager.make(SYSTEM_KEYCHAIN_PATH, false);
	CFRef<SecKeychainRef> systemKcRef(systemKc->handle());
	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(systemKcRef, keychains);
	
	/* search for specified cert */
	SecKeychainAttributeList	attrList;
	SecKeychainAttribute		attr;
	attr.tag        = kSecPublicKeyHashItemAttr;
	attr.length     = CFDataGetLength(entryValue);
	attr.data       = (void *)CFDataGetBytePtr(entryValue);
	attrList.count  = 1;
	attrList.attr   = &attr;

	KCCursor cursor(keychains, kSecCertificateItemClass, &attrList);
	Item certItem;
	if(!cursor->next(certItem)) {
		MacOSError::throwMe(errSecItemNotFound);
	}
	
	/* found the cert; try matching with key to cook up identity */
	SecPointer<Certificate> certificate(static_cast<Certificate *>(certItem.get()));
	SecPointer<Identity> identity(new Identity(keychains, certificate));

	Required(idRef) = identity->handle();
	if(actualDomain) {
		*actualDomain = domain;
		CFRetain(*actualDomain);
	}

    END_SECAPI
}

OSStatus SecIdentitySetSystemIdentity(
   CFStringRef domain,     
   SecIdentityRef idRef)
{
    BEGIN_SECAPI

	StLock<Mutex> _(systemIdentityLock());
	if(geteuid() != 0) {
		MacOSError::throwMe(errSecAuthFailed);
	}
	
	auto_ptr<MutableDictionary> identDict;
	MutableDictionary *d = MutableDictionary::CreateMutableDictionary(IDENTITY_DOMAIN, Dictionary::US_System);
	if (d)
	{
		identDict.reset(d);
	}
	else
	{
		if(idRef == NULL) {
			/* nothing there, nothing to set - done */
			return noErr;
		}
		identDict.reset(new MutableDictionary());
	}
	
	if(idRef == NULL) {
		/* Just delete the possible entry for this domain */
		identDict->removeValue(domain);
	}
	else {
		/* obtain public key hash of identity's cert */
		SecPointer<Identity> identity(Identity::required(idRef));
		SecPointer<Certificate> cert = identity->certificate();
		const CssmData &pubKeyHash = cert->publicKeyHash();
		CFRef<CFDataRef> pubKeyHashData(CFDataCreate(NULL, pubKeyHash.Data, 
			pubKeyHash.Length));
		
		/* add/replace to dictionary */
		identDict->setValue(domain, pubKeyHashData);
	}
	
	/* flush to disk */
	if(!identDict->writePlistToPrefs(IDENTITY_DOMAIN, Dictionary::US_System)) {
		MacOSError::throwMe(ioErr);
	}

    END_SECAPI
}

const CFStringRef kSecIdentityDomainDefault = CFSTR("com.apple.systemdefault");
const CFStringRef kSecIdentityDomainKerberosKDC = CFSTR("com.apple.kerberos.kdc");

