
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
 *  SecDbQuery.c - CoreFoundation-based constants and functions for
    access to Security items (certificates, keys, identities, and
    passwords.)
 */

#include <securityd/SecDbQuery.h>

#include <securityd/SecItemDb.h>
#include <securityd/SecItemSchema.h>
#include <securityd/SecItemServer.h>
#include <securityd/spi.h>
#include <Security/SecBasePriv.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemInternal.h>
#include <Security/SecAccessControl.h>
#include <Security/SecAccessControlPriv.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecuritydXPC.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>

#include <pthread/pthread.h>

#if USE_KEYSTORE
#include <LocalAuthentication/LAPublicDefines.h>
#include <coreauthd_spi.h>
#include <libaks_acl_cf_keys.h>
#endif

/* Upper limit for number of keys in a QUERY dictionary. */
#define QUERY_KEY_LIMIT_BASE    (128)
#ifdef NO_SERVER
#define QUERY_KEY_LIMIT  (31 + QUERY_KEY_LIMIT_BASE)
#else
#define QUERY_KEY_LIMIT  QUERY_KEY_LIMIT_BASE
#endif


static const uint8_t systemKeychainUUID[] = "\xF6\x23\xAE\x5C\xCC\x81\x4C\xAC\x8A\xD4\xF0\x01\x3F\x31\x35\x11";

CFDataRef
SecMUSRCopySystemKeychainUUID(void)
{
    return CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)systemKeychainUUID, 16, kCFAllocatorNull);
}

CFDataRef
SecMUSRGetSystemKeychainUUID(void)
{
    static dispatch_once_t onceToken;
    static CFDataRef systemKeychainData = NULL;
    dispatch_once(&onceToken, ^{
        systemKeychainData = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)systemKeychainUUID, 16, kCFAllocatorNull);
    });
    return systemKeychainData;
}

CFDataRef
SecMUSRGetSingleUserKeychainUUID(void)
{
    static dispatch_once_t onceToken;
    static CFDataRef singleUser = NULL;
    dispatch_once(&onceToken, ^{
        singleUser = CFDataCreateWithBytesNoCopy(NULL, NULL, 0, kCFAllocatorNull);
    });
    return singleUser;
}

bool
SecMUSRIsSingleUserView(CFDataRef musr)
{
    return CFEqual(musr, SecMUSRGetSingleUserKeychainUUID());
}

static const uint8_t allKeychainViewsUUID[16] = "\xC8\x60\x07\xEC\x89\x62\x4D\xAF\x85\x65\x1F\xE6\x0F\x50\x5D\xB7";

CFDataRef
SecMUSRGetAllViews(void)
{
    static dispatch_once_t onceToken;
    static CFDataRef allKeychainViewsData = NULL;
    dispatch_once(&onceToken, ^{
        allKeychainViewsData = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)allKeychainViewsUUID, 16, kCFAllocatorNull);
    });
    return allKeychainViewsData;
}

bool
SecMUSRIsViewAllViews(CFDataRef musr)
{
    return CFEqual(musr, SecMUSRGetAllViews());
}

#if TARGET_OS_IPHONE

CFDataRef
SecMUSRCreateActiveUserUUID(uid_t uid)
{
    uint8_t uuid[16] = "\xA7\x5A\x3A\x35\xA5\x57\x4B\x10\xBE\x2E\x83\x94\x7E\x4A\x34\x72";
    uint32_t num = htonl(uid);
    memcpy(&uuid[12], &num, sizeof(num));
    return CFDataCreate(NULL, uuid, sizeof(uuid));
}

CFDataRef
SecMUSRCreateSyncBubbleUserUUID(uid_t uid)
{
    uint8_t uuid[16] = "\x82\x1A\xAB\x9F\xA3\xC8\x4E\x11\xAA\x90\x4C\xE8\x9E\xA6\xD7\xEC";
    uint32_t num = htonl(uid);
    memcpy(&uuid[12], &num, sizeof(num));
    return CFDataCreate(NULL, uuid, sizeof(uuid));
}

static const uint8_t bothUserAndSystemUUID[12] = "\x36\xC4\xBE\x2E\x99\x0A\x46\x9A\xAC\x89\x09\xA4";


CFDataRef
SecMUSRCreateBothUserAndSystemUUID(uid_t uid)
{
    uint8_t uuid[16];
    memcpy(uuid, bothUserAndSystemUUID, 12);
    uint32_t num = htonl(uid);
    memcpy(&uuid[12], &num, sizeof(num));
    return CFDataCreate(NULL, uuid, sizeof(uuid));
}

