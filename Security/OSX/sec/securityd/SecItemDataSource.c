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
 *  SecItemDataSource.c - CoreFoundation-based constants and functions for
    access to Security items (certificates, keys, identities, and
    passwords.)
 */

#include <securityd/SecItemDataSource.h>

#include <securityd/SecItemDb.h>
#include <securityd/SecItemSchema.h>
#include <securityd/SOSCloudCircleServer.h>
#include <Security/SecureObjectSync/SOSDigestVector.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include <Security/SecBasePriv.h>
#include <Security/SecItem.h>
#include <Security/SecItemBackup.h>
#include <Security/SecItemPriv.h>
#include <utilities/array_size.h>

/*
 *
 *
 * SecItemDataSource
 *
 *
 */
typedef struct SecItemDataSource *SecItemDataSourceRef;

struct SecItemDataSource {
    struct SOSDataSource ds;
    SecDbRef db;                // The database we operate on
    CFStringRef name;           // The name of the slice of the database we represent.
};

static const SecDbClass *dsSyncedClassesV0[] = {
    &genp_class,
    &inet_class,
    &keys_class,
};

static const SecDbClass *dsSyncedClasses[] = {
    &genp_class,         // genp must be first!
    &inet_class,
    &keys_class,
    &cert_class,
};

static bool SecErrorIsSqliteDuplicateItemError(CFErrorRef error) {
    return error && CFErrorGetCode(error) == SQLITE_CONSTRAINT && CFEqual(kSecDbErrorDomain, CFErrorGetDomain(error));
}

static bool SecDbItemSelectSHA1(SecDbQueryRef query, SecDbConnectionRef dbconn, CFErrorRef *error,
                                bool (^use_attr_in_where)(const SecDbAttr *attr),
                                bool (^add_where_sql)(CFMutableStringRef sql, bool *needWhere),
                                bool (^bind_added_where)(sqlite3_stmt *stmt, int col),
                                void (^row)(sqlite3_stmt *stmt, bool *stop)) {
    __block bool ok = true;
    bool (^return_attr)(const SecDbAttr *attr) = ^bool (const SecDbAttr * attr) {
        return attr->kind == kSecDbSHA1Attr;
    };
    CFStringRef sql = SecDbItemCopySelectSQL(query, return_attr, use_attr_in_where, add_where_sql);
    if (sql) {
        ok &= SecDbPrepare(dbconn, sql, error, ^(sqlite3_stmt *stmt) {
            ok = (SecDbItemSelectBind(query, stmt, error, use_attr_in_where, bind_added_where) &&
                  SecDbStep(dbconn, stmt, error, ^(bool *stop){ row(stmt, stop); }));
        });
        CFRelease(sql);
    } else {
        ok = false;
    }
    return ok;
}

static SOSManifestRef SecItemDataSourceCopyManifestWithQueries(SecItemDataSourceRef ds, CFArrayRef queries, CFErrorRef *error) {
    __block SOSManifestRef manifest = NULL;
    __block CFErrorRef localError = NULL;
    if (!SecDbPerformRead(ds->db, error, ^(SecDbConnectionRef dbconn) {
        __block struct SOSDigestVector dv = SOSDigestVectorInit;
        Query *q;
        bool ok = true;
        CFArrayForEachC(queries, q) {
            if (!(ok &= SecDbItemSelectSHA1(q, dbconn, &localError, ^bool(const SecDbAttr *attr) {
                return CFDictionaryContainsKey(q->q_item, attr->name);
            }, NULL, NULL, ^(sqlite3_stmt *stmt, bool *stop) {
                const uint8_t *digest = sqlite3_column_blob(stmt, 0);
                size_t digestLen = sqlite3_column_bytes(stmt, 0);
                if (digestLen != SOSDigestSize) {
                    secerror("digest %zu bytes", digestLen);
                } else {
                    SOSDigestVectorAppend(&dv, digest);
                }
            }))) {
                secerror("SecDbItemSelectSHA1 failed: %@", localError);
                break;
            }
        }
        if (ok) {
            // TODO: This code assumes that the passed in queries do not overlap, otherwise we'd need something to eliminate dupes:
            //SOSDigestVectorUniqueSorted(&dv);
            manifest = SOSManifestCreateWithDigestVector(&dv, &localError);
        }
        SOSDigestVectorFree(&dv);
    })) {
        CFReleaseSafe(manifest);
    }
    CFErrorPropagate(localError, error);
    return manifest;
}

static Query *SecItemDataSourceAppendQuery(CFMutableArrayRef queries, const SecDbClass *qclass, bool noTombstones, CFErrorRef *error) {
    Query *q = query_create(qclass, NULL, NULL, error);
    if (q) {
        q->q_return_type = kSecReturnDataMask | kSecReturnAttributesMask;
        q->q_limit = kSecMatchUnlimited;
        q->q_keybag = KEYBAG_DEVICE;
        query_add_attribute(kSecAttrSynchronizable, kCFBooleanTrue, q);
        query_add_or_attribute(kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked, q);
        query_add_or_attribute(kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlock, q);
        query_add_or_attribute(kSecAttrAccessible, kSecAttrAccessibleAlwaysPrivate, q);
        query_add_or_attribute(kSecAttrAccessible, kSecAttrAccessibleWhenUnlockedThisDeviceOnly, q);
        query_add_or_attribute(kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly, q);
        query_add_or_attribute(kSecAttrAccessible, kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate, q);

        if (noTombstones) {
            query_add_attribute(kSecAttrTombstone, kCFBooleanFalse, q);
        }

        CFArrayAppendValue(queries, q);
    }
    return q;
}

