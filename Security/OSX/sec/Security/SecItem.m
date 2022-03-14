/*
 * Copyright (c) 2006-2014 Apple Inc. All Rights Reserved.
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

/* 
 * SecItem.m - CoreFoundation-based constants and functions for
    access to Security items (certificates, keys, identities, and
    passwords.)
 */

#include <AssertMacros.h>
#include <Security/SecBasePriv.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemInternal.h>
#include <Security/SecItemShim.h>
#include <Security/SecAccessControl.h>
#include <Security/SecAccessControlPriv.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecRandom.h>
#include <Security/SecBasePriv.h>
#include <Security/SecCTKKeyPriv.h>
#include <Security/SecTask.h>
#include <Security/SecPolicyInternal.h>
#include <errno.h>
#include <limits.h>
#include <sqlite3.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <Security/SecBase.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <CommonCrypto/CommonDigest.h>
#include <libkern/OSByteOrder.h>
#include <corecrypto/ccder.h>
#include <utilities/array_size.h>
#include <utilities/debugging.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecIOFormat.h>
#include <utilities/SecXPCError.h>
#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <utilities/simulatecrash_assert.h>
#include <libaks_acl_cf_keys.h>
#include <os/activity.h>
#include <pthread.h>
#include <os/lock.h>
#include <os/feature_private.h>

#include <Security/SecInternal.h>
#include "keychain/SecureObjectSync/SOSInternal.h"
#include <TargetConditionals.h>
#include <ipc/securityd_client.h>
#include <Security/SecuritydXPC.h>
#include <AssertMacros.h>
#include <asl.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <libDER/asn1Types.h>

#include <utilities/SecDb.h>
#include <IOKit/IOReturn.h>

#import <LocalAuthentication/LocalAuthentication_Private.h>
#import <CryptoTokenKit/CryptoTokenKit_Private.h>

#include "SecItemRateLimit.h"
#include "SecSoftLink.h"

#include <Security/OTConstants.h>

/*
 * See corresponding definition in SecDbKeychainItemV7. This is the unserialized
 * maximum, so the daemon's limit is not exactly the same.
 */
#define REASONABLE_DATA_SIZE 4096
#define UUID_PERSISTENT_REF_SIZE 20
#define PERSISTENT_REF_UUID_BYTES_LENGTH (sizeof(uuid_t))

const CFStringRef kSecNetworkExtensionAccessGroupSuffix = CFSTR("com.apple.networkextensionsharing");

/* Return an OSStatus for a sqlite3 error code. */
static OSStatus osstatus_for_s3e(int s3e)
{
	switch (s3e)
	{
        case SQLITE_OK:
        case SQLITE_DONE:
            return errSecSuccess;
        case SQLITE_READONLY:
            return errSecReadOnly;
        case SQLITE_CONSTRAINT:
            return errSecDuplicateItem;
        case SQLITE_ABORT:             // There is no errSecOperationCancelled
            return -1;
        case SQLITE_MISMATCH:
            return errSecNoSuchAttr;
        case SQLITE_NOMEM:
            return errSecAllocate;
        case SQLITE_IOERR:
            return errSecIO;
        case SQLITE_INTERNAL:
            return errSecInternalComponent;
        case SQLITE_FULL:              // Happens if we run out of uniqueids or disk is full (TODO: replace with better code)
        case SQLITE_PERM:              // No acess permission
        case SQLITE_AUTH:              // No authorization (e.g. no class key for file)
        case SQLITE_CANTOPEN:          // can be several reasons for this. Caller should sqlite3_system_errno()
        case SQLITE_EMPTY:             // SQLite does not seem to use this. Was already here, so keeping
        case SQLITE_ERROR:
        default:
            return errSecNotAvailable;
	}
}

static OSStatus osstatus_for_kern_return(CFIndex kernResult)
{
	switch (kernResult)
	{
        case KERN_SUCCESS:
            return errSecSuccess;
        case kIOReturnNotReadable:
        case kIOReturnNotWritable:
            return errSecAuthFailed;
        case kIOReturnNotPermitted:
        case kIOReturnNotPrivileged:
        case kIOReturnLockedRead:
        case kIOReturnLockedWrite:
            return errSecInteractionNotAllowed;
        case kIOReturnError:
            return errSecDecode;
        case kIOReturnBadArgument:
            return errSecParam;
        default:
            return errSecNotAvailable; /* TODO: Replace with a real error code. */
	}
}

static OSStatus osstatus_for_xpc_error(CFIndex xpcError) {
    switch (xpcError)
	{
        case kSecXPCErrorSuccess:
            return errSecSuccess;
        case kSecXPCErrorUnexpectedType:
        case kSecXPCErrorUnexpectedNull:
            return errSecParam;
        case kSecXPCErrorConnectionFailed:
            return errSecNotAvailable;
        case kSecXPCErrorUnknown:
        default:
            return errSecInternal;
    }
}

static OSStatus osstatus_for_der_error(CFIndex derError) {
    switch (derError)
	{
        case kSecDERErrorUnknownEncoding:
        case kSecDERErrorUnsupportedDERType:
        case kSecDERErrorUnsupportedNumberType:
            return errSecDecode;
        case kSecDERErrorUnsupportedCFObject:
            return errSecParam;
        case kSecDERErrorAllocationFailure:
            return errSecAllocate;
        default:
            return errSecInternal;
    }
}

static OSStatus osstatus_for_localauthentication_error(CFIndex laError) {
    // Wrap LA error in Sec error.
    switch (laError) {
        case LAErrorUserCancel:
            return errSecUserCanceled;
        case LAErrorParameter:
            return errSecParam;
        case LAErrorNotInteractive:
            return errSecInteractionNotAllowed;
        default:
            return errSecAuthFailed;
    }
}

static OSStatus osstatus_for_ctk_error(CFIndex ctkError) {
    switch (ctkError) {
        case TKErrorCodeBadParameter:
            return errSecParam;
        case TKErrorCodeNotImplemented:
            return errSecUnimplemented;
        case TKErrorCodeCanceledByUser:
            return errSecUserCanceled;
        case TKErrorCodeCorruptedData:
            return errSecDecode;
        case TKErrorCodeTokenNotFound:
        case TKErrorCodeObjectNotFound:
            return errSecItemNotFound;
        default:
            return errSecInternal;
    }
}


// Convert from securityd error codes to OSStatus for legacy API.
OSStatus SecErrorGetOSStatus(CFErrorRef error) {
    OSStatus status;
    if (error == NULL) {
        status = errSecSuccess;
    } else {
        CFStringRef domain = CFErrorGetDomain(error);
        if (domain == NULL) {
            secerror("No error domain for error: %@", error);
            status = errSecInternal;
        } else if (CFEqual(kSecErrorDomain, domain)) {
            status = (OSStatus)CFErrorGetCode(error);
        } else if (CFEqual(kSecDbErrorDomain, domain)) {
            status = osstatus_for_s3e((int)CFErrorGetCode(error));
        } else if (CFEqual(kSecErrnoDomain, domain)) {
            status = (OSStatus)CFErrorGetCode(error);
        } else if (CFEqual(kSecKernDomain, domain)) {
            status = osstatus_for_kern_return(CFErrorGetCode(error));
        } else if (CFEqual(sSecXPCErrorDomain, domain)) {
            status = osstatus_for_xpc_error(CFErrorGetCode(error));
        } else if (CFEqual(sSecDERErrorDomain, domain)) {
            status = osstatus_for_der_error(CFErrorGetCode(error));
        } else if (isLocalAuthenticationLAErrorDomainAvailable() && CFEqual((__bridge CFStringRef)getLAErrorDomain(), domain)) {
            status = osstatus_for_localauthentication_error(CFErrorGetCode(error));
        } else if (isCryptoTokenKitTKErrorDomainAvailable() && CFEqual((__bridge CFStringRef)getTKErrorDomain(), domain)) {
            status = osstatus_for_ctk_error(CFErrorGetCode(error));
        } else if (CFEqual(kSOSErrorDomain, domain)) {
            status = errSecInternal;
        } else {
            secnotice("securityd", "unknown error domain: %@ for error: %@", domain, error);
            status = errSecInternal;
        }
    }
    return status;
}

static void
lastErrorReleaseError(void *value)
{
    if (value)
        CFRelease(value);
}

static bool
getLastErrorKey(pthread_key_t *kv)
{
    static pthread_key_t key;
    static bool haveKey = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        if (pthread_key_create(&key, lastErrorReleaseError) == 0)
            haveKey = true;
    });
    *kv = key;
    return haveKey;
}

static void
SetLastError(CFErrorRef newError)
{
    pthread_key_t key;
    if (!getLastErrorKey(&key))
        return;
    CFErrorRef oldError = pthread_getspecific(key);
    if (oldError)
        CFRelease(oldError);
    if (newError)
        CFRetain(newError);
    pthread_setspecific(key, newError);
}

CFErrorRef
SecCopyLastError(OSStatus status)
{
    pthread_key_t key;
    CFErrorRef error;

    if (!getLastErrorKey(&key))
        return NULL;

    error = pthread_getspecific(key);
    if (error) {
        if (status && status != SecErrorGetOSStatus(error)) {
            error = NULL;
        } else {
            CFRetain(error);
        }
    }
    return error;
}

// Wrapper to provide a CFErrorRef for legacy API.
OSStatus SecOSStatusWith(bool (^perform)(CFErrorRef *error)) {
    CFErrorRef error = NULL;
    OSStatus status;
    if (perform(&error)) {
        assert(error == NULL);
        SetLastError(NULL);
        status = errSecSuccess;
    } else {
        assert(error);
        SetLastError(error);
        status = SecErrorGetOSStatus(error);
        if (status != errSecItemNotFound)           // Occurs in normal operation, so exclude
            secinfo("OSStatus", "error:[%" PRIdOSStatus "] %@", status, error);
        CFReleaseNull(error);
    }
    return status;
}

static void
logUnreasonableDataLength(CFDictionaryRef attributes)
{
    CFDataRef data;
    CFIndex length;

    if (isDictionary(attributes)) {
        data = CFDictionaryGetValue(attributes, kSecValueData);
        if (isData(data)) {
            length = CFDataGetLength(data);
            if (length > REASONABLE_DATA_SIZE) {
                // This log message is vague, as we may not know anything else about the item.
                // securityd logging (correlate by activity ID) will have more information.
                secwarning("keychain item data exceeds reasonable size (%lu bytes)", (unsigned long)length);
            }
        }
    }
}

/* Drop assorted kSecAttrCanXxxx attributes from the query, because these attributes are generated
   by SecKey implementation and may differ between OS versions, see <rdar://problem/27095761>.
 */

static CFDictionaryRef
AttributeCreateFilteredOutSecAttrs(CFDictionaryRef attributes)
{
    CFMutableDictionaryRef filtered = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, attributes);
    if (filtered == NULL)
        return NULL;
    CFDictionaryRemoveValue(filtered, kSecAttrCanSign);
    CFDictionaryRemoveValue(filtered, kSecAttrCanVerify);
    CFDictionaryRemoveValue(filtered, kSecAttrCanEncrypt);
    CFDictionaryRemoveValue(filtered, kSecAttrCanDecrypt);
    CFDictionaryRemoveValue(filtered, kSecAttrCanDerive);
    CFDictionaryRemoveValue(filtered, kSecAttrCanWrap);
    CFDictionaryRemoveValue(filtered, kSecAttrCanUnwrap);
    CFDictionaryRemoveValue(filtered, kSecAttrCanSignRecover);
    CFDictionaryRemoveValue(filtered, kSecAttrCanVerifyRecover);
    CFDictionaryRemoveValue(filtered, kSecAttrIsPermanent);

    return filtered;
}


/* IPC uses CFPropertyList to un/marshall input/output data and can handle:
   CFData, CFString, CFArray, CFDictionary, CFDate, CFBoolean, and CFNumber

   Currently in need of conversion below:
   @@@ kSecValueRef allows SecKeychainItemRef and SecIdentityRef
   @@@ kSecUseItemList allows a query against a list of itemrefs, this isn't
       currently implemented at all, but when it is needs to short circuit to
	   local evaluation, different from the sql query abilities
*/

static CFDictionaryRef
SecItemCopyAttributeDictionary(CFTypeRef ref, bool forQuery) {
	CFDictionaryRef refDictionary = NULL;
	CFTypeID typeID = CFGetTypeID(ref);
	if (typeID == SecKeyGetTypeID()) {
		refDictionary = SecKeyCopyAttributeDictionary((SecKeyRef)ref);
        if (refDictionary && forQuery) {
            CFDictionaryRef filtered = AttributeCreateFilteredOutSecAttrs(refDictionary);
            CFAssignRetained(refDictionary, filtered);
        }
	} else if (typeID == SecCertificateGetTypeID()) {
		refDictionary = SecCertificateCopyAttributeDictionary((SecCertificateRef)ref);
	} else if (typeID == SecIdentityGetTypeID()) {
        assert(false);
        SecIdentityRef identity = (SecIdentityRef)ref;
        SecCertificateRef cert = NULL;
        SecKeyRef key = NULL;
        if (!SecIdentityCopyCertificate(identity, &cert) &&
            !SecIdentityCopyPrivateKey(identity, &key)) 
        {
            CFDataRef data = SecCertificateCopyData(cert);
            CFDictionaryRef key_dict = SecKeyCopyAttributeDictionary(key);
            
            if (key_dict && data) {
                refDictionary = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, key_dict);
                CFDictionarySetValue((CFMutableDictionaryRef)refDictionary, kSecAttrIdentityCertificateData, data);
            }
            CFReleaseNull(key_dict);
            CFReleaseNull(data);
        }
        CFReleaseNull(cert);
        CFReleaseNull(key);
	}
	return refDictionary;
}

#ifdef SECITEM_SHIM_OSX
extern CFTypeRef SecItemCreateFromAttributeDictionary_osx(CFDictionaryRef refAttributes);
#endif

