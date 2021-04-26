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
 *  SecItemDb.c - CoreFoundation-based constants and functions for
    access to Security items (certificates, keys, identities, and
    passwords.)
 */

#if defined(TARGET_DARWINOS) && TARGET_DARWINOS
#undef OCTAGON
#undef SECUREOBJECTSYNC
#undef SHAREDWEBCREDENTIALS
#endif

#include "keychain/securityd/SecItemDb.h"
#include <utilities/SecAKSWrappers.h>

#include "keychain/securityd/SecDbKeychainItem.h"
#include "keychain/securityd/SecItemSchema.h"
#include "keychain/securityd/SecItemServer.h"
#include <Security/SecAccessControlPriv.h>
#include <Security/SecBasePriv.h>
#include <Security/SecItem.h>
#include <Security/SecSignpost.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemInternal.h>
#include "keychain/securityd/SOSCloudCircleServer.h"
#include <utilities/array_size.h>
#include <utilities/SecIOFormat.h>
#include <utilities/SecCFCCWrappers.h>
#include <uuid/uuid.h>

#include "utilities/SecABC.h"
#include "utilities/sec_action.h"

#include "keychain/ckks/CKKS.h"

#define kSecBackupKeybagUUIDKey CFSTR("keybag-uuid")

const SecDbAttr *SecDbAttrWithKey(const SecDbClass *c,
                                  CFTypeRef key,
                                  CFErrorRef *error) {
    /* Special case: identites can have all attributes of either cert
     or keys. */
    if (c == identity_class()) {
        const SecDbAttr *desc;
        if (!(desc = SecDbAttrWithKey(cert_class(), key, 0)))
            desc = SecDbAttrWithKey(keys_class(), key, error);
        return desc;
    }

    if (isString(key)) {
        SecDbForEachAttr(c, a) {
            if (CFEqual(a->name, key))
                return a;
        }
        if (CFEqual(kSecUseDataProtectionKeychain, key)) {
            return NULL; /* results in no ops for this attribute */
        }
    }

    SecError(errSecNoSuchAttr, error, CFSTR("attribute %@ not found in class %@"), key, c->name);

    return NULL;
}

bool kc_transaction(SecDbConnectionRef dbt, CFErrorRef *error, bool(^perform)(void)) {
    return kc_transaction_type(dbt, kSecDbExclusiveTransactionType, error, perform);
}

bool kc_transaction_type(SecDbConnectionRef dbt, SecDbTransactionType type, CFErrorRef *error, bool(^perform)(void)) {
    __block bool ok = true;
    return ok && SecDbTransaction(dbt, type, error, ^(bool *commit) {
        ok = *commit = perform();
    });
}

static CFStringRef SecDbGetKindSQL(SecDbAttrKind kind) {
    switch (kind) {
        case kSecDbBlobAttr:
        case kSecDbDataAttr:
        case kSecDbUUIDAttr:
        case kSecDbSHA1Attr:
        case kSecDbPrimaryKeyAttr:
        case kSecDbEncryptedDataAttr:
            return CFSTR("BLOB");
        case kSecDbAccessAttr:
        case kSecDbStringAttr:
            return CFSTR("TEXT");
        case kSecDbNumberAttr:
        case kSecDbSyncAttr:
        case kSecDbTombAttr:
            return CFSTR("INTEGER");
        case kSecDbDateAttr:
        case kSecDbCreationDateAttr:
        case kSecDbModificationDateAttr:
            return CFSTR("REAL");
        case kSecDbRowIdAttr:
            return CFSTR("INTEGER PRIMARY KEY AUTOINCREMENT");
        case kSecDbAccessControlAttr:
        case kSecDbUTombAttr:
            /* This attribute does not exist in the DB. */
            return NULL;
    }
}

static void SecDbAppendUnique(CFMutableStringRef sql, CFStringRef value, bool *haveUnique) {
    assert(haveUnique);
    if (!*haveUnique)
        CFStringAppend(sql, CFSTR("UNIQUE("));

    SecDbAppendElement(sql, value, haveUnique);
}

static void SecDbAppendCreateTableWithClass(CFMutableStringRef sql, const SecDbClass *c) {
    CFStringAppendFormat(sql, 0, CFSTR("CREATE TABLE %@("), c->name);
    SecDbForEachAttrWithMask(c,desc,kSecDbInFlag) {
        CFStringAppendFormat(sql, 0, CFSTR("%@ %@"), desc->name, SecDbGetKindSQL(desc->kind));
        if (desc->flags & kSecDbNotNullFlag)
            CFStringAppend(sql, CFSTR(" NOT NULL"));
        if (desc->flags & kSecDbDefault0Flag)
            CFStringAppend(sql, CFSTR(" DEFAULT 0"));
        if (desc->flags & kSecDbDefaultEmptyFlag)
            CFStringAppend(sql, CFSTR(" DEFAULT ''"));
        CFStringAppend(sql, CFSTR(","));
    }

    bool haveUnique = false;
    SecDbForEachAttrWithMask(c,desc,kSecDbPrimaryKeyFlag | kSecDbInFlag) {
        SecDbAppendUnique(sql, desc->name, &haveUnique);
    }
    if (haveUnique)
        CFStringAppend(sql, CFSTR(")"));

    CFStringAppend(sql, CFSTR(");"));

    // Create indices
    SecDbForEachAttrWithMask(c,desc, kSecDbIndexFlag | kSecDbInFlag) {
        if (desc->kind == kSecDbSyncAttr) {
            CFStringAppendFormat(sql, 0, CFSTR("CREATE INDEX %@%@0 ON %@(%@) WHERE %@=0;"), c->name, desc->name, c->name, desc->name, desc->name);
        } else {
            CFStringAppendFormat(sql, 0, CFSTR("CREATE INDEX %@%@ ON %@(%@);"), c->name, desc->name, c->name, desc->name);
        }
    }
}

static void SecDbAppendDropTableWithClass(CFMutableStringRef sql, const SecDbClass *c) {
    CFStringAppendFormat(sql, 0, CFSTR("DROP TABLE %@;"), c->name);
}

static CFDataRef SecPersistentRefCreateWithItem(SecDbItemRef item, CFErrorRef *error) {
    sqlite3_int64 row_id = SecDbItemGetRowId(item, error);
    if (row_id)
        return _SecItemCreatePersistentRef(SecDbItemGetClass(item)->name, row_id, item->attributes);
    return NULL;
}

bool SecItemDbCreateSchema(SecDbConnectionRef dbt, const SecDbSchema *schema, CFArrayRef classIndexesForNewTables, bool includeVersion, CFErrorRef *error)
{
    __block bool ok = true;
    CFMutableStringRef sql = CFStringCreateMutable(kCFAllocatorDefault, 0);
    
    if (classIndexesForNewTables) {
        CFArrayForEach(classIndexesForNewTables, ^(const void* index) {
            const SecDbClass* class = schema->classes[(int)index];
            SecDbAppendCreateTableWithClass(sql, class);
        });
    }
    else {
        for (const SecDbClass * const *pclass = schema->classes; *pclass; ++pclass) {
            SecDbAppendCreateTableWithClass(sql, *pclass);
        }
    }
    
    if (includeVersion) {
        CFStringAppendFormat(sql, NULL, CFSTR("INSERT INTO tversion(version,minor) VALUES(%d, %d);"),
                             schema->majorVersion, schema->minorVersion);
    }
    CFStringPerformWithCString(sql, ^(const char *sql_string) {
        ok = SecDbErrorWithDb(sqlite3_exec(SecDbHandle(dbt), sql_string, NULL, NULL, NULL),
                              SecDbHandle(dbt), error, CFSTR("sqlite3_exec: %s"), sql_string);
    });
    CFReleaseSafe(sql);
    return ok;
}

bool SecItemDbDeleteSchema(SecDbConnectionRef dbt, const SecDbSchema *schema, CFErrorRef *error)
{
    __block bool ok = true;
    CFMutableStringRef sql = CFStringCreateMutable(kCFAllocatorDefault, 0);
    for (const SecDbClass * const *pclass = schema->classes; *pclass; ++pclass) {
        SecDbAppendDropTableWithClass(sql, *pclass);
    }
    CFStringPerformWithCString(sql, ^(const char *sql_string) {
        ok = SecDbErrorWithDb(sqlite3_exec(SecDbHandle(dbt), sql_string, NULL, NULL, NULL),
                              SecDbHandle(dbt), error, CFSTR("sqlite3_exec: %s"), sql_string);
    });
    CFReleaseSafe(sql);
    return ok;
}

CFTypeRef SecDbItemCopyResult(SecDbItemRef item, ReturnTypeMask return_type, CFErrorRef *error) {
    CFTypeRef a_result;

	if (return_type == 0) {
		/* Caller isn't interested in any results at all. */
		a_result = kCFNull;
	} else if (return_type == kSecReturnDataMask) {
        a_result = SecDbItemGetCachedValueWithName(item, kSecValueData);
        if (a_result) {
            CFRetainSafe(a_result);
        } else {
            a_result = CFDataCreate(kCFAllocatorDefault, NULL, 0);
        }
	} else if (return_type == kSecReturnPersistentRefMask) {
		a_result = SecPersistentRefCreateWithItem(item, error);
	} else {
        CFMutableDictionaryRef dict = CFDictionaryCreateMutableForCFTypes(CFGetAllocator(item));
		/* We need to return more than one value. */
        if (return_type & kSecReturnRefMask) {
            CFDictionarySetValue(dict, kSecClass, SecDbItemGetClass(item)->name);
        }
        CFOptionFlags mask = (((return_type & kSecReturnDataMask || return_type & kSecReturnRefMask) ? kSecDbReturnDataFlag : 0) |
                              ((return_type & kSecReturnAttributesMask || return_type & kSecReturnRefMask) ? kSecDbReturnAttrFlag : 0));
        SecDbForEachAttr(SecDbItemGetClass(item), desc) {
            if ((desc->flags & mask) != 0) {
                CFTypeRef value = SecDbItemGetValue(item, desc, error);
                if (value && !CFEqual(kCFNull, value)) {
                    CFDictionarySetValue(dict, desc->name, value);
                } else if (value == NULL) {
                    CFReleaseNull(dict);
                    break;
                }
            }
        }
        CFDictionaryRemoveValue(dict, kSecAttrUUID);

		if (return_type & kSecReturnPersistentRefMask) {
            CFDataRef pref = SecPersistentRefCreateWithItem(item, error);
			CFDictionarySetValue(dict, kSecValuePersistentRef, pref);
            CFReleaseSafe(pref);
		}

		a_result = dict;
	}

	return a_result;
}

/* AUDIT[securityd](done):
 attributes (ok) is a caller provided dictionary, only its cf type has
 been checked.
 */