bool
SecMUSRGetBothUserAndSystemUUID(CFDataRef musr, uid_t *uid)
{
    if (CFDataGetLength(musr) != 16)
        return false;
    const uint8_t *uuid = CFDataGetBytePtr(musr);
    if (memcmp(uuid, bothUserAndSystemUUID, 12) != 0)
        return false;
    if (uid) {
        uint32_t num;
        memcpy(&num, &uuid[12], sizeof(num));
        *uid = htonl(num);
    }
    return true;
}

#endif

/* Inline accessors to attr and match values in a query. */
CFIndex query_attr_count(const Query *q)
{
    return q->q_attr_end;
}

Pair query_attr_at(const Query *q, CFIndex ix)
{
    return q->q_pairs[ix];
}

CFIndex query_match_count(const Query *q)
{
    return q->q_match_end - q->q_match_begin;
}

__unused static inline Pair query_match_at(const Query *q, CFIndex ix)
{
    return q->q_pairs[q->q_match_begin + ix];
}

/* Private routines used to parse a query. */

const SecDbClass *kc_class_with_name(CFStringRef name) {
    if (isString(name)) {
#if 0
        // TODO Iterate kc_db_classes and look for name == class->name.
        // Or get clever and switch on first letter of class name and compare to verify
        static const void *kc_db_classes[] = {
            &genp_class,
            &inet_class,
            &cert_class,
            &keys_class,
            &identity_class
        };
#endif
        if (CFEqual(name, kSecClassGenericPassword))
            return &genp_class;
        else if (CFEqual(name, kSecClassInternetPassword))
            return &inet_class;
        else if (CFEqual(name, kSecClassCertificate))
            return &cert_class;
        else if (CFEqual(name, kSecClassKey))
            return &keys_class;
        else if (CFEqual(name, kSecClassIdentity))
            return &identity_class;
    }
    return NULL;
}

static void query_set_access_control(Query *q, SecAccessControlRef access_control) {
    if (q->q_access_control) {
        if (!CFEqual(q->q_access_control, access_control)) {
            SecError(errSecItemIllegalQuery, &q->q_error, CFSTR("conflicting kSecAccess and kSecAccessControl attributes"));
        }
    } else {
        /* Store access control virtual attribute. */
        q->q_access_control = (SecAccessControlRef)CFRetain(access_control);
        
        /* Also set legacy access attribute. */
        CFTypeRef protection = SecAccessControlGetProtection(q->q_access_control);
        if (!protection) {
            SecError(errSecParam, &q->q_error, CFSTR("kSecAccessControl missing protection"));
            return;
        }
        CFDictionarySetValue(q->q_item, kSecAttrAccessible, protection);
    }
}

/* AUDIT[securityd](done):
 key (ok) is a caller provided, string or number of length 4.
 value (ok) is a caller provided, non NULL CFTypeRef.
 */
