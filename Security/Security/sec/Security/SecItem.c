;
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
 * SecItem.c - CoreFoundation-based constants and functions for
    access to Security items (certificates, keys, identities, and
    passwords.)
 */

#include <Security/SecBasePriv.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemInternal.h>
#include <Security/SecAccessControl.h>
#include <Security/SecAccessControlPriv.h>
#ifndef SECITEM_SHIM_OSX
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecRandom.h>
#include <Security/SecBasePriv.h>
#endif // *** END SECITEM_SHIM_OSX ***
#include <Security/SecTask.h>
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
#include <utilities/array_size.h>
#include <utilities/debugging.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecIOFormat.h>
#include <utilities/SecXPCError.h>
#include <utilities/der_plist.h>
#include <assert.h>

#include <Security/SecInternal.h>
#include <TargetConditionals.h>
#include <ipc/securityd_client.h>
#include <Security/SecuritydXPC.h>
#include <AssertMacros.h>
#include <asl.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#ifndef SECITEM_SHIM_OSX
#include <libDER/asn1Types.h>
#endif // *** END SECITEM_SHIM_OSX ***

/* label when certificate data is joined with key data */
#define CERTIFICATE_DATA_COLUMN_LABEL "certdata" 

#include <utilities/SecDb.h>
#include <IOKit/IOReturn.h>

/* Return an OSStatus for a sqlite3 error code. */
static OSStatus osstatus_for_s3e(int s3e)
{
	if (s3e > 0 && s3e <= SQLITE_DONE) switch (s3e)
	{
        case SQLITE_OK:
            return 0;
        case SQLITE_ERROR:
            return errSecNotAvailable; /* errSecDuplicateItem; */
        case SQLITE_FULL: /* Happens if we run out of uniqueids */
            return errSecNotAvailable; /* TODO: Replace with a better error code. */
        case SQLITE_PERM:
        case SQLITE_READONLY:
            return errSecNotAvailable;
        case SQLITE_CANTOPEN:
            return errSecNotAvailable;
        case SQLITE_EMPTY:
            return errSecNotAvailable;
        case SQLITE_CONSTRAINT:
            return errSecDuplicateItem;
        case SQLITE_ABORT:
            return -1;
        case SQLITE_MISMATCH:
            return errSecNoSuchAttr;
        case SQLITE_AUTH:
            return errSecNotAvailable;
        case SQLITE_NOMEM:
            return -2; /* TODO: Replace with a real error code. */
        case SQLITE_INTERNAL:
        default:
            return errSecNotAvailable; /* TODO: Replace with a real error code. */
	}
    return s3e;
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
        } else {
            secnotice("securityd", "unknown error domain: %@ for error: %@", domain, error);
            status = errSecInternal;
        }
    }
    return status;
}

// Wrapper to provide a CFErrorRef for legacy API.
OSStatus SecOSStatusWith(bool (^perform)(CFErrorRef *error)) {
    CFErrorRef error = NULL;
    OSStatus status;
    if (perform(&error)) {
        assert(error == NULL);
        status = errSecSuccess;
    } else {
        assert(error);
        status = SecErrorGetOSStatus(error);
        if (status != errSecItemNotFound)           // Occurs in normal operation, so exclude
            secerror("error:[%" PRIdOSStatus "] %@", status, error);
        CFReleaseNull(error);
    }
    return status;
}


/* IPC uses CFPropertyList to un/marshall input/output data and can handle:
   CFData, CFString, CFArray, CFDictionary, CFDate, CFBoolean, and CFNumber

   Currently in need of conversion below:
   @@@ kSecValueRef allows SecKeychainItemRef and SecIdentityRef
   @@@ kSecMatchPolicy allows a query with a SecPolicyRef
   @@@ kSecUseItemList allows a query against a list of itemrefs, this isn't
       currently implemented at all, but when it is needs to short circuit to
	   local evaluation, different from the sql query abilities
*/

