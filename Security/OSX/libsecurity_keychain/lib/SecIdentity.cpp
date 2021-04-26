/*
 * Copyright (c) 2002-2020 Apple Inc. All Rights Reserved.
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
#include <Security/SecCertificatePriv.h>

#include "SecBridge.h"
#include <security_keychain/Certificate.h>
#include <security_keychain/Identity.h>
#include <security_keychain/KeyItem.h>
#include <security_keychain/KCCursor.h>
#include <security_cdsa_utilities/Schema.h>
#include <security_utilities/simpleprefs.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecXPCUtils.h>
#include <sys/param.h>
#include <syslog.h>
#include <os/activity.h>
#include "LegacyAPICounts.h"

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
    os_activity_t activity = os_activity_create("SecIdentityCopyCertificate", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	if (!identityRef || !certificateRef) {
		return errSecParam;
	}
	CFTypeID itemType = CFGetTypeID(identityRef);
	if (itemType == SecIdentityGetTypeID()) {
		SecPointer<Certificate> certificatePtr(Identity::required(identityRef)->certificate());
		Required(certificateRef) = certificatePtr->handle();

		/* convert outgoing certificate item to a unified SecCertificateRef */
		CssmData certData = certificatePtr->data();
		CFDataRef data = NULL;
		if (certData.Data && certData.Length) {
			data = CFDataCreate(NULL, certData.Data, certData.Length);
		}
		if (!data) {
			*certificateRef = NULL;
			syslog(LOG_ERR, "ERROR: SecIdentityCopyCertificate failed to retrieve certificate data (length=%ld, data=0x%lX)",
					(long)certData.Length, (uintptr_t)certData.Data);
			return errSecInternal;
		}
		SecCertificateRef tmpRef = *certificateRef;
		*certificateRef = SecCertificateCreateWithKeychainItem(NULL, data, tmpRef);
		if (data) {
			CFRelease(data);
		}
		if (tmpRef) {
			CFRelease(tmpRef);
		}
	}
	else if (itemType == SecCertificateGetTypeID()) {
		// rdar://24483382
		// reconstituting a persistent identity reference could return the certificate
		SecCertificateRef certificate = (SecCertificateRef)identityRef;

		/* convert outgoing certificate item to a unified SecCertificateRef, if needed */
		if (SecCertificateIsItemImplInstance(certificate)) {
			*certificateRef = SecCertificateCreateFromItemImplInstance(certificate);
		}
		else {
			*certificateRef = (SecCertificateRef) CFRetain(certificate);
		}
		return errSecSuccess;
	}
	else {
		return errSecParam;
	}

	END_SECAPI
}


