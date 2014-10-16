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
        CFDictionarySetValue(q->q_item, kSecAttrAccessible, SecAccessControlGetProtection(q->q_access_control));
    }
}

/* AUDIT[securityd](done):
 key (ok) is a caller provided, string or number of length 4.
 value (ok) is a caller provided, non NULL CFTypeRef.
 */
static void query_add_attribute_with_desc(const SecDbAttr *desc, const void *value, Query *q)
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

    if (desc->kind == kSecDbAccessControlAttr) {
        SecAccessControlRef access_control = SecAccessControlCreateFromData(kCFAllocatorDefault, attr, &q->q_error);
        if (access_control) {
            query_set_access_control(q, access_control);
            CFRelease(access_control);
        }
    }
    
    if (desc->kind == kSecDbAccessAttr) {
        SecAccessControlRef access_control = SecAccessControlCreateWithFlags(kCFAllocatorDefault, attr, 0, &q->q_error);
        if (access_control) {
            query_set_access_control(q, access_control);
            CFRelease(access_control);
        }
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
    if (desc)
        query_add_attribute_with_desc(desc, value, q);
}

/* First remove key from q->q_pairs if it's present, then add the attribute again. */
static void query_set_attribute_with_desc(const SecDbAttr *desc, const void *value, Query *q) {
    if (CFDictionaryContainsKey(q->q_item, desc->name)) {
        CFIndex ix;
        for (ix = 0; ix < q->q_attr_end; ++ix) {
            if (CFEqual(desc->name, q->q_pairs[ix].key)) {
                CFReleaseSafe(q->q_pairs[ix].value);
                --q->q_attr_end;
                for (; ix < q->q_attr_end; ++ix) {
                    q->q_pairs[ix] = q->q_pairs[ix + 1];
                }
                CFDictionaryRemoveValue(q->q_item, desc->name);
                break;
            }
        }
    }
    query_add_attribute_with_desc(desc, value, q);
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
    } else if (CFEqual(key, kSecUseOperationPrompt)) {
        if (isString(value)) {
            CFRetainAssign(q->q_use_operation_prompt, value);
        } else {
            SecError(errSecItemInvalidValue, &q->q_error, CFSTR("add_use: value %@ for key %@ is not CFString"), value, key);
            return;
        }
    } else if (CFEqual(key, kSecUseNoAuthenticationUI)) {
        if (isBoolean(value)) {
            q->q_use_no_authentication_ui = value;
        } else if (isNumber(value)) {
            q->q_use_no_authentication_ui = CFBooleanGetValue(value) ? kCFBooleanTrue : kCFBooleanFalse;
        } else if (isString(value)) {
            q->q_use_no_authentication_ui = CFStringGetIntValue(value) ? kCFBooleanTrue : kCFBooleanFalse;
        } else {
            SecError(errSecItemInvalidValue, &q->q_error, CFSTR("add_use: value %@ for key %@ is neither CFBoolean nor CFNumber"), value, key);
            return;
        }
#if defined(MULTIPLE_KEYCHAINS)
    } else if (CFEqual(key, kSecUseKeychain)) {
        q->q_use_keychain = value;
    } else if (CFEqual(key, kSecUseKeychainList)) {
        q->q_use_keychain_list = value;
#endif /* !defined(MULTIPLE_KEYCHAINS) */
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
    /* apsd and lockdown are always dku. */
    if (CFEqual(agrp, CFSTR("com.apple.apsd"))
        || CFEqual(agrp, CFSTR("lockdown-identities"))) {
        return kSecAttrAccessibleAlwaysThisDeviceOnly;
    }
    /* All other certs or in the apple agrp is dk. */
    if (q->q_class == &cert_class) {
        /* third party certs are always dk. */
        return kSecAttrAccessibleAlways;
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
    return CFErrorPropagate(tmp, error);
}

bool query_destroy(Query *q, CFErrorRef *error) {
    bool ok = query_error(q, error);
    CFIndex ix, attr_count = query_attr_count(q);
    for (ix = 0; ix < attr_count; ++ix) {
        CFReleaseSafe(query_attr_at(q, ix).value);
    }
    CFReleaseSafe(q->q_item);
    CFReleaseSafe(q->q_primary_key_digest);
    CFReleaseSafe(q->q_match_issuer);
    CFReleaseSafe(q->q_access_control);
    CFReleaseSafe(q->q_use_cred_handle);
    CFReleaseSafe(q->q_required_access_controls);
    CFReleaseSafe(q->q_use_operation_prompt);
    CFReleaseSafe(q->q_caller_access_groups);

    free(q);
    return ok;
}

bool query_notify_and_destroy(Query *q, bool ok, CFErrorRef *error) {
    if (ok && !q->q_error && q->q_sync_changed) {
        SecKeychainChanged(true);
    }
    return query_destroy(q, error) && ok;
}

/* Allocate and initialize a Query object for query. */
Query *query_create(const SecDbClass *qclass, CFDictionaryRef query,
                    CFErrorRef *error)
{
    if (!qclass) {
        if (error && !*error)
            SecError(errSecItemClassMissing, error, CFSTR("Missing class"));
        return NULL;
    }

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

    q->q_keybag = KEYBAG_DEVICE;
    q->q_class = qclass;
    q->q_match_begin = q->q_match_end = key_count;
    q->q_item = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    q->q_crypto_op = kSecKsUnwrap;

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

Query *query_create_with_limit(CFDictionaryRef query, CFIndex limit,
                                      CFErrorRef *error) {
    Query *q;
    q = query_create(query_get_class(query, error), query, error);
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


//TODO: Move this to SecDbItemRef

/* Make sure all attributes that are marked as not_null have a value.  If
 force_date is false, only set mdat and cdat if they aren't already set. */
void
query_pre_add(Query *q, bool force_date) {
    CFDateRef now = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
    SecDbForEachAttrWithMask(q->q_class, desc, kSecDbInFlag) {
        if (desc->kind == kSecDbCreationDateAttr ||
            desc->kind == kSecDbModificationDateAttr) {
            if (force_date) {
                query_set_attribute_with_desc(desc, now, q);
            } else if (!CFDictionaryContainsKey(q->q_item, desc->name)) {
                query_add_attribute_with_desc(desc, now, q);
            }
        } else if ((desc->flags & kSecDbNotNullFlag) &&
                   !CFDictionaryContainsKey(q->q_item, desc->name)) {
            CFTypeRef value = NULL;
            if (desc->flags & kSecDbDefault0Flag) {
                if (desc->kind == kSecDbDateAttr)
                    value = CFDateCreate(kCFAllocatorDefault, 0.0);
                else {
                    SInt32 vzero = 0;
                    value = CFNumberCreate(0, kCFNumberSInt32Type, &vzero);
                }
            } else if (desc->flags & kSecDbDefaultEmptyFlag) {
                if (desc->kind == kSecDbDataAttr)
                    value = CFDataCreate(kCFAllocatorDefault, NULL, 0);
                else {
                    value = CFSTR("");
                    CFRetain(value);
                }
            }
            if (value) {
                /* Safe to use query_add_attribute here since the attr wasn't
                 set yet. */
                query_add_attribute_with_desc(desc, value, q);
                CFRelease(value);
            }
        }
    }
    CFReleaseSafe(now);
}

//TODO: Move this to SecDbItemRef

/* Update modification_date if needed. */
void
query_pre_update(Query *q) {
    SecDbForEachAttr(q->q_class, desc) {
        if (desc->kind == kSecDbModificationDateAttr) {
            CFDateRef now = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
            query_set_attribute_with_desc(desc, now, q);
            CFReleaseSafe(now);
        }
    }
}

void
query_set_caller_access_groups(Query *q, CFArrayRef caller_access_groups) {
    CFRetainAssign(q->q_caller_access_groups, caller_access_groups);
}

bool
query_needs_authentication(Query *q) {
    return q->q_required_access_controls && CFArrayGetCount(q->q_required_access_controls) > 0;
}

void
query_enable_interactive(Query *q) {
    CFAssignRetained(q->q_required_access_controls, CFArrayCreateMutableForCFTypes(kCFAllocatorDefault));
}

bool
query_authenticate(Query *q, CFErrorRef **error) {
    bool ok = true;
    CFMutableDictionaryRef hints = NULL;
#if USE_KEYSTORE
    if (q->q_use_no_authentication_ui && CFBooleanGetValue(q->q_use_no_authentication_ui)) {
        SecError(errSecInteractionNotAllowed, *error, CFSTR("authentication UI is not allowed"));
        return false;
    }
    if (isString(q->q_use_operation_prompt)) {
        if (!hints) {
            hints = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        }
        // VRHintAppAction
        CFNumberRef reasonKey = CFNumberCreateWithCFIndex(kCFAllocatorDefault, kLAOptionAuthenticationReason);
        CFDictionaryAddValue(hints, reasonKey, q->q_use_operation_prompt);
        CFRelease(reasonKey);
    }

    if (*error)
        CFReleaseNull(**error);
    
    CFErrorRef authError = NULL;
    CFDataRef ac_data;
    CFArrayForEachC(q->q_required_access_controls, ac_data) {
        ok = VREvaluateACL(q->q_use_cred_handle, ac_data, kAKSKeyOpDecrypt, hints, &authError);
        if (!ok) {
            ok = SecCFCreateError(errSecAuthFailed, kSecErrorDomain, CFSTR("LocalAuthentication failed"), authError, *error);
            CFReleaseSafe(authError);
            goto out;
        }
    }
    CFArrayRemoveAllValues(q->q_required_access_controls);

out:
#endif
    CFReleaseSafe(hints);
    return ok;
}