#ifndef SECITEM_SHIM_OSX
static CFDictionaryRef
SecItemCopyAttributeDictionary(CFTypeRef ref) {
	CFDictionaryRef refDictionary = NULL;
	CFTypeID typeID = CFGetTypeID(ref);
	if (typeID == SecKeyGetTypeID()) {
		refDictionary = SecKeyCopyAttributeDictionary((SecKeyRef)ref);
	} else if (typeID == SecCertificateGetTypeID()) {
		refDictionary =
			SecCertificateCopyAttributeDictionary((SecCertificateRef)ref);
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
                CFDictionarySetValue((CFMutableDictionaryRef)refDictionary, 
                    CFSTR(CERTIFICATE_DATA_COLUMN_LABEL), data);
            }
            CFReleaseNull(key_dict);
            CFReleaseNull(data);
        }
        CFReleaseNull(cert);
        CFReleaseNull(key);
    } else {
		refDictionary = NULL;
	}
	return refDictionary;
}

static CFTypeRef
SecItemCreateFromAttributeDictionary(CFDictionaryRef refAttributes) {
	CFTypeRef ref = NULL;
	CFStringRef class = CFDictionaryGetValue(refAttributes, kSecClass);
	if (CFEqual(class, kSecClassCertificate)) {
		ref = SecCertificateCreateFromAttributeDictionary(refAttributes);
	} else if (CFEqual(class, kSecClassKey)) {
		ref = SecKeyCreateFromAttributeDictionary(refAttributes);
	} else if (CFEqual(class, kSecClassIdentity)) {
		CFAllocatorRef allocator = NULL;
		CFDataRef data = CFDictionaryGetValue(refAttributes, CFSTR(CERTIFICATE_DATA_COLUMN_LABEL));
		SecCertificateRef cert = SecCertificateCreateWithData(allocator, data);
		SecKeyRef key = SecKeyCreateFromAttributeDictionary(refAttributes);
		if (key && cert)
			ref = SecIdentityCreate(allocator, cert, key);
		CFReleaseSafe(cert);
		CFReleaseSafe(key);
#if 0
	/* We don't support SecKeychainItemRefs yet. */
	} else if (CFEqual(class, kSecClassGenericPassword)) {
	} else if (CFEqual(class, kSecClassInternetPassword)) {
	} else if (CFEqual(class, kSecClassAppleSharePassword)) {
#endif
	} else {
		ref = NULL;
	}
	return ref;
}
#else

extern CFTypeRef SecItemCreateFromAttributeDictionary(CFDictionaryRef refAttributes);

#endif

OSStatus
SecItemCopyDisplayNames(CFArrayRef items, CFArrayRef *displayNames)
{
    // @@@ TBI
    return -1 /* errSecUnimplemented */;
}

#ifndef SECITEM_SHIM_OSX
static void merge_dictionary_by_overwrite(const void *key, const void *value, void *context)
{
	if (!CFEqual(key, kSecValueRef))
		CFDictionarySetValue((CFMutableDictionaryRef)context, key, value);
}

static void copy_applier(const void *key, const void *value, void *context)
{
    CFDictionarySetValue(context, key, value);
}