static Query *SecItemDataSourceAppendQueryWithClassAndViewHint(CFMutableArrayRef queries, const SecDbClass *qclass, bool noTombstones, bool allowTkid, CFStringRef viewHint, CFErrorRef *error) {
    Query *q = SecItemDataSourceAppendQuery(queries, qclass, noTombstones, error);
    if (q) {
        // For each attribute in current schema but not in v6, look for the
        // default value of those attributes in the query, since old items
        // will all have that as their values for these new attributes.
        SecDbForEachAttr(qclass, attr) {
            if (attr == &v7tkid || attr == &v7vwht) {
                // attr is a primary key attribute added in schema version 7 or later
                if (!allowTkid || attr != &v7tkid) {
                    if (attr == &v7vwht && viewHint) {
                        query_add_attribute_with_desc(attr, viewHint, q);
                    } else {
                        CFTypeRef value = SecDbAttrCopyDefaultValue(attr, &q->q_error);
                        if (value) {
                            query_add_attribute_with_desc(attr, value, q);
                            CFRelease(value);
                        }
                    }
                }
            }
        }
    }
    return q;
}

static Query *SecItemDataSourceAppendQueryWithClass(CFMutableArrayRef queries, const SecDbClass *qclass, bool noTombstones, bool allowTkid, CFErrorRef *error) {
    return SecItemDataSourceAppendQueryWithClassAndViewHint(queries, qclass, noTombstones, allowTkid, NULL, error);
}

static Query *SecItemDataSourceAppendQueryWithClassAndAgrp(CFMutableArrayRef queries, const SecDbClass *qclass, bool noTombstones, bool allowTkid, CFStringRef agrp, CFErrorRef *error) {
    Query *q = SecItemDataSourceAppendQueryWithClass(queries, qclass, noTombstones, allowTkid, error);
    if (q && agrp) {
        query_add_attribute(kSecAttrAccessGroup, agrp, q);
    }
    return q;
}

static bool SecItemDataSourceAppendQueriesForViewName(SecItemDataSourceRef ds, CFMutableArrayRef queries, CFStringRef compositeViewName, CFErrorRef *error) {
    bool ok = true;
    CFStringRef viewName;
    bool noTombstones = CFStringHasSuffix(compositeViewName, CFSTR("-tomb"));
    if (noTombstones) {
        viewName = CFStringCreateWithSubstring(kCFAllocatorDefault, compositeViewName, CFRangeMake(0, CFStringGetLength(compositeViewName) - 5));
    } else {
        viewName = CFRetain(compositeViewName);
    }

    const bool noTKID = false;
    const bool allowTKID = true;
    if (CFEqual(viewName, kSOSViewKeychainV0)) {
        for (size_t class_ix = 0; class_ix < array_size(dsSyncedClassesV0); ++class_ix) {
            SecItemDataSourceAppendQueryWithClass(queries, dsSyncedClassesV0[class_ix], noTombstones, noTKID, error);
        }
    } else if (CFEqual(viewName, kSOSViewWiFi)) {
        Query *q = SecItemDataSourceAppendQueryWithClassAndAgrp(queries, &genp_class, noTombstones, allowTKID, CFSTR("apple"), error);
        if (q) {
            query_add_attribute(kSecAttrService, CFSTR("AirPort"), q);
        }
    } else if (CFEqual(viewName, kSOSViewAutofillPasswords)) {
        SecItemDataSourceAppendQueryWithClassAndAgrp(queries, &inet_class, noTombstones, allowTKID, CFSTR("com.apple.cfnetwork"), error);
    } else if (CFEqual(viewName, kSOSViewSafariCreditCards)) {
        SecItemDataSourceAppendQueryWithClassAndAgrp(queries, &genp_class, noTombstones, allowTKID, CFSTR("com.apple.safari.credit-cards"), error);
    } else if (CFEqual(viewName, kSOSViewiCloudIdentity)) {
        SecItemDataSourceAppendQueryWithClassAndAgrp(queries, &keys_class, noTombstones, allowTKID, CFSTR("com.apple.security.sos"), error);
    } else if (CFEqual(viewName, kSOSViewBackupBagV0)) {
        SecItemDataSourceAppendQueryWithClassAndAgrp(queries, &genp_class, noTombstones, allowTKID, CFSTR("com.apple.sbd"), error);
    } else if (CFEqual(viewName, kSOSViewOtherSyncable)) {
        SecItemDataSourceAppendQueryWithClass(queries, &cert_class, noTombstones, allowTKID, error);

        Query *q1_genp = SecItemDataSourceAppendQueryWithClassAndAgrp(queries, &genp_class, noTombstones, allowTKID, CFSTR("apple"), error);
        query_add_not_attribute(kSecAttrService, CFSTR("AirPort"), q1_genp);

        Query *q2_genp = SecItemDataSourceAppendQueryWithClass(queries, &genp_class, noTombstones, allowTKID, error);
        query_add_not_attribute(kSecAttrAccessGroup, CFSTR("apple"), q2_genp);
        query_add_not_attribute(kSecAttrAccessGroup, CFSTR("com.apple.safari.credit-cards"), q2_genp);
        query_add_not_attribute(kSecAttrAccessGroup, CFSTR("com.apple.sbd"), q2_genp);

        Query *q_inet = SecItemDataSourceAppendQueryWithClass(queries, &inet_class, noTombstones, allowTKID, error);
        query_add_not_attribute(kSecAttrAccessGroup, CFSTR("com.apple.cfnetwork"), q_inet);

        Query *q_keys = SecItemDataSourceAppendQueryWithClass(queries, &keys_class, noTombstones, allowTKID, error);
        query_add_not_attribute(kSecAttrAccessGroup, CFSTR("com.apple.security.sos"), q_keys);
    } else {
        // All other viewNames should match on the ViewHint attribute.
        for (size_t class_ix = 0; class_ix < array_size(dsSyncedClasses); ++class_ix) {
            SecItemDataSourceAppendQueryWithClassAndViewHint(queries, dsSyncedClasses[class_ix], noTombstones, allowTKID, viewName, error);
        }
    }

    CFReleaseSafe(viewName);
    return ok;
}