void query_add_attribute_with_desc(const SecDbAttr *desc, const void *value, Query *q)
{
    if (CFEqual(desc->name, kSecAttrSynchronizable)) {
        q->q_sync = true;
        if (CFEqual(value, kSecAttrSynchronizableAny))
            return; /* skip the attribute so it isn't part of the search */
    }

    CFTypeRef attr = NULL;
    switch (desc->kind) {
        case kSecDbDataAttr:
            attr = copyData(value);
            break;
        case kSecDbBlobAttr:
        case kSecDbAccessControlAttr:
            attr = copyBlob(value);
            break;
        case kSecDbDateAttr:
        case kSecDbCreationDateAttr:
        case kSecDbModificationDateAttr:
            attr = copyDate(value);
            break;
        case kSecDbNumberAttr:
        case kSecDbSyncAttr:
        case kSecDbTombAttr:
            attr = copyNumber(value);
            break;
        case kSecDbAccessAttr:
        case kSecDbStringAttr:
            attr = copyString(value);
            break;
        case kSecDbSHA1Attr:
            attr = copySHA1(value);
            break;
        case kSecDbRowIdAttr:
        case kSecDbPrimaryKeyAttr:
        case kSecDbEncryptedDataAttr:
        case kSecDbUTombAttr:
            break;
        case kSecDbUUIDAttr:
            attr = copyUUID(value);
            break;
    }

    if (!attr) {
        SecError(errSecItemInvalidValue, &q->q_error, CFSTR("attribute %@: value: %@ failed to convert"), desc->name, value);
        return;
    }

    /* Store plaintext attr data in q_item unless it's a kSecDbSHA1Attr. */
    if (q->q_item && desc->kind != kSecDbSHA1Attr) {
        CFDictionarySetValue(q->q_item, desc->name, attr);
    }

    /* Convert attr to (sha1) digest if requested. */
    if (desc->flags & kSecDbSHA1ValueInFlag) {
        CFDataRef data = copyData(attr);
        CFRelease(attr);
        if (!data) {
            SecError(errSecInternal, &q->q_error, CFSTR("failed to get attribute %@ data"), desc->name);
            return;
        }

        CFMutableDataRef digest = CFDataCreateMutable(0, CC_SHA1_DIGEST_LENGTH);
        CFDataSetLength(digest, CC_SHA1_DIGEST_LENGTH);
        /* 64 bits cast: worst case is we generate the wrong hash */
        assert((unsigned long)CFDataGetLength(data)<UINT32_MAX); /* Debug check. Correct as long as CFIndex is long */
        CCDigest(kCCDigestSHA1, CFDataGetBytePtr(data), (CC_LONG)CFDataGetLength(data),
                 CFDataGetMutableBytePtr(digest));
        CFRelease(data);
        attr = digest;
    }

    if (desc->kind != kSecDbAccessControlAttr) {
        /* Record the new attr key, value in q_pairs. */
        q->q_pairs[q->q_attr_end].key = desc->name;
        q->q_pairs[q->q_attr_end++].value = attr;
    } else {
        CFReleaseSafe(attr);
    }
}

void query_add_attribute(const void *key, const void *value, Query *q)
{
    const SecDbAttr *desc = SecDbAttrWithKey(q->q_class, key, &q->q_error);
    if (desc) {
        query_add_attribute_with_desc(desc, value, q);

        if (desc->kind == kSecDbAccessControlAttr) {
            CFDataRef attr = (CFDataRef)CFDictionaryGetValue(q->q_item, desc->name);
            if (attr) {
                SecAccessControlRef access_control = SecAccessControlCreateFromData(kCFAllocatorDefault, attr, &q->q_error);
                if (access_control) {
                    query_set_access_control(q, access_control);
                    CFRelease(access_control);
                }
            }
        }

        if (desc->kind == kSecDbAccessAttr) {
            SecAccessControlRef access_control = SecAccessControlCreate(kCFAllocatorDefault, &q->q_error);
            if (access_control) {
                CFStringRef attr = (CFStringRef)CFDictionaryGetValue(q->q_item, desc->name);
                if (attr) {
                    if (SecAccessControlSetProtection(access_control, attr, &q->q_error))
                        query_set_access_control(q, access_control);
                }
                CFRelease(access_control);
            }
        }
    }
}

void query_add_or_attribute(const void *key, const void *value, Query *q)
{
    const SecDbAttr *desc = SecDbAttrWithKey(q->q_class, key, &q->q_error);
    if (desc) {
        CFTypeRef oldValue = CFDictionaryGetValue(q->q_item, desc->name);
        CFMutableArrayRef array = NULL;
        if (oldValue) {
            if (isArray(oldValue)) {
                array = (CFMutableArrayRef)CFRetain(oldValue);
            } else {
                array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
                CFArrayAppendValue(array, oldValue);
            }
            CFDictionaryRemoveValue(q->q_item, desc->name);
        } else {
            array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        }
        if (array) {
            query_add_attribute_with_desc(desc, value, q);
            CFTypeRef newValue = CFDictionaryGetValue(q->q_item, desc->name);
            CFArrayAppendValue(array, newValue);
            CFDictionarySetValue(q->q_item, desc->name, array);
            CFRelease(array);
        }
    }
}