bool
s3dl_query_add(SecDbConnectionRef dbt, Query *q, CFTypeRef *result, CFErrorRef *error)
{
    if (query_match_count(q) != 0)
        return errSecItemMatchUnsupported;

    /* Add requires a class to be specified unless we are adding a ref. */
    if (q->q_use_item_list)
        return errSecUseItemListUnsupported;

    /* Actual work here. */
    SecDbItemRef item = SecDbItemCreateWithAttributes(kCFAllocatorDefault, q->q_class, q->q_item, KEYBAG_DEVICE, error);
    if (!item)
        return false;
    if (SecDbItemIsTombstone(item))
        SecDbItemSetValue(item, &v7utomb, q->q_use_tomb ? q->q_use_tomb : kCFBooleanTrue, NULL);

    bool ok = true;
    if (q->q_data)
        ok = SecDbItemSetValueWithName(item, CFSTR("v_Data"), q->q_data, error);
    if (q->q_row_id)
        ok = SecDbItemSetRowId(item, q->q_row_id, error);
    if (q->q_musrView)
        ok = SecDbItemSetValueWithName(item, CFSTR("musr"), q->q_musrView, error);
    SecDbItemSetCredHandle(item, q->q_use_cred_handle);

#if OCTAGON
    if(SecCKKSIsEnabled() && !SecCKKSTestDisableAutomaticUUID()) {
        s3dl_item_make_new_uuid(item, q->q_uuid_from_primary_key, error);

        if(q->q_add_sync_callback) {
            CFTypeRef uuid = SecDbItemGetValue(item, &v10itemuuid, error);
            if(uuid) {
                CKKSRegisterSyncStatusCallback(uuid, q->q_add_sync_callback);
            } else {
                secerror("Couldn't fetch UUID from item; can't call callback");
            }
        }
    }
#endif

    if (ok) {
        // We care about the new item's UUID only when we just made it from the primary key.
        // Otherwise, we just made a random one, and don't mind if it changes.
        ok = SecDbItemInsert(item, dbt, q->q_uuid_from_primary_key, error);
    }

    if (ok) {
        if (result && q->q_return_type) {
            *result = SecDbItemCopyResult(item, q->q_return_type, error);
        }
    }
    if (!ok && error && *error) {
        if (CFEqual(CFErrorGetDomain(*error), kSecDbErrorDomain) && CFErrorGetCode(*error) == SQLITE_CONSTRAINT) {
            CFReleaseNull(*error);
            SecError(errSecDuplicateItem, error, CFSTR("duplicate item %@"), item);
        } else if (CFEqual(CFErrorGetDomain(*error), kSecErrorDomain) && CFErrorGetCode(*error) == errSecDecode) { //handle situation when item have pdmn=akpu but passcode is not set
            CFTypeRef value = SecDbItemGetValue(item, SecDbClassAttrWithKind(item->class, kSecDbAccessAttr, error), error);
            if (value && CFEqual(value, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly)) {
                CFReleaseNull(*error);
                SecError(errSecAuthFailed, error, CFSTR("authentication failed"));
            }
        }
    }

    if (ok) {
        q->q_changed = true;
        if (SecDbItemIsSyncable(item))
            q->q_sync_changed = true;
    }

    secdebug("dbitem", "inserting item %@%s%@", item, ok ? "" : "failed: ", ok || error == NULL ? (CFErrorRef)CFSTR("") : *error);

    CFRelease(item);

	return ok;
}

bool s3dl_item_make_new_uuid(SecDbItemRef item, bool uuid_from_primary_key, CFErrorRef* error) {
    if(!item) {
        return false;
    }

    // Set the item UUID.
    CFUUIDRef uuid = NULL;
    // Were we asked to make the UUID static?
    if (uuid_from_primary_key) {
        // This UUID isn't used in any security mechanism, so we can
        // just use the first bits of the SHA256 hash.
        CFDataRef pkhash = SecDbKeychainItemCopySHA256PrimaryKey(item, error);
        if(CFDataGetLength(pkhash) >= 16) {
            UInt8 uuidBytes[16];
            CFRange range = CFRangeMake(0, 16);
            CFDataGetBytes(pkhash, range, uuidBytes);

            uuid = CFUUIDCreateWithBytes(NULL,
                                         uuidBytes[ 0],
                                         uuidBytes[ 1],
                                         uuidBytes[ 2],
                                         uuidBytes[ 3],
                                         uuidBytes[ 4],
                                         uuidBytes[ 5],
                                         uuidBytes[ 6],
                                         uuidBytes[ 7],
                                         uuidBytes[ 8],
                                         uuidBytes[ 9],
                                         uuidBytes[10],
                                         uuidBytes[11],
                                         uuidBytes[12],
                                         uuidBytes[13],
                                         uuidBytes[14],
                                         uuidBytes[15]);
        }
        CFReleaseNull(pkhash);
    }
    if(uuid == NULL) {
        uuid = CFUUIDCreate(NULL);
    }
    SecDbItemSetValueWithName(item, kSecAttrUUID, uuid, error);
    CFReleaseNull(uuid);
    return true;
}

typedef void (*s3dl_handle_row)(sqlite3_stmt *stmt, void *context);

static CFDataRef
s3dl_copy_data_from_col(sqlite3_stmt *stmt, int col, CFErrorRef *error) {
    return CFDataCreateWithBytesNoCopy(0, sqlite3_column_blob(stmt, col),
                                       sqlite3_column_bytes(stmt, col),
                                       kCFAllocatorNull);
}

static bool
s3dl_item_from_col(sqlite3_stmt *stmt, Query *q, int col, CFArrayRef accessGroups,
                   CFMutableDictionaryRef *item, SecAccessControlRef *access_control, keyclass_t* keyclass, CFErrorRef *error) {
    CFDataRef edata = NULL;
    bool ok = false;
    require(edata = s3dl_copy_data_from_col(stmt, col, error), out);
    ok = s3dl_item_from_data(edata, q, accessGroups, item, access_control, keyclass, error);

out:
    CFReleaseSafe(edata);
    return ok;
}

struct s3dl_query_ctx {
    Query *q;
    CFArrayRef accessGroups;
    SecDbConnectionRef dbt;
    CFTypeRef result;
    int found;
};

/* Return whatever the caller requested based on the value of q->q_return_type.
 keys and values must be 3 larger than attr_count in size to accomadate the
 optional data, class and persistent ref results.  This is so we can use
 the CFDictionaryCreate() api here rather than appending to a
 mutable dictionary. */
static CF_RETURNS_RETAINED CFTypeRef
handle_result(Query *q,
              CFMutableDictionaryRef item,
              sqlite_int64 rowid)
{
    CFTypeRef a_result;
    CFDataRef data;
    data = CFDictionaryGetValue(item, kSecValueData);
	CFDataRef pref = NULL;
	if (q->q_return_type & kSecReturnPersistentRefMask) {
		pref = _SecItemCreatePersistentRef(q->q_class->name, rowid, item);
	}
	if (q->q_return_type == 0) {
		/* Caller isn't interested in any results at all. */
		a_result = kCFNull;
	} else if (q->q_return_type == kSecReturnDataMask) {
        if (data) {
            a_result = data;
            CFRetain(a_result);
        } else {
            a_result = CFDataCreate(kCFAllocatorDefault, NULL, 0);
        }
	} else if (q->q_return_type == kSecReturnPersistentRefMask) {
		a_result = _SecItemCreatePersistentRef(q->q_class->name, rowid, item);
	} else {
		/* We need to return more than one value. */
        if (q->q_return_type & kSecReturnRefMask) {
            CFDictionarySetValue(item, kSecClass, q->q_class->name);
        } else if ((q->q_return_type & kSecReturnAttributesMask)) {
            if (!(q->q_return_type & kSecReturnDataMask)) {
                CFDictionaryRemoveValue(item, kSecValueData);
            }

            // Add any attributes which are supposed to be returned, are not present in the decrypted blob,
            // and have a way to generate themselves.
            SecDbItemRef itemRef = NULL;
            SecDbForEachAttrWithMask(q->q_class, attr, kSecDbReturnAttrFlag) {
                if(!CFDictionaryGetValue(item, attr->name) && attr->copyValue) {
                    CFErrorRef cferror = NULL;
                    if(!itemRef) {
                        itemRef = SecDbItemCreateWithAttributes(NULL, q->q_class, item, KEYBAG_DEVICE, &cferror);
                    }
                    if(!cferror && itemRef) {
                        if (attr->kind != kSecDbSHA1Attr || (q->q_return_type & kSecReturnDataMask)) { // we'll skip returning the sha1 attribute unless the client has also asked us to return data, because without data our sha1 could be invalid
                            CFTypeRef attrValue = attr->copyValue(itemRef, attr, &cferror);
                            if (!cferror && attrValue) {
                                CFDictionarySetValue(item, attr->name, attrValue);
                            }
                            CFReleaseNull(attrValue);
                        }
                    }
                    CFReleaseNull(cferror);
                }
            }
            CFReleaseNull(itemRef);

            CFDictionaryRemoveValue(item, kSecAttrUUID);
        } else {
            CFRetainSafe(data);
            CFDictionaryRemoveAllValues(item);
            if ((q->q_return_type & kSecReturnDataMask) && data) {
                CFDictionarySetValue(item, kSecValueData, data);
            }
            CFReleaseSafe(data);
        }
		if (q->q_return_type & kSecReturnPersistentRefMask && pref != NULL) {
			CFDictionarySetValue(item, kSecValuePersistentRef, pref);
		}

		a_result = item;
        CFRetain(item);
	}
	CFReleaseSafe(pref);
	return a_result;
}

static void s3dl_merge_into_dict(const void *key, const void *value, void *context) {
    CFDictionarySetValue(context, key, value);
}

static bool checkTokenObjectID(CFDataRef token_object_id, CFDataRef value_data) {
    bool equalOID = false;
    require_quiet(value_data, out);
    CFDictionaryRef itemValue = SecTokenItemValueCopy(value_data, NULL);
    require_quiet(itemValue, out);
    CFDataRef oID = CFDictionaryGetValue(itemValue, kSecTokenValueObjectIDKey);
    equalOID = CFEqualSafe(token_object_id, oID);
    CFRelease(itemValue);
out:
    return equalOID;
}