static SOSManifestRef SecItemDataSourceCopyManifestWithViewNameSet(SecItemDataSourceRef ds, CFSetRef viewNames, CFErrorRef *error) {
    CFMutableArrayRef queries = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
    SOSManifestRef manifest = NULL;
    __block bool ok = true;
    CFSetForEach(viewNames, ^(const void *value) {
        CFStringRef viewName = (CFStringRef)value;
        ok &= SecItemDataSourceAppendQueriesForViewName(ds, queries, viewName, error);
    });
    if (ok)
        manifest = SecItemDataSourceCopyManifestWithQueries(ds, queries, error);
    Query *q;
    CFArrayForEachC(queries, q) {
        CFErrorRef localError = NULL;
        if (!query_destroy(q, &localError)) {
            secerror("query_destroy failed: %@", localError);
            CFErrorPropagate(localError, error);
            CFReleaseNull(manifest);
        }
    }
    CFReleaseSafe(queries);
    return manifest;
}

// Return the newest object (conflict resolver)
static SecDbItemRef SecItemDataSourceCopyMergedItem(SecDbItemRef item1, SecDbItemRef item2, CFErrorRef *error) {
    CFErrorRef localError = NULL;
    SecDbItemRef result = NULL;
    CFDateRef m1, m2;
    const SecDbAttr *desc = SecDbAttrWithKey(SecDbItemGetClass(item1), kSecAttrModificationDate, error);
    m1 = SecDbItemGetValue(item1, desc, &localError);
    m2 = SecDbItemGetValue(item2, desc, &localError);
    if (m1 && m2) switch (CFDateCompare(m1, m2, NULL)) {
        case kCFCompareGreaterThan:
            result = item1;
            break;
        case kCFCompareLessThan:
            result = item2;
            break;
        case kCFCompareEqualTo:
        {
            // Return the item with the smallest digest.
            CFDataRef digest1 = SecDbItemGetSHA1(item1, &localError);
            CFDataRef digest2 = SecDbItemGetSHA1(item2, &localError);
            if (digest1 && digest2) switch (CFDataCompare(digest1, digest2)) {
                case kCFCompareGreaterThan:
                case kCFCompareEqualTo:
                    result = item2;
                    break;
                case kCFCompareLessThan:
                    result = item1;
                    break;
            } else if (SecErrorGetOSStatus(localError) == errSecDecode) {
                if (digest1) result = item1;
                if (digest2) result = item2;
            }
            break;
        }
    } else if (SecErrorGetOSStatus(localError) == errSecDecode) {
        // If one of the two objects has an unparsable date,
        // the object with the parsable date wins.
        if (m1) result = item1;
        if (m2) result = item2;
    }

    if (localError) {
        if (!result && error && !*error)
            *error = localError;
        else
            CFRelease(localError);
    }
    return CFRetainSafe(result);
}

//
// MARK: DataSource protocol implementation
//

static CFStringRef dsGetName(SOSDataSourceRef data_source) {
    SecItemDataSourceRef ds = (SecItemDataSourceRef)data_source;
    return ds->name;
}

static void dsAddNotifyPhaseBlock(SOSDataSourceRef data_source, SOSDataSourceNotifyBlock notifyBlock) {
    SecItemDataSourceRef ds = (SecItemDataSourceRef)data_source;
    SecDbAddNotifyPhaseBlock(ds->db, ^(SecDbConnectionRef dbconn, SecDbTransactionPhase phase, SecDbTransactionSource source, CFArrayRef changes)
    {
        notifyBlock(&ds->ds, (SOSTransactionRef)dbconn, phase, source, changes);
    });
}

static SOSManifestRef dsCopyManifestWithViewNameSet(SOSDataSourceRef data_source, CFSetRef viewNameSet, CFErrorRef *error) {
    struct SecItemDataSource *ds = (struct SecItemDataSource *)data_source;
    return SecItemDataSourceCopyManifestWithViewNameSet(ds, viewNameSet, error);
}