static OSStatus cook_query(CFDictionaryRef query, CFMutableDictionaryRef *explode)
{
	/* If a ref was specified we get it's attribute dictionary and parse it. */
	CFMutableDictionaryRef args = NULL;
	CFTypeRef value = CFDictionaryGetValue(query, kSecValueRef);
    if (value) {
		CFDictionaryRef refAttributes = SecItemCopyAttributeDictionary(value);
		if (!refAttributes)
			return errSecValueRefUnsupported;
			
		/* Replace any attributes we already got from the ref with the ones
		   from the attributes dictionary the caller passed us.  This allows
		   a caller to add an item using attributes from the ref and still
		   override some of them in the dictionary directly.  */
		args = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, refAttributes);
		CFRelease(refAttributes);
		CFDictionaryApplyFunction(query, merge_dictionary_by_overwrite, args);
	}
    value = CFDictionaryGetValue(query, kSecAttrIssuer);
    if (value) {
        /* convert DN to canonical issuer, if value is DN (top level sequence) */
        const DERItem name = { (unsigned char *)CFDataGetBytePtr(value), CFDataGetLength(value) };
        DERDecodedInfo content;
        if (!DERDecodeItem(&name, &content) &&
            (content.tag == ASN1_CONSTR_SEQUENCE))
        {
            CFDataRef canonical_issuer = createNormalizedX501Name(kCFAllocatorDefault, &content.content);
            if (canonical_issuer) {
                if (!args) {
                    args = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                    /* This is necessary because we rely on non NULL callbacks */
                    CFDictionaryApplyFunction(query, copy_applier, args);
                }
                /* Overwrite with new issuer */
                CFDictionarySetValue(args, kSecAttrIssuer, canonical_issuer);
                CFRelease(canonical_issuer);
            }
        }
    }
	*explode = args;
	return errSecSuccess;
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
                            sqlite_int64 rowid;
                            if (_SecItemParsePersistentRef(result, NULL, &rowid)) {
                                *return_result = _SecItemMakePersistentRef(kSecClassIdentity, rowid);
                            }
                        }
                        CFRelease(result);
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

static void infer_cert_label(CFDictionaryRef attributes, CFMutableDictionaryRef args)
{
	if (!args || !attributes)
		return;
	
	if (CFDictionaryContainsKey(attributes, kSecAttrLabel))
		return;
	
	CFTypeRef value_ref = CFDictionaryGetValue(attributes, kSecValueRef);
	if (!value_ref || (CFGetTypeID(value_ref) != SecCertificateGetTypeID()))
		return;
	
	SecCertificateRef certificate = (SecCertificateRef)value_ref;
	CFStringRef label = SecCertificateCopySubjectSummary(certificate);
	if (label) {
		CFDictionarySetValue(args, kSecAttrLabel, label);
		CFReleaseNull(label);
	}
}

/* A persistent ref is just the class and the rowid of the record. */
CF_RETURNS_RETAINED CFDataRef _SecItemMakePersistentRef(CFTypeRef class, sqlite_int64 rowid)
{
    uint8_t bytes[sizeof(sqlite_int64) + 4];
    if (rowid < 0)
        return NULL;
    if (CFStringGetCString(class, (char *)bytes, 4 + 1 /*null-term*/, 
        kCFStringEncodingUTF8))
    {
        OSWriteBigInt64(bytes + 4, 0, rowid);
        return CFDataCreate(NULL, bytes, sizeof(bytes));
    }
    return NULL;
}

/* AUDIT[securityd](done):
   persistent_ref (ok) is a caller provided, non NULL CFTypeRef.
 */
bool _SecItemParsePersistentRef(CFDataRef persistent_ref, CFStringRef *return_class, sqlite_int64 *return_rowid)
{
	bool valid_ref = false;
    if (CFGetTypeID(persistent_ref) == CFDataGetTypeID() &&
        CFDataGetLength(persistent_ref) == (CFIndex)(sizeof(sqlite_int64) + 4)) {
        const uint8_t *bytes = CFDataGetBytePtr(persistent_ref);
        sqlite_int64 rowid = OSReadBigInt64(bytes + 4, 0);
		
        CFStringRef class = CFStringCreateWithBytes(kCFAllocatorDefault, 
            bytes, CFStringGetLength(kSecClassGenericPassword), 
            kCFStringEncodingUTF8, true);
        const void *valid_classes[] = { kSecClassGenericPassword,
            kSecClassInternetPassword,
            kSecClassAppleSharePassword,
            kSecClassCertificate,
            kSecClassKey,
            kSecClassIdentity };
        
        unsigned i;
        for (i=0; i< array_size(valid_classes); i++) {
            if (CFEqual(valid_classes[i], class)) {
                if (return_class)
                    *return_class = valid_classes[i];
                if (return_rowid)
                    *return_rowid = rowid;
                valid_ref = true;
                break;
            }
        }
        CFRelease(class);
    }
    return valid_ref;
}