static CFTypeRef
SecItemCreateFromAttributeDictionary(CFDictionaryRef refAttributes) {
	CFTypeRef ref = NULL;
	CFStringRef class = CFDictionaryGetValue(refAttributes, kSecClass);
    if (CFEqual(class, kSecClassKey)) {
        ref = SecKeyCreateFromAttributeDictionary(refAttributes);
    } else if (CFEqual(class, kSecClassCertificate)) {
		ref = SecCertificateCreateFromAttributeDictionary(refAttributes);
	} else if (CFEqual(class, kSecClassIdentity)) {
		CFDataRef data = CFDictionaryGetValue(refAttributes, kSecAttrIdentityCertificateData);
		SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, data);
        SecKeyRef key = SecKeyCreateFromAttributeDictionary(refAttributes);
        if (key && cert) {
            ref = SecIdentityCreate(kCFAllocatorDefault, cert, key);
        }
        else {
            secerror("SecItem: failed to create identity");
        }

        CFReleaseSafe(key);
		CFReleaseSafe(cert);
#ifdef SECITEM_SHIM_OSX
	} else {
        ref = SecItemCreateFromAttributeDictionary_osx(refAttributes);
#endif
	}
	return ref;
}

typedef OSStatus (*secitem_operation)(CFDictionaryRef attributes, CFTypeRef *result);

static bool explode_identity(CFDictionaryRef attributes, secitem_operation operation, 
    OSStatus *return_status, CFTypeRef *return_result)
{
    bool handled = false;
	CFTypeRef value = CFDictionaryGetValue(attributes, kSecValueRef);
    if (value) {
        CFTypeID typeID = CFGetTypeID(value);
        if (typeID == SecIdentityGetTypeID()) {
            handled = true;
            OSStatus status = errSecSuccess;
            SecIdentityRef identity = (SecIdentityRef)value;
            SecCertificateRef cert = NULL;
            SecKeyRef key = NULL;
            if (!SecIdentityCopyCertificate(identity, &cert) &&
                !SecIdentityCopyPrivateKey(identity, &key)) 
            {
                CFMutableDictionaryRef partial_query =
                    CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, attributes);
                CFDictionarySetValue(partial_query, kSecValueRef, cert);
                CFTypeRef result = NULL;
                bool duplicate_cert = false;
                /* an identity is first and foremost a key, but it can have multiple
                   certs associated with it: so we identify it by the cert */
                status = operation(partial_query, return_result ? &result : NULL);
                if ((operation == (secitem_operation)SecItemAdd) &&
                    (status == errSecDuplicateItem)) {
                        duplicate_cert = true;
                        status = errSecSuccess;
                }

                if (!status || status == errSecItemNotFound) {
					bool skip_key_operation = false;
	
					/* if the key is still in use, skip deleting it */
					if (operation == (secitem_operation)SecItemDelete) {
						// find certs with cert.pkhh == keys.klbl
						CFDictionaryRef key_dict = NULL, query_dict = NULL;
						CFDataRef pkhh = NULL;
						
						key_dict = SecKeyCopyAttributeDictionary(key);
						if (key_dict)
							pkhh = (CFDataRef)CFDictionaryGetValue(key_dict, kSecAttrApplicationLabel);
						const void *keys[] = { kSecClass, kSecAttrPublicKeyHash };
						const void *vals[] = { kSecClassCertificate, pkhh };
						if (pkhh)
							query_dict = CFDictionaryCreate(NULL, keys, 
					            vals, (array_size(keys)),
								NULL, NULL);
						if (query_dict)
							if (errSecSuccess == SecItemCopyMatching(query_dict, NULL))
								skip_key_operation = true;
						CFReleaseSafe(query_dict);
						CFReleaseSafe(key_dict);
					}
					
					if (!skip_key_operation) {
	                    /* now perform the operation for the key */
	                    CFDictionarySetValue(partial_query, kSecValueRef, key);
	                    CFDictionarySetValue(partial_query, kSecReturnPersistentRef, kCFBooleanFalse);

	                    status = operation(partial_query, NULL);
	                    if ((operation == (secitem_operation)SecItemAdd) &&
	                        (status == errSecDuplicateItem) &&
	                        !duplicate_cert)
	                            status = errSecSuccess;
					}
					
                    /* add and copy matching for an identityref have a persistent ref result */
                    if (result) {
                        if (!status) {
                            /* result is a persistent ref to a cert */
                            sqlite_int64 rowid = -1;
                            CFDataRef uuidData = NULL;
                            CFDictionaryRef tokenAttrs = NULL;
                            if (_SecItemParsePersistentRef(result, NULL, &rowid, &uuidData, &tokenAttrs)) {
                                if (SecKeychainIsStaticPersistentRefsEnabled() && uuidData && CFDataGetLength(uuidData) == PERSISTENT_REF_UUID_BYTES_LENGTH) {
                                    *return_result = _SecItemCreateUUIDBasedPersistentRef(kSecClassIdentity, uuidData, tokenAttrs);
                                } else {
                                    if (SecKeychainIsStaticPersistentRefsEnabled()) {
                                        secnotice("pref", "explode_identity: Creating old persistent ref for %llu", rowid);
                                    }
                                    *return_result = _SecItemCreatePersistentRef(kSecClassIdentity, rowid, tokenAttrs);
                                }
                            }
                            CFReleaseNull(uuidData);
                            CFReleaseNull(tokenAttrs);
                        }
                        CFReleaseNull(result);
                    }
                }
                CFReleaseNull(partial_query);
            }
            else
                status = errSecInvalidItemRef;
            
            CFReleaseNull(cert);
            CFReleaseNull(key);
            *return_status = status;
        }
    } else {
		value = CFDictionaryGetValue(attributes, kSecClass);
		if (value && CFEqual(kSecClassIdentity, value) && 
			(operation == (secitem_operation)SecItemDelete)) {
			CFMutableDictionaryRef dict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, attributes);
			CFDictionaryRemoveValue(dict, kSecClass);
			CFDictionarySetValue(dict, kSecClass, kSecClassCertificate);
			OSStatus status = SecItemDelete(dict);
			if (!status) {
				CFDictionarySetValue(dict, kSecClass, kSecClassKey);
				status = SecItemDelete(dict);
			}
			CFRelease(dict);
			*return_status = status;
            handled = true;
		}
	}
    return handled;
}

static bool
SecErrorPropagateLastError(OSStatus status, CFErrorRef *error)
{
    if (status) {
        CFErrorRef lastError = SecCopyLastError(status);
        if (lastError)
            CFErrorPropagate(lastError, error);
        else
            SecError(status, error, CFSTR("SecError: error not captured, OSStatus was: %d"), (int)status);
        return false;
    }
    return true;
}

static bool
handleUpdateIdentity(CFDictionaryRef query,
                     CFDictionaryRef update,
                     bool *result,
                     CFErrorRef *error)
{
    CFMutableDictionaryRef updatedQuery = NULL;
    SecCertificateRef cert = NULL;
    SecKeyRef key = NULL;
    bool handled = false;

    *result = false;

    CFTypeRef value = CFDictionaryGetValue(query, kSecValueRef);
    if (value) {
        CFTypeID typeID = CFGetTypeID(value);
        if (typeID == SecIdentityGetTypeID()) {
            SecIdentityRef identity = (SecIdentityRef)value;
            OSStatus status;

            handled = true;

            status = SecIdentityCopyCertificate(identity, &cert);
            require_noerr_action_quiet(status, errOut, SecErrorPropagateLastError(status, error));

            status = SecIdentityCopyPrivateKey(identity, &key);
            require_noerr_action_quiet(status, errOut, SecErrorPropagateLastError(status, error));

            updatedQuery = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, query);
            require_action_quiet(updatedQuery, errOut, *result = false);

            CFDictionarySetValue(updatedQuery, kSecValueRef, cert);
            require_quiet(SecItemUpdateWithError(updatedQuery, update, error), errOut);

            CFDictionarySetValue(updatedQuery, kSecValueRef, key);
            require_quiet(SecItemUpdateWithError(updatedQuery, update, error), errOut);

        }
    } else {
        value = CFDictionaryGetValue(query, kSecClass);
        if (value && CFEqual(kSecClassIdentity, value)) {
            handled = true;

            updatedQuery = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, query);
            require_action_quiet(updatedQuery, errOut, *result = false);

            CFDictionarySetValue(updatedQuery, kSecClass, kSecClassCertificate);
            require_quiet(SecItemUpdateWithError(updatedQuery, update, error), errOut);

            CFDictionarySetValue(updatedQuery, kSecClass, kSecClassKey);
            require_quiet(SecItemUpdateWithError(updatedQuery, update, error), errOut);

            CFReleaseNull(updatedQuery);
        }
    }
    *result = true;
errOut:
    CFReleaseNull(updatedQuery);
    CFReleaseNull(cert);
    CFReleaseNull(key);
    return handled;
}

static void infer_cert_label(SecCFDictionaryCOW *attributes)
{
    if (!CFDictionaryContainsKey(attributes->dictionary, kSecAttrLabel)) {
        CFTypeRef value_ref = CFDictionaryGetValue(attributes->dictionary, kSecValueRef);
        if (value_ref && CFGetTypeID(value_ref) == SecCertificateGetTypeID()) {
            SecCertificateRef certificate = (SecCertificateRef)value_ref;
            CFStringRef label = SecCertificateCopySubjectSummary(certificate);
            if (label) {
                CFDictionarySetValue(SecCFDictionaryCOWGetMutable(attributes), kSecAttrLabel, label);
                CFReleaseNull(label);
            }
        }
    }
}

static CFDataRef CreateTokenPersistentRefData(CFTypeRef class, CFDictionaryRef attributes) {
    CFDataRef tokenPersistentRef = NULL;
    CFStringRef tokenID = NULL;
    CFDataRef tokenData = NULL;
    CFDataRef oid = NULL;
    CFDictionaryRef itemValue = NULL;

    oid = CFDictionaryGetValue(attributes, kSecAttrTokenOID);
    if (oid != NULL) {
        require_quiet(tokenID = CFCast(CFString, CFDictionaryGetValue(attributes, kSecAttrTokenID)), out);
    } else {
        // Identities are identified by their contained certificate, so we need to get tokenID and OID from certificate,
        // not from the key.
        if (CFEqual(class, kSecClassIdentity) && oid == NULL) {
            require_quiet(tokenID = CFCast(CFString, CFDictionaryGetValue(attributes, kSecAttrIdentityCertificateTokenID)), out);
            require_quiet(tokenData = CFCast(CFData, CFDictionaryGetValue(attributes, kSecAttrIdentityCertificateData)), out);
        } else {
            require_quiet(tokenID = CFCast(CFString, CFDictionaryGetValue(attributes, kSecAttrTokenID)), out);
            require_quiet(tokenData = CFCast(CFData, CFDictionaryGetValue(attributes, kSecValueData)), out);
        }
        require_quiet(itemValue = SecTokenItemValueCopy(tokenData, NULL), out);
        require_quiet(oid = CFDictionaryGetValue(itemValue, kSecTokenValueObjectIDKey), out);
        require_quiet(CFCast(CFData, oid) != NULL, out);
    }
    CFArrayRef array = CFArrayCreateForCFTypes(kCFAllocatorDefault, class, tokenID, oid, NULL);
    tokenPersistentRef = CFPropertyListCreateDERData(kCFAllocatorDefault, array, NULL);
    CFRelease(array);
out:
    CFReleaseNull(itemValue);
    return tokenPersistentRef;
}

static const uint8_t tk_persistent_ref_id[] = {'t', 'k', 'p', 'r'};
/* A persistent ref is just the class and the rowid of the record.
   Persistent ref for token items is a der blob with class, tokenID and objectId. */
CFDataRef _SecItemCreatePersistentRef(CFTypeRef class, sqlite_int64 rowid, CFDictionaryRef attributes)
{
    CFDataRef result = NULL;
    CFDataRef tokenPersistentRef = NULL;
    if (attributes != NULL) {
        tokenPersistentRef = CreateTokenPersistentRefData(class, attributes);
    }

    if (tokenPersistentRef != NULL) {
        CFMutableDataRef tmpData = CFDataCreateMutable(kCFAllocatorDefault, sizeof(tk_persistent_ref_id) + CFDataGetLength(tokenPersistentRef));
        CFDataAppendBytes(tmpData, tk_persistent_ref_id, sizeof(tk_persistent_ref_id));
        CFDataAppend(tmpData, tokenPersistentRef);
        CFReleaseNull(tokenPersistentRef);
        result = tmpData;
    } else {
        require(rowid >= 0, out);
        uint8_t bytes[sizeof(sqlite_int64) + 4];
        if (CFStringGetCString(class, (char *)bytes, 4 + 1 /*null-term*/,
            kCFStringEncodingUTF8))
        {
            OSWriteBigInt64(bytes + 4, 0, rowid);
            result = CFDataCreate(NULL, bytes, sizeof(bytes));
        }
    }
out:
    return result;
}

/* A persistent ref is just the class and a persistent ref UUID of the record.
   Persistent ref for token items is a der blob with class, tokenID and objectId. */