static bool dsForEachObject(SOSDataSourceRef data_source, SOSTransactionRef txn, SOSManifestRef manifest, CFErrorRef *error, void (^handle_object)(CFDataRef key, SOSObjectRef object, bool *stop)) {
    struct SecItemDataSource *ds = (struct SecItemDataSource *)data_source;
    __block bool result = true;
    const SecDbAttr *sha1Attr = SecDbClassAttrWithKind(&genp_class, kSecDbSHA1Attr, error);
    if (!sha1Attr) return false;
    bool (^return_attr)(const SecDbAttr *attr) = ^bool (const SecDbAttr * attr) {
        return attr->kind == kSecDbRowIdAttr || attr->kind == kSecDbEncryptedDataAttr;
    };
    bool (^use_attr_in_where)(const SecDbAttr *attr) = ^bool (const SecDbAttr * attr) {
        return attr->kind == kSecDbSHA1Attr;
    };
    Query *select_queries[array_size(dsSyncedClasses)] = {};
    CFStringRef select_sql[array_size(dsSyncedClasses)] = {};
    sqlite3_stmt *select_stmts[array_size(dsSyncedClasses)] = {};

    __block Query **queries = select_queries;
    __block CFStringRef *sqls = select_sql;
    __block sqlite3_stmt **stmts = select_stmts;

    void (^readBlock)(SecDbConnectionRef dbconn) = ^(SecDbConnectionRef dbconn)
    {
        // Setup
        for (size_t class_ix = 0; class_ix < array_size(dsSyncedClasses); ++class_ix) {
            result = (result
                      && (queries[class_ix] = query_create(dsSyncedClasses[class_ix], NULL, NULL, error))
                      && (sqls[class_ix] = SecDbItemCopySelectSQL(queries[class_ix], return_attr, use_attr_in_where, NULL))
                      && (stmts[class_ix] = SecDbCopyStmt(dbconn, sqls[class_ix], NULL, error)));
        }

        if (result) SOSManifestForEach(manifest, ^(CFDataRef key, bool *stop) {
            __block SecDbItemRef item = NULL;
            for (size_t class_ix = 0; result && !item && class_ix < array_size(dsSyncedClasses); ++class_ix) {
                CFDictionarySetValue(queries[class_ix]->q_item, sha1Attr->name, key);
                result = SecDbItemSelectBind(queries[class_ix], stmts[class_ix], error, use_attr_in_where, NULL);
                if (result) {
                    result &= SecDbStep(dbconn, stmts[class_ix], error, ^(bool *unused_stop) {
                        item = SecDbItemCreateWithStatement(kCFAllocatorDefault, queries[class_ix]->q_class, stmts[class_ix], KEYBAG_DEVICE, error, return_attr);
                    });
                }
                if (result)
                    result &= SecDbReset(stmts[class_ix], error);
            }
            handle_object(key, (SOSObjectRef)item, stop);
            CFReleaseSafe(item);
        });

        // Cleanup
        for (size_t class_ix = 0; class_ix < array_size(dsSyncedClasses); ++class_ix) {
            result &= SecDbReleaseCachedStmt(dbconn, sqls[class_ix], stmts[class_ix], error);
            CFReleaseSafe(sqls[class_ix]);
            if (queries[class_ix])
                result &= query_destroy(queries[class_ix], error);
        }
    };

    if (txn) {
        readBlock((SecDbConnectionRef)txn);
    } else {
        result &= SecDbPerformRead(ds->db, error, readBlock);
    }

    return result;
}

static bool dsRelease(SOSDataSourceRef data_source, CFErrorRef *error) {
    // We never release our dataSource since it's tracking changes
    // to the keychain for the engine and its peers.
    return true;
}

static SOSObjectRef objectCreateWithPropertyList(CFDictionaryRef plist, CFErrorRef *error) {
    SecDbItemRef item = NULL;
    const SecDbClass *class = NULL;
    CFTypeRef cname = CFDictionaryGetValue(plist, kSecClass);
    if (cname) {
        class = kc_class_with_name(cname);
        if (class) {
            item = SecDbItemCreateWithAttributes(kCFAllocatorDefault, class, plist, KEYBAG_DEVICE, error);
        } else {
            SecError(errSecNoSuchClass, error, CFSTR("can find class named: %@"), cname);
        }
    } else {
        SecError(errSecItemClassMissing, error, CFSTR("query missing %@ attribute"), kSecClass);
    }
    return (SOSObjectRef)item;
}

static CFDataRef copyObjectDigest(SOSObjectRef object, CFErrorRef *error) {
    SecDbItemRef item = (SecDbItemRef) object;
    CFDataRef digest = SecDbItemGetSHA1(item, error);
    CFRetainSafe(digest);
    return digest;
}

static CFDateRef copyObjectModDate(SOSObjectRef object, CFErrorRef *error) {
    SecDbItemRef item = (SecDbItemRef) object;
    CFDateRef modDate = SecDbItemGetValueKind(item, kSecDbModificationDateAttr, NULL);
    CFRetainSafe(modDate);
    return modDate;
}

static CFDictionaryRef objectCopyPropertyList(SOSObjectRef object, CFErrorRef *error) {
    SecDbItemRef item = (SecDbItemRef) object;
    CFMutableDictionaryRef cryptoDataDict = SecDbItemCopyPListWithMask(item, kSecDbInCryptoDataFlag, error);
    CFMutableDictionaryRef authDataDict = SecDbItemCopyPListWithMask(item, kSecDbInAuthenticatedDataFlag, error);
    
    if (cryptoDataDict) {
        if (authDataDict) {
            CFDictionaryForEach(authDataDict, ^(const void *key, const void *value) {
                CFDictionarySetValue(cryptoDataDict, key, value);
            });
        }
        CFDictionaryAddValue(cryptoDataDict, kSecClass, SecDbItemGetClass(item)->name);
    }
    
    CFReleaseSafe(authDataDict);
    return cryptoDataDict;
}