void query_add_not_attribute(const void *key, const void *value, Query *q)
{
    const SecDbAttr *desc = SecDbAttrWithKey(q->q_class, key, &q->q_error);
    if (desc) {
        CFTypeRef oldValue = CFDictionaryGetValue(q->q_item, desc->name);
        CFMutableArrayRef array = NULL;
        if (oldValue) {
            if (isArray(oldValue)) {
                array = (CFMutableArrayRef)CFRetain(oldValue);
            } else {
                // This should never run, as we shouldn't be turning a attr = value into a attr not in (value, value2)
                secerror("negating %@ = %@ in query", desc->name, oldValue);
                array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
                CFArrayAppendValue(array, kCFNull);
                CFArrayAppendValue(array, oldValue);
            }
            CFDictionaryRemoveValue(q->q_item, desc->name);
        } else {
            array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
            CFArrayAppendValue(array, kCFNull);
        }
        if (array) {
            query_add_attribute_with_desc(desc, value, q);
            CFTypeRef newValue = CFDictionaryGetValue(q->q_item, desc->name);
            CFArrayAppendValue(array, newValue);
            CFDictionarySetValue(q->q_item, desc->name, array);
            CFRelease(array);
        }
    }
}


/* AUDIT[securityd](done):
 key (ok) is a caller provided, string starting with 'm'.
 value (ok) is a caller provided, non NULL CFTypeRef.
 */
static void query_add_match(const void *key, const void *value, Query *q)
{
    /* Record the match key, value in q_pairs. */
    --(q->q_match_begin);
    q->q_pairs[q->q_match_begin].key = key;
    q->q_pairs[q->q_match_begin].value = value;

    if (CFEqual(kSecMatchLimit, key)) {
        /* Figure out what the value for kSecMatchLimit is if specified. */
        if (CFGetTypeID(value) == CFNumberGetTypeID()) {
            if (!CFNumberGetValue(value, kCFNumberCFIndexType, &q->q_limit))
                SecError(errSecItemInvalidValue, &q->q_error, CFSTR("failed to convert match limit %@ to CFIndex"), value);
        } else if (CFEqual(kSecMatchLimitAll, value)) {
            q->q_limit = kSecMatchUnlimited;
        } else if (CFEqual(kSecMatchLimitOne, value)) {
            q->q_limit = 1;
        } else {
            SecError(errSecItemInvalidValue, &q->q_error, CFSTR("unsupported match limit %@"), value);
        }
    } else if (CFEqual(kSecMatchIssuers, key) &&
               (CFGetTypeID(value) == CFArrayGetTypeID()))
    {
        CFMutableArrayRef canonical_issuers = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        if (canonical_issuers) {
            CFIndex i, count = CFArrayGetCount(value);
            for (i = 0; i < count; i++) {
                CFTypeRef issuer_data = CFArrayGetValueAtIndex(value, i);
                CFDataRef issuer_canonical = NULL;
                if (CFDataGetTypeID() == CFGetTypeID(issuer_data))
                    issuer_canonical = SecDistinguishedNameCopyNormalizedContent((CFDataRef)issuer_data);
                if (issuer_canonical) {
                    CFArrayAppendValue(canonical_issuers, issuer_canonical);
                    CFRelease(issuer_canonical);
                }
            }

            if (CFArrayGetCount(canonical_issuers) > 0) {
                q->q_match_issuer = canonical_issuers;
            } else
                CFRelease(canonical_issuers);
        }
    } else if (CFEqual(kSecMatchPolicy, key)) {
        if (CFGetTypeID(value) != CFArrayGetTypeID()) {
            SecError(errSecParam, &q->q_error, CFSTR("unsupported value for kSecMatchPolicy attribute"));
            return;
        }
        xpc_object_t policiesArrayXPC = _CFXPCCreateXPCObjectFromCFObject(value);
        if (!policiesArrayXPC) {
            SecError(errSecParam, &q->q_error, CFSTR("unsupported kSecMatchPolicy object in query"));
            return;
        }

        CFArrayRef policiesArray = SecPolicyXPCArrayCopyArray(policiesArrayXPC, &q->q_error);
        xpc_release(policiesArrayXPC);
        if (!policiesArray)
            return;

        if (CFArrayGetCount(policiesArray) != 1 || CFGetTypeID(CFArrayGetValueAtIndex(policiesArray, 0)) != SecPolicyGetTypeID()) {
            CFRelease(policiesArray);
            SecError(errSecParam, &q->q_error, CFSTR("unsupported array of policies"));
            return;
        }

        query_set_policy(q, (SecPolicyRef)CFArrayGetValueAtIndex(policiesArray, 0));
        CFRelease(policiesArray);
    } else if (CFEqual(kSecMatchValidOnDate, key)) {
        if (CFGetTypeID(value) == CFNullGetTypeID()) {
            CFDateRef date = CFDateCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent());
            query_set_valid_on_date(q, date);
            CFRelease(date);
        } else if (CFGetTypeID(value) == CFDateGetTypeID()) {
            query_set_valid_on_date(q, value);
        } else {
            SecError(errSecParam, &q->q_error, CFSTR("unsupported value for kSecMatchValidOnDate attribute"));
            return;
        }
    } else if (CFEqual(kSecMatchTrustedOnly, key)) {
        if ((CFGetTypeID(value) == CFBooleanGetTypeID())) {
            query_set_trusted_only(q, value);
        } else {
            SecError(errSecParam, &q->q_error, CFSTR("unsupported value for kSecMatchTrustedOnly attribute"));
            return;
        }
    }
}