#endif // *** END SECITEM_SHIM_OSX ***

static bool cf_bool_value(CFTypeRef cf_bool)
{
	return (cf_bool && CFEqual(kCFBooleanTrue, cf_bool));
}

/* Turn the returned dictionary that contains all the attributes to create a
 ref into the exact result the client asked for */
CF_RETURNS_RETAINED
static CFTypeRef makeResult(CFTypeRef ref, bool return_ref, bool return_data, bool return_attributes, bool return_persistentref)
{
    CFTypeRef result = NULL;
    if (!ref || (CFGetTypeID(ref) != CFDictionaryGetTypeID()))
        return CFRetainSafe(ref);

    CFTypeRef returned_ref = return_ref ? SecItemCreateFromAttributeDictionary(ref) : NULL;

    if (return_data || return_attributes || return_persistentref) {
        if (return_attributes)
            result = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, ref);
        else
            result = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        if (returned_ref) {
            CFDictionarySetValue((CFMutableDictionaryRef)result, kSecValueRef, returned_ref);
            CFRelease(returned_ref);
        }

        if (return_data) {
            CFTypeRef r_data = CFDictionaryGetValue(ref, kSecValueData);
            if (r_data)
                CFDictionarySetValue((CFMutableDictionaryRef)result, kSecValueData, r_data);
        } else {
            CFDictionaryRemoveValue((CFMutableDictionaryRef)result, kSecValueData);
        }

        if (return_persistentref) {
            CFTypeRef persistent_ref = CFDictionaryGetValue(ref, kSecValuePersistentRef);
            if (persistent_ref)
                CFDictionarySetValue((CFMutableDictionaryRef)result, kSecValuePersistentRef, persistent_ref);
        }

        CFDataRef ac_data = CFDictionaryGetValue(result, kSecAttrAccessControl);
        if (ac_data) {
            SecAccessControlRef ac = SecAccessControlCreateFromData(kCFAllocatorDefault, ac_data, NULL);
            if (ac) {
                CFDictionarySetValue((CFMutableDictionaryRef)result, kSecAttrAccessControl, ac);
                CFRelease(ac);
            }
        }
    } else if (return_ref)
        result = returned_ref;

    return result;
}

static void
result_post(CFDictionaryRef query, CFTypeRef raw_result, CFTypeRef *result) {
    if (!raw_result)
        return;

    if (!result) {
        CFRelease(raw_result);
        return;
    }

	bool return_ref = cf_bool_value(CFDictionaryGetValue(query, kSecReturnRef));
	bool return_data = cf_bool_value(CFDictionaryGetValue(query, kSecReturnData));
	bool return_attributes = cf_bool_value(CFDictionaryGetValue(query, kSecReturnAttributes));
    bool return_persistentref = cf_bool_value(CFDictionaryGetValue(query, kSecReturnPersistentRef));

    if (CFGetTypeID(raw_result) == CFArrayGetTypeID()) {
        CFIndex i, count = CFArrayGetCount(raw_result);
        CFMutableArrayRef tmp_array = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
        for (i = 0; i < count; i++) {
            CFTypeRef ref = makeResult(CFArrayGetValueAtIndex(raw_result, i), return_ref, return_data, return_attributes,
                                       return_persistentref);
            if (ref) {
                CFArrayAppendValue(tmp_array, ref);
                CFRelease(ref);
            }
        }
        *result = tmp_array;
    } else
        *result = makeResult(raw_result, return_ref, return_data, return_attributes, return_persistentref);

    CFRelease(raw_result);
}