static bool dsWith(SOSDataSourceRef data_source, CFErrorRef *error, SOSDataSourceTransactionSource source, bool onCommitQueue, void(^transaction)(SOSTransactionRef txn, bool *commit)) {
    SecItemDataSourceRef ds = (SecItemDataSourceRef)data_source;
    __block bool ok = true;
    ok &= SecDbPerformWrite(ds->db, error, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbTransaction(dbconn,
                               source == kSOSDataSourceAPITransaction ? kSecDbExclusiveTransactionType : kSecDbExclusiveRemoteTransactionType,
                               error, ^(bool *commit) {
                                   if (onCommitQueue) {
                                       SecDbPerformOnCommitQueue(dbconn, false, ^{
                                           transaction((SOSTransactionRef)dbconn, commit);
                                       });
                                   } else {
                                       transaction((SOSTransactionRef)dbconn, commit);
                                   }
                               });
    });
    return ok;
}

static bool dsReadWith(SOSDataSourceRef data_source, CFErrorRef *error, SOSDataSourceTransactionSource source, void(^perform)(SOSTransactionRef txn)) {
    SecItemDataSourceRef ds = (SecItemDataSourceRef)data_source;
    __block bool ok = true;
    ok &= SecDbPerformRead(ds->db, error, ^(SecDbConnectionRef dbconn) {
        SecDbPerformOnCommitQueue(dbconn, false, ^{
            perform((SOSTransactionRef)dbconn);
        });
    });
    return ok;
}

static SOSMergeResult dsMergeObject(SOSTransactionRef txn, SOSObjectRef peersObject, SOSObjectRef *mergedObject, CFErrorRef *error) {
    SecDbConnectionRef dbconn = (SecDbConnectionRef)txn;
    SecDbItemRef peersItem = (SecDbItemRef)peersObject;
    __block SOSMergeResult mr = kSOSMergeFailure;
    __block SecDbItemRef mergedItem = NULL;
    __block SecDbItemRef replacedItem = NULL;
    __block CFErrorRef localError = NULL;

    if (!peersItem || !dbconn)
        return kSOSMergeFailure;
    if (!SecDbItemSetKeybag(peersItem, KEYBAG_DEVICE, &localError)) {
        secnotice("ds", "kSOSMergeFailure => SecDbItemSetKeybag: %@", localError);
        CFErrorPropagate(localError, error);
        return kSOSMergeFailure;
    }

    if (SecDbItemInsertOrReplace(peersItem, dbconn, &localError, ^(SecDbItemRef myItem, SecDbItemRef *replace) {
        // An item with the same primary key as dbItem already exists in the the database.  That item is old_item.
        // Let the conflict resolver choose which item to keep.
        mergedItem = SecItemDataSourceCopyMergedItem(peersItem, myItem, &localError);
        if (!mergedItem)
            return;     // from block
        if (mergedObject)
            *mergedObject = (SOSObjectRef)CFRetain(mergedItem);
        if (CFEqual(mergedItem, myItem)) {
            // Conflict resolver choose my (local) item
            secnotice("ds", "Conflict resolver chose my (local) item: %@", myItem);
            mr = kSOSMergeLocalObject;
        } else {
            CFRetainAssign(replacedItem, myItem);
            *replace = CFRetainSafe(mergedItem);
            if (CFEqual(mergedItem, peersItem)) {
                // Conflict resolver chose peer's item
                secnotice("ds", "Conflict resolver chose peers item: %@", peersItem);
                mr = kSOSMergePeersObject;
            } else {
                // Conflict resolver created a new item; return it to our caller
                secnotice("ds", "Conflict resolver created a new item; return it to our caller: %@", mergedItem);
                mr = kSOSMergeCreatedObject;
            }
        }
    })) {
        // either SecDbItemInsertOrReplace or SecItemDataSourceCopyMergedItem failed
        if (mr == kSOSMergeFailure)
        {
            secnotice("ds", "kSOSMergeFailure => kSOSMergePeersObject, %@", localError);
            CFReleaseSafe(localError);
            mr = kSOSMergePeersObject;
        }
    }

    if (localError && !SecErrorIsSqliteDuplicateItemError(localError)) {
        secnotice("ds", "dsMergeObject failed: mr=%ld, %@", mr, localError);
        // We should probably always propogate this, but for now we are only logging
        // See rdar://problem/26451072 for case where we might need to propogate
        if (mr == kSOSMergeFailure) {
            CFErrorPropagate(localError, error);
            localError = NULL;
        }
    }

    CFReleaseSafe(mergedItem);
    CFReleaseSafe(replacedItem);
    CFReleaseSafe(localError);
    return mr;
}

    /*
 Truthy backup format is a dictionary from sha1 => item.
 Each item has class, hash and item data.

 TODO: sha1 is included as binary blob to avoid parsing key.
 */
enum {
    kSecBackupIndexHash = 0,
    kSecBackupIndexClass,
    kSecBackupIndexData,
};

static const void *kSecBackupKeys[] = {
    [kSecBackupIndexHash] = kSecItemBackupHashKey,
    [kSecBackupIndexClass] = kSecItemBackupClassKey,
    [kSecBackupIndexData] = kSecItemBackupDataKey
};