OSStatus
SecIdentityCopyPrivateKey(
            SecIdentityRef identityRef,
            SecKeyRef *privateKeyRef)
{
    BEGIN_SECAPI
    os_activity_t activity = os_activity_create("SecIdentityCopyPrivateKey", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	Required(privateKeyRef) = (SecKeyRef)CFRetain(Identity::required(identityRef)->privateKeyRef());

    END_SECAPI
}

OSStatus
SecIdentityCreateWithCertificate(
	CFTypeRef keychainOrArray,
	SecCertificateRef certificate,
	SecIdentityRef *identityRef)
{
	// This macro converts a new-style SecCertificateRef to an old-style ItemImpl
	BEGIN_SECCERTAPI

	SecPointer<Certificate> certificatePtr(Certificate::required(__itemImplRef));
	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	SecPointer<Identity> identityPtr(new Identity(keychains, certificatePtr));
	Required(identityRef) = identityPtr->handle();

	END_SECCERTAPI
}

SecIdentityRef
SecIdentityCreate(
	CFAllocatorRef allocator,
	SecCertificateRef certificate,
	SecKeyRef privateKey)
{
	COUNTLEGACYAPI
	SecIdentityRef identityRef = NULL;
	OSStatus __secapiresult;
	SecCertificateRef __itemImplRef = NULL;
	if (SecCertificateIsItemImplInstance(certificate)) {
		__itemImplRef=(SecCertificateRef)CFRetain(certificate);
	}
	if (!__itemImplRef && certificate) {
		__itemImplRef=(SecCertificateRef)SecCertificateCopyKeychainItem(certificate);
	}
	if (!__itemImplRef && certificate) {
		__itemImplRef=SecCertificateCreateItemImplInstance(certificate);
		(void)SecCertificateSetKeychainItem(certificate,__itemImplRef);
	}
	try {
		SecPointer<Certificate> certificatePtr(Certificate::required(__itemImplRef));
		SecPointer<Identity> identityPtr(new Identity(privateKey, certificatePtr));
		identityRef = identityPtr->handle();

		__secapiresult=errSecSuccess;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	if (__itemImplRef) { CFRelease(__itemImplRef); }
	return identityRef;
}

static
bool _SecIdentityNameIsURL(CFStringRef name)
{
    CFURLRef url = (name) ? CFURLCreateWithString(NULL, name, NULL) : NULL;
    bool result = (url && CFURLCanBeDecomposed(url));
    if (result) {
        CFStringRef schemeStr = CFURLCopyScheme(url);
        result = (schemeStr && (CFStringGetLength(schemeStr) > 0));
        CFReleaseSafe(schemeStr);
    }
    CFReleaseSafe(url);
    return result;
}

static
CFArrayRef _SecIdentityCopyPossiblePaths(
    CFStringRef name, CFStringRef appIdentifier)
{
    // utility function to build and return an array of possible paths for the given name.
    // if name is not a URL, this returns a single-element array.
    // if name is a URL, the array may contain 1..N elements, one for each level of the path hierarchy.
    // if name is a URL and appIdentifier is non-NULL, it is appended in parentheses to each array entry.

    CFMutableArrayRef names = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (!name) {
        return names;
    }
    CFIndex oldLength = CFStringGetLength(name);
    CFArrayAppendValue(names, name);

    CFURLRef url = CFURLCreateWithString(NULL, name, NULL);
    if (url) {
		if (CFURLCanBeDecomposed(url)) {
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
		}
		else {
			CFRelease(url);
		}
	}
	// finally, add wildcard entries for each subdomain
	url = CFURLCreateWithString(NULL, name, NULL);
	if (url) {
		if (CFURLCanBeDecomposed(url)) {
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
		}
		CFRelease(url);
	}
	if (appIdentifier) {
		CFIndex count = CFArrayGetCount(names);
		for (CFIndex idx=0; idx < count; idx++) {
			CFStringRef oldStr = (CFStringRef)CFArrayGetValueAtIndex(names, idx);
			CFStringRef appStr = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@ (%@)"), oldStr, appIdentifier);
			if (appStr) {
				// only use app identifier string if name is a URL
				if (_SecIdentityNameIsURL(oldStr)) {
					CFArraySetValueAtIndex(names, idx, appStr);
				}
				CFRelease(appStr);
			}
		}
	}

	return names;
}

static
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
    if (keyUsage) {
        cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecScriptCodeItemAttr), (sint32)keyUsage);
    }

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
	status = SecIdentityCreateWithCertificate(NULL, (SecCertificateRef)certItemRef, identity);
	if (certItemRef) {
		CFRelease(certItemRef);
	}

	return status;
}