static void s3dl_query_row(sqlite3_stmt *stmt, void *context) {
    struct s3dl_query_ctx *c = context;
    Query *q = c->q;
    ReturnTypeMask saved_mask = q->q_return_type;

    sqlite_int64 rowid = sqlite3_column_int64(stmt, 0);
    CFMutableDictionaryRef item = NULL;
    bool ok;

decode:
    ok = s3dl_item_from_col(stmt, q, 1, c->accessGroups, &item, NULL, NULL, &q->q_error);
    if (!ok) {
        OSStatus status = SecErrorGetOSStatus(q->q_error);
        // errSecDecode means the item is corrupted, stash it for delete.
        if (status == errSecDecode) {
            secwarning("ignoring corrupt %@,rowid=%" PRId64 " %@", q->q_class->name, rowid, q->q_error);

            // Can't get rid of this item on the read path. Let's come back from elsewhere.
            CFStringRef tablename = CFStringCreateCopy(kCFAllocatorDefault, q->q_class->name);
            deleteCorruptedItemAsync(c->dbt, tablename, rowid);
            CFReleaseNull(tablename);

            // provide helpful logging statement
            CFDataRef edata = s3dl_copy_data_from_col(stmt, 1, NULL);
            CFMutableStringRef edatastring =  CFStringCreateMutable(kCFAllocatorDefault, 0);
            if(edatastring) {
                CFStringAppendEncryptedData(edatastring, edata);
                secnotice("item", "corrupted edata=%@", edatastring);
            }
            CFReleaseSafe(edata);
            CFReleaseSafe(edatastring);

            CFReleaseNull(q->q_error);  // This item was never here, keep going
        } else if (status == errSecAuthNeeded) {
            secwarning("Authentication is needed for %@,rowid=%" PRId64 " (%" PRIdOSStatus "): %@", q->q_class->name, rowid, status, q->q_error);
        } else if (status == errSecInteractionNotAllowed) {
            static dispatch_once_t kclockedtoken;
            static sec_action_t kclockedaction;
            dispatch_once(&kclockedtoken, ^{
                kclockedaction = sec_action_create("ratelimiterdisabledlogevent", 1);
                sec_action_set_handler(kclockedaction, ^{
                    secerror("decode item failed, keychain is locked (%d)", (int)errSecInteractionNotAllowed);
                });
            });
            sec_action_perform(kclockedaction);
        } else if (status == errSecMissingEntitlement) {
            // That's fine, let's pretend the item never existed for this query.
            // We may find other, better items for the caller!
            CFReleaseNull(q->q_error);
        } else {
            secerror("decode %@,rowid=%" PRId64 " failed (%" PRIdOSStatus "): %@", q->q_class->name, rowid, status, q->q_error);
        }
        // q->q_error will be released appropriately by a call to query_error
        return;
    }

    if (!item)
        goto out;

    if (CFDictionaryContainsKey(item, kSecAttrTokenID) && (q->q_return_type & kSecReturnDataMask) == 0) {
        // For token-based items, to get really meaningful set of attributes we must provide also data field, so augment mask
        // and restart item decoding cycle.
        q->q_return_type |= kSecReturnDataMask;
        CFReleaseNull(item);
        goto decode;
    }

    if (q->q_token_object_id != NULL && !checkTokenObjectID(q->q_token_object_id, CFDictionaryGetValue(item, kSecValueData)))
        goto out;
    
    if (q->q_class == identity_class()) {
        // TODO: Use col 2 for key rowid and use both rowids in persistent ref.

        CFMutableDictionaryRef key;
        /* TODO : if there is a errSecDecode error here, we should cleanup */
        if (!s3dl_item_from_col(stmt, q, 3, c->accessGroups, &key, NULL, NULL, &q->q_error) || !key)
            goto out;

        CFDataRef certData = CFDictionaryGetValue(item, kSecValueData);
        if (certData) {
            CFDictionarySetValue(key, kSecAttrIdentityCertificateData, certData);
            CFDictionaryRemoveValue(item, kSecValueData);
        }

        CFDataRef certTokenID = CFDictionaryGetValue(item, kSecAttrTokenID);
        if (certTokenID) {
            CFDictionarySetValue(key, kSecAttrIdentityCertificateTokenID, certTokenID);
            CFDictionaryRemoveValue(item, kSecAttrTokenID);
        }
        CFDictionaryApplyFunction(item, s3dl_merge_into_dict, key);
        CFRelease(item);
        item = key;
    }

    if (!match_item(c->dbt, q, c->accessGroups, item))
        goto out;

    CFTypeRef a_result = handle_result(q, item, rowid);
    if (a_result) {
        if (a_result == kCFNull) {
            /* Caller wasn't interested in a result, but we still
             count this row as found. */
            CFRelease(a_result);  // Help shut up clang
        } else if (q->q_limit == 1) {
            c->result = a_result;
        } else {
            CFArrayAppendValue((CFMutableArrayRef)c->result, a_result);
            CFRelease(a_result);
        }
        c->found++;
    }

out:
    q->q_return_type = saved_mask;
    CFReleaseSafe(item);
}

static void
SecDbAppendWhereROWID(CFMutableStringRef sql,
                      CFStringRef col, sqlite_int64 row_id,
                      bool *needWhere) {
    if (row_id > 0) {
        SecDbAppendWhereOrAnd(sql, needWhere);
        CFStringAppendFormat(sql, NULL, CFSTR("%@=%lld"), col, row_id);
    }
}

static void
SecDbAppendWhereAttrs(CFMutableStringRef sql, const Query *q, bool *needWhere) {
    CFIndex ix, attr_count = query_attr_count(q);
    for (ix = 0; ix < attr_count; ++ix) {
        SecDbAppendWhereOrAndEquals(sql, query_attr_at(q, ix).key, needWhere);
    }
}

static void
SecDbAppendWhereAccessGroups(CFMutableStringRef sql,
                             CFStringRef col,
                             CFArrayRef accessGroups,
                             bool *needWhere) {
    CFIndex ix, ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        return;
    }

    SecDbAppendWhereOrAnd(sql, needWhere);
    CFStringAppend(sql, col);
    CFStringAppend(sql, CFSTR(" IN (?"));
    for (ix = 1; ix < ag_count; ++ix) {
        CFStringAppend(sql, CFSTR(",?"));
    }
    CFStringAppend(sql, CFSTR(")"));
}

static bool
isQueryOverAllMUSRViews(CFTypeRef musrView)
{
    return SecMUSRIsViewAllViews(musrView);
}

static bool
isQueryOverSingleUserView(CFTypeRef musrView)
{
    return isNull(musrView);
}

#if TARGET_OS_IPHONE
static bool
isQueryOverBothUserAndSystem(CFTypeRef musrView, uid_t *uid)
{
    return SecMUSRGetBothUserAndSystemUUID(musrView, uid);
}
#endif

static void
SecDbAppendWhereMusr(CFMutableStringRef sql,
                     const Query *q,
                     bool *needWhere)
{
    SecDbAppendWhereOrAnd(sql, needWhere);

#if TARGET_OS_IPHONE
    if (isQueryOverBothUserAndSystem(q->q_musrView, NULL)) {
        CFStringAppend(sql, CFSTR("(musr = ? OR musr = ?)"));
    } else
#endif
    if (isQueryOverAllMUSRViews(q->q_musrView)) {
            /* query over all items, regardless of view */
    } else if (isQueryOverSingleUserView(q->q_musrView)) {
        CFStringAppend(sql, CFSTR("musr = ?"));
    } else {
        CFStringAppend(sql, CFSTR("musr = ?"));
    }
}

static void
SecDbAppendWhereAppClip(CFMutableStringRef sql,
                        const Query* q,
                        bool* needWhere)
{
    if (!q->q_skip_app_clip_items) {
        return;
    }

    SecDbAppendWhereOrAnd(sql, needWhere);
    CFStringAppend(sql, CFSTR("clip = 0"));
}

static void SecDbAppendWhereClause(CFMutableStringRef sql, const Query *q,
                                   CFArrayRef accessGroups) {
    bool needWhere = true;
    SecDbAppendWhereROWID(sql, CFSTR("ROWID"), q->q_row_id, &needWhere);
    SecDbAppendWhereAttrs(sql, q, &needWhere);
    SecDbAppendWhereMusr(sql, q, &needWhere);
    SecDbAppendWhereAccessGroups(sql, CFSTR("agrp"), accessGroups, &needWhere);
    SecDbAppendWhereAppClip(sql, q, &needWhere);
}

static void SecDbAppendLimit(CFMutableStringRef sql, CFIndex limit) {
    if (limit != kSecMatchUnlimited)
        CFStringAppendFormat(sql, NULL, CFSTR(" LIMIT %" PRIdCFIndex), limit);
}

static CFStringRef s3dl_create_select_sql(Query *q, CFArrayRef accessGroups) {
    CFMutableStringRef sql = CFStringCreateMutable(NULL, 0);
	if (q->q_class == identity_class()) {
        CFStringAppend(sql, CFSTR("SELECT crowid, certdata"
                                  ", rowid,data FROM "
                                  "(SELECT cert.rowid AS crowid, cert.labl AS labl,"
                                  " cert.issr AS issr, cert.slnr AS slnr, cert.skid AS skid,"
                                  " cert.tkid AS tkid,"
                                  " keys.*,cert.data AS certdata"
                                  " FROM keys, cert"
                                  " WHERE keys.priv == 1 AND cert.pkhh == keys.klbl"));
        SecDbAppendWhereAccessGroups(sql, CFSTR("cert.agrp"), accessGroups, 0);
        /* The next 3 SecDbAppendWhere calls are in the same order as in
         SecDbAppendWhereClause().  This makes sqlBindWhereClause() work,
         as long as we do an extra sqlBindAccessGroups first. */
        SecDbAppendWhereROWID(sql, CFSTR("crowid"), q->q_row_id, 0);
        CFStringAppend(sql, CFSTR(")"));
        bool needWhere = true;
        SecDbAppendWhereAttrs(sql, q, &needWhere);
        SecDbAppendWhereMusr(sql, q, &needWhere);
        SecDbAppendWhereAccessGroups(sql, CFSTR("agrp"), accessGroups, &needWhere);
        SecDbAppendWhereAppClip(sql, q, &needWhere);
	} else {
        // Most of the time we don't need agrp, but if an item fails to decode and we want to know more then this is helpful
        CFStringAppend(sql, CFSTR("SELECT rowid, data, agrp FROM "));
		CFStringAppend(sql, q->q_class->name);
        SecDbAppendWhereClause(sql, q, accessGroups);
    }
    //do not append limit for all queries which needs filtering
    if (q->q_match_issuer == NULL && q->q_match_policy == NULL && q->q_match_valid_on_date == NULL && q->q_match_trusted_only == NULL && q->q_token_object_id == NULL) {
        SecDbAppendLimit(sql, q->q_limit);
    }

    return sql;
}

static bool sqlBindMusr(sqlite3_stmt *stmt, const Query *q, int *pParam, CFErrorRef *error) {
    int param = *pParam;
    bool result = true;
#if TARGET_OS_IPHONE
    uid_t uid;

    if (isQueryOverBothUserAndSystem(q->q_musrView, &uid)) {
        /* network extensions are special and get to query both user and system views */
        CFDataRef systemUUID = SecMUSRGetSystemKeychainUUID();
        result = SecDbBindObject(stmt, param++, systemUUID, error);
        if (result) {
            CFDataRef activeUser = SecMUSRCreateActiveUserUUID(uid);
            result = SecDbBindObject(stmt, param++, activeUser, error);
            CFReleaseNull(activeUser);
        }
    } else
#endif
    if (isQueryOverAllMUSRViews(q->q_musrView)) {
        /* query over all items, regardless of view */
    } else if (isQueryOverSingleUserView(q->q_musrView)) {
        CFDataRef singleUUID = SecMUSRGetSingleUserKeychainUUID();
        result = SecDbBindObject(stmt, param++, singleUUID, error);
    } else {
        result = SecDbBindObject(stmt, param++, q->q_musrView, error);
    }

    *pParam = param;
    return result;
}


static bool sqlBindAccessGroups(sqlite3_stmt *stmt, CFArrayRef accessGroups,
                                int *pParam, CFErrorRef *error) {
    bool result = true;
    int param = *pParam;
    CFIndex ix, count = accessGroups ? CFArrayGetCount(accessGroups) : 0;
    for (ix = 0; ix < count; ++ix) {
        result = SecDbBindObject(stmt, param++,
                                 CFArrayGetValueAtIndex(accessGroups, ix),
                                 error);
        if (!result)
            break;
    }
    *pParam = param;
    return result;
}