static bool query_set_class(Query *q, CFStringRef c_name, CFErrorRef *error) {
    const SecDbClass *value;
    if (c_name && CFGetTypeID(c_name) == CFStringGetTypeID() &&
        (value = kc_class_with_name(c_name)) &&
        (q->q_class == 0 || q->q_class == value)) {
        q->q_class = value;
        return true;
    }

    if (error && !*error)
        SecError((c_name ? errSecNoSuchClass : errSecItemClassMissing), error, CFSTR("can find class named: %@"), c_name);


    return false;
}

static const SecDbClass *query_get_class(CFDictionaryRef query, CFErrorRef *error) {
    CFStringRef c_name = NULL;
    const void *value = CFDictionaryGetValue(query, kSecClass);
    if (isString(value)) {
        c_name = value;
    } else {
        value = CFDictionaryGetValue(query, kSecValuePersistentRef);
        if (isData(value)) {
            CFDataRef pref = value;
            _SecItemParsePersistentRef(pref, &c_name, 0);
        }
    }

    if (c_name && (value = kc_class_with_name(c_name))) {
        return value;
    } else {
        if (c_name)
            SecError(errSecNoSuchClass, error, CFSTR("can't find class named: %@"), c_name);
        else
            SecError(errSecItemClassMissing, error, CFSTR("query missing class name"));
        return NULL;
    }
}

/* AUDIT[securityd](done):
 key (ok) is a caller provided, string starting with 'c'.
 value (ok) is a caller provided, non NULL CFTypeRef.
 */
static void query_add_class(const void *key, const void *value, Query *q)
{
    if (CFEqual(key, kSecClass)) {
        query_set_class(q, value, &q->q_error);
    } else {
        SecError(errSecItemInvalidKey, &q->q_error, CFSTR("add_class: key %@ is not %@"), key, kSecClass);
    }
}

/* AUDIT[securityd](done):
 key (ok) is a caller provided, string starting with 'r'.
 value (ok) is a caller provided, non NULL CFTypeRef.
 */
static void query_add_return(const void *key, const void *value, Query *q)
{
    ReturnTypeMask mask;
    if (CFGetTypeID(value) != CFBooleanGetTypeID()) {
        SecError(errSecItemInvalidValue, &q->q_error, CFSTR("add_return: value %@ is not CFBoolean"), value);
        return;
    }

    int set_it = CFEqual(value, kCFBooleanTrue);

    if (CFEqual(key, kSecReturnData))
        mask = kSecReturnDataMask;
    else if (CFEqual(key, kSecReturnAttributes))
        mask = kSecReturnAttributesMask;
    else if (CFEqual(key, kSecReturnRef))
        mask = kSecReturnRefMask;
    else if (CFEqual(key, kSecReturnPersistentRef))
        mask = kSecReturnPersistentRefMask;
    else {
        SecError(errSecItemInvalidKey, &q->q_error, CFSTR("add_return: unknown key %@"), key);
        return;
    }

    if ((q->q_return_type & mask) && !set_it) {
        /* Clear out this bit (it's set so xor with the mask will clear it). */
        q->q_return_type ^= mask;
    } else if (!(q->q_return_type & mask) && set_it) {
        /* Set this bit. */
        q->q_return_type |= mask;
    }
}

/* AUDIT[securityd](done):
 key (ok) is a caller provided, string starting with 'u'.
 value (ok since q_use_item_list is unused) is a caller provided, non
 NULL CFTypeRef.
 */