static void attributes_pre(CFDictionaryRef attributes, CFMutableDictionaryRef *result) {
    CFDataRef data = NULL;
    SecAccessControlRef access_control = NULL;
    *result = NULL;
    access_control = (SecAccessControlRef)CFDictionaryGetValue(attributes, kSecAttrAccessControl);
    require_quiet(access_control, out);
    data = SecAccessControlCopyData(access_control);
    require_quiet(data, out);
    *result = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, attributes);
    CFDictionarySetValue(*result, kSecAttrAccessControl, data);

out:
    CFReleaseSafe(data);
}

#if SECITEM_SHIM_OSX
/* TODO: Should be in some header */
OSStatus SecItemAdd_ios(CFDictionaryRef attributes, CFTypeRef *result);
OSStatus SecItemCopyMatching_ios(CFDictionaryRef query, CFTypeRef *result);
OSStatus SecItemUpdate_ios(CFDictionaryRef query, CFDictionaryRef attributesToUpdate);
OSStatus SecItemDelete_ios(CFDictionaryRef query);
#endif

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

bool cftype_ag_to_bool_cftype_error_request(enum SecXPCOperation op, CFTypeRef attributes, __unused CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef *error) {
    return cftype_to_bool_cftype_error_request(op, attributes, result, error);
}

static bool dict_to_error_request(enum SecXPCOperation op, CFDictionaryRef query, CFErrorRef *error)
{
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetPList(message, kSecXPCKeyQuery, query, error);
    }, NULL);
}

static bool dict_ag_to_error_request(enum SecXPCOperation op, CFDictionaryRef query, __unused CFArrayRef accessGroups, CFErrorRef *error)
{
    return dict_to_error_request(op, query, error);
}

static CFDataRef data_data_to_data_error_request(enum SecXPCOperation op, CFDataRef keybag, CFDataRef passcode, CFErrorRef *error) {
    __block CFDataRef result = NULL;
    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetDataOptional(message, kSecXPCKeyKeybag, keybag, error)
        && SecXPCDictionarySetDataOptional(message, kSecXPCKeyUserPassword, passcode, error);
    }, ^bool(xpc_object_t response, CFErrorRef *error) {
        return (result = SecXPCDictionaryCopyData(response, kSecXPCKeyResult, error));
    });
    return result;
}

static bool data_data_data_to_error_request(enum SecXPCOperation op, CFDataRef backup, CFDataRef keybag, CFDataRef passcode, CFErrorRef *error) {
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetData(message, kSecXPCKeyBackup, backup, error)
        && SecXPCDictionarySetData(message, kSecXPCKeyKeybag, keybag, error)
        && SecXPCDictionarySetDataOptional(message, kSecXPCKeyUserPassword, passcode, error);
    } , NULL);
}

static bool dict_data_data_to_error_request(enum SecXPCOperation op, CFDictionaryRef backup, CFDataRef keybag, CFDataRef passcode, CFErrorRef *error) {
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetPList(message, kSecXPCKeyBackup, backup, error)
        && SecXPCDictionarySetData(message, kSecXPCKeyKeybag, keybag, error)
        && SecXPCDictionarySetDataOptional(message, kSecXPCKeyUserPassword, passcode, error);
    } , NULL);
}

static CFDictionaryRef data_data_dict_to_dict_error_request(enum SecXPCOperation op, CFDictionaryRef backup, CFDataRef keybag, CFDataRef passcode, CFErrorRef *error) {
    __block CFDictionaryRef dict = NULL;
    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetPListOptional(message, kSecXPCKeyBackup, backup, error)
        && SecXPCDictionarySetData(message, kSecXPCKeyKeybag, keybag, error)
        && SecXPCDictionarySetDataOptional(message, kSecXPCKeyUserPassword, passcode, error);
    }, ^bool(xpc_object_t response, CFErrorRef *error) {
        return (dict = SecXPCDictionaryCopyDictionary(response, kSecXPCKeyResult, error));
    });
    return dict;
}