CFDataRef _SecItemCreateUUIDBasedPersistentRef(CFTypeRef class, CFDataRef uuidData, CFDictionaryRef attributes)
{
    CFDataRef result = NULL;
    CFDataRef tokenPersistentRef = NULL;
    if (attributes != NULL) {
        tokenPersistentRef = CreateTokenPersistentRefData(class, attributes);
    }

    if (tokenPersistentRef != NULL) {
        CFMutableDataRef tmpData = CFDataCreateMutable(kCFAllocatorDefault, sizeof(tk_persistent_ref_id) + CFDataGetLength(tokenPersistentRef));
        CFDataAppendBytes(tmpData, tk_persistent_ref_id, sizeof(tk_persistent_ref_id));
        CFDataAppend(tmpData, tokenPersistentRef);
        CFReleaseNull(tokenPersistentRef);
        result = tmpData;
    } else {
        require(uuidData != NULL && CFDataGetLength(uuidData) == PERSISTENT_REF_UUID_BYTES_LENGTH, out);
        require(class != NULL && CFGetTypeID(class) == CFStringGetTypeID() && CFStringGetLength(class) > 0, out);

        CFMutableDataRef mutableResult = CFDataCreateMutable(kCFAllocatorDefault, 0);
        if( CFGetTypeID(class) == CFStringGetTypeID()) {
            CFStringRef classString = (CFStringRef)class;
            CFStringPerformWithUTF8CFData(classString, ^(CFDataRef stringAsData) {
                CFDataAppend(mutableResult, stringAsData);
            });
            
            CFDataAppend(mutableResult, uuidData);
            result = CFDataCreateCopy(kCFAllocatorDefault, mutableResult);
        }
       
        CFReleaseNull(mutableResult);
    }
out:
    return result;
}

static Boolean isValidClass(CFStringRef class, CFStringRef *return_class) {
    const void *valid_classes[] = { kSecClassGenericPassword,
        kSecClassInternetPassword,
        kSecClassAppleSharePassword,
        kSecClassCertificate,
        kSecClassKey,
        kSecClassIdentity };

    for (size_t i = 0; i < array_size(valid_classes); i++) {
        if (class && CFEqualSafe(valid_classes[i], class)) {
            if (return_class)
                *return_class = valid_classes[i];
            return true;
        }
    }
    
    return false;
}

static bool ParseTokenPersistentRefData(CFDataRef persistent_ref, CFStringRef *return_class, CFDictionaryRef *return_token_attrs) {
    bool valid_ref = false;
    CFPropertyListRef pl = NULL;
    const uint8_t *der = CFDataGetBytePtr(persistent_ref) + sizeof(tk_persistent_ref_id);
    const uint8_t *der_end = der + (CFDataGetLength(persistent_ref) - sizeof(tk_persistent_ref_id));
    require_quiet(der = der_decode_plist(0, &pl, NULL, der, der_end), out);
    require_quiet(der == der_end, out);
    require_quiet(CFGetTypeID(pl) == CFArrayGetTypeID(), out);
    require_quiet(CFArrayGetCount(pl) == 3, out);
    require_quiet(valid_ref = isValidClass(CFArrayGetValueAtIndex(pl, 0), return_class), out);
    if (return_token_attrs) {
            *return_token_attrs = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                               kSecAttrTokenID, CFArrayGetValueAtIndex(pl, 1),
                                                               kSecAttrTokenOID, CFArrayGetValueAtIndex(pl, 2), NULL);
    }
out:
    CFReleaseNull(pl);
    return valid_ref;
}

/* AUDIT[securityd](done):
   persistent_ref (ok) is a caller provided, non NULL CFTypeRef.
 */
bool _SecItemParsePersistentRef(CFDataRef persistent_ref, CFStringRef *return_class, sqlite_int64 *return_rowid, CFDataRef *return_uuid, CFDictionaryRef *return_token_attrs)
{
	bool valid_ref = false;
    require(CFGetTypeID(persistent_ref) == CFDataGetTypeID(), out);
    
    if (CFDataGetLength(persistent_ref) > (CFIndex)sizeof(tk_persistent_ref_id) &&
        memcmp(tk_persistent_ref_id, CFDataGetBytePtr(persistent_ref), sizeof(tk_persistent_ref_id)) == 0) {
        valid_ref = ParseTokenPersistentRefData(persistent_ref, return_class, return_token_attrs);
    } else if (CFDataGetLength(persistent_ref) == (CFIndex)(sizeof(sqlite_int64) + 4)) {
        const uint8_t *bytes = CFDataGetBytePtr(persistent_ref);
        sqlite_int64 rowid = OSReadBigInt64(bytes + 4, 0);
		
        CFStringRef class = CFStringCreateWithBytes(kCFAllocatorDefault, 
            bytes, CFStringGetLength(kSecClassGenericPassword), 
            kCFStringEncodingUTF8, true);
        
        require(class, out);

        if ((valid_ref = isValidClass(class, return_class))) {
            if (return_rowid)
                *return_rowid = rowid;
        }
        CFReleaseNull(class);
    } else if (SecKeychainIsStaticPersistentRefsEnabled() && CFDataGetLength(persistent_ref) == UUID_PERSISTENT_REF_SIZE){
        const uint8_t *bytes = CFDataGetBytePtr(persistent_ref);
        CFStringRef class = CFStringCreateWithBytes(kCFAllocatorDefault,
            bytes, CFStringGetLength(kSecClassGenericPassword),
            kCFStringEncodingUTF8, true);

        require(class, out);
        
        if ((valid_ref = isValidClass(class, return_class)) && return_uuid) {
            CFIndex classLength = CFStringGetLength(class);
            CFIndex uuidLength = CFDataGetLength(persistent_ref) - CFStringGetLength(class);
            CFDataRef uuid = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, bytes + classLength, uuidLength, kCFAllocatorNull);
          
            CFRetainAssign(*return_uuid, uuid);
            CFReleaseNull(uuid);
        }
        CFReleaseNull(class);
    }
out:
    return valid_ref;
}

static bool cf_bool_value(CFTypeRef cf_bool) {
	return cf_bool && CFBooleanGetValue(cf_bool);
}

CFMutableDictionaryRef SecCFDictionaryCOWGetMutable(SecCFDictionaryCOW *cow_dictionary) {
    if (cow_dictionary->mutable_dictionary == NULL) {
        cow_dictionary->mutable_dictionary = CFDictionaryCreateMutableForCFTypes(NULL);
        if (cow_dictionary->dictionary != NULL) {
            CFDictionaryForEach(cow_dictionary->dictionary, ^(const void *key, const void *value) {
                CFDictionarySetValue(cow_dictionary->mutable_dictionary, key, value);
            });
        }
        cow_dictionary->dictionary = cow_dictionary->mutable_dictionary;
    }

    return cow_dictionary->mutable_dictionary;
}

// Creates kSecValueData field stored in the DB for token-based items.  Data field consists of objectID, real
// access_control and optionally of the data value.
static CFDataRef SecTokenItemValueCreate(CFDataRef oid, CFDataRef access_control, CFDataRef object_value, CFErrorRef *error) {
    CFMutableDictionaryRef value = NULL;
    value = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                    kSecTokenValueObjectIDKey, oid,
                                                    kSecTokenValueAccessControlKey, access_control,
                                                    NULL);
    if (object_value != NULL) {
        CFDictionarySetValue(value, kSecTokenValueDataKey, object_value);
    }

    CFDataRef value_data = CFPropertyListCreateDERData(NULL, value, error);
    CFRelease(value);
    return value_data;
}

CFDictionaryRef SecTokenItemValueCopy(CFDataRef db_value, CFErrorRef *error) {
    CFPropertyListRef plist = NULL, result = NULL;
    require_quiet(CFCastWithError(CFData, db_value, error), out);
    const uint8_t *der = CFDataGetBytePtr(db_value);
    const uint8_t *der_end = der + CFDataGetLength(db_value);
    require_quiet(der = der_decode_plist(0, &plist, error, der, der_end), out);
    require_action_quiet(der == der_end, out, SecError(errSecDecode, error, CFSTR("trailing garbage at end of token data field")));
    CFTypeRef value = CFDictionaryGetValue(plist, kSecTokenValueObjectIDKey);
    require_action_quiet(CFCast(CFData, value) != NULL, out,
                         SecError(errSecInternal, error, CFSTR("token based item data does not have OID")));
    value = CFDictionaryGetValue(plist, kSecTokenValueAccessControlKey);
    require_quiet(value == NULL || CFCastWithError(CFData, value, error), out);
    value = CFDictionaryGetValue(plist, kSecTokenValueDataKey);
    require_quiet(value == NULL || CFCastWithError(CFData, value, error), out);
    result = CFRetainSafe(plist);
out:
    CFReleaseNull(plist);
    return result;
}

static TKClientTokenSession *SecTokenSessionCreate(CFStringRef token_id, SecCFDictionaryCOW *auth_params, CFErrorRef *error) {
    static CFMutableDictionaryRef sharedLAContexts = NULL;
    static dispatch_once_t onceToken;
    static os_unfair_lock lock = OS_UNFAIR_LOCK_INIT;
    if ((auth_params->dictionary == NULL || CFDictionaryGetValue(auth_params->dictionary, kSecUseCredentialReference) == NULL) && !CFStringHasPrefix(token_id, kSecAttrTokenIDSecureEnclave)) {
        dispatch_once(&onceToken, ^{
            sharedLAContexts = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        });

        os_unfair_lock_lock(&lock);
        CFTypeRef ctx = CFDictionaryGetValue(sharedLAContexts, token_id);
        if (ctx == nil) {
            if (isLocalAuthenticationAvailable()) {
                ctx = CFBridgingRetain([[getLAContextClass() alloc] init]);
                if (!ctx) {
                    os_unfair_lock_unlock(&lock);
                    SecError(errSecInternal, error, CFSTR("Failed to create authentication context"));
                    return nil;
                }
            } else {
                SecError(errSecInternal, error, CFSTR("LocalAuthentication is not available"));
                return nil;
            }
            CFDictionarySetValue(sharedLAContexts, token_id, ctx);
            CFRelease(ctx);
            ctx = CFDictionaryGetValue(sharedLAContexts, token_id);
        }

        CFDataRef credRef = NULL;
        if (ctx != nil) {
            credRef = CFBridgingRetain([(__bridge LAContext *)ctx externalizedContext]);
        }

        if (credRef) {
            CFDictionarySetValue(SecCFDictionaryCOWGetMutable(auth_params), kSecUseAuthenticationContext, ctx);
            CFDictionarySetValue(SecCFDictionaryCOWGetMutable(auth_params), kSecUseCredentialReference, credRef);
            CFRelease(credRef);
        }
        os_unfair_lock_unlock(&lock);
    }
    
    LAContext *authContext = [(__bridge NSDictionary *)auth_params->dictionary objectForKey:(id)kSecUseAuthenticationContext];
    if (authContext == nil) {
        NSData *credRef = [(__bridge NSDictionary *)auth_params->dictionary objectForKey:(id)kSecUseCredentialReference];
        if (credRef != nil) {
            if (isLocalAuthenticationAvailable()) {
                authContext = [[getLAContextClass() alloc] initWithExternalizedContext:credRef];
            } else {
                SecError(errSecInternal, error, CFSTR("LocalAuthentication is not available"));
                return nil;
            }
        }
    }

    NSError *err;
    TKClientTokenSession *session;
    if (isCryptoTokenKitAvailable()) {
        TKClientToken *token = [[getTKClientTokenClass() alloc] initWithTokenID:(__bridge NSString *)token_id];
        session = [token sessionWithLAContext:authContext error:&err];
    } else {
        err = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:@{NSDebugDescriptionErrorKey: @"CryptoTokenKit is not available"}];
    }
    if (session == nil && error != NULL) {
        *error = (CFErrorRef)CFBridgingRetain(err);
    }
    return session;
}

static bool SecTokenItemCreateFromAttributes(CFDictionaryRef attributes, CFDictionaryRef auth_params_dict,
                                             TKClientTokenSession *tokenSession, CFDataRef object_id, CFTypeRef *ref, CFErrorRef *error) {
    bool ok = false;
    SecCFDictionaryCOW auth_params = { auth_params_dict };
    CFMutableDictionaryRef attrs = CFDictionaryCreateMutableCopy(NULL, 0, attributes);
    CFTypeRef token_id = CFDictionaryGetValue(attributes, kSecAttrTokenID);
    if (token_id != NULL && object_id != NULL) {
        require_quiet(CFCastWithError(CFString, token_id, error), out);
        if (tokenSession == nil) {
            require_quiet(tokenSession = SecTokenSessionCreate(token_id, &auth_params, error), out);
        }

        if (auth_params.dictionary != NULL) {
            CFDictionaryForEach(auth_params.dictionary, ^(const void *key, const void *value) {
                CFDictionaryAddValue(attrs, key, value);
            });
        }
        CFDictionarySetValue(attrs, kSecUseTokenSession, (__bridge CFTypeRef)tokenSession);
        CFDictionarySetValue(attrs, kSecAttrTokenOID, object_id);
    }
    *ref = SecItemCreateFromAttributeDictionary(attrs);
    ok = true;

out:
    CFReleaseSafe(attrs);
    CFReleaseSafe(auth_params.mutable_dictionary);
    return ok;
}


/* Turn the returned single value or dictionary that contains all the attributes to create a
 ref into the exact result the client asked for */