static CFStringRef SecIdentityCopyPerAppNameForName(CFStringRef name)
{
    CFStringRef perAppName = NULL;
    // Currently, per-app identity preferences require a URL form name,
    // and the client must not be one whose role is to edit the item's ownership.
    if (_SecIdentityNameIsURL(name) && !SecXPCClientCanEditPreferenceOwnership()) {
        CFStringRef identifier = SecXPCCopyClientApplicationIdentifier();
        if (identifier) {
            // create per-app name with application identifier in parentheses
            perAppName = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@ (%@)"), name, identifier);
        }
        CFReleaseNull(identifier);
    }
    if (!perAppName && name) {
        // no application identifier, use name unchanged
        perAppName = (CFStringRef)CFRetain(name);
    }
    return perAppName;
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
	if (status != errSecSuccess && keyUse != CSSM_KEYUSE_ANY)
		status = SecIdentityCopyPreference(name, CSSM_KEYUSE_ANY, validIssuers, &identityRef);
	if (status != errSecSuccess && keyUse != 0)
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
    os_activity_t activity = os_activity_create("SecIdentityCopyPreference", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

    CFTypeRef val = (CFTypeRef)CFPreferencesCopyValue(CFSTR("LogIdentityPreferenceLookup"),
                    CFSTR("com.apple.security"),
                    kCFPreferencesCurrentUser,
                    kCFPreferencesAnyHost);
    Boolean logging = false;
    if (val) {
        if (CFGetTypeID(val) == CFBooleanGetTypeID()) {
            logging = CFBooleanGetValue((CFBooleanRef)val);
        }
    }
    CFReleaseNull(val);

    OSStatus status = errSecItemNotFound;
    CFStringRef appIdentifier = NULL;
    // We don't want to include the identifier if the app creating this pref has an editing role,
    // e.g. Keychain Access or security; this lets it create an item for another client, e.g. Safari.
    if (!SecXPCClientCanEditPreferenceOwnership()) {
        appIdentifier = SecXPCCopyClientApplicationIdentifier();
    }
    CFArrayRef names = _SecIdentityCopyPossiblePaths(name, appIdentifier);
    CFReleaseNull(appIdentifier);
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
            if (!status && aName) {
                char *nameBuf = NULL;
                CFIndex nameBufSize = CFStringGetLength(aName) * 4;
                nameBuf = (char *)malloc(nameBufSize);
                if (!CFStringGetCString(aName, nameBuf, nameBufSize, kCFStringEncodingUTF8)) {
                    nameBuf[0] = 0;
                }
                syslog(LOG_NOTICE, "lookup complete; will use: \"%s\" for \"%s\"\n", labelBuf, nameBuf);
                free(nameBuf);
            }

            free(labelBuf);
            free(serviceBuf);
        }

        if (status == errSecSuccess) {
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
        return errSecParam;
    }
    CFStringRef perAppName = SecIdentityCopyPerAppNameForName(name);
    if (!perAppName) {
        return errSecInternal;
    }
    if (!identity) {
        // treat NULL identity as a request to clear the preference
        // (note: if keyUsage is 0, this clears all key usage prefs for name)
        OSStatus result = SecIdentityDeletePreferenceItemWithNameAndKeyUsage(NULL, perAppName, keyUsage);
        CFReleaseNull(perAppName);
        return result;
    }

    BEGIN_SECAPI
    os_activity_t activity = os_activity_create("SecIdentitySetPreference", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

    CFRef<SecCertificateRef>  certRef;
    OSStatus status = SecIdentityCopyCertificate(identity, certRef.take());
    if(status != errSecSuccess) {
        CFReleaseNull(perAppName);
        MacOSError::throwMe(status);
    }

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
    SecCertificateInferLabel(certRef.get(), &labelStr);
    if (!labelStr) {
        CFReleaseNull(perAppName);
        MacOSError::throwMe(errSecDataTooLarge); // data is "in a format which cannot be displayed"
    }
    CFIndex accountUTF8Len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(labelStr), kCFStringEncodingUTF8) + 1;
    const char *templateStr = "%s [key usage 0x%X]";
    const int keyUsageMaxStrLen = 8;
    accountUTF8Len += strlen(templateStr) + keyUsageMaxStrLen;
    char *accountUTF8 = (char *)malloc(accountUTF8Len);
    if (!accountUTF8) {
        CFReleaseNull(perAppName);
        MacOSError::throwMe(errSecMemoryError);
    }
    if (!CFStringGetCString(labelStr, accountUTF8, accountUTF8Len-1, kCFStringEncodingUTF8)) {
        accountUTF8[0] = (char)'\0';
    }
    if (keyUsage) {
        snprintf(accountUTF8, accountUTF8Len-1, templateStr, accountUTF8, keyUsage);
    }
    snprintf(accountUTF8, accountUTF8Len-1, "%s ", accountUTF8);
    CssmDataContainer account(const_cast<char *>(accountUTF8), strlen(accountUTF8));
    free(accountUTF8);
    CFRelease(labelStr);

    // service attribute (name provided by the caller)
    CFIndex serviceUTF8Len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(perAppName), kCFStringEncodingUTF8) + 1;;
    char *serviceUTF8 = (char *)malloc(serviceUTF8Len);
    if (!serviceUTF8) {
        CFReleaseNull(perAppName);
        MacOSError::throwMe(errSecMemoryError);
    }
    if (!CFStringGetCString(perAppName, serviceUTF8, serviceUTF8Len-1, kCFStringEncodingUTF8)) {
        serviceUTF8[0] = (char)'\0';
    }
    CssmDataContainer service(const_cast<char *>(serviceUTF8), strlen(serviceUTF8));
    free(serviceUTF8);
    CFRelease(perAppName);

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
    SecKeychainItemCreatePersistentReference((SecKeychainItemRef)certRef.get(), &pItemRef);
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
                MacOSError::throwMe(errSecNoSuchKeychain); // Might be deleted or not available at this time.
        }
        catch(...) {
            keychain = globals().storageManager.defaultKeychainUI(item);
        }

        try {
            keychain->add(item);
        }
        catch (const MacOSError &err) {
            if (err.osStatus() != errSecDuplicateItem) {
                throw; // if item already exists, fall through to update
            }
        }
    }
    item->update();

    END_SECAPI
}

