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

#include <securityd/SecItemDb.h>

#include <securityd/SecDbKeychainItem.h>
#include <securityd/SecItemSchema.h>
#include <securityd/SecItemServer.h>
#include <Security/SecBasePriv.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemInternal.h>
#include <securityd/SOSCloudCircleServer.h>
#include <utilities/array_size.h>
#include <utilities/SecIOFormat.h>

/* label when certificate data is joined with key data */
#define CERTIFICATE_DATA_COLUMN_LABEL "certdata"

const SecDbAttr *SecDbAttrWithKey(const SecDbClass *c,
                                  CFTypeRef key,
                                  CFErrorRef *error) {
    /* Special case: identites can have all attributes of either cert
     or keys. */
    if (c == &identity_class) {
        const SecDbAttr *desc;
        if (!(desc = SecDbAttrWithKey(&cert_class, key, 0)))
            desc = SecDbAttrWithKey(&keys_class, key, error);
        return desc;
    }

    if (isString(key)) {
        SecDbForEachAttr(c, a) {
            if (CFEqual(a->name, key))
                return a;
        }
    }

    SecError(errSecNoSuchAttr, error, CFSTR("attribute %@ not found in class %@"), key, c->name);

    return NULL;
}

bool kc_transaction(SecDbConnectionRef dbt, CFErrorRef *error, bool(^perform)()) {
    __block bool ok = true;
    return ok && SecDbTransaction(dbt, kSecDbExclusiveTransactionType, error, ^(bool *commit) {
        ok = *commit = perform();
    });
}