static bool SecItemResultCopyPrepared(CFTypeRef raw_result, TKClientTokenSession* tokenSession,
                                      CFDictionaryRef query, CFDictionaryRef auth_params_dict,
                                      CFTypeRef *result, CFErrorRef *error) {
    bool ok = false;
    CFDataRef ac_data = NULL;
    CFDataRef value = NULL;
    CFTypeRef persistent_ref = NULL;
    CFStringRef token_id = NULL;
    CFStringRef cert_token_id = NULL;
    CFDataRef object_id = NULL;
    CFMutableDictionaryRef attrs = NULL;
    CFDataRef cert_data = NULL;
    CFDataRef cert_object_id = NULL;
    SecCFDictionaryCOW auth_params = { auth_params_dict };
    CFDataRef object_value = NULL;
    CFDictionaryRef parsed_value = NULL;
    TKClientTokenSession *certTokenSession;

    bool wants_ref = cf_bool_value(CFDictionaryGetValue(query, kSecReturnRef));
    bool wants_data = cf_bool_value(CFDictionaryGetValue(query, kSecReturnData));
    bool wants_attributes = cf_bool_value(CFDictionaryGetValue(query, kSecReturnAttributes));
    bool wants_persistent_ref = cf_bool_value(CFDictionaryGetValue(query, kSecReturnPersistentRef));

    // Get token value if not provided by the caller.
    bool token_item = false;
    bool cert_token_item = false;
    if (tokenSession == nil) {
        if (CFGetTypeID(raw_result) == CFDictionaryGetTypeID()) {
            token_id = CFDictionaryGetValue(raw_result, kSecAttrTokenID);
            require_quiet(token_id == NULL || CFCastWithError(CFString, token_id, error) != NULL, out);
            token_item = (token_id != NULL);

            cert_token_id = CFDictionaryGetValue(raw_result, kSecAttrIdentityCertificateTokenID);
            require_quiet(cert_token_id == NULL || CFCastWithError(CFString, cert_token_id, error) != NULL, out);
            cert_token_item = (cert_token_id != NULL);
        }
    } else {
        token_item = true;
        cert_token_item = true;
        certTokenSession = tokenSession;
    }

    if ((token_item || cert_token_item) && cf_bool_value(CFDictionaryGetValue(query, kSecUseTokenRawItems))) {
        token_item = false;
        cert_token_item = false;
    }

    // Decode and prepare data value, if it is requested at the output, or if we want attributes from token.
    if (wants_data || wants_ref || (token_item && wants_attributes)) {
        if (CFGetTypeID(raw_result) == CFDictionaryGetTypeID())
            value = CFRetainSafe(CFDictionaryGetValue(raw_result, kSecValueData));
        else
            value = CFRetainSafe(raw_result);
        if (token_item && value != NULL) {
            // Parse token-based item's data field.
            require_quiet(parsed_value = SecTokenItemValueCopy(value, error), out);
            object_id = CFRetainSafe(CFDictionaryGetValue(parsed_value, kSecTokenValueObjectIDKey));
            require_quiet(object_id == NULL || CFCastWithError(CFData, object_id, error) != NULL, out);
            ac_data = CFRetainSafe(CFDictionaryGetValue(parsed_value, kSecTokenValueAccessControlKey));
            require_quiet(ac_data == NULL || CFCastWithError(CFData, ac_data, error) != NULL, out);
            object_value = CFRetainSafe(CFDictionaryGetValue(parsed_value, kSecTokenValueDataKey));
            require_quiet(object_value == NULL || CFCastWithError(CFData, object_value, error) != NULL, out);
            if ((wants_data || wants_ref) && object_value == NULL) {
                // Retrieve value directly from the token.
                if (tokenSession == nil) {
                    require_quiet(tokenSession = SecTokenSessionCreate(token_id, &auth_params, error), out);
                }
                NSError *err;
                TKClientTokenObject *tokenObject = [tokenSession objectForObjectID:(__bridge NSData *)object_id error:&err];
                if (tokenObject == nil && error != NULL) {
                    *error = (CFErrorRef)CFBridgingRetain(err);
                }
                require_quiet(tokenObject != nil, out);
                object_value = CFBridgingRetain(tokenObject.value);
            }
            CFRetainAssign(value, object_value);
        }

        // If only thing requested is data, return them directly.
        if (!(wants_attributes || wants_persistent_ref || wants_ref)) {
            *result = CFRetainSafe(value);
            ok = true;
            goto out;
        }
    }

    // Extract persistent_ref, if caller wants it.
    if (wants_persistent_ref) {
        if (CFGetTypeID(raw_result) == CFDictionaryGetTypeID())
            persistent_ref = CFRetainSafe(CFDictionaryGetValue(raw_result, kSecValuePersistentRef));
        else
            persistent_ref = CFRetainSafe(raw_result);

        // If only thing requested is persistentref, extract it from dictionary if needed and return it.
        if (!(wants_attributes || wants_data || wants_ref)) {
            *result = CFRetainSafe(persistent_ref);
            ok = true;
            goto out;
        }
    }

    if (!wants_ref && !wants_attributes && (!wants_data || !wants_persistent_ref)) {
        *result = NULL;
        ok = true;
        goto out;
    }

    // For other cases we need an output dictionary.
    if (CFGetTypeID(raw_result) == CFDictionaryGetTypeID())
        *result = CFDictionaryCreateMutableCopy(NULL, 0, raw_result);
    else
        *result = CFDictionaryCreateForCFTypes(NULL, NULL);
    CFMutableDictionaryRef output = (CFMutableDictionaryRef)*result;

    if ((wants_data || wants_ref) && value != NULL)
        CFDictionarySetValue(output, kSecValueData, value);
    else
        CFDictionaryRemoveValue(output, kSecValueData);

    if (wants_persistent_ref && persistent_ref != NULL)
        CFDictionarySetValue(output, kSecValuePersistentRef, persistent_ref);
    else
        CFDictionaryRemoveValue(output, kSecValuePersistentRef);

    if ((wants_ref || wants_attributes) && cert_token_item &&
        CFEqualSafe(CFDictionaryGetValue(output, kSecClass), kSecClassIdentity)) {
        // Decode also certdata field of the identity.
        CFDataRef data = CFDictionaryGetValue(output, kSecAttrIdentityCertificateData);
        if (data != NULL) {
            require_quiet(CFAssignRetained(parsed_value, SecTokenItemValueCopy(data, error)), out);
            cert_data = CFRetainSafe(CFDictionaryGetValue(parsed_value, kSecTokenValueDataKey));
            require_quiet(cert_data == NULL || CFCastWithError(CFData, cert_data, error) != NULL, out);
            cert_object_id = CFRetainSafe(CFDictionaryGetValue(parsed_value, kSecTokenValueObjectIDKey));
            require_quiet(cert_object_id == NULL || CFCastWithError(CFData, cert_object_id, error) != NULL, out);
            if (cert_data == NULL) {
                // Retrieve value directly from the token.
                if (certTokenSession == nil) {
                    require_quiet(certTokenSession = SecTokenSessionCreate(token_id, &auth_params, error), out);
                }
                NSError *err;
                TKClientTokenObject *certTokenObject = [tokenSession objectForObjectID:(__bridge NSData *)cert_object_id error:&err];
                if (certTokenObject == nil && error != NULL) {
                    *error = (CFErrorRef)CFBridgingRetain(err);
                }
                require_quiet(certTokenObject != nil, out);
                cert_data = CFBridgingRetain(certTokenObject.value);
            }
            if (cert_data != NULL) {
                CFDictionarySetValue(output, kSecAttrIdentityCertificateData, cert_data);
            } else {
                CFDictionaryRemoveValue(output, kSecAttrIdentityCertificateData);
            }
        }
    }

    if (wants_ref || wants_attributes) {
        // Convert serialized form of access control to object form.
        if (!token_item) {
            CFRetainAssign(ac_data, CFDictionaryGetValue(output, kSecAttrAccessControl));
        }

        if (ac_data != NULL) {
            SecAccessControlRef ac;
            require_quiet(ac = SecAccessControlCreateFromData(kCFAllocatorDefault, ac_data, error), out);
            CFDictionarySetValue(output, kSecAttrAccessControl, ac);
            CFRelease(ac);
        }
    }

    if (wants_ref) {
        CFTypeRef ref;
        require_quiet(SecTokenItemCreateFromAttributes(output, auth_params.dictionary, tokenSession, object_id, &ref, error), out);
        if (!(wants_attributes || wants_data || wants_persistent_ref)) {
            CFAssignRetained(*result, ref);
        } else if (ref != NULL) {
            CFDictionarySetValue(output, kSecValueRef, ref);
            CFRelease(ref);
            if (!wants_data) {
                // We could have stored data value previously to make ref creation succeed.
                // They are not needed any more and in case that caller did not want the data, avoid returning them.
                CFDictionaryRemoveValue(output, kSecValueData);
            }
        }
    }

    ok = true;

out:
    CFReleaseSafe(parsed_value);
    CFReleaseSafe(object_value);
    CFReleaseSafe(cert_object_id);
    CFReleaseSafe(cert_data);
    CFReleaseSafe(ac_data);
    CFReleaseSafe(value);
    CFReleaseSafe(persistent_ref);
    CFReleaseSafe(object_id);
    CFReleaseSafe(attrs);
    CFReleaseSafe(auth_params.mutable_dictionary);
    return ok;
}

static bool SecItemResultProcess(CFDictionaryRef query, CFDictionaryRef auth_params, TKClientTokenSession *tokenSession,
                                 CFTypeRef raw_result, CFTypeRef *result, CFErrorRef *error) {
    bool ok = false;
    require_action_quiet(raw_result != NULL, out, ok = true);
    require_action_quiet(result != NULL, out, ok = true);

    if (CFGetTypeID(raw_result) == CFArrayGetTypeID()) {
        CFIndex i, count = CFArrayGetCount(raw_result);
        *result = CFArrayCreateMutableForCFTypes(NULL);
        for (i = 0; i < count; i++) {
            CFTypeRef ref;
            CFErrorRef localError = NULL;
            bool prepared = SecItemResultCopyPrepared(CFArrayGetValueAtIndex(raw_result, i),
                                                      tokenSession, query, auth_params, &ref, &localError);
            if (!prepared) {
                // TokenNotFound or TokenObjectNotFound will just not insert failing item into resulting array, other errors abort processing.
                require_action_quiet(localError != NULL && isCryptoTokenKitTKErrorDomainAvailable() &&
                                     CFEqual(CFErrorGetDomain(localError), (__bridge CFStringRef)getTKErrorDomain()) &&
                                     (CFErrorGetCode(localError) == TKErrorCodeTokenNotFound || CFErrorGetCode(localError) == TKErrorCodeObjectNotFound), out,
                                     CFErrorPropagate(localError, error));
                CFReleaseNull(localError);
            } else if (ref != NULL) {
                CFArrayAppendValue((CFMutableArrayRef)*result, ref);
                CFRelease(ref);
            }
        }
    } else {
        require_quiet(SecItemResultCopyPrepared(raw_result, tokenSession, query, auth_params, result, error), out);
    }

    ok = true;

out:
    return ok;
}

static bool SecItemAttributesPrepare(SecCFDictionaryCOW *attrs, bool forQuery, CFErrorRef *error) {
    bool ok = false;
    CFDataRef ac_data = NULL, acm_context = NULL;
    LAContext *authContext;

    // If a ref was specified we get its attribute dictionary and parse it.
    CFTypeRef value = CFDictionaryGetValue(attrs->dictionary, kSecValueRef);
    if (value) {
        CFDictionaryRef ref_attributes;
        require_action_quiet(ref_attributes = SecItemCopyAttributeDictionary(value, forQuery), out,
                             SecError(errSecValueRefUnsupported, error, CFSTR("unsupported kSecValueRef in query")));

        // Replace any attributes we already got from the ref with the ones from the attributes dictionary the caller passed us.
        // This allows a caller to add an item using attributes from the ref and still override some of them in the dictionary directly.
        CFDictionaryForEach(ref_attributes, ^(const void *key, const void *value) {
            // Attributes already present in 'attrs' have precedence over the generic ones retrieved from the ref,
            // so add only those attributes from 'ref' which are missing in attrs.
            CFDictionaryAddValue(SecCFDictionaryCOWGetMutable(attrs), key, value);
        });
        CFRelease(ref_attributes);

        if (forQuery) {
            CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(attrs), kSecAttrTokenOID);
        }

        // Remove original expanded valueRef. Do not remove it in case when adding token item, because it is needed later to avoid
        // another roundtrip to token driver.
        if (forQuery || !CFDictionaryContainsKey(attrs->dictionary, kSecAttrTokenID)) {
            CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(attrs), kSecValueRef);
        }
    }

    SecAccessControlRef access_control = (SecAccessControlRef)CFDictionaryGetValue(attrs->dictionary, kSecAttrAccessControl);
    if (access_control != NULL) {
        require_action_quiet(CFGetTypeID(access_control) == SecAccessControlGetTypeID(), out,
                             SecError(errSecParam, error, CFSTR("Unexpected type of kSecAttrAccessControl attribute")));
        require_action_quiet(ac_data = SecAccessControlCopyData(access_control), out,
                             SecError(errSecParam, error, CFSTR("unsupported kSecAttrAccessControl in query")));
        CFDictionarySetValue(SecCFDictionaryCOWGetMutable(attrs), kSecAttrAccessControl, ac_data);
    }

    authContext = [(__bridge NSDictionary *)attrs->dictionary objectForKey:(id)kSecUseAuthenticationContext];
    if (authContext != nil) {
        require_action_quiet(!CFDictionaryContainsKey(attrs->dictionary, kSecUseCredentialReference), out,
                             SecError(errSecParam, error, CFSTR("kSecUseAuthenticationContext cannot be used together with kSecUseCredentialReference")));
        require_action_quiet(acm_context = CFBridgingRetain(authContext.externalizedContext), out, SecError(errSecAuthFailed, error, CFSTR("Unable to get externalizedContext from LAContext")));
        CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(attrs), kSecUseAuthenticationContext);
        CFDictionarySetValue(SecCFDictionaryCOWGetMutable(attrs), kSecUseCredentialReference, acm_context);
    }

    CFTypeRef policy = CFDictionaryGetValue(attrs->dictionary, kSecMatchPolicy);
    if (policy) {
        require_action_quiet(CFGetTypeID(policy) == SecPolicyGetTypeID(), out,
                             SecError(errSecParam, error, CFSTR("unsupported kSecMatchPolicy in query")));

        CFTypeRef values[] = { policy };
        CFArrayRef policiesArray = CFArrayCreate(kCFAllocatorDefault, values, 1, &kCFTypeArrayCallBacks);
        xpc_object_t policiesArrayXPC = SecPolicyArrayCopyXPCArray(policiesArray, error);
        CFReleaseSafe(policiesArray);
        require_action_quiet(policiesArrayXPC, out,
                             SecError(errSecInternal, error, CFSTR("Failed to copy XPC policy")));

        CFTypeRef objectReadyForXPC = _CFXPCCreateCFObjectFromXPCObject(policiesArrayXPC);
        require_action_quiet(objectReadyForXPC, out,
                             SecError(errSecInternal, error, CFSTR("Failed to create CFObject from XPC policy")));
        
        CFDictionarySetValue(SecCFDictionaryCOWGetMutable(attrs), kSecMatchPolicy, objectReadyForXPC);
        CFRelease(objectReadyForXPC);
    }
    value = CFDictionaryGetValue(attrs->dictionary, kSecAttrIssuer);
    if (value) {
        /* convert DN to canonical issuer, if value is DN (top level sequence) */
        CFDataRef issuer;
        require_quiet(issuer = CFCastWithError(CFData, value, error), out);
        const DERItem name = { (unsigned char *)CFDataGetBytePtr(issuer), CFDataGetLength(issuer) };
        DERDecodedInfo content;
        if (DERDecodeItem(&name, &content) == DR_Success && content.tag == ASN1_CONSTR_SEQUENCE) {
            CFDataRef canonical_issuer = createNormalizedX501Name(kCFAllocatorDefault, &content.content);
            if (canonical_issuer) {
                CFDictionarySetValue(SecCFDictionaryCOWGetMutable(attrs), kSecAttrIssuer, canonical_issuer);
                CFRelease(canonical_issuer);
            }
        }
    }

    if (CFDictionaryContainsKey(attrs->dictionary, kSecUseTokenRawItems)) {
        // This use flag is client-only, securityd does not understand it.
        CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(attrs), kSecUseTokenRawItems);
    }

    ok = true;