static bool sqlBindWhereClause(sqlite3_stmt *stmt, const Query *q,
                               CFArrayRef accessGroups, int *pParam, CFErrorRef *error) {
    bool result = true;
    int param = *pParam;
    CFIndex ix, attr_count = query_attr_count(q);
    for (ix = 0; ix < attr_count; ++ix) {
        result = SecDbBindObject(stmt, param++, query_attr_at(q, ix).value, error);
        if (!result)
            break;
	}

    if (result) {
        result = sqlBindMusr(stmt, q, &param, error);
    }

    /* Bind the access group to the sql. */
    if (result) {
        result = sqlBindAccessGroups(stmt, accessGroups, &param, error);
    }

    *pParam = param;
    return result;
}

bool SecDbItemQuery(SecDbQueryRef query, CFArrayRef accessGroups, SecDbConnectionRef dbconn, CFErrorRef *error,
                    void (^handle_row)(SecDbItemRef item, bool *stop)) {
    __block bool ok = true;
    /* Sanity check the query. */
    if (query->q_ref)
        return SecError(errSecValueRefUnsupported, error, CFSTR("value ref not supported by queries"));

    bool (^return_attr)(const SecDbAttr *attr) = ^bool (const SecDbAttr * attr) {
        // The attributes here must match field list hardcoded in s3dl_select_sql used below, which is
        // "rowid, data"
        return attr->kind == kSecDbRowIdAttr || attr->kind == kSecDbEncryptedDataAttr;
    };

    CFStringRef sql = s3dl_create_select_sql(query, accessGroups);
    ok = sql;
    if (sql) {
        ok &= SecDbPrepare(dbconn, sql, error, ^(sqlite3_stmt *stmt) {
            /* Bind the values being searched for to the SELECT statement. */
            int param = 1;
            if (query->q_class == identity_class()) {
                /* Bind the access groups to cert.agrp. */
                ok &= sqlBindAccessGroups(stmt, accessGroups, &param, error);
            }
            if (ok)
                ok &= sqlBindWhereClause(stmt, query, accessGroups, &param, error);
            if (ok) {
                SecDbStep(dbconn, stmt, error, ^(bool *stop) {
                    SecDbItemRef itemFromStatement = SecDbItemCreateWithStatement(kCFAllocatorDefault, query->q_class, stmt, query->q_keybag, error, return_attr);
                    if (itemFromStatement) {
                        CFTransferRetained(itemFromStatement->credHandle, query->q_use_cred_handle);
                        if (match_item(dbconn, query, accessGroups, itemFromStatement->attributes))
                            handle_row(itemFromStatement, stop);
                        CFReleaseNull(itemFromStatement);
                    } else {
                        secerror("failed to create item from stmt: %@", error ? *error : (CFErrorRef)"no error");
                        if (error) {
                            CFReleaseNull(*error);
                        }
                        //*stop = true;
                        //ok = false;
                    }
                });
            }
        });
        CFRelease(sql);
    }

    return ok;
}

static bool
s3dl_query(s3dl_handle_row handle_row,
           void *context, CFErrorRef *error)
{
    struct s3dl_query_ctx *c = context;
    SecDbConnectionRef dbt = c->dbt;
    Query *q = c->q;
    CFArrayRef accessGroups = c->accessGroups;

    /* Sanity check the query. */
    if (q->q_ref)
        return SecError(errSecValueRefUnsupported, error, CFSTR("value ref not supported by queries"));

	/* Actual work here. */
    if (q->q_limit == 1) {
        c->result = NULL;
    } else {
        c->result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    }
    CFStringRef sql = s3dl_create_select_sql(q, accessGroups);
    bool ok = SecDbWithSQL(dbt, sql, error, ^(sqlite3_stmt *stmt) {
        bool sql_ok = true;
        /* Bind the values being searched for to the SELECT statement. */
        int param = 1;
        if (q->q_class == identity_class()) {
            /* Bind the access groups to cert.agrp. */
            sql_ok = sqlBindAccessGroups(stmt, accessGroups, &param, error);
        }
        if (sql_ok)
            sql_ok = sqlBindWhereClause(stmt, q, accessGroups, &param, error);
        if (sql_ok) {
            SecDbForEach(dbt, stmt, error, ^bool (int row_index) {
                handle_row(stmt, context);

                bool needs_auth = q->q_error && CFErrorGetCode(q->q_error) == errSecAuthNeeded;
                if (q->q_skip_acl_items && needs_auth)
                    // Skip items needing authentication if we are told to do so.
                    CFReleaseNull(q->q_error);

                bool stop = q->q_limit != kSecMatchUnlimited && c->found >= q->q_limit;
                stop = stop || (q->q_error && !needs_auth);
                return !stop;
            });
        }
        return sql_ok;
    });

    CFRelease(sql);

    // First get the error from the query, since errSecDuplicateItem from an
    // update query should superceed the errSecItemNotFound below.
    if (!query_error(q, error))
        ok = false;
    if (ok && c->found == 0) {
        ok = SecError(errSecItemNotFound, error, CFSTR("no matching items found"));
        if (q->q_spindump_on_failure) {
            __security_stackshotreport(CFSTR("ItemNotFound"), __sec_exception_code_LostInMist);
        }
    }

    return ok;
}

bool
s3dl_copy_matching(SecDbConnectionRef dbt, Query *q, CFTypeRef *result,
                   CFArrayRef accessGroups, CFErrorRef *error)
{
    struct s3dl_query_ctx ctx = {
        .q = q, .accessGroups = accessGroups, .dbt = dbt,
    };
    if (q->q_row_id && query_attr_count(q))
        return SecError(errSecItemIllegalQuery, error,
                        CFSTR("attributes to query illegal; both row_id and other attributes can't be searched at the same time"));
    if (q->q_token_object_id && query_attr_count(q) != 1)
        return SecError(errSecItemIllegalQuery, error,
                        CFSTR("attributes to query illegal; both token persistent ref and other attributes can't be searched at the same time"));

    // Only copy things that aren't tombstones unless the client explicitly asks otherwise.
    if (!CFDictionaryContainsKey(q->q_item, kSecAttrTombstone))
        query_add_attribute(kSecAttrTombstone, kCFBooleanFalse, q);
    bool ok = s3dl_query(s3dl_query_row, &ctx, error);
    if (ok && result)
        *result = ctx.result;
    else
        CFReleaseSafe(ctx.result);

    return ok;
}

typedef void (^s3dl_item_digest_callback)(CFDataRef persistantReference, CFDataRef encryptedData);

struct s3dl_digest_ctx {
    Query *q;
    SecDbConnectionRef dbt;
    s3dl_item_digest_callback item_callback;
};

static void s3dl_query_row_digest(sqlite3_stmt *stmt, void *context) {
    struct s3dl_query_ctx *c = context;
    Query *q = c->q;

    sqlite_int64 rowid = sqlite3_column_int64(stmt, 0);
    CFDataRef edata = s3dl_copy_data_from_col(stmt, 1, NULL);
    CFDataRef persistant_reference = _SecItemCreatePersistentRef(q->q_class->name, rowid, NULL);
    CFDataRef digest = NULL;

    if (edata) {
        digest = CFDataCopySHA256Digest(edata, NULL);
    }

    if (digest && persistant_reference) {
        CFDictionaryRef item = CFDictionaryCreateForCFTypes(NULL,
                                                            kSecValuePersistentRef, persistant_reference,
                                                            kSecValueData, digest,
                                                            NULL);
        if (item)
            CFArrayAppendValue((CFMutableArrayRef)c->result, item);
        CFReleaseNull(item);
        c->found++;
    } else {
        secinfo("item", "rowid %lu in %@ failed to create pref/digest", (unsigned long)rowid, q->q_class->name);
    }
    CFReleaseNull(digest);
    CFReleaseNull(edata);
    CFReleaseNull(persistant_reference);
}


