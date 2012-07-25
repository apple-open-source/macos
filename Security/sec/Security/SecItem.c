/*
 * Copyright (c) 2006-2010 Apple Inc. All Rights Reserved.
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

#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemInternal.h>
#ifndef SECITEM_SHIM_OSX
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecRandom.h>
#include <Security/SecBasePriv.h>
#endif // *** END SECITEM_SHIM_OSX ***
#include <errno.h>
#include <limits.h>
#include <pthread.h>
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
#include <security_utilities/debugging.h>
#include <assert.h>
#include <Security/SecInternal.h>
#include <TargetConditionals.h>
#include "securityd_client.h"
#include "securityd_server.h"
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

/* Turn the returned dictionary that contains all the attributes to create a
   ref into the exact result the client asked for */
static CFTypeRef makeRef(CFTypeRef ref, bool return_data, bool return_attributes)
{
	CFTypeRef result = NULL;
	if (!ref || (CFGetTypeID(ref) != CFDictionaryGetTypeID()))
		return NULL;

	CFTypeRef return_ref = SecItemCreateFromAttributeDictionary(ref);

	if (return_data || return_attributes) {
		if (return_attributes)
			result = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, ref);
		else
			result = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

		if (return_ref) {
			CFDictionarySetValue((CFMutableDictionaryRef)result, kSecValueRef, return_ref);
			CFRelease(return_ref);
		}

		if (!return_data)
			CFDictionaryRemoveValue((CFMutableDictionaryRef)result, kSecValueData);
	} else
		result = return_ref;

	return result;
}

OSStatus
SecItemCopyDisplayNames(CFArrayRef items, CFArrayRef *displayNames)
{
    // @@@ TBI
    return -1 /* unimpErr */;
}


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
	return noErr;
}

typedef OSStatus (*secitem_operation)(CFDictionaryRef attributes, ...);

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
					            vals, (sizeof(keys) / sizeof(*keys)), 
								NULL, NULL);
						if (query_dict)
							if (noErr == SecItemCopyMatching(query_dict, NULL))
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
CFDataRef _SecItemMakePersistentRef(CFTypeRef class, sqlite_int64 rowid)
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
        for (i=0; i< sizeof(valid_classes)/sizeof(*valid_classes); i++) {
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

static bool cf_bool_value(CFTypeRef cf_bool)
{
	return (cf_bool && CFEqual(kCFBooleanTrue, cf_bool));
}

static void
result_post(CFDictionaryRef query, CFTypeRef raw_result, CFTypeRef *result) {
    if (!raw_result)
        return;

	bool return_ref = cf_bool_value(CFDictionaryGetValue(query, kSecReturnRef));
	bool return_data = cf_bool_value(CFDictionaryGetValue(query, kSecReturnData));
	bool return_attributes = cf_bool_value(CFDictionaryGetValue(query, kSecReturnAttributes));

	if (return_ref) {
		if (CFGetTypeID(raw_result) == CFArrayGetTypeID()) {
			CFIndex i, count = CFArrayGetCount(raw_result);
			CFMutableArrayRef tmp_array = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
			for (i = 0; i < count; i++) {
				CFTypeRef ref = makeRef(CFArrayGetValueAtIndex(raw_result, i), return_data, return_attributes);
				if (ref) {
					CFArrayAppendValue(tmp_array, ref);
                    CFRelease(ref);
                }
			}
			*result = tmp_array;
		} else
			*result = makeRef(raw_result, return_data, return_attributes);

		CFRelease(raw_result);
	} else
		*result = raw_result;
}
#endif // *** END SECITEM_SHIM_OSX ***

OSStatus
#if SECITEM_SHIM_OSX
SecItemAdd_ios(CFDictionaryRef attributes, CFTypeRef *result)
#else
SecItemAdd(CFDictionaryRef attributes, CFTypeRef *result)
#endif // *** END SECITEM_SHIM_OSX ***
{
    CFMutableDictionaryRef args = NULL;
    CFTypeRef raw_result = NULL;
    OSStatus status;

#ifndef SECITEM_SHIM_OSX
    require_quiet(!explode_identity(attributes, (secitem_operation)SecItemAdd, &status, result), errOut);
	require_noerr_quiet(status = cook_query(attributes, &args), errOut);
	infer_cert_label(attributes, args);
#endif // *** END SECITEM_SHIM_OSX ***
	if (args)
		attributes = args;
    status = SECURITYD_AG(sec_item_add, attributes, &raw_result);
#ifndef SECITEM_SHIM_OSX
    result_post(attributes, raw_result, result);
#endif // *** END SECITEM_SHIM_OSX ***

errOut:
    CFReleaseSafe(args);
	return status;
}

OSStatus
#if SECITEM_SHIM_OSX
SecItemCopyMatching_ios(CFDictionaryRef query, CFTypeRef *result)
#else
SecItemCopyMatching(CFDictionaryRef query, CFTypeRef *result)
#endif // *** END SECITEM_SHIM_OSX ***
{
    CFMutableDictionaryRef args = NULL;
    CFTypeRef raw_result = NULL;
    OSStatus status;

#ifndef SECITEM_SHIM_OSX
    require_quiet(!explode_identity(query, (secitem_operation)SecItemCopyMatching, &status, result), errOut);
    require_noerr_quiet(status = cook_query(query, &args), errOut);
#endif // *** END SECITEM_SHIM_OSX ***    
	if (args)
		query = args;

    status = SECURITYD_AG(sec_item_copy_matching, query, &raw_result);
#ifndef SECITEM_SHIM_OSX
    result_post(query, raw_result, result);
#endif // *** END SECITEM_SHIM_OSX ***    

errOut:
    CFReleaseSafe(args);
	return status;
}

OSStatus
#if SECITEM_SHIM_OSX
SecItemUpdate_ios(CFDictionaryRef query, CFDictionaryRef attributesToUpdate)
#else
SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate)
#endif // *** END SECITEM_SHIM_OSX ***
{
    CFMutableDictionaryRef args = NULL;
    OSStatus status;

#ifndef SECITEM_SHIM_OSX
	require_noerr_quiet(status = cook_query(query, &args), errOut);
#endif // *** END SECITEM_SHIM_OSX ***     
    if (args)
        query = args;

    if (gSecurityd) {
        status = gSecurityd->sec_item_update(query, attributesToUpdate, SecAccessGroupsGetCurrent());
    } else {
        const void *values[] = { (const void *)query, (const void *)attributesToUpdate };
        CFArrayRef pair = CFArrayCreate(kCFAllocatorDefault, values, 2, NULL/*&kCFTypeArrayCallBacks*/);
        if (pair) {
            status = ServerCommandSendReceive(sec_item_update_id, pair, NULL);
            CFRelease(pair);
        } else {
            status = errSecAllocate;
        }
    }

errOut:
    CFReleaseSafe(args);
	return status;
}