out:
    CFReleaseSafe(ac_data);
    CFReleaseSafe(acm_context);
    return ok;
}

static bool SecItemAuthMaxAttemptsReached(CFArrayRef ac_pairs, CFErrorRef *error)
{
    CFMutableStringRef log_string = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFArrayRef ac_pair;
    CFArrayForEachC(ac_pairs, ac_pair) {
        CFStringRef acl_hex_string = CFDataCopyHexString(CFArrayGetValueAtIndex(ac_pair, 0));
        CFStringRef str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("operation: %@ acl:%@\n"), CFArrayGetValueAtIndex(ac_pair, 1), acl_hex_string);
        CFStringAppend(log_string, str);
        CFRelease(acl_hex_string);
        CFRelease(str);
    }

    CFStringRef reason = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Reached maximum count of authentication attempts\n %@"), log_string);
    SecError(errSecAuthFailed, error, CFSTR("%@"), reason);
    __security_simulatecrash(reason, __sec_exception_code_AuthLoop);
    CFRelease(reason);
    CFRelease(log_string);
    return false;
}

static bool SecItemAuthDo(SecCFDictionaryCOW *auth_params, CFErrorRef *error, SecItemAuthResult (^perform)(CFArrayRef *ac_pairs, CFErrorRef *error)) {
    bool ok = false;
    CFArrayRef ac_pairs = NULL;
    SecCFDictionaryCOW auth_options = { NULL };

    for (uint32_t i = 0;; ++i) {
        // If the operation succeeded or failed with other than auth-needed error, just leave.
        SecItemAuthResult auth_result = perform(&ac_pairs, error);
        require_quiet(auth_result != kSecItemAuthResultError, out);
        require_action_quiet(auth_result == kSecItemAuthResultNeedAuth, out, ok = true);

        require_action_quiet(isLocalAuthenticationAvailable(), out, SecError(errSecUnimplemented, error, CFSTR("LocalAuthentication is not available")));

        // If auth_params were not created up to now, do create them because we will definitely need them.
        SecCFDictionaryCOWGetMutable(auth_params);

        // Retrieve or create authentication handle and/or ACM context.
        LAContext *authContext = [(__bridge NSDictionary *)auth_params->dictionary objectForKey:(id)kSecUseAuthenticationContext];
        if (authContext == nil) {
            NSData *acmContext = [(__bridge NSDictionary *)auth_params->dictionary objectForKey:(id)kSecUseCredentialReference];
            authContext = (acmContext == nil) ? [[getLAContextClass() alloc] init] : [[getLAContextClass() alloc] initWithExternalizedContext:acmContext];
            require_action_quiet(authContext != nil, out, SecError(errSecAuthFailed, error, CFSTR("Unable to create LAContext from externalizedContext")));
            CFDictionarySetValue(SecCFDictionaryCOWGetMutable(auth_params), kSecUseAuthenticationContext, (__bridge CFTypeRef)authContext);
            if (acmContext == nil) {
                require_action_quiet(acmContext = authContext.externalizedContext, out, SecError(errSecAuthFailed, error, CFSTR("Failed to get externalized context from LAContext")));
                CFDictionarySetValue(SecCFDictionaryCOWGetMutable(auth_params), kSecUseCredentialReference, (__bridge CFDataRef)acmContext);
            }
        }

        // Throttle max authentication attempts.  This is mainly protection against exceptional states, not ordinary
        // user retry limit.
        require_action(i < 20, out, SecItemAuthMaxAttemptsReached(ac_pairs, error));

        // Prepare auth options dictionary.
        if (auth_options.dictionary == NULL) {
            CFStringRef operation_prompt = CFDictionaryGetValue(auth_params->dictionary, kSecUseOperationPrompt);
            if (operation_prompt != NULL) {
                CFNumberRef key = CFNumberCreateWithCFIndex(NULL, kLAOptionAuthenticationReason);
                CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&auth_options), key, operation_prompt);
                CFRelease(key);
            }

            CFStringRef caller_name = CFDictionaryGetValue(auth_params->dictionary, kSecUseCallerName);
            if (caller_name != NULL) {
                CFNumberRef key = CFNumberCreateWithCFIndex(NULL, kLAOptionCallerName);
                CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&auth_options), key, caller_name);
                CFRelease(key);
            }

            CFTypeRef auth_ui = CFDictionaryGetValue(auth_params->dictionary, kSecUseAuthenticationUI);
            if (CFEqualSafe(auth_ui, kSecUseAuthenticationUIFail)) {
                CFNumberRef key = CFNumberCreateWithCFIndex(NULL, kLAOptionNotInteractive);
                CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&auth_options), key, kCFBooleanTrue);
                CFRelease(key);
            }
        }

        // Go through all access_control-operation pairs and evaluate them.
        for (NSArray *accessControlPair in (__bridge NSArray *)ac_pairs) {
            NSError *err;
            id accessControl = CFBridgingRelease(SecAccessControlCreateFromData(kCFAllocatorDefault, (__bridge CFDataRef)accessControlPair[0], error));
            require_quiet(accessControl != nil, out);
            NSDictionary *result = [authContext evaluateAccessControl:(__bridge SecAccessControlRef)accessControl aksOperation:(__bridge CFTypeRef)accessControlPair[1] options:(__bridge NSDictionary *)auth_options.dictionary error:&err];
            if (result == nil && error != NULL) {
                *error = (CFErrorRef)CFBridgingRetain(err);
            }
            require_quiet(result != nil, out);
            if ([accessControlPair[1] isEqual:@""]) {
                // we assume that only one ACL can be modified during ItemAdd or ItemUpdate
                SecAccessControlSetBound((__bridge SecAccessControlRef)accessControl, true);
                NSData *accessControlData = CFBridgingRelease(SecAccessControlCopyData((__bridge SecAccessControlRef)accessControl));
                CFDictionarySetValue(SecCFDictionaryCOWGetMutable(auth_params), kSecAttrAccessControl, (__bridge CFDataRef)accessControlData);
            }
        }
    }

    ok = true;

out:
    CFReleaseSafe(auth_options.mutable_dictionary);
    CFReleaseSafe(ac_pairs);
    return ok;
}

void SecItemAuthCopyParams(SecCFDictionaryCOW *auth_params, SecCFDictionaryCOW *query) {
    // Store operation prompt.
    CFStringRef operation_prompt = CFDictionaryGetValue(query->dictionary, kSecUseOperationPrompt);
    if (operation_prompt != NULL) {
        CFDictionarySetValue(SecCFDictionaryCOWGetMutable(auth_params), kSecUseOperationPrompt, operation_prompt);
        CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(query), kSecUseOperationPrompt);
    }

    // Store caller name.
    CFStringRef caller_name = CFDictionaryGetValue(query->dictionary, kSecUseCallerName);
    if (caller_name != NULL) {
        CFDictionarySetValue(SecCFDictionaryCOWGetMutable(auth_params), kSecUseCallerName, caller_name);
        CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(query), kSecUseCallerName);
    }

    // Find out whether we are allowed to pop up a UI.
    CFTypeRef auth_ui = CFDictionaryGetValue(query->dictionary, kSecUseAuthenticationUI) ?:
    (CFEqualSafe(CFDictionaryGetValue(query->dictionary, kSecUseNoAuthenticationUI), kCFBooleanTrue) ?
     kSecUseAuthenticationUIFail : kSecUseAuthenticationUIAllow);
    if (!CFEqual(auth_ui, kSecUseAuthenticationUISkip) || CFDictionaryGetValue(query->dictionary, kSecUseNoAuthenticationUI)) {
        if (CFDictionaryContainsKey(query->dictionary, kSecUseNoAuthenticationUI))
            CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(query), kSecUseNoAuthenticationUI);
        if (!CFEqualSafe(auth_ui, kSecUseAuthenticationUISkip) && CFDictionaryContainsKey(query->dictionary, kSecUseAuthenticationUI))
            CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(query), kSecUseAuthenticationUI);
    }

    if (!CFEqual(auth_ui, kSecUseAuthenticationUIAllow)) {
        CFDictionarySetValue(SecCFDictionaryCOWGetMutable(auth_params), kSecUseAuthenticationUI, auth_ui);
    }

    CFDataRef acm_context = CFDictionaryGetValue(query->dictionary, kSecUseCredentialReference);
    if (acm_context != NULL) {
        CFDictionarySetValue(SecCFDictionaryCOWGetMutable(auth_params), kSecUseCredentialReference, acm_context);
    }
}

static SecItemAuthResult SecItemCreatePairsFromError(CFErrorRef *error, CFArrayRef *ac_pairs)
{
    if (error && *error && CFErrorGetCode(*error) == errSecAuthNeeded && CFEqualSafe(CFErrorGetDomain(*error), kSecErrorDomain)) {
        // Extract ACLs to be verified from the error.
        CFDictionaryRef user_info = CFErrorCopyUserInfo(*error);
        CFNumberRef key = CFNumberCreateWithCFIndex(NULL, errSecAuthNeeded);
        CFRetainAssign(*ac_pairs, CFDictionaryGetValue(user_info, key));
        if (*ac_pairs == NULL)
            CFAssignRetained(*ac_pairs, CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));

        CFRelease(key);
        CFRelease(user_info);
        CFReleaseNull(*error);
        return kSecItemAuthResultNeedAuth;
    }
    return kSecItemAuthResultError;
}

// Wrapper to handle automatic authentication and token/secd case switching.
static bool SecItemAuthDoQuery(SecCFDictionaryCOW *query, SecCFDictionaryCOW *attributes, const void *secItemOperation, CFErrorRef *error,
                               bool (^perform)(TKClientTokenSession *tokenSession, CFDictionaryRef query, CFDictionaryRef attributes, CFDictionaryRef auth_params, CFErrorRef *error)) {
    bool ok = false;
    __block SecCFDictionaryCOW auth_params = { NULL };
    __block TKClientTokenSession *tokenSession;

    CFDictionaryRef dict = attributes ? attributes->dictionary : query->dictionary;
    SecAccessControlRef access_control = (SecAccessControlRef)CFDictionaryGetValue(dict, kSecAttrAccessControl);
    require_action_quiet(access_control == NULL || CFGetTypeID(access_control) == SecAccessControlGetTypeID(), out,
                         SecError(errSecParam, error, CFSTR("Unexpected type of kSecAttrAccessControl attribute")));

    if (secItemOperation == SecItemAdd || secItemOperation == SecItemUpdate) {
        if (access_control && SecAccessControlGetConstraints(access_control) &&
            CFEqualSafe(CFDictionaryGetValue(dict, kSecAttrSynchronizable), kCFBooleanTrue))
            require_quiet(SecError(errSecParam, error, CFSTR("item with kSecAttrAccessControl is not synchronizable")), out);
    }

    // Perform initial surgery on query/attributes (resolve LAContext to serialized ACM handle, resolve
    // SecAccessControlRef to serialized forms, expand kSecValueRef etc.)
    bool forQuery =
        secItemOperation == SecItemCopyMatching ||
        secItemOperation == SecItemUpdate ||
        secItemOperation == SecItemDelete;

    require_quiet(SecItemAttributesPrepare(query, forQuery, error), out);
    if (attributes != NULL)
        require_quiet(SecItemAttributesPrepare(attributes, false, error), out);

    // Populate auth_params dictionary according to initial query contents.
    SecItemAuthCopyParams(&auth_params, query);

    if (secItemOperation != SecItemCopyMatching) {
        // UISkip is allowed only for CopyMatching.
        require_action_quiet(!CFEqualSafe(CFDictionaryGetValue(query->dictionary, kSecUseAuthenticationUI), kSecUseAuthenticationUISkip), out,
                             SecError(errSecParam, error,
                                      CFSTR("kSecUseAuthenticationUISkip is allowed only for SecItemCopyMatching")));
    }

    {  // This scope exists to pacify compiler complaining about above require_ macros causing jumps into lifetime of block argument of this method.
        ok = SecItemAuthDo(&auth_params, error, ^SecItemAuthResult(CFArrayRef *ac_pairs, CFErrorRef *error) {
            SecItemAuthResult result = kSecItemAuthResultError;

            // Propagate actual credential reference to the query.
            if (auth_params.dictionary != NULL) {
                CFDataRef acm_context = CFDictionaryGetValue(auth_params.dictionary, kSecUseCredentialReference);
                if (acm_context != NULL) {
                    CFDictionarySetValue(SecCFDictionaryCOWGetMutable(query), kSecUseCredentialReference, acm_context);
                }

                CFDataRef acl_data_ref = CFDictionaryGetValue(auth_params.dictionary, kSecAttrAccessControl);
                if (acl_data_ref != NULL) {
                    CFDictionarySetValue(SecCFDictionaryCOWGetMutable(attributes ?: query), kSecAttrAccessControl, acl_data_ref);
                }
            }

            // Prepare connection to target token if it is present.
            CFStringRef token_id = CFDictionaryGetValue(query->dictionary, kSecAttrTokenID);
            require_quiet(token_id == NULL || CFCastWithError(CFString, token_id, error) != NULL, out);
            if (secItemOperation != SecItemCopyMatching && token_id != NULL && !cf_bool_value(CFDictionaryGetValue(query->dictionary, kSecUseTokenRawItems))) {
                CFErrorRef localError = NULL;
                tokenSession = SecTokenSessionCreate(token_id, &auth_params, &localError);
                if (tokenSession == NULL) {
                    require_action_quiet(secItemOperation == SecItemDelete && isCryptoTokenKitTKErrorDomainAvailable() &&
                                         CFEqual(CFErrorGetDomain(localError), (__bridge CFStringRef)getTKErrorDomain()) &&
                                         CFErrorGetCode(localError) == TKErrorCodeTokenNotFound,
                                         out, CFErrorPropagate(localError, error));

                    // In case that token cannot be found and deletion is required, just continue and delete item from keychain only.
                    CFReleaseNull(localError);
                }
            }

            CFDictionaryRef attrs = (attributes != NULL) ? attributes->dictionary : NULL;
            if (perform(tokenSession, query->dictionary, attrs, auth_params.dictionary, error)) {
                result = kSecItemAuthResultOK;
                if (error && *error) {
                    // <rdar://problem/60642633> SecItemAuthDoQuery perform() sometimes returns success and fills in error parameter
                    secdebug("SecItemAuthDoQuery", "perform() succeded but returned an error: %@", *error);
                    CFReleaseNull(*error);
                }
            } else {
                result = SecItemCreatePairsFromError(error, ac_pairs);
            }

            out:
            return result;
        });
    }
    require_quiet(ok, out);

    ok = true;

out:
    CFReleaseSafe(auth_params.mutable_dictionary);
    return ok;
}