static CFDictionaryRef objectCopyBackup(SOSObjectRef object, uint64_t handle, CFErrorRef *error) {
    const void *values[array_size(kSecBackupKeys)];
    SecDbItemRef item = (SecDbItemRef)object;
    CFDictionaryRef backup_item = NULL;

    if ((values[kSecBackupIndexHash] = SecDbItemGetSHA1(item, error))) {
        if ((values[kSecBackupIndexData] = SecDbItemCopyEncryptedDataToBackup(item, handle, error))) {
            values[kSecBackupIndexClass] = SecDbItemGetClass(item)->name;
            backup_item = CFDictionaryCreate(kCFAllocatorDefault, kSecBackupKeys, values, array_size(kSecBackupKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFRelease(values[kSecBackupIndexData]);
        }
    }

    return backup_item;
}

static CFDataRef dsCopyStateWithKey(SOSDataSourceRef data_source, CFStringRef key, CFStringRef pdmn, SOSTransactionRef txn, CFErrorRef *error) {
    SecItemDataSourceRef ds = (SecItemDataSourceRef)data_source;
    CFStringRef dataSourceID = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("SOSDataSource-%@"), ds->name);
    CFMutableDictionaryRef dict = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault,
                                                                          kSecAttrAccessGroup, kSOSInternalAccessGroup,
                                                                          kSecAttrAccount, key,
                                                                          kSecAttrService, dataSourceID,
                                                                          kSecAttrAccessible, pdmn,
                                                                          kSecAttrSynchronizable, kCFBooleanFalse,
                                                                          NULL);
    CFReleaseSafe(dataSourceID);
    __block CFDataRef data = NULL;
    SecDbQueryRef query = query_create(&genp_class, NULL, dict, error);
    if (query) {
        if (query->q_item)  CFReleaseSafe(query->q_item);
        query->q_item = dict;
        void (^read_it)(SecDbConnectionRef dbconn) = ^(SecDbConnectionRef dbconn) {
            SecDbItemSelect(query, dbconn, error, NULL, ^bool(const SecDbAttr *attr) {
                return CFDictionaryContainsKey(dict, attr->name);
            }, NULL, NULL, ^(SecDbItemRef item, bool *stop) {
                secnotice("ds", "found item for key %@@%@", key, pdmn);
                data = CFRetainSafe(SecDbItemGetValue(item, &v6v_Data, error));
            });
        };
        if (txn) {
            read_it((SecDbConnectionRef) txn);
        } else {
            SecDbPerformRead(ds->db, error, read_it);
        }
        query_destroy(query, error);
    } else {
        CFReleaseSafe(dict);
    }
    if (!data) secnotice("ds", "failed to load %@@%@ state: %@", key, pdmn, error ? *error : NULL);
    return data;
}

static CFDataRef dsCopyItemDataWithKeys(SOSDataSourceRef data_source, CFDictionaryRef keys, CFErrorRef *error) {
    /*
     Values for V0 are:
        kSecAttrAccessGroup ==> CFSTR("com.apple.sbd")
        kSecAttrAccessible  ==> kSecAttrAccessibleWhenUnlocked
        kSecAttrAccount     ==> CFSTR("SecureBackupPublicKeybag")
        kSecAttrService     ==> CFSTR("SecureBackupService")

     CFMutableDictionaryRef dict = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault,
     kSecAttrAccessGroup, CFSTR("com.apple.sbd"),
     kSecAttrAccount, account,
     kSecAttrService, service,
     kSecAttrAccessible, pdmn,
     kSecAttrSynchronizable, kCFBooleanTrue,
     NULL);
     */

    SecItemDataSourceRef ds = (SecItemDataSourceRef)data_source;
    CFMutableDictionaryRef dict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, keys);
    __block CFDataRef data = NULL;
    SecDbQueryRef query = query_create(&genp_class, NULL, dict, error);
    if (query) {
        if (query->q_item)  CFReleaseSafe(query->q_item);
        query->q_item = dict;
        SecDbPerformRead(ds->db, error, ^(SecDbConnectionRef dbconn) {
            SecDbItemSelect(query, dbconn, error, NULL, ^bool(const SecDbAttr *attr) {
                return CFDictionaryContainsKey(dict, attr->name);
            }, NULL, NULL, ^(SecDbItemRef item, bool *stop) {
                secnotice("ds", "found item for keys %@", keys);
                data = CFRetainSafe(SecDbItemGetValue(item, &v6v_Data, error));
            });
        });
        query_destroy(query, error);
    } else {
        CFReleaseSafe(dict);
    }
    if (!data) secnotice("ds", "failed to load item %@: %@", keys, error ? *error : NULL);
    return data;
}