OSStatus
#if SECITEM_SHIM_OSX
SecItemAdd_ios(CFDictionaryRef attributes, CFTypeRef *result)
#else
SecItemAdd(CFDictionaryRef attributes, CFTypeRef *result)
#endif // *** END SECITEM_SHIM_OSX ***
{
    CFMutableDictionaryRef args1 = NULL, args2 = NULL;
    OSStatus status;

#ifndef SECITEM_SHIM_OSX
    require_quiet(!explode_identity(attributes, (secitem_operation)SecItemAdd, &status, result), errOut);
	require_noerr_quiet(status = cook_query(attributes, &args1), errOut);
	infer_cert_label(attributes, args1);
	if (args1)
		attributes = args1;
#endif // *** END SECITEM_SHIM_OSX ***

    SecAccessControlRef access_control = (SecAccessControlRef)CFDictionaryGetValue(attributes, kSecAttrAccessControl);
    if(access_control && SecAccessControlGetConstraints(access_control) && CFEqualSafe(CFDictionaryGetValue(attributes, kSecAttrSynchronizable), kCFBooleanTrue))
        require_noerr_quiet(status = errSecParam, errOut);
    
    attributes_pre(attributes, &args2);
    if (args2)
        attributes = args2;

    require_noerr_quiet(status = SecOSStatusWith(^bool (CFErrorRef *error) {
        CFTypeRef raw_result = NULL;
        if (!SECURITYD_XPC(sec_item_add, cftype_ag_to_bool_cftype_error_request, attributes, SecAccessGroupsGetCurrent(), &raw_result, error))
            return false;

        result_post(attributes, raw_result, result);
        return true;
    }), errOut);

errOut:
    CFReleaseSafe(args1);
    CFReleaseSafe(args2);
	return status;
}


OSStatus
#if SECITEM_SHIM_OSX
SecItemCopyMatching_ios(CFDictionaryRef inQuery, CFTypeRef *result)
#else
SecItemCopyMatching(CFDictionaryRef inQuery, CFTypeRef *result)
#endif // *** END SECITEM_SHIM_OSX ***
{
    __block CFDictionaryRef query = inQuery;
    __block CFMutableDictionaryRef args1 = NULL, args2 = NULL;
    OSStatus status;

#ifndef SECITEM_SHIM_OSX
    require_quiet(!explode_identity(query, (secitem_operation)SecItemCopyMatching, &status, result), errOut);
    require_noerr_quiet(status = cook_query(query, &args1), errOut);
    if (args1)
        query = args1;
#endif // *** END SECITEM_SHIM_OSX ***

    attributes_pre(query, &args2);
    if (args2)
        query = args2;

    require_noerr_quiet(status = SecOSStatusWith(^bool (CFErrorRef *error) {
        CFTypeRef raw_result = NULL;
        if (!SECURITYD_XPC(sec_item_copy_matching, cftype_ag_to_bool_cftype_error_request, query, SecAccessGroupsGetCurrent(), &raw_result, error))
            return false;

        result_post(query, raw_result, result);
        return true;
    }), errOut);

errOut:
    CFReleaseSafe(args1);
    CFReleaseSafe(args2);
    return status;
}