static bool cftype_to_bool_cftype_error_request(enum SecXPCOperation op, CFTypeRef attributes, CFTypeRef *result, CFErrorRef *error)
{
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetPList(message, kSecXPCKeyQuery, attributes, error);
    }, ^bool(xpc_object_t response, CFErrorRef *error) {
        if (result) {
            return SecXPCDictionaryCopyPListOptional(response, kSecXPCKeyResult, result, error);
        }
        return true;
    });
}

static CFArrayRef dict_to_array_error_request(enum SecXPCOperation op, CFDictionaryRef attributes, CFErrorRef *error)
{
    CFArrayRef result = NULL;
    bool success = cftype_to_bool_cftype_error_request(op, attributes, (CFTypeRef*)&result, error);
    if(success && !isArray(result)){
        SecError(errSecUnimplemented, error, CFSTR("Unexpected nonarray returned: %@"), result);
        CFReleaseNull(result);
    }
    return result;
}

bool cftype_client_to_bool_cftype_error_request(enum SecXPCOperation op, CFTypeRef attributes, __unused SecurityClient *client, CFTypeRef *result, CFErrorRef *error) {
    return cftype_to_bool_cftype_error_request(op, attributes, result, error);
}

static bool dict_to_error_request(enum SecXPCOperation op, CFDictionaryRef query, CFErrorRef *error)
{
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetPList(message, kSecXPCKeyQuery, query, error);
    }, NULL);
}

static bool cfstring_array_array_to_error_request(enum SecXPCOperation op, CFStringRef string, CFArrayRef groups, CFArrayRef items, __unused SecurityClient *client, CFErrorRef *error)
{
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        if (string) {
            if (!SecXPCDictionarySetString(message, kSecXPCKeyString, string, error))
                return false;
        }

        if (groups) {
            if (!SecXPCDictionarySetPList(message, kSecXPCKeyArray, groups, error))
                return false;
        }
        
        if (items) {
            if (!SecXPCDictionarySetPList(message, kSecXPCKeyQuery, items, error))
                return false;
        }
        
        return true;
    }, NULL);
}

static bool dict_client_to_error_request(enum SecXPCOperation op, CFDictionaryRef query, __unused SecurityClient *client, CFErrorRef *error)
{
    return dict_to_error_request(op, query, error);
}

static bool SecTokenCreateAccessControlError(CFStringRef operation, CFDataRef access_control, CFErrorRef *error) {
    CFArrayRef ac_pair = CFArrayCreateForCFTypes(NULL, access_control, operation, NULL);
    const void *ac_pairs[] = { CFArrayCreateForCFTypes(NULL, ac_pair, NULL) };
    const void *keys[] = { CFNumberCreateWithCFIndex(NULL, errSecAuthNeeded) };
    CFAssignRetained(*error, CFErrorCreateWithUserInfoKeysAndValues(NULL, kSecErrorDomain, errSecAuthNeeded,
                                                                    keys, ac_pairs, 1));
    CFRelease(keys[0]);
    CFRelease(ac_pairs[0]);
    CFRelease(ac_pair);
    return false;
}

static bool SecTokenProcessError(CFStringRef operation, TKClientTokenSession *tokenSession, CFTypeRef object_or_attrs, CFErrorRef *error) {
    if (isCryptoTokenKitTKErrorDomainAvailable() && CFEqualSafe(CFErrorGetDomain(*error), (__bridge CFStringRef)getTKErrorDomain()) &&
        CFErrorGetCode(*error) == TKErrorCodeAuthenticationNeeded) {
        // Replace error with the one which is augmented with access control and operation which failed,
        // which will cause SecItemDoWithAuth to throw UI.
        // Create array containing tuple (array) with error and requested operation.
        NSError *err;
        TKClientTokenObject *tokenObject = [tokenSession objectForObjectID:(__bridge id)object_or_attrs error:&err];
        if (tokenObject == nil && error != NULL) {
            *error = (CFErrorRef)CFBridgingRetain(err);
        }
        if (tokenObject.accessControl != nil) {
            SecTokenCreateAccessControlError(operation, (__bridge CFDataRef)tokenObject.accessControl, error);
        }
    }
    return false;
}

static CFTypeRef SecTokenCopyUpdatedObjectID(TKClientTokenSession *tokenSession, CFDataRef object_id, CFMutableDictionaryRef attributes, CFErrorRef *error) {
    CFDataRef access_control = NULL, db_value = NULL, result = NULL;
    CFDictionaryRef old_attrs = NULL;
    TKClientTokenObject *tokenObject;
    NSError *err;

    // Create or update the object on the token.
    old_attrs = CFDictionaryCreateCopy(kCFAllocatorDefault, attributes);
    if (object_id == NULL) {
        tokenObject = [tokenSession createObjectWithAttributes:(__bridge NSDictionary *)attributes error:&err];
    } else {
        tokenObject = [tokenSession objectForObjectID:(__bridge NSData *)object_id error:&err];
    }
    if (tokenObject == nil && error != NULL) {
        *error = (CFErrorRef)CFBridgingRetain(err);
    }
    require_action_quiet(tokenObject != nil, out,
                         SecTokenProcessError(kAKSKeyOpEncrypt, tokenSession, object_id ?: (CFTypeRef)attributes, error));

    CFDictionaryRemoveAllValues(attributes);
    [(__bridge NSMutableDictionary *)attributes addEntriesFromDictionary:tokenObject.keychainAttributes];
    for (NSString *attr in (__bridge NSDictionary *)old_attrs) {
        if (![attr isEqualToString:(id)kSecValueData]) {
            CFDictionaryAddValue(attributes, (__bridge CFStringRef)attr, (__bridge CFTypeRef)[(__bridge NSDictionary *)old_attrs objectForKey:attr]);
        }
    }

    // Prepare kSecValueData field for the item to be stored into the keychain DB.
    access_control = CFBridgingRetain(tokenObject.accessControl);
    require_quiet(db_value = SecTokenItemValueCreate((__bridge CFDataRef)tokenObject.objectID, access_control,
                                                     CFDictionaryGetValue(attributes, kSecValueData), error), out);
    CFDictionarySetValue(attributes, kSecValueData, db_value);

    // kSecAttrAccessControl is handled directly by the token and stored inside data field.
    CFDictionaryRemoveValue(attributes, kSecAttrAccessControl);
    CFRetainAssign(result, (__bridge CFDataRef)tokenObject.objectID);

out:
    CFReleaseSafe(access_control);
    CFReleaseSafe(db_value);
    CFReleaseSafe(old_attrs);
    return result;
}

static bool SecTokenItemAdd(TKClientTokenSession *tokenSession, CFDictionaryRef attributes, CFDictionaryRef auth_params,
                            CFTypeRef *result, CFErrorRef *error) {
    bool ok = false;
    CFTypeRef object_id = NULL, ref = NULL;
    CFDictionaryRef ref_attrs = NULL;
    CFTypeRef db_result = NULL;
    CFDataRef db_value = NULL;
    CFMutableDictionaryRef attrs = CFDictionaryCreateMutableCopy(NULL, 0, attributes);

    CFDictionarySetValue(attrs, kSecAttrAccessible, kSecAttrAccessibleAlwaysPrivate);     //token items should be accesible always because have own ACL encoded in OID
    object_id = CFRetainSafe(CFDictionaryGetValue(attrs, kSecAttrTokenOID));
    CFDictionaryRemoveValue(attrs, kSecAttrTokenOID);
    require_quiet(CFAssignRetained(object_id, SecTokenCopyUpdatedObjectID(tokenSession, object_id, attrs, error)), out);
    CFDictionaryRemoveValue(attrs, kSecAttrTokenOID);
    if (CFDictionaryContainsKey(attrs, kSecValueRef)) {
        // All attributes already had been extracted from valueRef, so do not go through that step again, just remove
        // the ref from the dictionary since it is of no use any more.
        CFDictionaryRemoveValue(attrs, kSecValueRef);
    } else {
        // Augment attributes from default attributes of the related ref (SecKeyRef, SecCertificateRef).  This is best done
        // by creating ref and getting back its attributes.
        require_quiet(SecTokenItemCreateFromAttributes(attrs, auth_params, tokenSession, object_id, &ref, error), out);
        if (ref != NULL) {
            if ((ref_attrs = SecItemCopyAttributeDictionary(ref, false)) != NULL) {
                CFDictionaryForEach(ref_attrs, ^(const void *key, const void *value) {
                    if (!CFEqual(key, kSecValueData) && !CFEqual(key, kSecAttrTokenOID) && !CFEqual(key, kSecAttrAccessControl)) {
                        CFDictionaryAddValue(attrs, key, value);
                    }
                });
            }
        }
    }

    // Make sure that both attributes and data are returned.
    CFDictionarySetValue(attrs, kSecReturnAttributes, kCFBooleanTrue);
    CFDictionarySetValue(attrs, kSecReturnData, kCFBooleanTrue);

    if (!CFEqualSafe(CFDictionaryGetValue(attrs, kSecAttrIsPermanent), kCFBooleanFalse)) {
        // IsPermanent is not present or is true, so add item to the db.
        require_quiet(SECURITYD_XPC(sec_item_add, cftype_client_to_bool_cftype_error_request, attrs,
                                    SecSecurityClientGet(), &db_result, error), out);
    } else {
        // Process directly result of token call.
        db_result = CFRetain(attrs);
    }
    require_quiet(SecItemResultProcess(attributes, auth_params, tokenSession, db_result, result, error), out);
    ok = true;

out:
    CFReleaseSafe(db_result);
    CFReleaseSafe(db_value);
    CFReleaseSafe(attrs);
    CFReleaseSafe(ref_attrs);
    CFReleaseSafe(object_id);
    CFReleaseSafe(ref);
    return ok;
}

static void countReadOnlyAPICall() {
    if (!isReadOnlyAPIRateWithinLimits()) {
    }
}

static void countModifyingAPICall() {
    if (!isModifyingAPIRateWithinLimits()) {
    }
}