static bool dsSetStateWithKey(SOSDataSourceRef data_source, SOSTransactionRef txn, CFStringRef key, CFStringRef pdmn, CFDataRef state, CFErrorRef *error) {
    SecItemDataSourceRef ds = (SecItemDataSourceRef)data_source;
    CFStringRef dataSourceID = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("SOSDataSource-%@"), ds->name);
    CFMutableDictionaryRef dict = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault,
                                                                          kSecAttrAccessGroup, kSOSInternalAccessGroup,
                                                                          kSecAttrAccount, key,
                                                                          kSecAttrService, dataSourceID,
                                                                          kSecAttrAccessible, pdmn,
                                                                          kSecAttrSynchronizable, kCFBooleanFalse,
                                                                          kSecValueData, state,
                                                                          NULL);
    CFReleaseSafe(dataSourceID);
    SecDbItemRef item = SecDbItemCreateWithAttributes(kCFAllocatorDefault, &genp_class, dict, KEYBAG_DEVICE, error);
    SOSMergeResult mr = dsMergeObject(txn, (SOSObjectRef)item, NULL, error);
    if (mr == kSOSMergeFailure) secerror("failed to save %@@%@ state: %@", key, pdmn, error ? *error : NULL);
    CFReleaseSafe(item);
    CFReleaseSafe(dict);
    return mr != kSOSMergeFailure;
}

static bool dsDeleteStateWithKey(SOSDataSourceRef data_source, CFStringRef key, CFStringRef pdmn, SOSTransactionRef txn, CFErrorRef *error) {
    SecItemDataSourceRef ds = (SecItemDataSourceRef)data_source;
    CFStringRef dataSourceID = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("SOSDataSource-%@"), ds->name);
    CFMutableDictionaryRef dict = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault,
                                                                          kSecAttrAccessGroup, kSOSInternalAccessGroup,
                                                                          kSecAttrAccount, key,
                                                                          kSecAttrService, dataSourceID,
                                                                          kSecAttrAccessible, pdmn,
                                                                          kSecAttrSynchronizable, kCFBooleanFalse,
                                                                          NULL);
    CFReleaseSafe(dataSourceID);
    SecDbItemRef item = SecDbItemCreateWithAttributes(kCFAllocatorDefault, &genp_class, dict, KEYBAG_DEVICE, error);
    bool ok = SecDbItemDoDeleteSilently(item, (SecDbConnectionRef)txn, error);
    CFReleaseNull(dict);
    CFReleaseSafe(item);
    return ok;
}

static bool dsRestoreObject(SOSTransactionRef txn, uint64_t handle, CFDictionaryRef item, CFErrorRef *error) {
    CFStringRef item_class = CFDictionaryGetValue(item, kSecItemBackupClassKey);
    CFDataRef data = CFDictionaryGetValue(item, kSecItemBackupDataKey);
    const SecDbClass *dbclass = NULL;

    if (!item_class || !data)
        return SecError(errSecDecode, error, CFSTR("no class or data in object"));

    dbclass = kc_class_with_name(item_class);
    if (!dbclass)
        return SecError(errSecDecode, error, CFSTR("no such class %@; update kc_class_with_name "), item_class);

    SecDbItemRef dbitem = SecDbItemCreateWithEncryptedData(kCFAllocatorDefault, dbclass, data, (keybag_handle_t)handle, error);
    bool ok = dbitem && (dsMergeObject(txn, (SOSObjectRef)dbitem, NULL, error) != kSOSMergeFailure);
    CFReleaseSafe(dbitem);
    return ok;
}

SOSDataSourceRef SecItemDataSourceCreate(SecDbRef db, CFStringRef name, CFErrorRef *error) {
    SecItemDataSourceRef ds = calloc(1, sizeof(struct SecItemDataSource));
    ds->ds.dsGetName = dsGetName;
    ds->ds.dsAddNotifyPhaseBlock = dsAddNotifyPhaseBlock;
    ds->ds.dsCopyManifestWithViewNameSet = dsCopyManifestWithViewNameSet;
    ds->ds.dsCopyStateWithKey = dsCopyStateWithKey;
    ds->ds.dsCopyItemDataWithKeys = dsCopyItemDataWithKeys;

    ds->ds.dsForEachObject = dsForEachObject;
    ds->ds.dsWith = dsWith;
    ds->ds.dsReadWith = dsReadWith;
    ds->ds.dsRelease = dsRelease;

    ds->ds.dsMergeObject = dsMergeObject;
    ds->ds.dsSetStateWithKey = dsSetStateWithKey;
    ds->ds.dsDeleteStateWithKey = dsDeleteStateWithKey;
    ds->ds.dsRestoreObject = dsRestoreObject;

    // Object field accessors
    ds->ds.objectCopyDigest = copyObjectDigest;
    ds->ds.objectCopyModDate = copyObjectModDate;

    // Object encode and decode.
    ds->ds.objectCreateWithPropertyList = objectCreateWithPropertyList;
    ds->ds.objectCopyPropertyList = objectCopyPropertyList;
    ds->ds.objectCopyBackup = objectCopyBackup;

    ds->db = CFRetainSafe(db);
    ds->name = CFRetainSafe(name);

    // Do this after the ds is fully setup so the engine can query us right away.
    ds->ds.engine = SOSEngineCreate(&ds->ds, error);
    if (!ds->ds.engine) {
        free(ds);
        ds = NULL;
    }
    return &ds->ds;
}

static CFStringRef SecItemDataSourceFactoryCopyName(SOSDataSourceFactoryRef factory)
{
    // This is the name of the v0 datasource, a.k.a. "ak"
    return kSecAttrAccessibleWhenUnlocked;
}

struct SecItemDataSourceFactory {
    struct SOSDataSourceFactory factory;
    CFMutableDictionaryRef dsCache;
    dispatch_queue_t queue;
    SecDbRef db;
};