OSStatus
#if SECITEM_SHIM_OSX
SecItemUpdate_ios(CFDictionaryRef query, CFDictionaryRef attributesToUpdate)
#else
SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate)
#endif // *** END SECITEM_SHIM_OSX ***
{
    CFMutableDictionaryRef args1 = NULL, args2 = NULL, args3 = NULL;
    __block OSStatus status; // TODO loose block once gSecurityd functions return CFErrorRefs
#ifndef SECITEM_SHIM_OSX
    require_noerr_quiet(status = cook_query(query, &args1), errOut);
    if (args1)
        query = args1;
#endif

    attributes_pre(attributesToUpdate, &args2);
    if (args2)
        attributesToUpdate = args2;

    attributes_pre(query, &args3);
    if (args3)
        query = args3;

    require_noerr_quiet(status = SecOSStatusWith(^bool (CFErrorRef *error) {
        bool ok = false;
        if (gSecurityd) {
            // Ensure the dictionary passed to securityd has proper kCFTypeDictionaryKeyCallBacks.
            CFMutableDictionaryRef tmp = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
            CFDictionaryForEach(attributesToUpdate, ^(const void *key, const void *value) { CFDictionaryAddValue(tmp, key, value); });
            ok = gSecurityd->sec_item_update(query, tmp, SecAccessGroupsGetCurrent(), error);
            CFRelease(tmp);
        } else {
            xpc_object_t message = securityd_create_message(sec_item_update_id, error);
            if (message) {
                if (SecXPCDictionarySetPList(message, kSecXPCKeyQuery, query, error) &&
                    SecXPCDictionarySetPList(message, kSecXPCKeyAttributesToUpdate, attributesToUpdate, error)) {
                    xpc_object_t reply = securityd_message_with_reply_sync(message, error);
                    if (reply) {
                        ok = securityd_message_no_error(reply, error);
                        xpc_release(reply);
                    }
                }
                xpc_release(message);
            }
        }
        return ok;
    }), errOut);

errOut:
    CFReleaseSafe(args1);
    CFReleaseSafe(args2);
    CFReleaseSafe(args3);
    return status;
}

#ifndef SECITEM_SHIM_OSX
static void copy_all_keys_and_values(const void *key, const void *value, void *context)
{
    CFDictionaryAddValue((CFMutableDictionaryRef)context, key, value);
}
#endif 

#ifndef SECITEM_SHIM_OSX
static OSStatus explode_persistent_identity_ref(CFDictionaryRef query, CFMutableDictionaryRef *delete_query)
{
    OSStatus status = errSecSuccess;
    CFTypeRef persist = CFDictionaryGetValue(query, kSecValuePersistentRef);
    CFStringRef class;
    if (persist && _SecItemParsePersistentRef(persist, &class, NULL)
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
        CFMutableDictionaryRef new_query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (new_query) {
            CFDictionaryApplyFunction(query, copy_all_keys_and_values, new_query);
            CFDictionaryRemoveValue(new_query, kSecValuePersistentRef);
            CFDictionarySetValue(new_query, kSecValueRef, item_query);
            *delete_query = new_query;
        } else
            status = errSecAllocate;
        CFRelease(item_query);
    }

    return status;
}
#endif

OSStatus
#if SECITEM_SHIM_OSX
SecItemDelete_ios(CFDictionaryRef query)
#else
SecItemDelete(CFDictionaryRef query)
#endif // *** END SECITEM_SHIM_OSX *** 
{
	CFMutableDictionaryRef args1 = NULL, args2 = NULL, args3 = NULL;
    OSStatus status;

#ifndef SECITEM_SHIM_OSX
    require_noerr_quiet(status = explode_persistent_identity_ref(query, &args1), errOut);
    if (args1)
        query = args1;
    require_quiet(!explode_identity(query, (secitem_operation)SecItemDelete, &status, NULL), errOut);
	require_noerr_quiet(status = cook_query(query, &args2), errOut);
    if (args2)
        query = args2;
#endif // *** END SECITEM_SHIM_OSX ***

    attributes_pre(query, &args3);
    if (args3)
        query = args3;

    require_noerr_quiet(status = SecOSStatusWith(^bool (CFErrorRef *error) {
        return SECURITYD_XPC(sec_item_delete, dict_ag_to_error_request, query, SecAccessGroupsGetCurrent(), error);
    }), errOut);
    
errOut:
    CFReleaseSafe(args1);
    CFReleaseSafe(args2);
    CFReleaseSafe(args3);
	return status;
}