OSStatus SecItemAdd(CFDictionaryRef attributes, CFTypeRef *result) {
    @autoreleasepool {
        __block SecCFDictionaryCOW attrs = { attributes };
        OSStatus status;

        os_activity_t activity = os_activity_create("SecItemAdd_ios", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
        os_activity_scope(activity);

        require_quiet(!explode_identity(attrs.dictionary, (secitem_operation)SecItemAdd, &status, result), errOut);
        infer_cert_label(&attrs);

        status = SecOSStatusWith(^bool(CFErrorRef *error) {
            return SecItemAuthDoQuery(&attrs, NULL, SecItemAdd, error, ^bool(TKClientTokenSession *tokenSession, CFDictionaryRef attributes, CFDictionaryRef unused, CFDictionaryRef auth_params, CFErrorRef *error) {
                if (tokenSession == NULL) {
                    CFTypeRef raw_result = NULL;
                    logUnreasonableDataLength(attributes);
                    countModifyingAPICall();
                    if (!SECURITYD_XPC(sec_item_add, cftype_client_to_bool_cftype_error_request, attributes, SecSecurityClientGet(), &raw_result, error)) {
                        return false;
                    }

                    bool ok = SecItemResultProcess(attributes, auth_params, tokenSession, raw_result, result, error);
                    CFReleaseSafe(raw_result);
                    return ok;
                } else {
                    // Send request to an appropriate token instead of secd.
                    return SecTokenItemAdd(tokenSession, attributes, auth_params, result, error);
                }
            });
        });

    errOut:
        CFReleaseSafe(attrs.mutable_dictionary);
        secdebug("secitem", "SecItemAdd returned: %d", (int)status);

        return status;
    }
}


OSStatus SecItemCopyMatching(CFDictionaryRef inQuery, CFTypeRef *result) {
    @autoreleasepool {
        OSStatus status;
        __block SecCFDictionaryCOW query = { inQuery };

        os_activity_t activity = os_activity_create("SecItemCopyMatching_ios", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
        os_activity_scope(activity);

        require_quiet(!explode_identity(query.dictionary, (secitem_operation)SecItemCopyMatching, &status, result), errOut);

        bool wants_data = cf_bool_value(CFDictionaryGetValue(query.dictionary, kSecReturnData));
        bool wants_attributes = cf_bool_value(CFDictionaryGetValue(query.dictionary, kSecReturnAttributes));
        if ((wants_data && !wants_attributes)) {
            // When either attributes or data are requested, we need to query both, because for token based items,
            // both are needed in order to generate proper data and/or attributes results.
            CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&query), kSecReturnAttributes, kCFBooleanTrue);
        }

        status = SecOSStatusWith(^bool(CFErrorRef *error) {
            return SecItemAuthDoQuery(&query, NULL, SecItemCopyMatching, error, ^bool(TKClientTokenSession *tokenSession, CFDictionaryRef query, CFDictionaryRef attributes, CFDictionaryRef auth_params, CFErrorRef *error) {
                CFTypeRef raw_result = NULL;
                countReadOnlyAPICall();
                if (!SECURITYD_XPC(sec_item_copy_matching, cftype_client_to_bool_cftype_error_request, query, SecSecurityClientGet(), &raw_result, error)) {
                    return false;
                }

                // We intentionally pass NULL as token argument, because we want to be able to decide about token on which the item lives
                // on per-record basis, not wholesale.  Logic inside SecItemResultCopyPrepared will open proper token according
                // to currently processed item.
                bool ok = SecItemResultProcess(inQuery, auth_params, NULL, raw_result, result, error);
                CFReleaseSafe(raw_result);
                return ok;
            });
        });

    errOut:
        secdebug("secitem", "SecItemCopyMatching_ios returned: %d", (int)status);
        CFReleaseSafe(query.mutable_dictionary);
        return status;
    }
}

// Invokes token-object handler for each item matching specified query.
static bool SecTokenItemForEachMatching(CFDictionaryRef query, CFErrorRef *error,
                                        bool (^perform)(CFDictionaryRef item_value, CFDictionaryRef item_query,
                                                        CFErrorRef *error)) {
    bool ok = false;
    CFMutableDictionaryRef list_query = NULL;
    CFTypeRef items = NULL;
    CFArrayRef ref_array = NULL;
    CFDictionaryRef item_query = NULL, item_data = NULL;

    // Query all items with data and persistent_ref, so that we can extract objectIDs and also identify originating
    // items in the keychain.
    list_query = CFDictionaryCreateMutableCopy(NULL, 0, query);
    if (CFDictionaryGetValue(list_query, kSecMatchLimit) == NULL) {
        CFDictionarySetValue(list_query, kSecMatchLimit, kSecMatchLimitAll);
    }
    CFDictionarySetValue(list_query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(list_query, kSecReturnPersistentRef, kCFBooleanTrue);
    require_quiet(SECURITYD_XPC(sec_item_copy_matching, cftype_client_to_bool_cftype_error_request, list_query,
                                SecSecurityClientGet(), &items, error), out);
    if (CFGetTypeID(items) != CFArrayGetTypeID()) {
        // Wrap single returned item into the array.
        CFArrayRef item_array = CFArrayCreateForCFTypes(NULL, items, NULL);
        CFAssignRetained(items, item_array);
    }

    CFTypeRef item;
    CFArrayForEachC(items, item) {
        CFDataRef data = CFDictionaryGetValue(item, kSecValueData);
        require_action_quiet(data != NULL, out, SecError(errSecInternal, error, CFSTR("value not present for token item")));

        CFAssignRetained(item_data, SecTokenItemValueCopy(data, error));
        require_quiet(item_data, out);

        CFAssignRetained(item_query,
                         CFDictionaryCreateForCFTypes(NULL,
                                                      kSecValuePersistentRef, CFDictionaryGetValue(item, kSecValuePersistentRef),
                                                      NULL));
        require_quiet(perform(item_data, item_query, error), out);
    }

    ok = true;

out:
    CFReleaseSafe(list_query);
    CFReleaseSafe(items);
    CFReleaseSafe(item_data);
    CFReleaseSafe(ref_array);
    CFReleaseSafe(item_query);
    return ok;
}

static bool SecItemRawUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate, CFErrorRef *error) {
    bool ok = false;
    if (gSecurityd) {
        // Ensure the dictionary passed to securityd has proper kCFTypeDictionaryKeyCallBacks.
        CFMutableDictionaryRef tmp = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
        CFDictionaryForEach(attributesToUpdate, ^(const void *key, const void *value) { CFDictionaryAddValue(tmp, key, value); });
        ok = gSecurityd->sec_item_update(query, tmp, SecSecurityClientGet(), error);
        CFRelease(tmp);
    } else {
        xpc_object_t message = securityd_create_message(sec_item_update_id, error);
        if (message) {
            if (SecXPCDictionarySetPList(message, kSecXPCKeyQuery, query, error) &&
                SecXPCDictionarySetPList(message, kSecXPCKeyAttributesToUpdate, attributesToUpdate, error)) {
                logUnreasonableDataLength(attributesToUpdate);
                xpc_object_t reply = securityd_message_with_reply_sync(message, error);
                if (reply) {
                    ok = securityd_message_no_error(reply, error);
                }
            }
        }
    }
    return ok;
}

static bool SecTokenItemUpdate(TKClientTokenSession *tokenSession, CFDictionaryRef query, CFDictionaryRef attributesToUpdate, CFErrorRef *error) {
    return SecTokenItemForEachMatching(query, error, ^bool(CFDictionaryRef object_data, CFDictionaryRef item_query, CFErrorRef *error) {
        bool ok = false;
        CFDataRef object_id = NULL;
        CFMutableDictionaryRef db_value = NULL;

        // Update attributes on the token.
        CFMutableDictionaryRef attributes = CFDictionaryCreateMutableCopy(NULL, 0, attributesToUpdate);
        require_quiet(object_id = SecTokenCopyUpdatedObjectID(tokenSession, CFDictionaryGetValue(object_data, kSecTokenValueObjectIDKey),
                                                              attributes, error), out);

        // Update attributes in the database.
        require_quiet(SecItemRawUpdate(item_query, attributes, error), out);

        ok = true;

    out:
        CFReleaseSafe(object_id);
        CFReleaseSafe(attributes);
        CFReleaseSafe(db_value);
        return ok;
    });
}

OSStatus SecItemUpdate(CFDictionaryRef inQuery, CFDictionaryRef inAttributesToUpdate) {
    @autoreleasepool {
        os_activity_t activity = os_activity_create("SecItemUpdate_ios", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
        os_activity_scope(activity);

        return SecOSStatusWith(^bool(CFErrorRef *error) {
            return SecItemUpdateWithError(inQuery, inAttributesToUpdate, error);
        });
    }
}

bool
SecItemUpdateWithError(CFDictionaryRef inQuery,
                       CFDictionaryRef inAttributesToUpdate,
                       CFErrorRef *error)
{
    @autoreleasepool {
        __block SecCFDictionaryCOW query = { inQuery };
        __block SecCFDictionaryCOW attributesToUpdate = { inAttributesToUpdate };
        bool result = false;

        if (handleUpdateIdentity(inQuery, inAttributesToUpdate, &result, error))
            goto errOut;

        result = SecItemAuthDoQuery(&query, &attributesToUpdate, SecItemUpdate, error, ^bool(TKClientTokenSession *tokenSession, CFDictionaryRef query, CFDictionaryRef attributes, CFDictionaryRef auth_params, CFErrorRef *error) {
            countModifyingAPICall();
            if (tokenSession == nil) {
                return SecItemRawUpdate(query, attributes, error);
            } else {
                return SecTokenItemUpdate(tokenSession, query, attributes, error);
            }
        });

    errOut:
        CFReleaseSafe(query.mutable_dictionary);
        CFReleaseSafe(attributesToUpdate.mutable_dictionary);
        secdebug("secitem", "SecItemUpdateWithError returned: %d", (int)result);
        return result;
    }
}

static OSStatus explode_persistent_identity_ref(SecCFDictionaryCOW *query)
{
    OSStatus status = errSecSuccess;
    CFTypeRef persist = CFDictionaryGetValue(query->dictionary, kSecValuePersistentRef);
    CFStringRef class = NULL;
    if (persist && _SecItemParsePersistentRef(persist, &class, NULL, NULL, NULL)
        && CFEqual(class, kSecClassIdentity)) {
        const void *keys[] = { kSecReturnRef, kSecValuePersistentRef };
        const void *vals[] = { kCFBooleanTrue, persist };
        CFDictionaryRef persistent_query = CFDictionaryCreate(NULL, keys, 
            vals, (array_size(keys)), NULL, NULL);
        CFTypeRef item_query = NULL;
        status = SecItemCopyMatching(persistent_query, &item_query);
        CFReleaseNull(persistent_query);
        if (status)
            return status;
        if (item_query == NULL)
            return errSecItemNotFound;

        CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(query), kSecValuePersistentRef);
        CFDictionarySetValue(SecCFDictionaryCOWGetMutable(query), kSecValueRef, item_query);
        CFRelease(item_query);
    }

    return status;
}

OSStatus SecItemDelete(CFDictionaryRef inQuery) {
    @autoreleasepool {
        OSStatus status;
        __block SecCFDictionaryCOW query = { inQuery };

        os_activity_t activity = os_activity_create("SecItemDelete_ios", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
        os_activity_scope(activity);

        require_noerr_quiet(status = explode_persistent_identity_ref(&query), errOut);
        require_quiet(!explode_identity(query.dictionary, (secitem_operation)SecItemDelete, &status, NULL), errOut);

        status = SecOSStatusWith(^bool(CFErrorRef *error) {
            return SecItemAuthDoQuery(&query, NULL, SecItemDelete, error, ^bool(TKClientTokenSession *tokenSession, CFDictionaryRef query, CFDictionaryRef attributes, CFDictionaryRef auth_params, CFErrorRef *error) {
                countModifyingAPICall();
                if (tokenSession == NULL) {
                    return SECURITYD_XPC(sec_item_delete, dict_client_to_error_request, query, SecSecurityClientGet(), error);
                } else {
                    return SecTokenItemForEachMatching(query, error, ^bool(CFDictionaryRef object_data, CFDictionaryRef item_query, CFErrorRef *error) {
                        bool ok = false;

                        // Delete item from the token.
                        CFDataRef object_id = CFDictionaryGetValue(object_data, kSecTokenValueObjectIDKey);
                        NSError *localError;
                        if (![tokenSession deleteObject:(__bridge NSData *)object_id error:&localError]) {
                            // Check whether object was not found; in this case, ignore the error.
                            require_action_quiet(isCryptoTokenKitTKErrorDomainAvailable() &&
                                                 [localError.domain isEqual:getTKErrorDomain()] &&
                                                 localError.code == TKErrorCodeObjectNotFound, out,
                                                 (CFErrorPropagate((__bridge CFErrorRef)localError, error), SecTokenProcessError(kAKSKeyOpDelete, tokenSession, object_id, error)));
                        }

                        // Delete the item from the keychain.
                        require_quiet(SECURITYD_XPC(sec_item_delete, dict_client_to_error_request, item_query,
                                                    SecSecurityClientGet(), error), out);
                        ok = true;

                        out:
                        return ok;
                    });
                }
            });
        });

    errOut:
        CFReleaseSafe(query.mutable_dictionary);
        secdebug("secitem", "SecItemDelete returned: %d", (int)status);

        return status;
    }
}

OSStatus
SecItemDeleteAll(void)
{
    @autoreleasepool {
        return SecOSStatusWith(^bool (CFErrorRef *error) {
            bool ok = true;
            if (gSecurityd) {
                if (!gSecurityd->sec_item_delete_all(error))
                    ok &= SecError(errSecInternal, error, CFSTR("sec_item_delete_all is NULL"));
            } else {
                ok &= securityd_send_sync_and_do(sec_delete_all_id, error, NULL, NULL);
            }
            return ok;
        });
    }
}

bool SecItemDeleteAllWithAccessGroups(CFArrayRef accessGroups, CFErrorRef *error) {
    return true;
}

OSStatus
SecItemUpdateTokenItemsForAccessGroups(CFTypeRef tokenID, CFArrayRef accessGroups, CFArrayRef tokenItemsAttributes)
{
    @autoreleasepool {
        OSStatus status;

        os_activity_t activity = os_activity_create("SecItemUpdateTokenItemsForAccessGroups", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
        os_activity_scope(activity);

        status = SecOSStatusWith(^bool(CFErrorRef *error) {
            bool ok = false;
            CFMutableArrayRef tokenItemsForServer = NULL;
            if (tokenItemsAttributes) {
                tokenItemsForServer = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
                for (CFIndex i = 0; i < CFArrayGetCount(tokenItemsAttributes); ++i) {
                    CFDictionaryRef itemAttributes = CFArrayGetValueAtIndex(tokenItemsAttributes, i);
                    CFTypeRef accessControl = CFDictionaryGetValue(itemAttributes, kSecAttrAccessControl);
                    CFTypeRef tokenOID = CFDictionaryGetValue(itemAttributes, kSecAttrTokenOID);
                    CFTypeRef valueData = CFDictionaryGetValue(itemAttributes, kSecValueData);
                    if (tokenOID != NULL && accessControl != NULL && CFDataGetTypeID() == CFGetTypeID(accessControl)) {
                        CFDataRef data;
                        require_quiet(data = SecTokenItemValueCreate(tokenOID, accessControl, valueData, error), out);
                        CFMutableDictionaryRef attributes = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, itemAttributes);
                        CFDictionarySetValue(attributes, kSecValueData, data);
                        CFDictionarySetValue(attributes, kSecAttrTokenID, tokenID);
                        CFDictionaryRemoveValue(attributes, kSecAttrAccessControl);
                        CFDictionaryRemoveValue(attributes, kSecAttrTokenOID);
                        CFArrayAppendValue(tokenItemsForServer, attributes);
                        CFReleaseNull(attributes);
                        CFReleaseNull(data);
                    } else {
                        CFArrayAppendValue(tokenItemsForServer, itemAttributes);
                    }
                }
            }

            ok = SECURITYD_XPC(sec_item_update_token_items_for_access_groups, cfstring_array_array_to_error_request, tokenID,
                               accessGroups ?: (__bridge CFArrayRef)@[], tokenItemsForServer ?: (__bridge CFArrayRef)@[], SecSecurityClientGet(), error);
            out:
            CFReleaseNull(tokenItemsForServer);
            return ok;
        });

        return status;
    }
}

CFArrayRef _SecKeychainSyncUpdateMessage(CFDictionaryRef updates, CFErrorRef *error) {
    @autoreleasepool {
        __block CFArrayRef result;
        os_activity_initiate("_SecKeychainSyncUpdateMessage", OS_ACTIVITY_FLAG_DEFAULT, ^{
            result = SECURITYD_XPC(sec_keychain_sync_update_message, dict_to_array_error_request, updates, error);
        });
        return result;
    }
}

#define do_if_registered(sdp, ...) if (gSecurityd && gSecurityd->sdp) { return gSecurityd->sdp(__VA_ARGS__); }

bool _SecKeychainRollKeys(bool force, CFErrorRef *error)
{
    @autoreleasepool {
        do_if_registered(sec_roll_keys, force, error);

        __block bool result = false;

        secdebug("secitem","enter - %s", __FUNCTION__);
        securityd_send_sync_and_do(kSecXPCOpRollKeys, error,
                                   ^bool(xpc_object_t message, CFErrorRef *error) {
            xpc_dictionary_set_bool(message, "force", force);
            return true;
        },
                                   ^bool(xpc_object_t response, __unused CFErrorRef *error) {
            result = xpc_dictionary_get_bool(response, kSecXPCKeyResult);
            return result;
        });
        return result;
    }
}

static CFArrayRef data_array_to_array_error_request(enum SecXPCOperation op, CFDataRef normalizedIssuer, CFArrayRef accessGroups, CFErrorRef *error) {
    __block CFArrayRef results = NULL;
    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        SecXPCDictionarySetData(message, kSecXPCKeyNormalizedIssuer, normalizedIssuer, error);
        SecXPCDictionarySetPList(message, kSecXPCKeyAccessGroups, accessGroups, error);
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *error) {
        return SecXPCDictionaryCopyArrayOptional(response, kSecXPCKeyResult, &results, error);
    });
    return results;
}