static void copy_all_keys_and_values(const void *key, const void *value, void *context)
{
    CFDictionaryAddValue((CFMutableDictionaryRef)context, key, value);
}

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
            vals, (sizeof(keys) / sizeof(*keys)), NULL, NULL);
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

OSStatus
#if SECITEM_SHIM_OSX
SecItemDelete_ios(CFDictionaryRef query)
#else
SecItemDelete(CFDictionaryRef query)
#endif // *** END SECITEM_SHIM_OSX *** 
{
	CFMutableDictionaryRef args1 = NULL, args2 = NULL;
    OSStatus status;

#ifndef SECITEM_SHIM_OSX
    require_noerr_quiet(status = explode_persistent_identity_ref(query, &args1), errOut);
    if (args1)
        query = args1;
    require_quiet(!explode_identity(query, (secitem_operation)SecItemDelete, &status, NULL), errOut);
	require_noerr_quiet(status = cook_query(query, &args2), errOut);
#endif // *** END SECITEM_SHIM_OSX ***    
    if (args2)
        query = args2;

    if (gSecurityd) {
        status = gSecurityd->sec_item_delete(query,
            SecAccessGroupsGetCurrent());
    } else {
        status = ServerCommandSendReceive(sec_item_delete_id, query, NULL);
    }

errOut:
    CFReleaseSafe(args1);
    CFReleaseSafe(args2);
	return status;
}

OSStatus
SecItemDeleteAll(void)
{
    OSStatus status = noErr;
    if (gSecurityd) {
#ifndef SECITEM_SHIM_OSX
        SecTrustStoreRef ts = SecTrustStoreForDomain(kSecTrustStoreDomainUser);
        if (!gSecurityd->sec_truststore_remove_all(ts))
            status = errSecInternal;
#endif // *** END SECITEM_SHIM_OSX ***    
        if (!gSecurityd->sec_item_delete_all())
            status = errSecInternal;
    } else {
        status = ServerCommandSendReceive(sec_delete_all_id, NULL, NULL);
    }
    return status;
}