OSStatus
SecItemDeleteAll(void)
{
    return SecOSStatusWith(^bool (CFErrorRef *error) {
        bool ok;
        if (gSecurityd) {
            ok = true;
#ifndef SECITEM_SHIM_OSX
            SecTrustStoreRef ts = SecTrustStoreForDomain(kSecTrustStoreDomainUser);
            if (!gSecurityd->sec_truststore_remove_all(ts, error))
                ok = SecError(errSecInternal, error, CFSTR("sec_truststore_remove_all is NULL"));
#endif // *** END SECITEM_SHIM_OSX ***
            if (!gSecurityd->sec_item_delete_all(error))
                ok = SecError(errSecInternal, error, CFSTR("sec_item_delete_all is NULL"));
        } else {
            ok = securityd_send_sync_and_do(sec_delete_all_id, error, NULL, NULL);
        }
        return ok;
    });
}

CFDataRef _SecKeychainCopyOTABackup(void) {
    return SECURITYD_XPC(sec_keychain_backup, data_data_to_data_error_request, NULL, NULL, NULL);
}

CFDataRef _SecKeychainCopyBackup(CFDataRef backupKeybag, CFDataRef password) {
    return SECURITYD_XPC(sec_keychain_backup, data_data_to_data_error_request, backupKeybag, password, NULL);
}

OSStatus _SecKeychainRestoreBackup(CFDataRef backup, CFDataRef backupKeybag,
    CFDataRef password) {
    return SecOSStatusWith(^bool (CFErrorRef *error) {
        return SECURITYD_XPC(sec_keychain_restore, data_data_data_to_error_request, backup, backupKeybag, password, error);
    });
}

CFArrayRef _SecKeychainSyncUpdateKeyParameter(CFDictionaryRef updates, CFErrorRef *error) {
    
    return SECURITYD_XPC(sec_keychain_sync_update_key_parameter, dict_to_array_error_request, updates, error);
}

CFArrayRef _SecKeychainSyncUpdateCircle(CFDictionaryRef updates, CFErrorRef *error) {
    
    return SECURITYD_XPC(sec_keychain_sync_update_circle, dict_to_array_error_request, updates, error);
}

CFArrayRef _SecKeychainSyncUpdateMessage(CFDictionaryRef updates, CFErrorRef *error) {
    
    return SECURITYD_XPC(sec_keychain_sync_update_message, dict_to_array_error_request, updates, error);
}

OSStatus _SecKeychainBackupSyncable(CFDataRef keybag, CFDataRef password, CFDictionaryRef backup_in, CFDictionaryRef *backup_out)
{
    return SecOSStatusWith(^bool (CFErrorRef *error) {
        *backup_out = SECURITYD_XPC(sec_keychain_backup_syncable, data_data_dict_to_dict_error_request, backup_in, keybag, password, error);
        return *backup_out != NULL;
    });
}

OSStatus _SecKeychainRestoreSyncable(CFDataRef keybag, CFDataRef password, CFDictionaryRef backup_in)
{
    return SecOSStatusWith(^bool (CFErrorRef *error) {
        return SECURITYD_XPC(sec_keychain_restore_syncable, dict_data_data_to_error_request, backup_in, keybag, password, error);
    });
}

#ifndef SECITEM_SHIM_OSX
OSStatus SecTaskValidateForRequirement(SecTaskRef task, CFStringRef requirement);

OSStatus SecTaskValidateForRequirement(SecTaskRef task, CFStringRef requirement)
{
    return -1; /* this is only on OS X currently */
}

#else

extern OSStatus SecTaskValidateForRequirement(SecTaskRef task, CFStringRef requirement);

#endif

#define do_if_registered(sdp, ...) if (gSecurityd && gSecurityd->sdp) { return gSecurityd->sdp(__VA_ARGS__); }

bool _SecKeychainRollKeys(bool force, CFErrorRef *error)
{
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