static bool data_data_array_to_bool_error_request(enum SecXPCOperation op, CFDataRef normalizedIssuer, CFDataRef serialNumber, CFArrayRef accessGroups, CFErrorRef *error) {
    __block bool result = false;
    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        SecXPCDictionarySetData(message, kSecXPCKeyNormalizedIssuer, normalizedIssuer, error);
        SecXPCDictionarySetData(message, kSecXPCKeySerialNumber, serialNumber, error);
        SecXPCDictionarySetPList(message, kSecXPCKeyAccessGroups, accessGroups, error);
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *error) {
        result = xpc_dictionary_get_bool(response, kSecXPCKeyResult);
        return result;
    });
    return result;
}

CFArrayRef SecItemCopyParentCertificates_ios(CFDataRef normalizedIssuer, CFArrayRef accessGroups, CFErrorRef *error) {
    @autoreleasepool {
        CFArrayRef results = NULL;

        os_activity_t activity = os_activity_create("SecItemCopyParentCertificates_ios", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
        os_activity_scope(activity);

        results = SECURITYD_XPC(sec_item_copy_parent_certificates, data_array_to_array_error_request, normalizedIssuer, accessGroups, error);

        return results;
    }
}

bool SecItemCertificateExists(CFDataRef normalizedIssuer, CFDataRef serialNumber, CFArrayRef accessGroups, CFErrorRef *error) {
    @autoreleasepool {
        bool results = false;

        os_activity_t activity = os_activity_create("SecItemCertificateExists", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
        os_activity_scope(activity);

        results = SECURITYD_XPC(sec_item_certificate_exists, data_data_array_to_bool_error_request, normalizedIssuer, serialNumber, accessGroups, error);

        return results;
    }
}

OSStatus _SecItemAddAndNotifyOnSync(CFDictionaryRef attributes, CFTypeRef * CF_RETURNS_RETAINED result, void (^syncCallback)(bool didSync, CFErrorRef error)) {
    __block SecCFDictionaryCOW attrs = { attributes };
    OSStatus status = errSecParam;

    os_activity_t activity = os_activity_create("_SecItemAddAndNotifyOnSync", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    status = SecOSStatusWith(^bool(CFErrorRef *statuserror) {
        return SecItemAuthDoQuery(&attrs, NULL, SecItemAdd, statuserror, ^bool(TKClientTokenSession *tokenSession, CFDictionaryRef authedattributes, CFDictionaryRef unused, CFDictionaryRef auth_params, CFErrorRef *autherror) {
            if (tokenSession != NULL) {
                syncCallback(false, NULL);
                return false;
            }

            __block CFTypeRef raw_result = NULL;
            __block CFErrorRef raw_error = NULL;

            id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(false, ^(NSError *error) {
                syncCallback(false, (__bridge CFErrorRef)error);
            });
            if (rpc == NULL) {
                return false;
            }
            SecuritydXPCCallback* xpcCallback = [[SecuritydXPCCallback alloc] initWithCallback: ^void(bool didSync, NSError* error) {
                syncCallback(didSync, (__bridge CFErrorRef) error);
            }];

            dispatch_semaphore_t wait_for_secd = dispatch_semaphore_create(0);
            [rpc SecItemAddAndNotifyOnSync: (__bridge NSDictionary*) authedattributes syncCallback:xpcCallback complete: ^void (NSDictionary* opDictResult, NSArray* opArrayResult, NSError* operror) {
                raw_result = opDictResult  ? CFBridgingRetain(opDictResult)  :
                             opArrayResult ? CFBridgingRetain(opArrayResult) : NULL;
                raw_error = (CFErrorRef) CFBridgingRetain(operror);
                dispatch_semaphore_signal(wait_for_secd);
            }];
            dispatch_semaphore_wait(wait_for_secd, DISPATCH_TIME_FOREVER);

            if(autherror) {
                *autherror = raw_error;
            }

            bool ok = false;

            // SecItemResultProcess isn't intended to handle error cases, so bypass it.
            if(!raw_error) {
                ok = SecItemResultProcess(authedattributes, auth_params, tokenSession, raw_result, result, autherror);
            }
            CFReleaseNull(raw_result);
            return ok;
       });
    });
    CFReleaseNull(attrs.mutable_dictionary);

    return status;
}

void SecItemSetCurrentItemAcrossAllDevices(CFStringRef accessGroup,
                                           CFStringRef identifier,
                                           CFStringRef viewHint,
                                           CFDataRef newCurrentItemReference,
                                           CFDataRef newCurrentItemHash,
                                           CFDataRef oldCurrentItemReference,
                                           CFDataRef oldCurrentItemHash,
                                           void (^complete)(CFErrorRef error))
{
    os_activity_t activity = os_activity_create("SecItemSetCurrentItemAcrossAllDevices", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    @autoreleasepool {
        id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(false, ^(NSError *error) {
            complete((__bridge CFErrorRef) error);
        });
        [rpc secItemSetCurrentItemAcrossAllDevices:(__bridge NSData*)newCurrentItemReference
                                newCurrentItemHash:(__bridge NSData*)newCurrentItemHash
                                       accessGroup:(__bridge NSString*)accessGroup
                                        identifier:(__bridge NSString*)identifier
                                          viewHint:(__bridge NSString*)viewHint
                           oldCurrentItemReference:(__bridge NSData*)oldCurrentItemReference
                                oldCurrentItemHash:(__bridge NSData*)oldCurrentItemHash
                                          complete: ^ (NSError* operror) {
            complete((__bridge CFErrorRef) operror);
        }];
    }
}

void SecItemFetchCurrentItemAcrossAllDevices(CFStringRef accessGroup,
                                             CFStringRef identifier,
                                             CFStringRef viewHint,
                                             bool fetchCloudValue,
                                             void (^complete)(CFDataRef persistentRef, CFErrorRef error))
{
    os_activity_t activity = os_activity_create("SecItemFetchCurrentItemAcrossAllDevices", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    @autoreleasepool {
        id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(false, ^(NSError *error) {
            complete(NULL, (__bridge CFErrorRef) error);
        });
        [rpc secItemFetchCurrentItemAcrossAllDevices:(__bridge NSString*)accessGroup
                                          identifier:(__bridge NSString*)identifier
                                            viewHint:(__bridge NSString*)viewHint
                                     fetchCloudValue:fetchCloudValue
                                            complete: ^(NSData* persistentRef, NSError* operror) {
                                                complete((__bridge CFDataRef) persistentRef, (__bridge CFErrorRef) operror);
                                            }];
    }
}

void _SecItemFetchDigests(NSString *itemClass, NSString *accessGroup, void (^complete)(NSArray<NSDictionary *> *, NSError *))
{
    os_activity_t activity = os_activity_create("_SecItemFetchDigests", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(false, ^(NSError *error) {
        complete(NULL, error);
    });
    [rpc secItemDigest:itemClass accessGroup:accessGroup complete:complete];
}

void _SecKeychainDeleteMultiUser(NSString *musr, void (^complete)(bool, NSError *))
{
    os_activity_t activity = os_activity_create("_SecKeychainDeleteMultiUser", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:musr];
    if (uuid == NULL) {
        complete(false, NULL);
        return;
    }

    uuid_t musrUUID;
    [uuid getUUIDBytes:musrUUID];

    id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(false, ^(NSError *error) {
        complete(false, error);
    });
    [rpc secKeychainDeleteMultiuser:[NSData dataWithBytes:musrUUID length:sizeof(uuid_t)] complete:^(bool status, NSError *error) {
        complete(status, error);
    }];
}

void SecItemVerifyBackupIntegrity(BOOL lightweight,
                                  void(^completion)(NSDictionary<NSString*, NSString*>* results, NSError* error))
{
    @autoreleasepool {
        id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(true, ^(NSError *error) {
            completion(@{@"summary" : @"XPC Error"}, error);
        });
        [rpc secItemVerifyBackupIntegrity:lightweight completion:completion];
    }
}

OSStatus SecItemDeleteKeychainItemsForAppClip(CFStringRef applicationIdentifier)
{
    __block OSStatus status = errSecInternal;
    @autoreleasepool {
        id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(true, ^(NSError *error) {
            secerror("xpc: failure to obtain XPC proxy object for app clip deletion, %@", error);
        });
        [rpc secItemDeleteForAppClipApplicationIdentifier:(__bridge NSString*)applicationIdentifier
                                               completion:^(OSStatus xpcStatus) {
            // Other errors turn into errSecInternal for caller
            secnotice("xpc", "app clip deletion result: %i", (int)xpcStatus);
            if (xpcStatus == errSecMissingEntitlement || xpcStatus == errSecSuccess) {
                status = xpcStatus;
            }
        }];
    }
    return status;
}

OSStatus SecItemPersistKeychainWritesAtHighPerformanceCost(CFErrorRef* error)
{
    os_activity_t activity = os_activity_create("SecItemPersistKeychainWritesAtHighPerformanceCost", OS_ACTIVITY_CURRENT, OS_ACTIVITY_FLAG_DEFAULT);
    os_activity_scope(activity);

    __block OSStatus status = errSecInternal;
    __block CFErrorRef activityError = NULL;
    @autoreleasepool {
        secnotice("xpc", "This process is requesting a expensive full keychain database checkpoint");
        id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(true, ^(NSError *error) {
            secerror("xpc: failure to obtain XPC proxy object for Item Persistence, %@", error);
            activityError = (CFErrorRef)CFBridgingRetain(error);
        });
        [rpc secItemPersistKeychainWritesAtHighPerformanceCost:^(OSStatus xpcStatus,
                                                            NSError* xpcError) {
            if(xpcStatus != errSecSuccess) {
                secerror("xpc: Failed to persist keychain writes: %d %@", (int)xpcStatus, xpcError);
                activityError = (CFErrorRef)CFBridgingRetain(xpcError);
            } else {
                secnotice("xpc", "Successfully persisted keychain data to disk");
            }
            status = xpcStatus;
        }];
    }
    if(activityError) {
        if(error) {
            *error = CFRetainSafe(activityError);
        }
        CFReleaseNull(activityError);
    }
    return status;
}

OSStatus SecItemPromoteAppClipItemsToParentApp(CFStringRef appClipAppID, CFStringRef parentAppID)
{
    __block OSStatus status = errSecInternal;
    @autoreleasepool {
        id<SecuritydXPCProtocol> rpc = SecuritydXPCProxyObject(true, ^(NSError *error) {
            secerror("xpc: failure to obtain XPC proxy object for app clip promotion, %@", error);
        });
        [rpc secItemPromoteItemsForAppClip:(__bridge NSString*)appClipAppID toParentApp:(__bridge NSString*)parentAppID completion:^(OSStatus xpcStatus) {
            secnotice("xpc", "app clip promotion result: %i", (int)xpcStatus);

            if (xpcStatus == errSecMissingEntitlement ||
                xpcStatus == errSecSuccess ||
                xpcStatus == errSecInteractionNotAllowed)
            {
                status = xpcStatus;
            }
        }];
    }
    return status;
}