OSStatus
SecIdentitySetPreferred(SecIdentityRef identity, CFStringRef name, CFArrayRef keyUsage)
{
	COUNTLEGACYAPI
	CSSM_KEYUSE keyUse = ConvertArrayToKeyUsage(keyUsage);
	return SecIdentitySetPreference(identity, name, keyUse);
}

OSStatus
SecIdentityFindPreferenceItemWithNameAndKeyUsage(
	CFTypeRef keychainOrArray,
	CFStringRef name,
	int32_t keyUsage,
	SecKeychainItemRef *itemRef)
{
    BEGIN_SECAPI
    os_activity_t activity = os_activity_create("SecIdentityFindPreferenceItemWithNameAndKeyUsage", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

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
        MacOSError::throwMe(errSecParam);

    CssmData service(const_cast<char *>(idUTF8), idUTF8Len);
    cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecServiceItemAttr), service);
	cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecTypeItemAttr), (FourCharCode)'iprf');
    if (keyUsage) {
        cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecScriptCodeItemAttr), (sint32)keyUsage);
    }

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
	COUNTLEGACYAPI
	// when a specific key usage is passed, we'll only match & delete that pref;
	// when a key usage of 0 is passed, all matching prefs should be deleted.
	// maxUsages represents the most matches there could theoretically be, so
	// cut things off at that point if we're still finding items (if they can't
	// be deleted for some reason, we'd never break out of the loop.)

	OSStatus status = errSecInternalError;
	SecKeychainItemRef item = NULL;
	int count = 0, maxUsages = 12;
	while (++count <= maxUsages &&
			(status = SecIdentityFindPreferenceItemWithNameAndKeyUsage(keychainOrArray, name, keyUsage, &item)) == errSecSuccess) {
		status = SecKeychainItemDelete(item);
		CFRelease(item);
		item = NULL;
	}

	// it's not an error if the item isn't found
	return (status == errSecItemNotFound) ? errSecSuccess : status;
}