OSStatus _SecMigrateKeychain(int32_t handle_in, CFDataRef data_in,
    int32_t *handle_out, CFDataRef *data_out)
{
    CFMutableArrayRef args = NULL;
    CFTypeRef raw_result = NULL;
    CFNumberRef hin = NULL, hout = NULL;
    OSStatus status = errSecAllocate;

    require_quiet(args = CFArrayCreateMutable(kCFAllocatorDefault, 2, NULL), errOut);
    require_quiet(hin = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &handle_in), errOut);
    CFArrayAppendValue(args, hin);
    if (data_in)
        CFArrayAppendValue(args, data_in);

    require_noerr_quiet(status = SECURITYD(sec_migrate_keychain, args, &raw_result), errOut);
    hout = CFArrayGetValueAtIndex(raw_result, 0);
    if (data_out) {
        CFDataRef data;
        if (CFArrayGetCount(raw_result) > 1) {
            data = CFArrayGetValueAtIndex(raw_result, 1);
            require_quiet(CFGetTypeID(data) == CFDataGetTypeID(), errOut);
            CFRetain(data);
        } else {
            data = NULL;
        }
        *data_out = data;
    }

errOut:
    CFReleaseSafe(hin);
    CFReleaseSafe(args);
    CFReleaseSafe(raw_result);
	return status;
}

/* TODO: Move to a location shared between securityd and Security framework. */
const char *restore_keychain_location = "/Library/Keychains/keychain.restoring";

OSStatus _SecRestoreKeychain(const char *path)
{
    OSStatus status = errSecInternal;

    require_quiet(!geteuid(), out);
    struct passwd *pass = NULL;
    require_quiet(pass = getpwnam("_securityd"), out);
    uid_t securityUID = pass->pw_uid;
    struct group *grp = NULL;
    require_quiet(grp = getgrnam("wheel"), out);
    gid_t wheelGID = grp->gr_gid;

    require_noerr_quiet(rename(path , restore_keychain_location), out);
    require_noerr_quiet(chmod(restore_keychain_location, 0600), out);
    require_noerr_quiet(chown(restore_keychain_location, securityUID, wheelGID),
        out);

    status = ServerCommandSendReceive(sec_restore_keychain_id, NULL, NULL);
    require_noerr_quiet(unlink(restore_keychain_location), out);

out:    
    return status;
}

CFDataRef _SecKeychainCopyOTABackup(void) {
    CFTypeRef raw_result = NULL;
    CFDataRef backup = NULL;
    OSStatus status;

    require_noerr_quiet(status = SECURITYD(sec_keychain_backup, NULL,
                                           &raw_result), errOut);
    if (raw_result && CFGetTypeID(raw_result) == CFDataGetTypeID()) {
        backup = raw_result;
        raw_result = NULL; /* So it doesn't get released below. */
    }

errOut:
    CFReleaseSafe(raw_result);
	return backup;
}

CFDataRef _SecKeychainCopyBackup(CFDataRef backupKeybag, CFDataRef password) {
    CFMutableArrayRef args;
    CFTypeRef raw_result = NULL;
    CFDataRef backup = NULL;
    OSStatus status;

    require_quiet(args = CFArrayCreateMutable(kCFAllocatorDefault, 2, NULL),
        errOut);
    CFArrayAppendValue(args, backupKeybag);
    if (password)
        CFArrayAppendValue(args, password);

    require_noerr_quiet(status = SECURITYD(sec_keychain_backup, args,
        &raw_result), errOut);
    if (raw_result && CFGetTypeID(raw_result) == CFDataGetTypeID()) {
        backup = raw_result;
        raw_result = NULL; /* So it doesn't get released below. */
    }

errOut:
    CFReleaseSafe(args);
    CFReleaseSafe(raw_result);
	return backup;
}

bool _SecKeychainRestoreBackup(CFDataRef backup, CFDataRef backupKeybag,
    CFDataRef password) {
    CFMutableArrayRef args = NULL;
    CFTypeRef raw_result = NULL;
    OSStatus status = errSecAllocate;

    require_quiet(args = CFArrayCreateMutable(kCFAllocatorDefault, 3, NULL),
        errOut);
    CFArrayAppendValue(args, backup);
    CFArrayAppendValue(args, backupKeybag);
    if (password)
        CFArrayAppendValue(args, password);

    require_noerr_quiet(status = SECURITYD(sec_keychain_restore, args, NULL),
    errOut);

errOut:
    CFReleaseSafe(args);
    CFReleaseSafe(raw_result);
	return status;
}