bool
s3dl_copy_digest(SecDbConnectionRef dbt, Query *q, CFArrayRef *result, CFArrayRef accessGroups, CFErrorRef *error)
{
    struct s3dl_query_ctx ctx = {
        .q = q, .dbt = dbt, .accessGroups = accessGroups,
    };
    // Force to always return an array
    q->q_limit = kSecMatchUnlimited;
    // This interface only queries live data
    query_add_attribute(kSecAttrTombstone, kCFBooleanFalse, q);
    bool ok = s3dl_query(s3dl_query_row_digest, &ctx, error);
    if (ok && result)
        *result = (CFArrayRef)ctx.result;
    else
        CFReleaseSafe(ctx.result);

    return ok;
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

/* Update modification_date if needed. */
static void query_pre_update(Query *q) {
    SecDbForEachAttr(q->q_class, desc) {
        if (desc->kind == kSecDbModificationDateAttr) {
            CFDateRef now = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
            query_set_attribute_with_desc(desc, now, q);
            CFReleaseSafe(now);
        }
    }
}

/* Make sure all attributes that are marked as not_null have a value.  If
 force_date is false, only set mdat and cdat if they aren't already set. */
void query_pre_add(Query *q, bool force_date) {
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
                if (desc->kind == kSecDbDataAttr || desc->kind == kSecDbUUIDAttr)
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

// Return a tri state value false->never make a tombstone, true->always make a
// tombstone, NULL->make a tombstone, but delete it if the tombstone itself is not currently being synced.
static CFBooleanRef s3dl_should_make_tombstone(Query *q, bool item_is_syncable, SecDbItemRef item) {
    if (q->q_use_tomb)
        return q->q_use_tomb;
    else if (item_is_syncable && !SecDbItemIsTombstone(item))
        return NULL;
    else
        return kCFBooleanFalse;
}

bool
s3dl_query_update(SecDbConnectionRef dbt, Query *q,
                  CFDictionaryRef attributesToUpdate, CFArrayRef accessGroups, CFErrorRef *error)
{
    /* Sanity check the query. */
    if (query_match_count(q) != 0)
        return SecError(errSecItemMatchUnsupported, error, CFSTR("match not supported in attributes to update"));
    if (q->q_ref)
        return SecError(errSecValueRefUnsupported, error, CFSTR("value ref not supported in attributes to update"));
    if (q->q_row_id && query_attr_count(q))
        return SecError(errSecItemIllegalQuery, error, CFSTR("attributes to update illegal; both row_id and other attributes can't be updated at the same time"));
    if (q->q_token_object_id && query_attr_count(q) != 1)
        return SecError(errSecItemIllegalQuery, error, CFSTR("attributes to update illegal; both token persistent ref and other attributes can't be updated at the same time"));

    __block bool result = true;
    Query *u = query_create(q->q_class, NULL, attributesToUpdate, NULL, error);
    if (u == NULL) return false;
    require_action_quiet(query_update_parse(u, attributesToUpdate, error), errOut, result = false);
    query_pre_update(u);
    result &= SecDbTransaction(dbt, kSecDbExclusiveTransactionType, error, ^(bool *commit) {
        // Make sure we only update real items, not tombstones, unless the client explicitly asks otherwise.
        if (!CFDictionaryContainsKey(q->q_item, kSecAttrTombstone))
            query_add_attribute(kSecAttrTombstone, kCFBooleanFalse, q);
        result &= SecDbItemQuery(q, accessGroups, dbt, error, ^(SecDbItemRef item, bool *stop) {
            // We always need to know the error here.
            CFErrorRef localError = NULL;
            if (q->q_token_object_id) {
                const SecDbAttr *valueDataAttr = SecDbClassAttrWithKind(item->class, kSecDbDataAttr, NULL);
                CFDataRef valueData = SecDbItemGetValue(item, valueDataAttr, NULL);
                if (q->q_token_object_id != NULL && !checkTokenObjectID(q->q_token_object_id, valueData))
                    return;
            }
            // Cache the storedSHA1 digest so we use the one from the db not the recomputed one for notifications.
            const SecDbAttr *sha1attr = SecDbClassAttrWithKind(item->class, kSecDbSHA1Attr, NULL);
            CFDataRef storedSHA1 = CFRetainSafe(SecDbItemGetValue(item, sha1attr, NULL));
            SecDbItemRef new_item = SecDbItemCopyWithUpdates(item, u->q_item, &localError);
            SecDbItemSetValue(item, sha1attr, storedSHA1, NULL);
            CFReleaseSafe(storedSHA1);
            if (SecErrorGetOSStatus(localError) == errSecDecode) {
                // We just ignore this, and treat as if item is not found.
                secwarning("deleting corrupt %@,rowid=%" PRId64 " %@", q->q_class->name, SecDbItemGetRowId(item, NULL), localError);
                CFReleaseNull(localError);
                if (!SecDbItemDelete(item, dbt, false, false, &localError)) {
                    secerror("failed to delete corrupt %@,rowid=%" PRId64 " %@", q->q_class->name, SecDbItemGetRowId(item, NULL), localError);
                    CFReleaseNull(localError);
                }
                CFReleaseNull(new_item);
                return;
            }
            if (new_item != NULL && u->q_access_control != NULL)
                SecDbItemSetAccessControl(new_item, u->q_access_control, &localError);
            result = SecErrorPropagate(localError, error) && new_item;
            if (new_item) {
                bool item_is_sync = SecDbItemIsSyncable(item);
                result = SecDbItemUpdate(item, new_item, dbt, s3dl_should_make_tombstone(q, item_is_sync, item), q->q_uuid_from_primary_key, error);
                if (result) {
                    q->q_changed = true;
                    if (item_is_sync || SecDbItemIsSyncable(new_item))
                        q->q_sync_changed = true;
                }
                CFRelease(new_item);
            }
        });
        if (!result)
            *commit = false;
    });
    if (result && !q->q_changed)
        result = SecError(errSecItemNotFound, error, CFSTR("No items updated"));
errOut:
    if (!query_destroy(u, error))
        result = false;
    return result;
}

static bool SecDbItemNeedAuth(SecDbItemRef item, CFErrorRef *error)
{
    CFErrorRef localError = NULL;
    if (!SecDbItemEnsureDecrypted(item, true, &localError) && localError && CFErrorGetCode(localError) == errSecAuthNeeded) {
        if (error)
        *error = localError;
        return true;
    }

    CFReleaseSafe(localError);
    return false;
}

bool
s3dl_query_delete(SecDbConnectionRef dbt, Query *q, CFArrayRef accessGroups, CFErrorRef *error)
{
    __block bool ok = true;
    __block bool needAuth = false;
    // Only delete things that aren't tombstones, unless the client explicitly asks otherwise.
    if (!CFDictionaryContainsKey(q->q_item, kSecAttrTombstone))
        query_add_attribute(kSecAttrTombstone, kCFBooleanFalse, q);
    ok &= SecDbItemSelect(q, dbt, error, NULL, ^bool(const SecDbAttr *attr) {
        return false;
    },^bool(CFMutableStringRef sql, bool *needWhere) {
        SecDbAppendWhereClause(sql, q, accessGroups);
        return true;
    },^bool(sqlite3_stmt * stmt, int col) {
        return sqlBindWhereClause(stmt, q, accessGroups, &col, error);
    }, ^(SecDbItemRef item, bool *stop) {
        // Check if item for token persitence ref
        if (q->q_token_object_id) {
            const SecDbAttr *valueDataAttr = SecDbClassAttrWithKind(item->class, kSecDbDataAttr, NULL);
            CFDataRef valueData = SecDbItemGetValue(item, valueDataAttr, NULL);
            if (q->q_token_object_id != NULL && !checkTokenObjectID(q->q_token_object_id, valueData))
                return;
        }
        // Check if item need to be authenticated by LocalAuthentication
        item->cryptoOp = kAKSKeyOpDelete;
        if (SecDbItemNeedAuth(item, error)) {
            needAuth = true;
            return;
        }
        // Cache the storedSHA1 digest so we use the one from the db not the recomputed one for notifications.
        const SecDbAttr *sha1attr = SecDbClassAttrWithKind(item->class, kSecDbSHA1Attr, NULL);
        CFDataRef storedSHA1 = CFRetainSafe(SecDbItemGetValue(item, sha1attr, NULL));
        bool item_is_sync = SecDbItemIsSyncable(item);
        SecDbItemSetValue(item, sha1attr, storedSHA1, NULL);
        CFReleaseSafe(storedSHA1);
        ok = SecDbItemDelete(item, dbt, s3dl_should_make_tombstone(q, item_is_sync, item), q->q_tombstone_use_mdat_from_item, error);
        if (ok) {
            q->q_changed = true;
            if (item_is_sync)
                q->q_sync_changed = true;
        }
    });
    if (ok && !q->q_changed && !needAuth) {
        ok = SecError(errSecItemNotFound, error, CFSTR("Delete failed to delete anything"));
    }
    return ok && !needAuth;
}

static bool
matchAnyString(CFStringRef needle, CFStringRef *haystack)
{
    while (*haystack) {
        if (CFEqual(needle, *haystack))
            return true;
        haystack++;
    }
    return false;
}

/* Return true iff the item in question should not be backed up, nor restored,
 but when restoring a backup the original version of the item should be
 added back to the keychain again after the restore completes. */
bool SecItemIsSystemBound(CFDictionaryRef item, const SecDbClass *cls, bool multiUser) {
    CFNumberRef sysb =  CFDictionaryGetValue(item, kSecAttrSysBound);
    if (isNumber(sysb)) {
        int32_t num = 0;
        if (!CFNumberGetValue(sysb, kCFNumberSInt32Type, &num))
            return false;
        if (num == kSecSecAttrSysBoundNot) {
            return false;
        } else if (num == kSecSecAttrSysBoundPreserveDuringRestore) {
            return true;
        }
        return true;
    }

    CFStringRef agrp = CFDictionaryGetValue(item, kSecAttrAccessGroup);
    if (!isString(agrp))
        return false;

    if (CFEqualSafe(agrp, kSOSInternalAccessGroup)) {
        secdebug("backup", "found sysbound item: %@", item);
        return true;
    }

    if (CFEqual(agrp, CFSTR("lockdown-identities"))) {
        secdebug("backup", "found sys_bound item: %@", item);
        return true;
    }

    if (CFEqual(agrp, CFSTR("apple")) && cls == genp_class()) {
        CFStringRef service = CFDictionaryGetValue(item, kSecAttrService);
        CFStringRef account = CFDictionaryGetValue(item, kSecAttrAccount);

        if (isString(service) && isString(account)) {
            static CFStringRef mcAccounts[] = {
                CFSTR("Public"),
                CFSTR("Private"),
                NULL,
            };

            if (CFEqual(service, CFSTR("com.apple.managedconfiguration"))
                && matchAnyString(account, mcAccounts))
            {
                secdebug("backup", "found sys_bound item: %@", item);
                return true;
            }
        }

        if (isString(service) && CFEqual(service, CFSTR("com.apple.account.CloudKit.token"))) {
            secdebug("backup", "found sys_bound item: %@", item);
            return true;
        }

        if (isString(service) && CFEqual(service, CFSTR("com.apple.account.idms.continuation-key"))) {
            secdebug("backup", "found sys_bound item: %@", item);
            return true;
        }
    }

    if (multiUser && CFEqual(agrp, CFSTR("com.apple.apsd")) && cls == genp_class()) {
        static CFStringRef pushServices[] = {
            CFSTR("push.apple.com"),
            CFSTR("push.apple.com,PerAppToken.v0"),
            NULL
        };
        CFStringRef service = CFDictionaryGetValue(item, kSecAttrService);

        if (isString(service) && matchAnyString(service, pushServices)) {
            secdebug("backup", "found sys_bound item: %@", item);
            return true;
        }
    }

    if (multiUser && CFEqual(agrp, CFSTR("appleaccount")) && cls == genp_class()) {
        static CFStringRef accountServices[] = {
            CFSTR("com.apple.appleaccount.fmf.token"), /* temporary tokens while accout is being setup */
            CFSTR("com.apple.appleaccount.fmf.apptoken"),
            CFSTR("com.apple.appleaccount.fmip.siritoken"),
            CFSTR("com.apple.appleaccount.cloudkit.token"),
            NULL
        };
        CFStringRef service = CFDictionaryGetValue(item, kSecAttrService);

        if (isString(service) && matchAnyString(service, accountServices)) {
            secdebug("backup", "found exact sys_bound item: %@", item);
            return true;
        }
    }

    if (multiUser && CFEqual(agrp, CFSTR("apple")) && cls == genp_class()) {
        static CFStringRef accountServices[] = {
            /* accounts, remove with rdar://37595482 */
            CFSTR("com.apple.account.AppleAccount.token"),
            CFSTR("com.apple.account.AppleAccount.password"),
            CFSTR("com.apple.account.AppleAccount.rpassword"),
            CFSTR("com.apple.account.idms.token"),
            CFSTR("com.apple.account.idms.heartbeat-token"),
            CFSTR("com.apple.account.idms.continuation-key"),
            CFSTR("com.apple.account.CloudKit.token"),
            CFSTR("com.apple.account.IdentityServices.password"), /* accountsd for ids */
            CFSTR("com.apple.account.IdentityServices.rpassword"),
            CFSTR("com.apple.account.IdentityServices.token"),
            /* IDS stuff */
            CFSTR("BackupIDSAccountToken"),
            CFSTR("com.apple.ids"),
            CFSTR("ids"),
            CFSTR("IDS"),
            NULL
        };
        CFStringRef service = CFDictionaryGetValue(item, kSecAttrService);

        if (isString(service) && matchAnyString(service, accountServices)) {
            secdebug("backup", "found exact sys_bound item: %@", item);
            return true;
        }
        if (isString(service) && CFStringHasPrefix(service, CFSTR("com.apple.gs."))) {
            secdebug("backup", "found exact sys_bound item: %@", item);
            return true;
        }
        if (isString(service) && CFEqual(service, CFSTR("com.apple.facetime"))) {
            CFStringRef account = CFDictionaryGetValue(item, kSecAttrAccount);
            if (isString(account) && CFEqual(account, CFSTR("registrationV1"))) {
                secdebug("backup", "found exact sys_bound item: %@", item);
                return true;
            }
        }
    }

    /* accounts, remove with rdar://37595482 */
    if (multiUser && CFEqual(agrp, CFSTR("com.apple.ind")) && cls == genp_class()) {
        CFStringRef service = CFDictionaryGetValue(item, kSecAttrService);
        if (isString(service) && CFEqual(service, CFSTR("com.apple.ind.registration"))) {
            secdebug("backup", "found exact sys_bound item: %@", item);
            return true;
        }
    }

    if (multiUser && CFEqual(agrp, CFSTR("ichat")) && cls == genp_class()) {
        static CFStringRef accountServices[] = {
            CFSTR("ids"),
            NULL
        };
        CFStringRef service = CFDictionaryGetValue(item, kSecAttrService);

        if (isString(service) && matchAnyString(service, accountServices)) {
            secdebug("backup", "found exact sys_bound item: %@", item);
            return true;
        }
    }

    if (multiUser && CFEqual(agrp, CFSTR("ichat")) && cls == keys_class()) {
        static CFStringRef exactMatchingLabel[] = {
            CFSTR("iMessage Encryption Key"),
            CFSTR("iMessage Signing Key"),
        };
        CFStringRef label = CFDictionaryGetValue(item, kSecAttrLabel);
        if (isString(label)) {
            if (matchAnyString(label, exactMatchingLabel)) {
                secdebug("backup", "found exact sys_bound item: %@", item);
                return true;
            }
        }
    }

    if (multiUser && CFEqual(agrp, CFSTR("com.apple.rapport")) && cls == genp_class()) {
        secdebug("backup", "found exact sys_bound item: %@", item);
        return true;
    }

    secdebug("backup", "found non sys_bound item: %@", item);
    return false;
}

/* Delete all items from the current keychain.  If this is not an in
 place upgrade we don't delete items in the 'lockdown-identities'
 access group, this ensures that an import or restore of a backup
 will never overwrite an existing activation record. */
static bool SecServerDeleteAll(SecDbConnectionRef dbt, CFErrorRef *error) {
    secwarning("SecServerDeleteAll");

    return kc_transaction(dbt, error, ^{

        bool ok = (SecDbExec(dbt, CFSTR("DELETE from genp;"), error) &&
                   SecDbExec(dbt, CFSTR("DELETE from inet;"), error) &&
                   SecDbExec(dbt, CFSTR("DELETE from cert;"), error) &&
                   SecDbExec(dbt, CFSTR("DELETE from keys;"), error));
        return ok;
    });
}

#if TARGET_OS_IPHONE

static bool DeleteAllFromTableForMUSRView(SecDbConnectionRef dbt,
                                          CFStringRef sql,
                                          CFDataRef musr,
                                          bool keepU,
                                          CFErrorRef *error)
{
    sqlite3_stmt *stmt = NULL;
    CFStringRef sql2 = NULL;
    bool ok = false;
    
    if (keepU) {
        sql2 = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@ AND pdmn NOT IN ('aku','akpu','cku','dku')"), sql);
    } else {
        sql2 = CFRetain(sql);
    }
    require(sql2, fail);

    stmt = SecDbCopyStmt(dbt, sql2, NULL, error);
    require(stmt, fail);

    ok = SecDbBindObject(stmt, 1, musr, error);
    require(ok, fail);

    ok = SecDbStep(dbt, stmt, error, ^(bool *stop) { });
    require(ok, fail);

fail:
    if (stmt) {
        ok = SecDbFinalize(stmt, error);
    }
    if (!ok) {
        secwarning("DeleteAllFromTableForMUSRView failed for %@ for musr: %@: %@", sql2, musr, error ? *error : NULL);
    }
    
    CFReleaseNull(sql2);

    return ok;
}

bool SecServerDeleteAllForUser(SecDbConnectionRef dbt, CFDataRef musrView, bool keepU, CFErrorRef *error) {
    secwarning("SecServerDeleteAllForUser for user: %@ keepU %s", musrView, keepU ? "yes" : "no");

    return kc_transaction(dbt, error, ^{
        bool ok;

        ok = (DeleteAllFromTableForMUSRView(dbt, CFSTR("DELETE FROM genp WHERE musr = ?"), musrView, keepU, error) &&
              DeleteAllFromTableForMUSRView(dbt, CFSTR("DELETE FROM inet WHERE musr = ?"), musrView, keepU, error) &&
              DeleteAllFromTableForMUSRView(dbt, CFSTR("DELETE FROM cert WHERE musr = ?"), musrView, keepU, error) &&
              DeleteAllFromTableForMUSRView(dbt, CFSTR("DELETE FROM keys WHERE musr = ?"), musrView, keepU, error));

        return ok;
    });
}
#endif

OSStatus SecServerDeleteForAppClipApplicationIdentifier(CFStringRef identifier) {
    secnotice("item", "Request to delete app clip keychain items for identifier '%@'", identifier);

    __block CFErrorRef cfError = NULL;
    __block bool ok = true;
    ok &= kc_with_dbt(true, &cfError, ^bool(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, &cfError, ^bool{
            const SecDbSchema* schema = current_schema();
            for (const SecDbClass *const * class = schema->classes; *class != NULL; ++class) {
                if ((*class)->itemclass) {
                    CFStringRef sqlStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("DELETE FROM %@ WHERE agrp = ? AND clip = 1"), (*class)->name);
                    ok &= SecDbPrepare(dbt, sqlStr, &cfError, ^(sqlite3_stmt *stmt) {
                        ok &= SecDbBindObject(stmt, 1, identifier, &cfError);
                        ok &= SecDbStep(dbt, stmt, &cfError, NULL);
                    });
                    CFReleaseNull(sqlStr);
                }
            }
            return ok;
        });
    });

    OSStatus status = ok ? errSecSuccess : errSecInternal;
    secnotice("item", "Finished request to delete app clip keychain items for identifier '%@' with status %i: %@", identifier, (int)status, cfError);
    CFReleaseNull(cfError);

    return status;
}