static void query_add_use(const void *key, const void *value, Query *q)
{
    if (CFEqual(key, kSecUseItemList)) {
        /* TODO: Add sanity checking when we start using this. */
        q->q_use_item_list = value;
    } else if (CFEqual(key, kSecUseTombstones)) {
        if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
            q->q_use_tomb = value;
        } else if (CFGetTypeID(value) == CFNumberGetTypeID()) {
            q->q_use_tomb = CFBooleanGetValue(value) ? kCFBooleanTrue : kCFBooleanFalse;
        } else if (CFGetTypeID(value) == CFStringGetTypeID()) {
            q->q_use_tomb = CFStringGetIntValue(value) ? kCFBooleanTrue : kCFBooleanFalse;
        } else {
            SecError(errSecItemInvalidValue, &q->q_error, CFSTR("add_use: value %@ for key %@ is neither CFBoolean nor CFNumber"), value, key);
            return;
        }
    } else if (CFEqual(key, kSecUseCredentialReference)) {
        if (isData(value)) {
            CFRetainAssign(q->q_use_cred_handle, value);
        } else {
            SecError(errSecItemInvalidValue, &q->q_error, CFSTR("add_use: value %@ for key %@ is not CFData"), value, key);
            return;
        }
    } else if (CFEqual(key, kSecUseAuthenticationUI)) {
        if (isString(value)) {
            q->q_skip_acl_items = CFEqualSafe(kSecUseAuthenticationUISkip, value);
        } else {
            SecError(errSecItemInvalidValue, &q->q_error, CFSTR("add_use: value %@ for key %@ is not CFString"), value, key);
            return;
        }
#if TARGET_OS_IPHONE
    } else if (CFEqual(key, kSecUseSystemKeychain)) {
#if TARGET_OS_EMBEDDED
        q->q_keybag = KEYBAG_DEVICE;
#endif
        q->q_system_keychain = true;
    } else if (CFEqual(key, kSecUseSyncBubbleKeychain)) {
        if (isNumber(value) && CFNumberGetValue(value, kCFNumberSInt32Type, &q->q_sync_bubble) && q->q_sync_bubble > 0) {
#if TARGET_OS_EMBEDDED
            q->q_keybag = KEYBAG_DEVICE;
#endif
        } else {
            SecError(errSecItemInvalidValue, &q->q_error, CFSTR("add_use: value %@ for key %@ is not valid uid"), value, key);
            return;
        }
#endif
    } else {
        SecError(errSecItemInvalidKey, &q->q_error, CFSTR("add_use: unknown key %@"), key);
        return;
    }
}

static void query_set_data(const void *value, Query *q) {
    if (!isData(value)) {
        SecError(errSecItemInvalidValue, &q->q_error, CFSTR("set_data: value %@ is not type data"), value);
    } else {
        q->q_data = value;
        if (q->q_item)
            CFDictionarySetValue(q->q_item, kSecValueData, value);
    }
}

/* AUDIT[securityd](done):
 key (ok) is a caller provided, string starting with 'u'.
 value (ok) is a caller provided, non NULL CFTypeRef.
 */
static void query_add_value(const void *key, const void *value, Query *q)
{
    if (CFEqual(key, kSecValueData)) {
        query_set_data(value, q);
#ifdef NO_SERVER
    } else if (CFEqual(key, kSecValueRef)) {
        q->q_ref = value;
        /* TODO: Add value type sanity checking. */
#endif
    } else if (CFEqual(key, kSecValuePersistentRef)) {
        CFStringRef c_name;
        if (_SecItemParsePersistentRef(value, &c_name, &q->q_row_id))
            query_set_class(q, c_name, &q->q_error);
        else
            SecError(errSecItemInvalidValue, &q->q_error, CFSTR("add_value: value %@ is not a valid persitent ref"), value);
    } else {
        SecError(errSecItemInvalidKey, &q->q_error, CFSTR("add_value: unknown key %@"), key);
        return;
    }
}

/* AUDIT[securityd](done):
 key (ok) is a caller provided, unchecked.
 value (ok) is a caller provided, unchecked.
 */
static void query_update_applier(const void *key, const void *value,
                                 void *context)
{
    Query *q = (Query *)context;
    /* If something went wrong there is no point processing any more args. */
    if (q->q_error)
        return;

    /* Make sure we have a string key. */
    if (!isString(key)) {
        SecError(errSecItemInvalidKeyType, &q->q_error, CFSTR("update_applier: unknown key type %@"), key);
        return;
    }

    if (!value) {
        SecError(errSecItemInvalidValue, &q->q_error, CFSTR("update_applier: key %@ has NULL value"), key);
        return;
    }

    if (CFEqual(key, CFSTR("musr"))) {
        secnotice("item", "update_applier: refusing to update musr");
        return;
    }

    if (CFEqual(key, kSecValueData)) {
        query_set_data(value, q);
    } else {
        query_add_attribute(key, value, q);
    }
}