static CFStringRef SecDbGetKindSQL(SecDbAttrKind kind) {
    switch (kind) {
        case kSecDbBlobAttr:
        case kSecDbDataAttr:
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

void SecDbAppendCreateTableWithClass(CFMutableStringRef sql, const SecDbClass *c) {
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
}

static CFDataRef SecPersistentRefCreateWithItem(SecDbItemRef item, CFErrorRef *error) {
    sqlite3_int64 row_id = SecDbItemGetRowId(item, error);
    if (row_id)
        return _SecItemMakePersistentRef(SecDbItemGetClass(item)->name, row_id);
    return NULL;
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

    bool ok = true;
    if (q->q_data)
        ok = SecDbItemSetValueWithName(item, CFSTR("v_Data"), q->q_data, error);
    if (q->q_row_id)
        ok = SecDbItemSetRowId(item, q->q_row_id, error);
    SecDbItemSetCredHandle(item, q->q_use_cred_handle);

    if (ok) {
        ok = SecDbItemInsert(item, dbt, q->q_required_access_controls, error);
        CFRetainAssign(q->q_use_cred_handle, item->credHandle);
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

typedef void (*s3dl_handle_row)(sqlite3_stmt *stmt, void *context);

static CFDataRef
s3dl_copy_data_from_col(sqlite3_stmt *stmt, int col, CFErrorRef *error) {
    return CFDataCreateWithBytesNoCopy(0, sqlite3_column_blob(stmt, col),
                                       sqlite3_column_bytes(stmt, col),
                                       kCFAllocatorNull);
}

static bool
s3dl_item_from_col(sqlite3_stmt *stmt, Query *q, int col, CFArrayRef accessGroups,
                   CFMutableDictionaryRef *item, SecAccessControlRef *access_control, CFErrorRef *error) {
    CFDataRef edata = NULL;
    bool ok = false;
    require(edata = s3dl_copy_data_from_col(stmt, col, error), out);
    ok = s3dl_item_from_data(edata, q, accessGroups, item, access_control, error);

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
static CF_RETURNS_RETAINED CFTypeRef handle_result(Query *q, CFMutableDictionaryRef item,
                               sqlite_int64 rowid) {
    CFTypeRef a_result;
    CFDataRef data;
    data = CFDictionaryGetValue(item, kSecValueData);
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
		a_result = _SecItemMakePersistentRef(q->q_class->name, rowid);
	} else {
		/* We need to return more than one value. */
        if (q->q_return_type & kSecReturnRefMask) {
            CFDictionarySetValue(item, kSecClass, q->q_class->name);
        } else if ((q->q_return_type & kSecReturnAttributesMask)) {
            if (!(q->q_return_type & kSecReturnDataMask)) {
                CFDictionaryRemoveValue(item, kSecValueData);
            }
        } else {
            CFRetainSafe(data);
            CFDictionaryRemoveAllValues(item);
            if ((q->q_return_type & kSecReturnDataMask) && data) {
                CFDictionarySetValue(item, kSecValueData, data);
            }
            CFReleaseSafe(data);
        }
		if (q->q_return_type & kSecReturnPersistentRefMask) {
            CFDataRef pref = _SecItemMakePersistentRef(q->q_class->name, rowid);
			CFDictionarySetValue(item, kSecValuePersistentRef, pref);
            CFRelease(pref);
		}

		a_result = item;
        CFRetain(item);
	}

	return a_result;
}

static void s3dl_merge_into_dict(const void *key, const void *value, void *context) {
    CFDictionarySetValue(context, key, value);
}

static void s3dl_query_row(sqlite3_stmt *stmt, void *context) {
    struct s3dl_query_ctx *c = context;
    Query *q = c->q;

    sqlite_int64 rowid = sqlite3_column_int64(stmt, 0);
    CFMutableDictionaryRef item;
    bool ok = s3dl_item_from_col(stmt, q, 1, c->accessGroups, &item, NULL, &q->q_error);
    if (!ok) {
        OSStatus status = SecErrorGetOSStatus(q->q_error);
        // errSecDecode means the item is corrupted, stash it for delete.
        if (status == errSecDecode) {
            secwarning("ignoring corrupt %@,rowid=%" PRId64 " %@", q->q_class->name, rowid, q->q_error);
            {
                CFDataRef edata = s3dl_copy_data_from_col(stmt, 1, NULL);
                CFMutableStringRef edatastring =  CFStringCreateMutable(kCFAllocatorDefault, 0);
                if(edatastring) {
                    CFStringAppendEncryptedData(edatastring, edata);
                    secnotice("item", "corrupted edata=%@", edatastring);
                }
                CFReleaseSafe(edata);
                CFReleaseSafe(edatastring);
            }
            CFReleaseNull(q->q_error);
        } else {
            secerror("decode %@,rowid=%" PRId64 " failed (%" PRIdOSStatus "): %@", q->q_class->name, rowid, status, q->q_error);
        }
        // q->q_error will be released appropriately by a call to query_error
        return;
    }

    if (!item)
        goto out;

    if (q->q_class == &identity_class) {
        // TODO: Use col 2 for key rowid and use both rowids in persistent ref.

        CFMutableDictionaryRef key;
        /* TODO : if there is a errSecDecode error here, we should cleanup */
        if (!s3dl_item_from_col(stmt, q, 3, c->accessGroups, &key, NULL, &q->q_error) || !key)
            goto out;

        CFDataRef certData = CFDictionaryGetValue(item, kSecValueData);
        if (certData) {
            CFDictionarySetValue(key, CFSTR(CERTIFICATE_DATA_COLUMN_LABEL),
                                 certData);
            CFDictionaryRemoveValue(item, kSecValueData);
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

static void SecDbAppendWhereClause(CFMutableStringRef sql, const Query *q,
                                   CFArrayRef accessGroups) {
    bool needWhere = true;
    SecDbAppendWhereROWID(sql, CFSTR("ROWID"), q->q_row_id, &needWhere);
    SecDbAppendWhereAttrs(sql, q, &needWhere);
    SecDbAppendWhereAccessGroups(sql, CFSTR("agrp"), accessGroups, &needWhere);
}

static void SecDbAppendLimit(CFMutableStringRef sql, CFIndex limit) {
    if (limit != kSecMatchUnlimited)
        CFStringAppendFormat(sql, NULL, CFSTR(" LIMIT %" PRIdCFIndex), limit);
}

static CFStringRef s3dl_select_sql(Query *q, CFArrayRef accessGroups) {
    CFMutableStringRef sql = CFStringCreateMutable(NULL, 0);
	if (q->q_class == &identity_class) {
        CFStringAppendFormat(sql, NULL, CFSTR("SELECT crowid, "
                                              CERTIFICATE_DATA_COLUMN_LABEL ", rowid,data FROM "
                                              "(SELECT cert.rowid AS crowid, cert.labl AS labl,"
                                              " cert.issr AS issr, cert.slnr AS slnr, cert.skid AS skid,"
                                              " keys.*,cert.data AS " CERTIFICATE_DATA_COLUMN_LABEL
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
        SecDbAppendWhereAccessGroups(sql, CFSTR("agrp"), accessGroups, &needWhere);
	} else {
        CFStringAppend(sql, CFSTR("SELECT rowid, data FROM "));
		CFStringAppend(sql, q->q_class->name);
        SecDbAppendWhereClause(sql, q, accessGroups);
    }
    SecDbAppendLimit(sql, q->q_limit);

    return sql;
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
    __block bool decrypted = true;
    /* Sanity check the query. */
    if (query->q_ref)
        return SecError(errSecValueRefUnsupported, error, CFSTR("value ref not supported by queries"));

    bool (^return_attr)(const SecDbAttr *attr) = ^bool (const SecDbAttr * attr) {
        return attr->kind == kSecDbRowIdAttr || attr->kind == kSecDbEncryptedDataAttr || attr->kind == kSecDbSHA1Attr;
    };

    CFStringRef sql = s3dl_select_sql(query, accessGroups);
    ok = sql;
    if (sql) {
        ok &= SecDbPrepare(dbconn, sql, error, ^(sqlite3_stmt *stmt) {
            /* Bind the values being searched for to the SELECT statement. */
            int param = 1;
            if (query->q_class == &identity_class) {
                /* Bind the access groups to cert.agrp. */
                ok &= sqlBindAccessGroups(stmt, accessGroups, &param, error);
            }
            if (ok)
                ok &= sqlBindWhereClause(stmt, query, accessGroups, &param, error);
            if (ok) {
                SecDbStep(dbconn, stmt, error, ^(bool *stop) {
                    SecDbItemRef item = SecDbItemCreateWithStatement(kCFAllocatorDefault, query->q_class, stmt, query->q_keybag, error, return_attr);
                    if (item) {
                        if (query->q_required_access_controls) {
                            CFDataRef authNeeded = NULL;
                            item->cryptoOp = query->q_crypto_op;
                            SecDbItemSetCallerAccessGroups(item, query->q_caller_access_groups);
                            decrypted = SecDbItemEnsureDecrypted(item, &authNeeded, &query->q_use_cred_handle, NULL);
                            if (decrypted && authNeeded) {
                                // Do not process the item right now, just remember that it will need
                                // authentication to properly process it.
                                CFArrayAppendValue(query->q_required_access_controls, authNeeded);
                                CFRelease(authNeeded);
                                CFRelease(item);
                                return;
                            }
                            CFReleaseSafe(authNeeded);
                        }
                        if (match_item(dbconn, query, accessGroups, item->attributes))
                            handle_row(item, stop);
                        CFRelease(item);
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
    CFStringRef sql = s3dl_select_sql(q, accessGroups);
    bool ok = SecDbWithSQL(dbt, sql, error, ^(sqlite3_stmt *stmt) {
        bool sql_ok = true;
        /* Bind the values being searched for to the SELECT statement. */
        int param = 1;
        if (q->q_class == &identity_class) {
            /* Bind the access groups to cert.agrp. */
            sql_ok = sqlBindAccessGroups(stmt, accessGroups, &param, error);
        }
        if (sql_ok)
            sql_ok = sqlBindWhereClause(stmt, q, accessGroups, &param, error);
        if (sql_ok) {
            SecDbForEach(stmt, error, ^bool (int row_index) {
                handle_row(stmt, context);
                return (!q->q_error) && (q->q_limit == kSecMatchUnlimited || c->found < q->q_limit);
            });
        }
        return sql_ok;
    });

    CFRelease(sql);

    // First get the error from the query, since errSecDuplicateItem from an
    // update query should superceed the errSecItemNotFound below.
    if (!query_error(q, error))
        ok = false;
    if (ok && c->found == 0)
        ok = SecError(errSecItemNotFound, error, CFSTR("no matching items found"));

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

/* AUDIT[securityd](done):
 attributesToUpdate (ok) is a caller provided dictionary,
 only its cf types have been checked.
 */
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

    __block bool result = true;
    Query *u = query_create(q->q_class, attributesToUpdate, error);
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
                if (!SecDbItemDelete(item, dbt, false, &localError)) {
                    secerror("failed to delete corrupt %@,rowid=%" PRId64 " %@", q->q_class->name, SecDbItemGetRowId(item, NULL), localError);
                    CFReleaseNull(localError);
                }
                return;
            }
            if (new_item != NULL && u->q_access_control != NULL)
                SecDbItemSetAccessControl(new_item, u->q_access_control, &localError);
            result = CFErrorPropagate(localError, error) && new_item;
            if (new_item) {
                bool item_is_sync = SecDbItemIsSyncable(item);
                bool makeTombstone = q->q_use_tomb ? CFBooleanGetValue(q->q_use_tomb) : (item_is_sync && !SecDbItemIsTombstone(item));
                result = SecDbItemUpdate(item, new_item, dbt, makeTombstone, error);
                if (result) {
                    q->q_changed = true;
                    if (item_is_sync || SecDbItemIsSyncable(new_item))
                        q->q_sync_changed = true;
                }
                CFRelease(new_item);
            }
            if (!result)
                *stop = true;
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

bool
s3dl_query_delete(SecDbConnectionRef dbt, Query *q, CFArrayRef accessGroups, CFErrorRef *error)
{
    __block bool ok = true;
    // Only delete things that aren't tombstones, unless the client explicitly asks otherwise.
    if (!CFDictionaryContainsKey(q->q_item, kSecAttrTombstone))
        query_add_attribute(kSecAttrTombstone, kCFBooleanFalse, q);
    ok &= SecDbItemSelect(q, dbt, error, ^bool(const SecDbAttr *attr) {
        return false;
    },^bool(CFMutableStringRef sql, bool *needWhere) {
        SecDbAppendWhereClause(sql, q, accessGroups);
        return true;
    },^bool(sqlite3_stmt * stmt, int col) {
        return sqlBindWhereClause(stmt, q, accessGroups, &col, error);
    }, ^(SecDbItemRef item, bool *stop) {
        // Cache the storedSHA1 digest so we use the one from the db not the recomputed one for notifications.
        const SecDbAttr *sha1attr = SecDbClassAttrWithKind(item->class, kSecDbSHA1Attr, NULL);
        CFDataRef storedSHA1 = CFRetainSafe(SecDbItemGetValue(item, sha1attr, NULL));
        bool item_is_sync = SecDbItemIsSyncable(item);
        SecDbItemSetValue(item, sha1attr, storedSHA1, NULL);
        CFReleaseSafe(storedSHA1);
        bool makeTombstone = q->q_use_tomb ? CFBooleanGetValue(q->q_use_tomb) : (item_is_sync && !SecDbItemIsTombstone(item));
        item->cryptoOp = kSecKsDelete;
        ok = SecDbItemDelete(item, dbt, makeTombstone, error);
        if (ok) {
            q->q_changed = true;
            if (item_is_sync)
                q->q_sync_changed = true;
        }
    });
    if (ok && !q->q_changed) {
        ok = SecError(errSecItemNotFound, error, CFSTR("Delete failed to delete anything"));
    }
    return ok;
}

/* Return true iff the item in question should not be backed up, nor restored,
 but when restoring a backup the original version of the item should be
 added back to the keychain again after the restore completes. */
static bool SecItemIsSystemBound(CFDictionaryRef item, const SecDbClass *class) {
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

    if (CFEqual(agrp, CFSTR("apple")) && class == &genp_class) {
        CFStringRef service = CFDictionaryGetValue(item, kSecAttrService);
        CFStringRef account = CFDictionaryGetValue(item, kSecAttrAccount);
        if (isString(service) && isString(account) &&
            CFEqual(service, CFSTR("com.apple.managedconfiguration")) &&
            (CFEqual(account, CFSTR("Public")) ||
             CFEqual(account, CFSTR("Private")))) {
                secdebug("backup", "found sys_bound item: %@", item);
                return true;
            }
    }
    secdebug("backup", "found non sys_bound item: %@", item);
    return false;
}

/* Delete all items from the current keychain.  If this is not an in
 place upgrade we don't delete items in the 'lockdown-identities'
 access group, this ensures that an import or restore of a backup
 will never overwrite an existing activation record. */
static bool SecServerDeleteAll(SecDbConnectionRef dbt, CFErrorRef *error) {
    return kc_transaction(dbt, error, ^{
        bool ok = (SecDbExec(dbt, CFSTR("DELETE from genp;"), error) &&
                   SecDbExec(dbt, CFSTR("DELETE from inet;"), error) &&
                   SecDbExec(dbt, CFSTR("DELETE from cert;"), error) &&
                   SecDbExec(dbt, CFSTR("DELETE from keys;"), error));
        return ok;
    });
}

struct s3dl_export_row_ctx {
    struct s3dl_query_ctx qc;
    keybag_handle_t dest_keybag;
    enum SecItemFilter filter;
};

static void s3dl_export_row(sqlite3_stmt *stmt, void *context) {
    struct s3dl_export_row_ctx *c = context;
    Query *q = c->qc.q;
    SecAccessControlRef access_control = NULL;
    CFErrorRef localError = NULL;

    sqlite_int64 rowid = sqlite3_column_int64(stmt, 0);
    CFMutableDictionaryRef item;
    if (s3dl_item_from_col(stmt, q, 1, c->qc.accessGroups, &item, &access_control, &localError)) {
        if (item) {
            /* Only export sysbound items is do_sys_bound is true, only export non sysbound items otherwise. */
            bool do_sys_bound = c->filter == kSecSysBoundItemFilter;
            if (c->filter == kSecNoItemFilter ||
                SecItemIsSystemBound(item, q->q_class) == do_sys_bound) {
                /* Re-encode the item. */
                secdebug("item", "export rowid %llu item: %@", rowid, item);
                /* The code below could be moved into handle_row. */
                CFDataRef pref = _SecItemMakePersistentRef(q->q_class->name, rowid);
                if (pref) {
                    if (c->dest_keybag != KEYBAG_NONE) {
                        CFMutableDictionaryRef auth_attribs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                        SecDbForEachAttrWithMask(q->q_class, desc, kSecDbInAuthenticatedDataFlag) {
                            CFTypeRef value = CFDictionaryGetValue(item, desc->name);
                            if(value) {
                                CFDictionaryAddValue(auth_attribs, desc->name, value);
                                CFDictionaryRemoveValue(item, desc->name);
                            }
                        }
                        
                        /* Encode and encrypt the item to the specified keybag. */
                        CFDataRef edata = NULL;
                        bool encrypted = ks_encrypt_data(c->dest_keybag, access_control, &q->q_use_cred_handle, item, auth_attribs, &edata, &q->q_error);
                        CFDictionaryRemoveAllValues(item);
                        CFRelease(auth_attribs);
                        if (encrypted) {
                            CFDictionarySetValue(item, kSecValueData, edata);
                            CFReleaseSafe(edata);
                        } else {
                            seccritical("ks_encrypt_data %@,rowid=%" PRId64 ": failed: %@", q->q_class->name, rowid, q->q_error);
                            CFReleaseNull(q->q_error);
                        }
                    }
                    if (CFDictionaryGetCount(item)) {
                        CFDictionarySetValue(item, kSecValuePersistentRef, pref);
                        CFArrayAppendValue((CFMutableArrayRef)c->qc.result, item);
                        c->qc.found++;
                    }
                    CFReleaseSafe(pref);
                }
            }
            CFRelease(item);
        }
    } else {
        /* This happens a lot when trying to migrate keychain before first unlock, so only a notice */
        /* If the error is "corrupted item" then we just ignore it, otherwise we save it in the query */
        secnotice("item","Could not export item for rowid %llu: %@", rowid, localError);
        if(SecErrorGetOSStatus(localError)==errSecDecode) {
            CFReleaseNull(localError);
        } else {
            CFReleaseSafe(q->q_error);
            q->q_error=localError;
        }
    }
    CFReleaseSafe(access_control);
}

CF_RETURNS_RETAINED CFDictionaryRef SecServerExportKeychainPlist(SecDbConnectionRef dbt,
                                                                        keybag_handle_t src_keybag, keybag_handle_t dest_keybag,
                                                                        enum SecItemFilter filter, CFErrorRef *error) {
    CFMutableDictionaryRef keychain;
    keychain = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                         &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!keychain) {
        if (error && !*error)
            SecError(errSecAllocate, error, CFSTR("Can't create keychain dictionary"));
        goto errOut;
    }
    unsigned class_ix;
    Query q = { .q_keybag = src_keybag };
    q.q_return_type = kSecReturnDataMask | kSecReturnAttributesMask | \
    kSecReturnPersistentRefMask;
    q.q_limit = kSecMatchUnlimited;

    /* Enable interactive means that auth-protected items will be silently ignored
       (and their ACLs copied into query, which we will simply ignore). */
    query_enable_interactive(&q);

    /* Get rid of this duplicate. */
    const SecDbClass *SecDbClasses[] = {
        &genp_class,
        &inet_class,
        &cert_class,
        &keys_class
    };

    for (class_ix = 0; class_ix < array_size(SecDbClasses);
         ++class_ix) {
        q.q_class = SecDbClasses[class_ix];
        struct s3dl_export_row_ctx ctx = {
            .qc = { .q = &q, .dbt = dbt },
            .dest_keybag = dest_keybag, .filter = filter,
        };

        secnotice("item", "exporting class '%@'", q.q_class->name);

        CFErrorRef localError = NULL;
        if (s3dl_query(s3dl_export_row, &ctx, &localError)) {
            if (CFArrayGetCount(ctx.qc.result))
                CFDictionaryAddValue(keychain, q.q_class->name, ctx.qc.result);

        } else {
            OSStatus status = (OSStatus)CFErrorGetCode(localError);
            if (status == errSecItemNotFound) {
                CFRelease(localError);
            } else {
                secerror("Export failed: %@", localError);
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

    /* Release all ignored ACLs of auth-protected items. */
    CFReleaseNull(q.q_required_access_controls);
errOut:
    return keychain;
}

struct SecServerImportClassState {
	SecDbConnectionRef dbt;
    CFErrorRef error;
    keybag_handle_t src_keybag;
    keybag_handle_t dest_keybag;
    enum SecItemFilter filter;
};

struct SecServerImportItemState {
    const SecDbClass *class;
	struct SecServerImportClassState *s;
};

static void SecServerImportItem(const void *value, void *context) {
    struct SecServerImportItemState *state =
    (struct SecServerImportItemState *)context;
    if (state->s->error)
        return;
    if (!isDictionary(value)) {
        SecError(errSecParam, &state->s->error, CFSTR("value %@ is not a dictionary"), value);
        return;
    }

    CFDictionaryRef dict = (CFDictionaryRef)value;

    secdebug("item", "Import Item : %@", dict);

    /* We don't filter non sys_bound items during import since we know we
     will never have any in this case, we use the kSecSysBoundItemFilter
     to indicate that we don't preserve rowid's during import instead. */
    if (state->s->filter == kSecBackupableItemFilter &&
        SecItemIsSystemBound(dict, state->class))
        return;

    SecDbItemRef item;

    /* This is sligthly confusing:
     - During upgrade all items are exported with KEYBAG_NONE.
     - During restore from backup, existing sys_bound items are exported with KEYBAG_NONE, and are exported as dictionary of attributes.
     - Item in the actual backup are export with a real keybag, and are exported as encrypted v_Data and v_PersistentRef
     */
    if (state->s->src_keybag == KEYBAG_NONE) {
        item = SecDbItemCreateWithAttributes(kCFAllocatorDefault, state->class, dict, state->s->dest_keybag,  &state->s->error);
    } else {
        item = SecDbItemCreateWithBackupDictionary(kCFAllocatorDefault, state->class, dict, state->s->src_keybag, state->s->dest_keybag, &state->s->error);
    }

    if (item) {
        if(state->s->filter != kSecSysBoundItemFilter) {
            SecDbItemExtractRowIdFromBackupDictionary(item, dict, &state->s->error);
        }
        SecDbItemInferSyncable(item, &state->s->error);
        SecDbItemInsert(item, state->s->dbt, NULL, &state->s->error);
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
    const SecDbClass *class = kc_class_with_name(key);
    if (!class || class == &identity_class) {
        SecError(errSecParam, &state->error, CFSTR("attempt to import an identity"));
        return;
    }
    struct SecServerImportItemState item_state = {
        .class = class, .s = state
    };
    if (isArray(value)) {
        CFArrayRef items = (CFArrayRef)value;
        CFArrayApplyFunction(items, CFRangeMake(0, CFArrayGetCount(items)),
                             SecServerImportItem, &item_state);
    } else {
        CFDictionaryRef item = (CFDictionaryRef)value;
        SecServerImportItem(item, &item_state);
    }
}

bool SecServerImportKeychainInPlist(SecDbConnectionRef dbt,
                                           keybag_handle_t src_keybag, keybag_handle_t dest_keybag,
                                           CFDictionaryRef keychain, enum SecItemFilter filter, CFErrorRef *error) {
    bool ok = true;

    CFDictionaryRef sys_bound = NULL;
    if (filter == kSecBackupableItemFilter) {
        /* Grab a copy of all the items for which SecItemIsSystemBound()
         returns true. */
        require(sys_bound = SecServerExportKeychainPlist(dbt, KEYBAG_DEVICE,
                                                         KEYBAG_NONE, kSecSysBoundItemFilter,
                                                         error), errOut);
    }

    /* Delete everything in the keychain. */
    require(ok = SecServerDeleteAll(dbt, error), errOut);

    struct SecServerImportClassState state = {
        .dbt = dbt,
        .src_keybag = src_keybag,
        .dest_keybag = dest_keybag,
        .filter = filter,
    };
    /* Import the provided items, preserving rowids. */
    CFDictionaryApplyFunction(keychain, SecServerImportClass, &state);

    if (sys_bound) {
        state.src_keybag = KEYBAG_NONE;
        /* Import the items we preserved with random rowids. */
        state.filter = kSecSysBoundItemFilter;
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

errOut:
    CFReleaseSafe(sys_bound);

    return ok;
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
        &genp_class,
        &inet_class,
        &keys_class,
        &cert_class,
    };

    for (size_t class_ix = 0; class_ix < array_size(classes); ++class_ix) {
        Query *q = query_create(classes[class_ix], NULL, &localError);
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

bool s3dl_dbt_update_keys(SecDbConnectionRef dbt, CFErrorRef *error) {
    return SecDbTransaction(dbt, kSecDbExclusiveTransactionType, error, ^(bool *commit) {
        __block bool ok = false;
        uint32_t keystore_generation_status;
        
        /* can we migrate to new class keys right now? */
        if (!aks_generation(KEYBAG_DEVICE, generation_noop, &keystore_generation_status) &&
            (keystore_generation_status & generation_change_in_progress)) {
            
            /* take a lock assertion */
            bool operated_while_unlocked = SecAKSDoWhileUserBagLocked(error, ^{
                CFErrorRef localError = NULL;
                CFDictionaryRef backup = SecServerExportKeychainPlist(dbt,
                                                                      KEYBAG_DEVICE, KEYBAG_NONE, kSecNoItemFilter, &localError);
                if (backup) {
                    if (localError) {
                        secerror("Ignoring export error: %@ during roll export", localError);
                        CFReleaseNull(localError);
                    }
                    ok = SecServerImportKeychainInPlist(dbt, KEYBAG_NONE,
                                                        KEYBAG_DEVICE, backup, kSecNoItemFilter, &localError);
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