struct s3dl_export_row_ctx {
    struct s3dl_query_ctx qc;
    keybag_handle_t dest_keybag;
    enum SecItemFilter filter;
    bool multiUser;
};

static void s3dl_export_row(sqlite3_stmt *stmt, void *context) {
    struct s3dl_export_row_ctx *c = context;
    Query *q = c->qc.q;
    SecAccessControlRef access_control = NULL;
    CFErrorRef localError = NULL;

    sqlite_int64 rowid = sqlite3_column_int64(stmt, 0);
    CFMutableDictionaryRef allAttributes = NULL;
    CFMutableDictionaryRef metadataAttributes = NULL;
    CFMutableDictionaryRef secretStuff = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    keyclass_t keyclass = 0;
    bool ok = s3dl_item_from_col(stmt, q, 1, c->qc.accessGroups, &allAttributes, &access_control, &keyclass, &localError);

    if (ok) {
        metadataAttributes = CFDictionaryCreateMutableCopy(NULL, 0, allAttributes);
        SecDbForEachAttrWithMask(q->q_class, desc, kSecDbReturnDataFlag) {
            CFTypeRef value = CFDictionaryGetValue(metadataAttributes, desc->name);
            if (value) {
                CFDictionarySetValue(secretStuff, desc->name, value);
                CFDictionaryRemoveValue(metadataAttributes, desc->name);
            }
        }
    }

    /* Skip akpu items when backing up, those are intentionally lost across restores. The same applies to SEP-based keys */
    bool is_akpu = access_control ? CFEqualSafe(SecAccessControlGetProtection(access_control), kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly)
                                  // Mask generation, only look at class per se
                                  : (keyclass & key_class_last) == key_class_akpu;
    bool is_token = (ok && allAttributes != NULL) ? CFDictionaryContainsKey(allAttributes, kSecAttrTokenID) : false;
    bool skip_akpu_or_token = (is_akpu || is_token) && c->filter == kSecBackupableItemFilter;

    if (ok && allAttributes && !skip_akpu_or_token) {
        /* Only export sysbound items if do_sys_bound is true, only export non sysbound items otherwise. */
        bool do_sys_bound = c->filter == kSecSysBoundItemFilter;
        if (c->filter == kSecNoItemFilter ||
            SecItemIsSystemBound(allAttributes, q->q_class, c->multiUser) == do_sys_bound) {
            /* Re-encode the item. */
            secdebug("item", "export rowid %llu item: %@", rowid, allAttributes);
            /* The code below could be moved into handle_row. */
            CFDataRef pref = _SecItemCreatePersistentRef(q->q_class->name, rowid, allAttributes);
            if (pref) {
                if (c->dest_keybag != KEYBAG_NONE) {
                    CFMutableDictionaryRef auth_attribs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                    SecDbForEachAttrWithMask(q->q_class, desc, kSecDbInAuthenticatedDataFlag) {
                        CFTypeRef value = CFDictionaryGetValue(metadataAttributes, desc->name);
                        if(value) {
                            CFDictionaryAddValue(auth_attribs, desc->name, value);
                            CFDictionaryRemoveValue(metadataAttributes, desc->name);
                        }
                    }

                    /* Encode and encrypt the item to the specified keybag. */
                    CFDataRef edata = NULL;
                    bool encrypted = ks_encrypt_data(c->dest_keybag, access_control, q->q_use_cred_handle, secretStuff, metadataAttributes, auth_attribs, &edata, false, &q->q_error);
                    CFDictionaryRemoveAllValues(allAttributes);
                    CFRelease(auth_attribs);
                    if (encrypted) {
                        CFDictionarySetValue(allAttributes, kSecValueData, edata);
                        CFReleaseSafe(edata);
                    } else {
                        seccritical("ks_encrypt_data %@,rowid=%" PRId64 ": failed: %@", q->q_class->name, rowid, q->q_error);
                        CFReleaseNull(q->q_error);
                    }
                }
                if (CFDictionaryGetCount(allAttributes)) {
                    CFDictionarySetValue(allAttributes, kSecValuePersistentRef, pref);
                    CFArrayAppendValue((CFMutableArrayRef)c->qc.result, allAttributes);
                    c->qc.found++;
                }
                CFReleaseSafe(pref);
            }
        }
    } else if (!ok || !allAttributes) {
        OSStatus status = SecErrorGetOSStatus(localError);

        if (status == errSecInteractionNotAllowed && is_akpu) {
            if (skip_akpu_or_token) {
                secdebug("item", "Skipping akpu item for backup");
            } else {    // Probably failed to decrypt sysbound item. Should never be an akpu item in backup.
                secerror("Encountered akpu item we cannot export (filter %d), skipping. %@", c->filter, localError);
                if (sqlite3_column_count(stmt) > 2) {   // Should have rowid,data,agrp from s3dl_create_select_sql
                    CFStringRef agrp = CFStringCreateWithCString(kCFAllocatorDefault, (const char*)sqlite3_column_text(stmt, 2), kCFStringEncodingUTF8);
                    if (agrp) {
                        CFDictionaryRef payload = CFDictionaryCreateForCFTypes(NULL, CFSTR("agrp"), agrp, NULL);
                        SecABCTrigger(CFSTR("keychain"), CFSTR("invalid-akpu+sysbound"), NULL, payload);
                        CFReleaseNull(payload);
                    }
                    CFReleaseNull(agrp);
                }
            }
            // We expect akpu items to be inaccessible when the device is locked.
            CFReleaseNull(localError);
        } else {
            /* This happens a lot when trying to migrate keychain before first unlock, so only a notice */
            /* If the error is "corrupted item" then we just ignore it, otherwise we save it in the query */
            secinfo("item","Could not export item for rowid %llu: %@", rowid, localError);

            if (status == errSecDecode) {
                CFReleaseNull(localError);
            } else {
                CFReleaseSafe(q->q_error);
                q->q_error = localError;
            }
        }
    } else {
        secnotice("item", "export rowid %llu skipped. akpu/token: %i", rowid, skip_akpu_or_token);
    }

    CFReleaseNull(access_control);
    CFReleaseNull(allAttributes);
    CFReleaseNull(metadataAttributes);
    CFReleaseNull(secretStuff);
}