OSStatus SecIdentityDeleteApplicationPreferenceItems(void)
{
    BEGIN_SECAPI
    os_activity_t activity = os_activity_create("SecIdentityDeleteApplicationPreferenceItems", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

    StorageManager::KeychainList keychains;
    globals().storageManager.optionalSearchList(NULL, keychains);
    KCCursor cursor(keychains, kSecGenericPasswordItemClass, NULL);
    cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecTypeItemAttr), (FourCharCode)'iprf');

    CFMutableArrayRef matches = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFStringRef appIdentifier = SecXPCCopyClientApplicationIdentifier();
    CFStringRef suffixString = NULL;
    if (appIdentifier) {
        suffixString = CFStringCreateWithFormat(NULL, NULL, CFSTR(" (%@)"), appIdentifier);
        CFReleaseNull(appIdentifier);
    }
    if (!suffixString || !matches) {
        CFReleaseNull(matches);
        CFReleaseNull(suffixString);
        MacOSError::throwMe(errSecItemNotFound);
    }
    // iterate through candidate items and add matches to array
    Item item;
    while (cursor->next(item)) {
        SecKeychainAttribute attr[] = {
            { kSecServiceItemAttr, 0, NULL },
        };
        SecKeychainAttributeList attrList = { sizeof(attr) / sizeof(SecKeychainAttribute), attr };
        SecKeychainItemRef itemRef = item->handle();
        if (errSecSuccess == SecKeychainItemCopyContent(itemRef, NULL, &attrList, NULL, NULL)) {
            if (attr[0].length > 0 && attr[0].data != NULL) {
                CFStringRef serviceString = CFStringCreateWithBytes(NULL,
                    (const UInt8 *)attr[0].data, attr[0].length,
                    kCFStringEncodingUTF8, false);
                if (serviceString && (CFStringHasSuffix(serviceString, suffixString))) {
                    CFArrayAppendValue(matches, itemRef);
                }
                CFReleaseNull(serviceString);
            }
            SecKeychainItemFreeContent(&attrList, NULL);
        }
        CFReleaseNull(itemRef);
    }
    // delete matching items, if any
    CFIndex numDeleted=0, count = CFArrayGetCount(matches);
    OSStatus status = (count > 0) ? errSecSuccess : errSecItemNotFound;
    for (CFIndex idx=0; idx < count; idx++) {
        SecKeychainItemRef itemRef = (SecKeychainItemRef)CFArrayGetValueAtIndex(matches, idx);
        OSStatus tmpStatus = SecKeychainItemDelete(itemRef);
        if (errSecSuccess == tmpStatus) {
            ++numDeleted;
        } else {
            status = tmpStatus; // remember failure, but keep going
        }
    }
    syslog(LOG_NOTICE, "identity preferences found: %ld, deleted: %ld (status: %ld)",
           (long)count, (long)numDeleted, (long)status);
    CFReleaseNull(matches);
    CFReleaseNull(suffixString);

    if (status != errSecSuccess) {
        MacOSError::throwMe(status);
    }
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
    os_activity_t activity = os_activity_create("SecIdentityCopySystemIdentity", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	StLock<Mutex> _(systemIdentityLock());
	unique_ptr<Dictionary> identDict;

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
	attr.length     = (UInt32)CFDataGetLength(entryValue);
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
    os_activity_t activity = os_activity_create("SecIdentitySetSystemIdentity", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_IF_NONE_PRESENT);
    os_activity_scope(activity);
    os_release(activity);

	StLock<Mutex> _(systemIdentityLock());
	if(geteuid() != 0) {
		MacOSError::throwMe(errSecAuthFailed);
	}

	unique_ptr<MutableDictionary> identDict;
	MutableDictionary *d = MutableDictionary::CreateMutableDictionary(IDENTITY_DOMAIN, Dictionary::US_System);
	if (d)
	{
		identDict.reset(d);
	}
	else
	{
		if(idRef == NULL) {
			/* nothing there, nothing to set - done */
			return errSecSuccess;
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
		MacOSError::throwMe(errSecIO);
	}

    END_SECAPI
}

const CFStringRef kSecIdentityDomainDefault = CFSTR("com.apple.systemdefault");
const CFStringRef kSecIdentityDomainKerberosKDC = CFSTR("com.apple.kerberos.kdc");