/* AUDIT[securityd](done):
 key (ok) is a caller provided, unchecked.
 value (ok) is a caller provided, unchecked.
 */
static void query_applier(const void *key, const void *value, void *context)
{
    Query *q = (Query *)context;
    /* If something went wrong there is no point processing any more args. */
    if (q->q_error)
        return;

    /* Make sure we have a key. */
    if (!key) {
        SecError(errSecItemInvalidKeyType, &q->q_error, CFSTR("applier: NULL key"));
        return;
    }

    /* Make sure we have a value. */
    if (!value) {
        SecError(errSecItemInvalidValue, &q->q_error, CFSTR("applier: key %@ has NULL value"), key);
        return;
    }

    /* Figure out what type of key we are dealing with. */
    CFTypeID key_id = CFGetTypeID(key);
    if (key_id == CFStringGetTypeID()) {
        CFIndex key_len = CFStringGetLength(key);
        /* String keys can be different things.  The subtype is determined by:
         length 4 strings are all attributes.  Otherwise the first char
         determines the type:
         c: class must be kSecClass
         m: match like kSecMatchPolicy
         r: return like kSecReturnData
         u: use keys
         v: value
         */
        if (key_len == 4) {
            /* attributes */
            query_add_attribute(key, value, q);
        } else if (key_len > 1) {
            UniChar k_first_char = CFStringGetCharacterAtIndex(key, 0);
            switch (k_first_char)
            {
                case 'c': /* class */
                    query_add_class(key, value, q);
                    break;
                case 'm': /* match */
                    query_add_match(key, value, q);
                    break;
                case 'r': /* return */
                    query_add_return(key, value, q);
                    break;
                case 'u': /* use */
                    query_add_use(key, value, q);
                    break;
                case 'v': /* value */
                    query_add_value(key, value, q);
                    break;
                default:
                    SecError(errSecItemInvalidKey, &q->q_error, CFSTR("applier: key %@ invalid"), key);
                    break;
            }
        } else {
            SecError(errSecItemInvalidKey, &q->q_error, CFSTR("applier: key %@ invalid length"), key);
        }
    } else if (key_id == CFNumberGetTypeID()) {
        /* Numeric keys are always (extended) attributes. */
        /* TODO: Why is this here? query_add_attribute() doesn't take numbers. */
        query_add_attribute(key, value, q);
    } else {
        /* We only support string and number type keys. */
        SecError(errSecItemInvalidKeyType, &q->q_error, CFSTR("applier: key %@ neither string nor number"), key);
    }
}

static CFStringRef query_infer_keyclass(Query *q, CFStringRef agrp) {
    /* apsd are always dku. */
    if (CFEqual(agrp, CFSTR("com.apple.apsd"))) {
        return kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate;
    }
    /* All other certs or in the apple agrp is dk. */
    if (q->q_class == &cert_class) {
        /* third party certs are always dk. */
        return kSecAttrAccessibleAlwaysPrivate;
    }
    /* The rest defaults to ak. */
    return kSecAttrAccessibleWhenUnlocked;
}

void query_ensure_access_control(Query *q, CFStringRef agrp) {
    if (q->q_access_control == 0) {
        CFStringRef accessible = query_infer_keyclass(q, agrp);
        query_add_attribute(kSecAttrAccessible, accessible, q);
    }
}

bool query_error(Query *q, CFErrorRef *error) {
    CFErrorRef tmp = q->q_error;
    q->q_error = NULL;
    return SecErrorPropagate(tmp, error);
}

bool query_destroy(Query *q, CFErrorRef *error) {
    bool ok = query_error(q, error);
    CFIndex ix, attr_count = query_attr_count(q);
    for (ix = 0; ix < attr_count; ++ix) {
        CFReleaseSafe(query_attr_at(q, ix).value);
    }
    CFReleaseSafe(q->q_item);
    CFReleaseSafe(q->q_musrView);
    CFReleaseSafe(q->q_primary_key_digest);
    CFReleaseSafe(q->q_match_issuer);
    CFReleaseSafe(q->q_access_control);
    CFReleaseSafe(q->q_use_cred_handle);
    CFReleaseSafe(q->q_caller_access_groups);
    CFReleaseSafe(q->q_match_policy);
    CFReleaseSafe(q->q_match_valid_on_date);
    CFReleaseSafe(q->q_match_trusted_only);

    free(q);
    return ok;
}