static CFStringRef
SecCreateKeybagUUID(keybag_handle_t keybag)
{
#if !TARGET_HAS_KEYSTORE
    return NULL;
#else
    char uuidstr[37];
    uuid_t uuid;
    if (aks_get_bag_uuid(keybag, uuid) != KERN_SUCCESS)
        return NULL;
    uuid_unparse_lower(uuid, uuidstr);
    return CFStringCreateWithCString(NULL, uuidstr, kCFStringEncodingUTF8);
#endif
}

CFDictionaryRef
SecServerCopyKeychainPlist(SecDbConnectionRef dbt,
                           SecurityClient *client,
                           keybag_handle_t src_keybag,
                           keybag_handle_t dest_keybag,
                           enum SecItemFilter filter,
                           CFErrorRef *error) {
    CFMutableDictionaryRef keychain;
    keychain = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                         &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    unsigned class_ix;
    bool inMultiUser = false;
    CFStringRef keybaguuid = NULL;
    Query q = { .q_keybag = src_keybag,
        .q_musrView = NULL
    };

    if (!keychain) {
        if (error && !*error)
            SecError(errSecAllocate, error, CFSTR("Can't create keychain dictionary"));
        goto errOut;
    }

    q.q_return_type =
        kSecReturnDataMask |
        kSecReturnAttributesMask |
        kSecReturnPersistentRefMask;
    q.q_limit = kSecMatchUnlimited;
    q.q_skip_acl_items = true;
    q.q_skip_app_clip_items = true;

#if TARGET_OS_IPHONE
    if (client && client->inMultiUser) {
        q.q_musrView = SecMUSRCreateActiveUserUUID(client->uid);
        inMultiUser = true;
    } else
#endif
    {
        q.q_musrView = SecMUSRGetSingleUserKeychainUUID();
        CFRetain(q.q_musrView);

        keybaguuid = SecCreateKeybagUUID(dest_keybag);
        if (keybaguuid) {
            CFDictionarySetValue(keychain, kSecBackupKeybagUUIDKey, keybaguuid);
        }
    }

    /* Get rid of this duplicate. */
    const SecDbClass *SecDbClasses[] = {
        genp_class(),
        inet_class(),
        cert_class(),
        keys_class()
    };

    for (class_ix = 0; class_ix < array_size(SecDbClasses);
         ++class_ix) {
        q.q_class = SecDbClasses[class_ix];
        struct s3dl_export_row_ctx ctx = {
            .qc = { .q = &q, .dbt = dbt },
            .dest_keybag = dest_keybag, .filter = filter,
            .multiUser = inMultiUser,
        };

        secnotice("item", "exporting %ssysbound class '%@'", filter != kSecSysBoundItemFilter ? "non-" : "", q.q_class->name);

        CFErrorRef localError = NULL;
        if (s3dl_query(s3dl_export_row, &ctx, &localError)) {
            secnotice("item", "exporting class '%@' complete", q.q_class->name);
            if (CFArrayGetCount(ctx.qc.result)) {
                SecSignpostBackupCount(SecSignpostImpulseBackupClassCount, q.q_class->name, CFArrayGetCount(ctx.qc.result), filter);
                CFDictionaryAddValue(keychain, q.q_class->name, ctx.qc.result);
            }

        } else {
            OSStatus status = (OSStatus)CFErrorGetCode(localError);
            if (status == errSecItemNotFound) {
                secnotice("item", "exporting class '%@' complete (no items)", q.q_class->name);
                CFRelease(localError);
            } else {
                secerror("exporting class '%@' failed: %@", q.q_class->name, localError);
                if (error) {
                    CFReleaseSafe(*error);
                    *error = localError;
                } else {
                    CFRelease(localError);
                }
                CFReleaseNull(keychain);
                CFReleaseNull(ctx.qc.result);
                break;
            }
        }
        CFReleaseNull(ctx.qc.result);
    }

errOut:
    CFReleaseNull(q.q_musrView);
    CFReleaseNull(keybaguuid);

    return keychain;
}

struct SecServerImportClassState {
	SecDbConnectionRef dbt;
    CFErrorRef error;
    keybag_handle_t src_keybag;
    keybag_handle_t dest_keybag;
    SecurityClient *client;
    enum SecItemFilter filter;
};

struct SecServerImportItemState {
    const SecDbClass *class;
	struct SecServerImportClassState *s;
};

static void
SecServerImportItem(const void *value, void *context)
{
    struct SecServerImportItemState *state = (struct SecServerImportItemState *)context;
    bool inMultiUser = false;
#if TARGET_OS_IPHONE
    if (state->s->client->inMultiUser)
        inMultiUser = true;
#endif

    if (state->s->error)
        return;

    if (!isDictionary(value)) {
        SecError(errSecParam, &state->s->error, CFSTR("value %@ is not a dictionary"), value);
        return;
    }

    CFDictionaryRef dict = (CFDictionaryRef)value;

    secdebug("item", "Import Item : %@", dict);

    SecDbItemRef item = NULL;

    /* This is sligthly confusing:
     - During upgrade all items are exported with KEYBAG_NONE.
     - During restore from backup, existing sys_bound items are exported with KEYBAG_NONE, and are exported as dictionary of attributes.
     - Item in the actual backup are export with a real keybag, and are exported as encrypted v_Data and v_PersistentRef
     */
    if (state->s->src_keybag == KEYBAG_NONE) {
        item = SecDbItemCreateWithAttributes(kCFAllocatorDefault, state->class, dict, state->s->dest_keybag,  &state->s->error);
    } else {
        item = SecDbItemCreateWithBackupDictionary(state->class, dict, state->s->src_keybag, state->s->dest_keybag, &state->s->error);
    }

    /* If item is NULL here, control flow ends up at the end where error is cleared. */
    if (item && !SecDbItemEnsureDecrypted(item, true, &state->s->error)) {
        secdebug("item", "Failed to import item because of decryption failure: %@", state->s->error);
        CFReleaseNull(item);
        /* No early return; as just above, go to the end where error is cleared. */
    }

    /* We use the kSecSysBoundItemFilter to indicate that we don't
     * preserve rowid's during import.
     */
    if (item && item->attributes && state->s->filter == kSecBackupableItemFilter) {
        CFTypeRef pdmu;

        /* We don't filter non sys_bound items during import since we know we
         * will never have any in this case.
         */
        if (SecItemIsSystemBound(item->attributes, state->class, inMultiUser)) {
            secdebug("item", "skipping backup of item: %@", dict);
            CFReleaseNull(item);
            return;
        }

        /*
         * Don't bother with u items when in edu mode since our current backup system
         * don't keep track of items that blongs to the device (u) but rather just
         * merge them into one blob.
         */
        if (inMultiUser && (pdmu = CFDictionaryGetValue(item->attributes, kSecAttrAccessible))) {
            if (CFEqual(pdmu, kSecAttrAccessibleWhenUnlockedThisDeviceOnly)     ||
                CFEqual(pdmu, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly) ||
                CFEqual(pdmu, kSecAttrAccessibleWhenUnlockedThisDeviceOnly)     ||
                CFEqual(pdmu, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly))
            {
                secdebug("item", "Skipping KU item : %@", dict);
                CFReleaseNull(item);
                return;
            }
        }

        /* Avoid importing token-based items.  Although newer backups should not have them,
         * older (iOS9, iOS10.0) produced backups with token-based items.
         */
        if (CFDictionaryContainsKey(item->attributes, kSecAttrTokenID)) {
            secdebug("item", "Skipping token-based item : %@", dict);
            CFReleaseNull(item);
            return;
        }
    }

    /*
     *
     */

    if (item && item->attributes) {
        CFDataRef musr = NULL;
        CFDataRef musrBackup = CFDictionaryGetValue(item->attributes, kSecAttrMultiUser);
        CFDataRef systemKeychainUUID = SecMUSRGetSystemKeychainUUID();
        bool systemKeychain = CFEqualSafe(musrBackup, systemKeychainUUID);

#if TARGET_OS_IPHONE
        if (state->s->client && state->s->client->inMultiUser) {
            if (systemKeychain) {
                secwarning("system keychain not allowed in multi user mode for item: %@", item);
            } else {
                musr = SecMUSRCreateActiveUserUUID(state->s->client->uid);
            }
        } else
#endif
        {
            if (systemKeychain) {
                musr = SecMUSRCopySystemKeychainUUID();
            } else {
                musr = SecMUSRGetSingleUserKeychainUUID();
                CFRetainSafe(musr);
            }
        }
        if (musr == NULL) {
            CFReleaseNull(item);
        } else {
            SecDbItemSetValueWithName(item, CFSTR("musr"), musr, &state->s->error);
            CFRelease(musr);
        }
    }

    /*
     *
     */

    if (item) {
        bool insertStatus;

        if(state->s->filter != kSecSysBoundItemFilter) {
            SecDbItemExtractRowIdFromBackupDictionary(item, dict, &state->s->error);
        }
        SecDbItemInferSyncable(item, &state->s->error);
        insertStatus = SecDbItemInsert(item, state->s->dbt, false, &state->s->error);
        if (!insertStatus) {
            /*
             When running in EduMode, multiple users share the same
             keychain and unfortionaly the rowid is used a
             persistant reference and is part of the contraints (its
             UNIQUE), so lets clear the rowid and try to insert the
             entry again.

             This even happens for normal operation because of
             SysBound entries, so in case of a failure, lets try
             again to insert the record.
             */
            SecDbItemClearRowId(item, NULL);
            SecDbItemInsert(item, state->s->dbt, false, &state->s->error);
        }
    }

    /* Reset error if we had one, since we just skip the current item
     and continue importing what we can. */
    if (state->s->error) {
        secwarning("Failed to import an item (%@) of class '%@': %@ - ignoring error.",
                   item, state->class->name, state->s->error);
        CFReleaseNull(state->s->error);
    }

    CFReleaseSafe(item);
}