static SOSDataSourceRef SecItemDataSourceFactoryCopyDataSource(SOSDataSourceFactoryRef factory, CFStringRef dataSourceName, CFErrorRef *error)
{
    struct SecItemDataSourceFactory *f = (struct SecItemDataSourceFactory *)factory;
    __block SOSDataSourceRef dataSource = NULL;
    dispatch_sync(f->queue, ^{
        dataSource = (SOSDataSourceRef)CFDictionaryGetValue(f->dsCache, dataSourceName);
        if (!dataSource && f->db) {
            dataSource = (SOSDataSourceRef)SecItemDataSourceCreate(f->db, dataSourceName, error);
            CFDictionarySetValue(f->dsCache, dataSourceName, dataSource);
        }
    });
    return dataSource;
}

static void SecItemDataSourceFactoryDispose(SOSDataSourceFactoryRef factory)
{
    // Nothing to do here.
}

static void SecItemDataSourceFactoryCircleChanged(SOSDataSourceFactoryRef factory, CFStringRef myPeerID, CFArrayRef trustedPeerIDs, CFArrayRef untrustedPeerIDs) {
    CFStringRef dsName = SOSDataSourceFactoryCopyName(factory);
    SOSEngineRef engine = SOSDataSourceFactoryGetEngineForDataSourceName(factory, dsName, NULL);
    if (engine)
        SOSEngineCircleChanged(engine, myPeerID, trustedPeerIDs, untrustedPeerIDs);
    CFReleaseSafe(dsName);
}

// Fire up the SOSEngines so they can
static bool SOSDataSourceFactoryStartYourEngines(SOSDataSourceFactoryRef factory) {
    bool ok = true;
    CFStringRef dsName = SOSDataSourceFactoryCopyName(factory);
    CFErrorRef localError = NULL;
    SOSDataSourceRef ds = SOSDataSourceFactoryCreateDataSource(factory, dsName, &localError);
    if (!ds)
        secerror("create_datasource %@ failed %@", dsName, localError);
    CFReleaseNull(localError);
    SOSDataSourceRelease(ds, &localError);
    CFReleaseNull(localError);
    CFReleaseNull(dsName);
    return ok;
}

static SOSDataSourceFactoryRef SecItemDataSourceFactoryCreate(SecDbRef db) {
    struct SecItemDataSourceFactory *dsf = calloc(1, sizeof(struct SecItemDataSourceFactory));
    dsf->factory.copy_name = SecItemDataSourceFactoryCopyName;
    dsf->factory.create_datasource = SecItemDataSourceFactoryCopyDataSource;
    dsf->factory.release = SecItemDataSourceFactoryDispose;
    dsf->factory.circle_changed = SecItemDataSourceFactoryCircleChanged;

    dsf->dsCache = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    dsf->queue = dispatch_queue_create("dsf queue", DISPATCH_QUEUE_SERIAL);
    dsf->db = CFRetainSafe(db);
    if (!SOSDataSourceFactoryStartYourEngines(&dsf->factory))
        secerror("Failed to start engines, gonna lose the race.");
    return &dsf->factory;
}

SOSDataSourceFactoryRef SecItemDataSourceFactoryGetShared(SecDbRef db) {
    static dispatch_once_t sDSFQueueOnce;
    static dispatch_queue_t sDSFQueue;
    static CFMutableDictionaryRef sDSTable = NULL;
    
    dispatch_once(&sDSFQueueOnce, ^{
        sDSFQueue = dispatch_queue_create("dataSourceFactory queue", DISPATCH_QUEUE_SERIAL);
        sDSTable = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    });
    
    __block SOSDataSourceFactoryRef result = NULL;
    dispatch_sync(sDSFQueue, ^{
        if(db) {
            CFStringRef dbPath = SecDbGetPath(db);
            if(dbPath) {
                result = (SOSDataSourceFactoryRef) CFDictionaryGetValue(sDSTable, dbPath);

                if(!result) {
                    result = SecItemDataSourceFactoryCreate(db);
                    CFDictionaryAddValue(sDSTable, dbPath, result);
                }
            }
        }
    });

    return result;
}

//  TODO: These should move to SecItemServer.c

void SecItemServerAppendItemDescription(CFMutableStringRef desc, CFDictionaryRef object) {
    SOSObjectRef item = objectCreateWithPropertyList(object, NULL);
    if (item) {
        CFStringRef itemDesc = CFCopyDescription(item);
        if (itemDesc) {
            CFStringAppend(desc, itemDesc);
            CFReleaseSafe(itemDesc);
        }
        CFRelease(item);
    }
}

SOSManifestRef SOSCreateManifestWithBackup(CFDictionaryRef backup, CFErrorRef *error)
{
    __block struct SOSDigestVector dv = SOSDigestVectorInit;
    if (backup) {
        CFDictionaryForEach(backup, ^void (const void * key, const void * value) {
            if (isDictionary(value)) {
                /* converting key back to binary blob is horrible */
                CFDataRef sha1 = CFDictionaryGetValue(value, kSecItemBackupHashKey);
                if (isData(sha1) && CFDataGetLength(sha1) == CCSHA1_OUTPUT_SIZE)
                    SOSDigestVectorAppend(&dv, CFDataGetBytePtr(sha1));
            }
        });
    }
    SOSManifestRef manifest = SOSManifestCreateWithDigestVector(&dv, error);
    SOSDigestVectorFree(&dv);
    return manifest;
}