bool query_notify_and_destroy(Query *q, bool ok, CFErrorRef *error) {
    if (ok && !q->q_error && (q->q_sync_changed || (q->q_changed && !SecMUSRIsSingleUserView(q->q_musrView)))) {
        SecKeychainChanged();
    }
    return query_destroy(q, error) && ok;
}

/* Allocate and initialize a Query object for query. */
Query *query_create(const SecDbClass *qclass,
                    CFDataRef musr,
                    CFDictionaryRef query,
                    CFErrorRef *error)
{
    if (!qclass) {
        if (error && !*error)
            SecError(errSecItemClassMissing, error, CFSTR("Missing class"));
        return NULL;
    }

    if (musr == NULL)
        musr = SecMUSRGetSingleUserKeychainUUID();

    /* Number of pairs we need is the number of attributes in this class
     plus the number of keys in the dictionary, minus one for each key in
     the dictionary that is a regular attribute. */
    CFIndex key_count = SecDbClassAttrCount(qclass);
    if (key_count == 0) {
        // Identities claim to have 0 attributes, but they really support any keys or cert attribute.
        key_count = SecDbClassAttrCount(&cert_class) + SecDbClassAttrCount(&keys_class);
    }

    if (query) {
        key_count += CFDictionaryGetCount(query);
        SecDbForEachAttr(qclass, attr) {
            if (CFDictionaryContainsKey(query, attr->name))
                --key_count;
        }
    }

    if (key_count > QUERY_KEY_LIMIT) {
        if (error && !*error)
        {
            secerror("key_count: %ld, QUERY_KEY_LIMIT: %d", (long)key_count, QUERY_KEY_LIMIT);
            SecError(errSecItemIllegalQuery, error, CFSTR("Past query key limit"));
        }
        return NULL;
    }

    Query *q = calloc(1, sizeof(Query) + sizeof(Pair) * key_count);
    if (q == NULL) {
        if (error && !*error)
            SecError(errSecAllocate, error, CFSTR("Out of memory"));
        return NULL;
    }

    q->q_musrView = (CFDataRef)CFRetain(musr);
    q->q_keybag = KEYBAG_DEVICE;
    q->q_class = qclass;
    q->q_match_begin = q->q_match_end = key_count;
    q->q_item = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    return q;
}

/* Parse query for a Query object q. */
static bool query_parse_with_applier(Query *q, CFDictionaryRef query,
                                     CFDictionaryApplierFunction applier,
                                     CFErrorRef *error) {
    CFDictionaryApplyFunction(query, applier, q);
    return query_error(q, error);
}

/* Parse query for a Query object q. */
static bool query_parse(Query *q, CFDictionaryRef query,
                        CFErrorRef *error) {
    return query_parse_with_applier(q, query, query_applier, error);
}

/* Parse query for a Query object q. */
bool query_update_parse(Query *q, CFDictionaryRef update,
                               CFErrorRef *error) {
    return query_parse_with_applier(q, update, query_update_applier, error);
}

Query *query_create_with_limit(CFDictionaryRef query, CFDataRef musr, CFIndex limit, CFErrorRef *error) {
    Query *q;
    q = query_create(query_get_class(query, error), musr, query, error);
    if (q) {
        q->q_limit = limit;
        if (!query_parse(q, query, error)) {
            query_destroy(q, error);
            return NULL;
        }
        if (!q->q_sync && !q->q_row_id) {
            /* query did not specify a kSecAttrSynchronizable attribute,
             * and did not contain a persistent reference. */
            query_add_attribute(kSecAttrSynchronizable, kCFBooleanFalse, q);
        }
    }
    return q;
}


void
query_set_caller_access_groups(Query *q, CFArrayRef caller_access_groups) {
    CFRetainAssign(q->q_caller_access_groups, caller_access_groups);
}

void
query_set_policy(Query *q, SecPolicyRef policy) {
    CFRetainAssign(q->q_match_policy, policy);
}

void query_set_valid_on_date(Query *q, CFDateRef date) {
    CFRetainAssign(q->q_match_valid_on_date, date);
}

void query_set_trusted_only(Query *q, CFBooleanRef trusted_only) {
    CFRetainAssign(q->q_match_trusted_only, trusted_only);
}