static void SecServerImportClass(const void *key, const void *value,
                                 void *context) {
    struct SecServerImportClassState *state =
    (struct SecServerImportClassState *)context;
    if (state->error)
        return;
    if (!isString(key)) {
        SecError(errSecParam, &state->error, CFSTR("class name %@ is not a string"), key);
        return;
    }
    /* ignore the Keybag UUID */
    if (CFEqual(key, kSecBackupKeybagUUIDKey))
        return;
    const SecDbClass *class = kc_class_with_name(key);
    if (!class) {
        secwarning("Ignoring unknown key class '%@'", key);
        return;
    }
    if (class == identity_class()) {
        SecError(errSecParam, &state->error, CFSTR("attempt to import an identity"));
        return;
    }
    struct SecServerImportItemState item_state = {
        .class = class, .s = state,
    };
    if (isArray(value)) {
        CFArrayRef items = (CFArrayRef)value;
        secwarning("Import %ld items of class %@ (filter %d)", (long)CFArrayGetCount(items), key, state->filter);
        SecSignpostBackupCount(SecSignpostImpulseRestoreClassCount, class->name, CFArrayGetCount(items), state->filter);
        CFArrayApplyFunction(items, CFRangeMake(0, CFArrayGetCount(items)),
                             SecServerImportItem, &item_state);
    } else if (isDictionary(value)) {
        CFDictionaryRef item = (CFDictionaryRef)value;
        secwarning("Import %ld items of class %@ (filter %d)", (long)1, key, state->filter);
        SecSignpostBackupCount(SecSignpostImpulseRestoreClassCount, class->name, 1, state->filter);
        SecServerImportItem(item, &item_state);
    } else {
        secwarning("Unknown value type for class %@ (filter %d)", key, state->filter);
    }
}

bool SecServerImportKeychainInPlist(SecDbConnectionRef dbt, SecurityClient *client,
                                            keybag_handle_t src_keybag, keybag_handle_t dest_keybag,
                                            CFDictionaryRef keychain, enum SecItemFilter filter,
                                            bool removeKeychainContent, CFErrorRef *error) {
    CFStringRef keybaguuid = NULL;
    bool ok = true;

    CFDictionaryRef sys_bound = NULL;
    if (filter == kSecBackupableItemFilter) {
        /* Grab a copy of all the items for which SecItemIsSystemBound()
         returns true. */
        require(sys_bound = SecServerCopyKeychainPlist(dbt, client, KEYBAG_DEVICE,
                                                       KEYBAG_NONE, kSecSysBoundItemFilter,
                                                       error), errOut);
    }

    /*
     * Validate the uuid of the source keybag matches what we have in the backup
     */
    keybaguuid = SecCreateKeybagUUID(src_keybag);
    if (keybaguuid) {
        CFStringRef uuid = CFDictionaryGetValue(keychain, kSecBackupKeybagUUIDKey);
        if (isString(uuid)) {
            require_action(CFEqual(keybaguuid, uuid), errOut,
                           SecError(errSecDecode, error, CFSTR("Keybag UUID (%@) mismatch with backup (%@)"),
                                    keybaguuid, uuid));
        }
    }

#if TARGET_OS_IOS
    /*
     * Shared iPad is very special, it always delete's the user keychain, and never merge content
     */
    if (client->inMultiUser) {
        CFDataRef musrView = SecMUSRCreateActiveUserUUID(client->uid);
        require_action(musrView, errOut, ok = false);
        require_action(ok = SecServerDeleteAllForUser(dbt, musrView, true, error), errOut, CFReleaseNull(musrView));
        CFReleaseNull(musrView);
    } else
#endif
    {
        /*
         * Delete everything in the keychain.
         * We don't want this if we're restoring backups because we probably already synced stuff over
         */
        if (removeKeychainContent) {
            require(ok = SecServerDeleteAll(dbt, error), errOut);
        } else {
            // Custom hack to support bluetooth's workflow for 11.3. Should be removed in a future release.
            __block CFErrorRef btError = NULL;
            bool deletedBT = kc_transaction(dbt, &btError, ^bool{

#define EXCLUDE_AGRPS "'com.apple.security.sos', 'com.apple.security.sos-usercredential', 'com.apple.security.ckks', 'com.apple.security.egoIdentities', 'com.apple.security.octagon'"

                bool tok = SecDbExec(dbt, CFSTR("DELETE FROM genp WHERE sync = 0 AND NOT agrp IN (" EXCLUDE_AGRPS ");"), &btError);
                tok &= SecDbExec(dbt, CFSTR("DELETE FROM inet WHERE sync = 0 AND NOT agrp IN (" EXCLUDE_AGRPS ");"), &btError);
                tok &= SecDbExec(dbt, CFSTR("DELETE FROM cert WHERE sync = 0 AND NOT agrp IN (" EXCLUDE_AGRPS ");"), &btError);
                tok &= SecDbExec(dbt, CFSTR("DELETE FROM keys WHERE sync = 0 AND NOT agrp IN (" EXCLUDE_AGRPS ");"), &btError);

#undef EXCLUDE_AGRPS
                return tok;
            });
            if (!deletedBT) {
                secerror("Unable to delete nonsyncable items prior to keychain restore: %@", btError);
            } else {
                secnotice("restore", "Successfully deleted nonsyncable items");
            }
            CFReleaseNull(btError);
        }
    }

    struct SecServerImportClassState state = {
        .dbt = dbt,
        .src_keybag = src_keybag,
        .dest_keybag = dest_keybag,
        .client = client,
        .filter = filter,
    };
    /* Import the provided items, preserving rowids. */
    secwarning("Restoring backup items '%ld'", (long)CFDictionaryGetCount(keychain));
    CFDictionaryApplyFunction(keychain, SecServerImportClass, &state);

    if (sys_bound) {
        state.src_keybag = KEYBAG_NONE;
        /* Import the items we preserved with random rowids. */
        state.filter = kSecSysBoundItemFilter;
        secwarning("Restoring sysbound items '%ld'", (long)CFDictionaryGetCount(sys_bound));
        CFDictionaryApplyFunction(sys_bound, SecServerImportClass, &state);
    }
    if (state.error) {
        if (error) {
            CFReleaseSafe(*error);
            *error = state.error;
        } else {
            CFRelease(state.error);
        }
        ok = false;
    }

    // If CKKS had spun up, it's very likely that we just deleted its data.
    // Tell it to perform a local resync.
#if OCTAGON
    SecCKKSPerformLocalResync();
#endif

errOut:
    CFReleaseSafe(sys_bound);
    CFReleaseSafe(keybaguuid);

    return ok;
}

CFStringRef
SecServerBackupGetKeybagUUID(CFDictionaryRef keychain, CFErrorRef *error)
{
    CFStringRef uuid = CFDictionaryGetValue(keychain, kSecBackupKeybagUUIDKey);
    if (!isString(uuid)) {
        SecError(errSecDecode, error, CFSTR("Missing or invalid %@ in backup dictionary"), kSecBackupKeybagUUIDKey);
        return NULL;
    }
    return uuid;
}

#pragma mark - key rolling support
#if USE_KEYSTORE

struct check_generation_ctx {
    struct s3dl_query_ctx query_ctx;
    uint32_t current_generation;
};

static void check_generation(sqlite3_stmt *stmt, void *context) {
    struct check_generation_ctx *c = context;
    CFDataRef blob = NULL;
    size_t blobLen = 0;
    const uint8_t *cursor = NULL;
    uint32_t version;
    keyclass_t keyclass;
    uint32_t current_generation = c->current_generation;
    
    require(blob = s3dl_copy_data_from_col(stmt, 1, &c->query_ctx.q->q_error), out);
    blobLen = CFDataGetLength(blob);
    cursor = CFDataGetBytePtr(blob);
    
    /* Check for underflow, ensuring we have at least one full AES block left. */
    if (blobLen < sizeof(version) + sizeof(keyclass)) {
        SecError(errSecDecode, &c->query_ctx.q->q_error, CFSTR("check_generation: Check for underflow"));
        goto out;
    }
    
    version = *((uint32_t *)cursor);
    cursor += sizeof(version);

    (void) version; // TODO: do something with the version number.
    
    keyclass = *((keyclass_t *)cursor);
    
    // TODO: export get_key_gen macro
    if (((keyclass & ~key_class_last) == 0) != (current_generation == 0)) {
        c->query_ctx.found++;
    }
    
    CFReleaseSafe(blob);
    return;
    
out:
    c->query_ctx.found++;
    CFReleaseSafe(blob);
}

bool s3dl_dbt_keys_current(SecDbConnectionRef dbt, uint32_t current_generation, CFErrorRef *error) {
    CFErrorRef localError = NULL;
    struct check_generation_ctx ctx = { .query_ctx = { .dbt = dbt }, .current_generation = current_generation };

    const SecDbClass *classes[] = {
        genp_class(),
        inet_class(),
        keys_class(),
        cert_class(),
    };

    for (size_t class_ix = 0; class_ix < array_size(classes); ++class_ix) {
        Query *q = query_create(classes[class_ix], NULL, NULL, NULL, &localError);
        if (!q)
            return false;

        ctx.query_ctx.q = q;
        q->q_limit = kSecMatchUnlimited;

        bool ok = s3dl_query(check_generation, &ctx, &localError);
        query_destroy(q, NULL);
        CFReleaseNull(ctx.query_ctx.result);
        
        if (!ok && localError && (CFErrorGetCode(localError) == errSecItemNotFound)) {
            CFReleaseNull(localError);
            continue;
        }
        secerror("Class %@ not up to date", classes[class_ix]->name);
        return false;
    }
    return true;
}

bool s3dl_dbt_update_keys(SecDbConnectionRef dbt, SecurityClient *client, CFErrorRef *error) {
    return SecDbTransaction(dbt, kSecDbExclusiveTransactionType, error, ^(bool *commit) {
        __block bool ok = false;
        uint32_t keystore_generation_status;
        
        /* can we migrate to new class keys right now? */
        if (!aks_generation(KEYBAG_DEVICE, generation_noop, &keystore_generation_status) &&
            (keystore_generation_status & generation_change_in_progress)) {
            
            /* take a lock assertion */
            bool operated_while_unlocked = SecAKSDoWithUserBagLockAssertion(error, ^{
                CFErrorRef localError = NULL;
                CFDictionaryRef backup = SecServerCopyKeychainPlist(dbt, NULL,
                                                                    KEYBAG_DEVICE, KEYBAG_NONE, kSecNoItemFilter, &localError);
                if (backup) {
                    if (localError) {
                        secerror("Ignoring export error: %@ during roll export", localError);
                        CFReleaseNull(localError);
                    }
                    // 'true' argument: we're replacing everything with newly wrapped entries so remove the old stuff
                    ok = SecServerImportKeychainInPlist(dbt, client, KEYBAG_NONE,
                                                        KEYBAG_DEVICE, backup, kSecNoItemFilter, true, &localError);
                    if (localError) {
                        secerror("Ignoring export error: %@ during roll export", localError);
                        CFReleaseNull(localError);
                    }
                    CFRelease(backup);
                }
            });
            if (!operated_while_unlocked)
                ok = false;
        } else {
            ok = SecError(errSecBadReq, error, CFSTR("No key roll in progress."));
        }
        
        *commit = ok;
    });
}
#endif
