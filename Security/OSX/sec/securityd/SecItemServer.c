/*
 * Copyright (c) 2006-2017 Apple Inc. All Rights Reserved.
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
 * SecItemServer.c - CoreFoundation-based constants and functions for
    access to Security items (certificates, keys, identities, and
    passwords.)
 */

#if TARGET_DARWINOS
#undef OCTAGON
#undef SECUREOBJECTSYNC
#undef SHAREDWEBCREDENTIALS
#endif

#include <securityd/SecItemServer.h>

#include <notify.h>
#include <securityd/SecItemDataSource.h>
#include <securityd/SecItemDb.h>
#include <securityd/SecItemSchema.h>
#include <utilities/SecDb.h>
#include <securityd/SecDbKeychainItem.h>
#include <securityd/SOSCloudCircleServer.h>
#include <Security/SecBasePriv.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemInternal.h>
#include <securityd/SecDbBackupManager.h>
#import "keychain/SecureObjectSync/SOSChangeTracker.h"
#include "keychain/SecureObjectSync/SOSDigestVector.h"
#include "keychain/SecureObjectSync/SOSEngine.h"
#include <Security/SecureObjectSync/SOSViews.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecTrustInternal.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecEntitlements.h>
#include <Security/SecSignpost.h>

#include <keychain/ckks/CKKS.h>
#import "keychain/ot/OT.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/escrowrequest/EscrowRequestServerHelpers.h"

#if USE_KEYSTORE

#if __has_include(<MobileKeyBag/MobileKeyBag.h>)
#include <MobileKeyBag/MobileKeyBag.h>
#else
#include "OSX/utilities/SecAKSWrappers.h"
#endif

#if __has_include(<Kernel/IOKit/crypto/AppleKeyStoreDefs.h>)
#include <Kernel/IOKit/crypto/AppleKeyStoreDefs.h>
#endif

#include <IOKit/IOReturn.h>

#endif

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
#include <utilities/SecADWrapper.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <utilities/array_size.h>
#include <utilities/SecFileLocations.h>
#include <utilities/SecTrace.h>
#include <utilities/SecXPCError.h>
#include <utilities/sec_action.h>
#include <Security/SecuritydXPC.h>
#include "swcagent_client.h"
#include "SecPLWrappers.h"

#if TARGET_OS_IOS && !TARGET_OS_BRIDGE
#include <SharedWebCredentials/SharedWebCredentials.h>
#endif

#include <os/variant_private.h>


#include "Analytics/Clients/LocalKeychainAnalytics.h"

/* Changed the name of the keychain changed notification, for testing */
static const char *g_keychain_changed_notification = kSecServerKeychainChangedNotification;

void SecItemServerSetKeychainChangedNotification(const char *notification_name)
{
    g_keychain_changed_notification = notification_name;
}

void SecKeychainChanged() {
    static dispatch_once_t once;
    static sec_action_t action;

    dispatch_once(&once, ^{
        action = sec_action_create("SecKeychainChanged", 1);
        sec_action_set_handler(action, ^{
            uint32_t result = notify_post(g_keychain_changed_notification);
            if (result == NOTIFY_STATUS_OK)
                secnotice("item", "Sent %s", g_keychain_changed_notification);
            else
                secerror("notify_post %s returned: %" PRIu32, g_keychain_changed_notification, result);
        });
    });

    sec_action_perform(action);
}

/* Return the current database version in *version. */
bool SecKeychainDbGetVersion(SecDbConnectionRef dbt, int *version, CFErrorRef *error)
{
    __block bool ok = true;
    __block CFErrorRef localError = NULL;
    __block bool found = false;

    /*
     * First check for the version table itself
     */

    ok &= SecDbPrepare(dbt, CFSTR("SELECT name FROM sqlite_master WHERE type='table' AND name='tversion'"), &localError, ^(sqlite3_stmt *stmt) {
        ok = SecDbStep(dbt, stmt, NULL, ^(bool *stop) {
            found = true;
            *stop = true;
        });
    });
    require_action(ok, out, SecDbError(SQLITE_CORRUPT, error, CFSTR("Failed to read sqlite_master table: %@"), localError));
    if (!found) {
        secnotice("upgr", "no tversion table, will setup a new database: %@", localError);
        *version = 0;
        goto out;
    }

    /*
     * Now build up major.minor
     */

    ok &= SecDbPrepare(dbt, CFSTR("SELECT version FROM tversion"), &localError, ^(sqlite3_stmt *stmt) {
        ok = SecDbStep(dbt, stmt, NULL, ^(bool *stop) {
            *version = sqlite3_column_int(stmt, 0);
            if (*version)
                *stop = true;
        });
    });
    if (ok && (*version & 0xffff) >= 9) {
        ok &= SecDbPrepare(dbt, CFSTR("SELECT minor FROM tversion WHERE version = ?"), &localError, ^(sqlite3_stmt *stmt) {
            ok = SecDbBindInt(stmt, 1, *version, &localError) &&
            SecDbStep(dbt, stmt, NULL, ^(bool *stop) {
                int64_t minor = sqlite3_column_int64(stmt, 0);
                *version |= ((minor & 0xff) << 8) | ((minor & 0xff0000) << 8);
                *stop = true;
            });
        });
        ok = true;
    }
out:
    secnotice("upgr", "database version is: 0x%08x : %d : %@", *version, ok, localError);
    CFReleaseSafe(localError);


    return ok;
}

static bool
isClassD(SecDbItemRef item)
{
    CFTypeRef accessible = SecDbItemGetCachedValueWithName(item, kSecAttrAccessible);

    if (CFEqualSafe(accessible, kSecAttrAccessibleAlwaysPrivate) || CFEqualSafe(accessible, kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate))
        return true;
    return false;
}

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR

static int64_t
measureDuration(struct timeval *start)
{
    struct timeval stop;
    int64_t duration;

    gettimeofday(&stop, NULL);

    duration = (stop.tv_sec-start->tv_sec) * 1000;
    duration += (stop.tv_usec / 1000) - (start->tv_usec / 1000);

    return SecBucket2Significant(duration);
}

static void
measureUpgradePhase1(struct timeval *start, bool success, int64_t itemsMigrated)
{
    int64_t duration = measureDuration(start);

    if (success) {
        SecADSetValueForScalarKey(CFSTR("com.apple.keychain.phase1.migrated-items-success"), itemsMigrated);
        SecADSetValueForScalarKey(CFSTR("com.apple.keychain.phase1.migrated-time-success"), duration);
    } else {
        SecADSetValueForScalarKey(CFSTR("com.apple.keychain.phase1.migrated-items-fail"), itemsMigrated);
        SecADSetValueForScalarKey(CFSTR("com.apple.keychain.phase1.migrated-time-fail"), duration);
    }
}

static void
measureUpgradePhase2(struct timeval *start, int64_t itemsMigrated)
{
    int64_t duration = measureDuration(start);

    SecADSetValueForScalarKey(CFSTR("com.apple.keychain.phase2.migrated-items"), itemsMigrated);
    SecADSetValueForScalarKey(CFSTR("com.apple.keychain.phase2.migrated-time"), duration);
}
#endif /* TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR */

static bool DBClassesAreEqual(const SecDbClass* class1, const SecDbClass* class2)
{
    if (CFEqual(class1->name, class2->name) && class1->itemclass == class2->itemclass) {
        int attrIndex = 0;
        const SecDbAttr* class1Attr = class1->attrs[attrIndex];
        const SecDbAttr* class2Attr = class2->attrs[attrIndex];
        
        while (class1Attr && class2Attr) {
            if (CFEqual(class1Attr->name, class2Attr->name) && class1Attr->kind == class2Attr->kind && class1Attr->flags == class2Attr->flags && class1Attr->copyValue == class2Attr->copyValue && class1Attr->setValue == class2Attr->setValue) {
                attrIndex++;
                class1Attr = class1->attrs[attrIndex];
                class2Attr = class2->attrs[attrIndex];
            }
            else {
                return false;
            }
        }
        
        // if everything has checked out to this point, and we've hit the end of both class's attr list, then they're equal
        if (class1Attr == NULL && class2Attr == NULL) {
            return true;
        }
    }
    
    return false;
}

static bool ShouldRenameTable(const SecDbClass* class1, const SecDbClass* class2, int oldTableVersion)
{
    return oldTableVersion < 10 || !DBClassesAreEqual(class1, class2);
}

#define SCHEMA_VERSION(schema) ((((schema)->minorVersion) << 8) | ((schema)->majorVersion))
#define VERSION_MAJOR(version)  ((version)        & 0xff)
#define VERSION_MINOR(version) (((version) >>  8) & 0xff)
#define VERSION_NEW(version)    ((version)        & 0xffff)
#define VERSION_OLD(version)   (((version) >> 16) & 0xffff)

// Goes through all tables represented by old_schema and tries to migrate all items from them into new (current version) tables.
static bool UpgradeSchemaPhase1(SecDbConnectionRef dbt, const SecDbSchema *oldSchema, CFErrorRef *error)
{
    int oldVersion = SCHEMA_VERSION(oldSchema);
    const SecDbSchema *newSchema = current_schema();
    int newVersion = SCHEMA_VERSION(newSchema);
    __block bool ok = true;
    SecDbClass const *const *oldClass;
    SecDbClass const *const *newClass;
    SecDbQueryRef query = NULL;
    CFMutableStringRef sql = NULL;
    SecDbClass* renamedOldClass = NULL;
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    __block int64_t itemsMigrated = 0;
    struct timeval start;

    gettimeofday(&start, NULL);
#endif

    // Rename existing tables to names derived from old schema names
    sql = CFStringCreateMutable(NULL, 0);
    bool oldClassDone = false;
    CFMutableArrayRef classIndexesForNewTables = CFArrayCreateMutable(NULL, 0, NULL);
    int classIndex = 0;
    for (oldClass = oldSchema->classes, newClass = newSchema->classes;
         *newClass != NULL; classIndex++, oldClass++, newClass++) {
        if (!oldClassDone) {
            oldClassDone |= (*oldClass) == NULL; // Check if the new schema has more tables than the old
        }

        if (!oldClassDone && !CFEqual((*oldClass)->name, (*newClass)->name) && ShouldRenameTable(*oldClass, *newClass, oldSchema->majorVersion)) {
            CFStringAppendFormat(sql, NULL, CFSTR("ALTER TABLE %@ RENAME TO %@_old;"), (*newClass)->name, (*oldClass)->name);
            CFArrayAppendValue(classIndexesForNewTables, (void*)(long)classIndex);

        } else if (!oldClassDone && !DBClassesAreEqual(*oldClass, *newClass)) {
            CFStringAppendFormat(sql, NULL, CFSTR("ALTER TABLE %@ RENAME TO %@_old;"), (*newClass)->name, (*oldClass)->name);
            CFArrayAppendValue(classIndexesForNewTables, (void*)(long)classIndex);
        }

        if(oldClassDone && *newClass) {
            // These should be no-ops, unless you're upgrading a previously-upgraded database with an invalid version number
            CFStringAppendFormat(sql, NULL, CFSTR("DROP TABLE IF EXISTS %@;"), (*newClass)->name);
            
            if (classIndexesForNewTables) {
                CFArrayAppendValue(classIndexesForNewTables, (void*)(long)classIndex);
            }
        }
    }

    if(CFStringGetLength(sql) > 0) {
        require_action_quiet(ok &= SecDbExec(dbt, sql, error), out,
                             LKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, LKAKeychainUpgradeOutcomePhase1AlterTables, error ? *error : NULL));
    }
    CFReleaseNull(sql);

    // Drop indices that that new schemas will use
    sql = CFStringCreateMutable(NULL, 0);
    for (newClass = newSchema->classes; *newClass != NULL; newClass++) {
        SecDbForEachAttrWithMask((*newClass), desc, kSecDbIndexFlag | kSecDbInFlag) {
            CFStringAppendFormat(sql, 0, CFSTR("DROP INDEX IF EXISTS %@%@;"), (*newClass)->name, desc->name);
            if (desc->kind == kSecDbSyncAttr) {
                CFStringAppendFormat(sql, 0, CFSTR("DROP INDEX IF EXISTS %@%@0;"), (*newClass)->name, desc->name);
            }
        }
    }
    require_action_quiet(ok &= SecDbExec(dbt, sql, error), out,
                         LKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, LKAKeychainUpgradeOutcomePhase1DropIndices, error ? *error : NULL));
    CFReleaseNull(sql);

    // Create tables for new schema.
    require_action_quiet(ok &= SecItemDbCreateSchema(dbt, newSchema, classIndexesForNewTables, false, error), out,
                         LKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, LKAKeychainUpgradeOutcomePhase1CreateSchema, error ? *error : NULL));
    // Go through all classes of current schema to transfer all items to new tables.
    for (oldClass = oldSchema->classes, newClass = newSchema->classes;
         *oldClass != NULL && *newClass != NULL; oldClass++, newClass++) {

        if (CFEqual((*oldClass)->name, (*newClass)->name) && DBClassesAreEqual(*oldClass, *newClass)) {
            continue;
        }

        secnotice("upgr", "Upgrading table %@", (*oldClass)->name);

        // Create a new 'old' class with a new 'old' name.
        int count = 0;
        SecDbForEachAttr(*oldClass, attr) {
            count++;
        }
        if(renamedOldClass) {
            CFReleaseNull(renamedOldClass->name);
            free(renamedOldClass);
        }
        renamedOldClass = (SecDbClass*) malloc(sizeof(SecDbClass) + sizeof(SecDbAttr*)*(count+1));
        renamedOldClass->name = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@_old"), (*oldClass)->name);
        renamedOldClass->itemclass = (*oldClass)->itemclass;
        for(; count >= 0; count--) {
            renamedOldClass->attrs[count] = (*oldClass)->attrs[count];
        }

        // SecDbItemSelect only works for item classes.
        if((*oldClass)->itemclass) {
            // Prepare query to iterate through all items in cur_class.
            if (query != NULL)
                query_destroy(query, NULL);
            require_quiet(query = query_create(renamedOldClass, SecMUSRGetAllViews(), NULL, error), out);

            ok &= SecDbItemSelect(query, dbt, error, ^bool(const SecDbAttr *attr) {
                // We are interested in all attributes which are physically present in the DB.
                return (attr->flags & kSecDbInFlag) != 0;
            }, ^bool(const SecDbAttr *attr) {
                // No filtering please.
                return false;
            }, NULL, NULL, ^(SecDbItemRef item, bool *stop) {
                CFErrorRef localError = NULL;

    #if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
                itemsMigrated++;
    #endif
                // Switch item to the new class.
                item->class = *newClass;

                if (isClassD(item)) {
                    // Decrypt the item.
                    ok &= SecDbItemEnsureDecrypted(item, true, &localError);
                    require_quiet(ok, out);

                    // Delete SHA1 field from the item, so that it is newly recalculated before storing
                    // the item into the new table.
                    require_quiet(ok &= SecDbItemSetValue(item, SecDbClassAttrWithKind(item->class, kSecDbSHA1Attr, error),
                                                              kCFNull, error), out);
                } else {
                    // Leave item encrypted, do not ever try to decrypt it since it will fail.
                    item->_edataState = kSecDbItemAlwaysEncrypted;
                }
                // Drop items with kSecAttrAccessGroupToken, as these items should not be there at all. Since agrp attribute
                // is always stored as cleartext in the DB column, we can always rely on this attribute being present in item->attributes.
                // <rdar://problem/33401870>
                if (CFEqualSafe(SecDbItemGetCachedValueWithName(item, kSecAttrAccessGroup), kSecAttrAccessGroupToken) &&
                    SecDbItemGetCachedValueWithName(item, kSecAttrTokenID) == NULL) {
                    secnotice("upgr", "dropping item during schema upgrade due to agrp=com.apple.token: %@", item);
                } else {
                    // Insert new item into the new table.
                    if (!SecDbItemInsert(item, dbt, &localError)) {
                        secerror("item: %@ insert during upgrade: %@", item, localError);
                        ok = false;
                    }
                }

            out:
                if (localError) {
                    OSStatus status = SecErrorGetOSStatus(localError);

                    switch (status) {
                        // continue to upgrade and don't propagate errors for insert failures
                        // that are typical of a single item failure
                        case errSecDecode:
                        case errSecDuplicateItem:
                            ok = true;
                            break;
                        case errSecInteractionNotAllowed:
                        case errSecAuthNeeded:
                            ok = true;
                            break;
                        // This does not mean the keychain is hosed, we just can't use it right now
#if USE_KEYSTORE
                        case kAKSReturnNotReady:
                        case kAKSReturnTimeout:
#endif
                        case errSecNotAvailable:
                            secnotice("upgr", "Bailing in phase 1 because AKS is unavailable: %@", localError);
                            // FALLTHROUGH
                        default:
                            ok &= CFErrorPropagate(CFRetainSafe(localError), error);
                            break;
                    }
                    CFReleaseSafe(localError);
                }

                *stop = !ok;

            });

            require_action_quiet(ok, out,
                                 LKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, LKAKeychainUpgradeOutcomePhase1Items, error ? *error : NULL));
        } else {
            // This table does not contain secdb items, and must be transferred without using SecDbItemSelect.
            // For now, this code does not support removing or renaming any columns, or adding any new non-null columns.
            CFReleaseNull(sql);
            sql = CFStringCreateMutable(NULL, 0);
            bool comma = false;

            CFMutableStringRef columns = CFStringCreateMutable(NULL, 0);

            SecDbForEachAttr(renamedOldClass, attr) {
                if(comma) {
                    CFStringAppendFormat(columns, NULL, CFSTR(","));
                }
                CFStringAppendFormat(columns, NULL, CFSTR("%@"), attr->name);
                comma = true;
            }

            CFStringAppendFormat(sql, NULL, CFSTR("INSERT OR REPLACE INTO %@ (%@) SELECT %@ FROM %@;"), (*newClass)->name, columns, columns, renamedOldClass->name);

            CFReleaseNull(columns);
            require_action_quiet(ok &= SecDbExec(dbt, sql, error), out,
                                 LKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, LKAKeychainUpgradeOutcomePhase1NonItems, error ? *error : NULL));
        }
    }

    // Remove old tables from the DB.
    CFReleaseNull(sql);
    sql = CFStringCreateMutable(NULL, 0);
    for (oldClass = oldSchema->classes, newClass = newSchema->classes;
         *oldClass != NULL && *newClass != NULL; oldClass++, newClass++) {
        if (CFEqual((*oldClass)->name, (*newClass)->name) && DBClassesAreEqual(*oldClass, *newClass)) {
            continue;
        }

        CFStringAppendFormat(sql, NULL, CFSTR("DROP TABLE %@_old;"), (*oldClass)->name);
    }

    if(CFStringGetLength(sql) > 0) {
        require_action_quiet(ok &= SecDbExec(dbt, sql, error), out,
                             LKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, LKAKeychainUpgradeOutcomePhase1DropOld, error ? *error : NULL));
    }
out:
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    measureUpgradePhase1(&start, ok, SecBucket2Significant(itemsMigrated));
#endif

    if (query != NULL) {
        query_destroy(query, NULL);
    }
    CFReleaseSafe(sql);
    CFReleaseNull(classIndexesForNewTables);
    if(renamedOldClass) {
        CFReleaseNull(renamedOldClass->name);
        free(renamedOldClass);
    }
    return ok;
}

__thread SecDbConnectionRef threadDbt = NULL;

// Goes through all tables represented by old_schema and tries to migrate all items from them into new (current version) tables.
static bool UpgradeItemPhase2(SecDbConnectionRef inDbt, bool *inProgress, int oldVersion, CFErrorRef *error) {
    SecDbConnectionRef oldDbt = threadDbt;
    threadDbt = inDbt;
    __block bool ok = true;
    SecDbQueryRef query = NULL;
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    __block int64_t itemsMigrated = 0;
    struct timeval start;

    gettimeofday(&start, NULL);
#endif

#if SECDB_BACKUPS_ENABLED
    CFErrorRef backuperror = NULL;
    if (!SecDbBackupCreateOrLoadBackupInfrastructure(&backuperror)) {
        secerror("upgr: failed to create backup infrastructure: %@", backuperror);
    } else {
        secnotice("upgr", "setup backup infrastructure");
    }
#else
    secnotice("upgr", "skipping backup setup for this platform");
#endif

    // Go through all classes in new schema
    const SecDbSchema *newSchema = current_schema();
    int newVersion = SCHEMA_VERSION(newSchema);
    for (const SecDbClass *const *class = newSchema->classes; *class != NULL && !*inProgress; class++) {
        if(!((*class)->itemclass)) {
            //Don't try to decrypt non-item 'classes'
            continue;
        }

        const SecDbAttr *pdmn = SecDbClassAttrWithKind(*class, kSecDbAccessAttr, error);
        if (pdmn == nil) {
            continue;
        }

        // Prepare query to go through all non-DK|DKU items
        if (query != NULL) {
            query_destroy(query, NULL);
        }
        require_action_quiet(query = query_create(*class, SecMUSRGetAllViews(), NULL, error), out, ok = false);
        ok &= SecDbItemSelect(query, threadDbt, error, NULL, ^bool(const SecDbAttr *attr) {
            // No simple per-attribute filtering.
            return false;
        }, ^bool(CFMutableStringRef sql, bool *needWhere) {
            // Select only non-D-class items
            SecDbAppendWhereOrAnd(sql, needWhere);
            CFStringAppendFormat(sql, NULL, CFSTR("NOT %@ IN (?,?)"), pdmn->name);
            return true;
        }, ^bool(sqlite3_stmt *stmt, int col) {
            return SecDbBindObject(stmt, col++, kSecAttrAccessibleAlwaysPrivate, error) &&
            SecDbBindObject(stmt, col++, kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate, error);
        }, ^(SecDbItemRef item, bool *stop) {
            CFErrorRef localError = NULL;

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
            itemsMigrated++;
#endif

            // Decrypt the item.
            if (SecDbItemEnsureDecrypted(item, true, &localError)) {

                // Delete SHA1 field from the item, so that it is newly recalculated before storing
                // the item into the new table.
                require_quiet(ok = SecDbItemSetValue(item, SecDbClassAttrWithKind(item->class, kSecDbSHA1Attr, error),
                                                     kCFNull, &localError), out);
                // Drop items with kSecAttrAccessGroupToken, as these items should not be there at all. Since agrp attribute
                // is always stored as cleartext in the DB column, we can always rely on this attribute being present in item->attributes.
                // <rdar://problem/33401870>
                if (CFEqualSafe(SecDbItemGetCachedValueWithName(item, kSecAttrAccessGroup), kSecAttrAccessGroupToken) &&
                    SecDbItemGetCachedValueWithName(item, kSecAttrTokenID) == NULL) {
                    secnotice("upgr", "dropping item during item upgrade due to agrp=com.apple.token: %@", item);
                    ok = SecDbItemDelete(item, threadDbt, kCFBooleanFalse, &localError);
                } else {
                    // Replace item with the new value in the table; this will cause the item to be decoded and recoded back,
                    // incl. recalculation of item's hash.
                    ok = SecDbItemUpdate(item, item, threadDbt, false, query->q_uuid_from_primary_key, &localError);
                }
            }

            if (localError) {
                CFIndex status = CFErrorGetCode(localError);

                switch (status) {
                    case errSecDecode: {
                        // Items producing errSecDecode are silently dropped - they are not decodable and lost forever.
                        // make sure we use a local error so that this error is not proppaged upward and cause a
                        // migration failure.
                        CFErrorRef deleteError = NULL;
                        (void)SecDbItemDelete(item, threadDbt, false, &deleteError);
                        CFReleaseNull(deleteError);
                        ok = true;
                        break;
                    }
                    case errSecInteractionNotAllowed:
                        // If we are still not able to decrypt the item because the class key is not released yet,
                        // remember that DB still needs phase2 migration to be run next time a connection is made.  Also
                        // stop iterating next items, it would be just waste of time because the whole iteration will be run
                        // next time when this phase2 will be rerun.
                        LKAReportKeychainUpgradeOutcome(oldVersion, newVersion, LKAKeychainUpgradeOutcomeLocked);
                        *inProgress = true;
                        *stop = true;
                        ok = true;
                        break;
                    case errSecAuthNeeded:
                        // errSecAuthNeeded means that it is an ACL-based item which requires authentication (or at least
                        // ACM context, which we do not have).
                        ok = true;
                        break;
                    case SQLITE_CONSTRAINT:         // yeah...
                        if (!CFEqual(kSecDbErrorDomain, CFErrorGetDomain(localError))) {
                            secerror("Received SQLITE_CONSTRAINT with wrong error domain. Huh? Item: %@, error: %@", item, localError);
                            break;
                        }
                    case errSecDuplicateItem:
                        // continue to upgrade and don't propagate errors for insert failures
                        // that are typical of a single item failure
                        secnotice("upgr", "Ignoring duplicate item: %@", item);
                        secdebug("upgr", "Duplicate item error: %@", localError);
                        ok = true;
                        break;
#if USE_KEYSTORE
                    case kAKSReturnNotReady:
                    case kAKSReturnTimeout:
#endif
                    case errSecNotAvailable:
                        *inProgress = true;     // We're not done, call me again later!
                        secnotice("upgr", "Bailing in phase 2 because AKS is unavailable: %@", localError);
                        // FALLTHROUGH
                    default:
                        //  Other errors should abort the migration completely.
                        ok = CFErrorPropagate(CFRetainSafe(localError), error);
                        break;
                }
            }

        out:
            CFReleaseSafe(localError);
            *stop = *stop || !ok;

        });
        require_action(ok, out, LKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, LKAKeychainUpgradeOutcomePhase2, error ? *error : NULL));
    }

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    measureUpgradePhase2(&start, SecBucket2Significant(itemsMigrated));
#endif

out:
    if (query != NULL)
        query_destroy(query, NULL);

    threadDbt = oldDbt;
    return ok;
}

static bool SecKeychainDbUpgradeFromVersion(SecDbConnectionRef dbt, int version, bool *inProgress, CFErrorRef *error) {
    __block bool didPhase2 = false;
    __block bool ok = true;
    __block CFErrorRef localError = NULL;
 
    if (error)
        *error = NULL;

    const SecDbSchema *newSchema = current_schema();
    int newVersion = SCHEMA_VERSION(newSchema);
    bool skipped_upgrade = false;

    // If DB schema is the one we want, we are done.
    require_action_quiet(SCHEMA_VERSION(newSchema) != version, out, skipped_upgrade = true);

    // Check if the schema of the database on disk is the same major, but newer version then what we have
    // in code, lets just skip this since a newer version of the OS have upgrade it. Since its the same
    // major, its a promise that it will be compatible.
    if (newSchema->majorVersion == VERSION_MAJOR(version) && newSchema->minorVersion < VERSION_MINOR(version)) {
        secnotice("upgr", "skipping upgrade since minor is newer");
        goto out;
    }

    ok &= SecDbTransaction(dbt, kSecDbExclusiveTransactionType, &localError, ^(bool *commit) {
        CFStringRef sql = NULL;
        bool didPhase1 = false;

        // Get version again once we start a transaction, someone else might change the migration state.
        int version2 = 0;
        require_quiet(ok = SecKeychainDbGetVersion(dbt, &version2, &localError), out);
        // Check if someone has raced us to the migration of the database
        require_action(version == version2, out, CFReleaseNull(localError); ok = true);

        require_quiet(SCHEMA_VERSION(newSchema) != version2, out);

        // If this is empty database, just create table according to schema and be done with it.
        require_action_quiet(version2 != 0, out, ok = SecItemDbCreateSchema(dbt, newSchema, NULL, true, &localError);
                             LKAReportKeychainUpgradeOutcomeWithError(version2, newVersion, LKAKeychainUpgradeOutcomeNewDb, localError));

        int oldVersion = VERSION_OLD(version2);
        version2 = VERSION_NEW(version2);

        require_action_quiet(version2 == SCHEMA_VERSION(newSchema) || oldVersion == 0, out,
                             ok = SecDbError(SQLITE_CORRUPT, &localError,
                                             CFSTR("Half migrated but obsolete DB found: found 0x%x(0x%x) but 0x%x is needed"),
                                             version2, oldVersion, SCHEMA_VERSION(newSchema));
                             LKAReportKeychainUpgradeOutcome(version2, newVersion, LKAKeychainUpgradeOutcomeObsoleteDb));

        // Check whether we have both old and new tables in the DB.
        if (oldVersion == 0) {
            // Pure old-schema migration attempt, with full blown table renames etc (a.k.a. phase1)
            oldVersion = version2;
            version2 = SCHEMA_VERSION(newSchema);

            // Find schema for old database.
            const SecDbSchema *oldSchema = NULL;
            for (const SecDbSchema * const *pschema = all_schemas(); *pschema; ++pschema) {
                if (SCHEMA_VERSION((*pschema)) == oldVersion) {
                    oldSchema = *pschema;
                    break;
                }
            }

            // If we are attempting to upgrade from a version for which we have no schema, fail.
            require_action_quiet(oldSchema != NULL, out,
                                 ok = SecDbError(SQLITE_CORRUPT, &localError, CFSTR("no schema for version: 0x%x"), oldVersion);
                                 secerror("no schema for version 0x%x", oldVersion);
                                 LKAReportKeychainUpgradeOutcome(version2, newVersion, LKAKeychainUpgradeOutcomeNoSchema));

            secnotice("upgr", "Upgrading from version 0x%x to 0x%x", oldVersion, SCHEMA_VERSION(newSchema));
            SecSignpostStart(SecSignpostUpgradePhase1);
            require_action(ok = UpgradeSchemaPhase1(dbt, oldSchema, &localError), out, secerror("upgrade: Upgrade phase1 failed: %@", localError));
            SecSignpostStop(SecSignpostUpgradePhase1);

            didPhase1 = true;
        }

        {
            CFErrorRef phase2Error = NULL;

            SecSignpostStart(SecSignpostUpgradePhase2);

            // Lets try to go through non-D-class items in new tables and apply decode/encode on them
            // If this fails the error will be ignored after doing a phase1 since but not in the second
            // time when we are doing phase2.
            ok = UpgradeItemPhase2(dbt, inProgress, version2, &phase2Error);
            if (!ok) {
                if (didPhase1) {
                    *inProgress = true;
                    ok = true;
                    CFReleaseNull(phase2Error);
                } else {
                    SecErrorPropagate(phase2Error, &localError);
                }
            }
            require_action(ok, out, secerror("upgrade: Upgrade phase2 (%d) failed: %@", didPhase1, localError));

            if (!*inProgress) {
                // If either migration path we did reported that the migration was complete, signalize that
                // in the version database by cleaning oldVersion (which is stored in upper halfword of the version)
                secnotice("upgr", "Done upgrading from version 0x%x to 0x%x", oldVersion, SCHEMA_VERSION(newSchema));
                oldVersion = 0;

                didPhase2 = true;
                SecSignpostStop(SecSignpostUpgradePhase2);
            }
        }

        // Update database version table.
        uint32_t major = (VERSION_MAJOR(version2)) | (VERSION_MAJOR(oldVersion) << 16);
        uint32_t minor = (VERSION_MINOR(version2)) | (VERSION_MINOR(oldVersion) << 16);
        secnotice("upgr", "Upgrading saving version major 0x%x minor 0x%x", major, minor);
        sql = CFStringCreateWithFormat(NULL, NULL, CFSTR("UPDATE tversion SET version='%d', minor='%d'"),
                                       major, minor);
        require_action_quiet(ok = SecDbExec(dbt, sql, &localError), out, secerror("upgrade: Setting version failed: %@", localError));

    out:
        if (!ok) {
            secerror("upgrade: SecDB upgrade failed: %@", localError);
        }
        CFReleaseSafe(sql);
        *commit = ok;
    });

    if (ok && didPhase2) {
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
        SecADSetValueForScalarKey(CFSTR("com.apple.keychain.migration-success"), 1);
#endif
    }

out:
    if (!ok || localError) {
        // TODO: This logic should be inverted to a do-not-corrupt-unless default, <rdar://problem/29771874>
        /*
         * We assume that database is corrupt at this point, but we need to
         * check if the error we got isn't severe enough to mark the database as corrupt.
         * In those cases we opt out of corrupting the database.
         */
        bool markedCorrupt = true;

        if (ok) {
            secwarning("upgrade: error has been set but status is true");
            ok = false;
        }
        secerror("upgrade: error occurred, considering marking database as corrupt: %@", localError);
        if (localError) {
            CFStringRef domain = CFErrorGetDomain(localError);
            CFIndex code = CFErrorGetCode(localError);
            
            if ((CFEqualSafe(domain, kSecDbErrorDomain) &&
                ((code & 0xff) == SQLITE_LOCKED || (code & 0xff) == SQLITE_BUSY || (code & 0xff) == SQLITE_FULL)) ||
#if USE_KEYSTORE
                code == kAKSReturnNotReady || code == kAKSReturnTimeout ||
#endif
                code == errSecNotAvailable)
            {
                secerror("upgrade: not marking keychain database corrupt for error: %@", localError);
                markedCorrupt = false;
                CFReleaseNull(localError);
            } else {
                secerror("upgrade: unable to complete upgrade, marking DB as corrupt: %@", localError);
            }
        } else {
            secerror("upgrade: unable to complete upgrade and no error object returned, marking DB as corrupt");
        }
        if (markedCorrupt) {
            secerror("upgrade: marking database as corrupt");
            SecDbCorrupt(dbt, localError);
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
            SecADSetValueForScalarKey(CFSTR("com.apple.keychain.migration-failure"), 1);
#endif
        }
    } else {
        // Things seemed to go okay!
        if (didPhase2) {
            LKAReportKeychainUpgradeOutcome(version, newVersion, LKAKeychainUpgradeOutcomeSuccess);
        }

        //If we're done here, we should opportunistically re-add all indices (just in case)
        if(skipped_upgrade || didPhase2) {
            // Create indices, ignoring all errors
            for (SecDbClass const* const* newClass = newSchema->classes; *newClass; ++newClass) {
                SecDbForEachAttrWithMask((*newClass), desc, kSecDbIndexFlag | kSecDbInFlag) {
                    CFStringRef sql = NULL;
                    CFErrorRef classLocalError = NULL;
                    bool localOk = true;

                    if (desc->kind == kSecDbSyncAttr) {
                        // Replace the complete sync index with a partial index for sync=0. Most items are sync=1, so the complete index isn't helpful for sync=1 queries.
                        sql = CFStringCreateWithFormat(NULL, NULL, CFSTR("DROP INDEX IF EXISTS %@%@; CREATE INDEX IF NOT EXISTS %@%@0 on %@(%@) WHERE %@=0;"),
                            (*newClass)->name, desc->name, (*newClass)->name, desc->name, (*newClass)->name, desc->name, desc->name);
                    } else {
                        sql = CFStringCreateWithFormat(NULL, NULL, CFSTR("CREATE INDEX IF NOT EXISTS %@%@ ON %@(%@);"), (*newClass)->name, desc->name, (*newClass)->name, desc->name);
                    }
                    localOk &= SecDbExec(dbt, sql, &classLocalError);
                    CFReleaseNull(sql);

                    if(!localOk) {
                        secerror("upgrade: unable to opportunistically create index (%@,%@): %@", (*newClass)->name, desc->name, classLocalError);
                    }
                    CFReleaseNull(classLocalError);
                }
            }
        }
    }
    if (localError) {
        if (error) {
            *error = (CFErrorRef)CFRetain(localError);
        }
        CFReleaseNull(localError);
    }

    return ok;
}

static bool accessGroupIsNetworkExtensionAndClientIsEntitled(CFStringRef accessGroup, SecurityClient* client)
{
    return client && client->canAccessNetworkExtensionAccessGroups && accessGroup && CFStringHasSuffix(accessGroup, kSecNetworkExtensionAccessGroupSuffix);
}

/* AUDIT[securityd](done):
   accessGroup (ok) is a caller provided, non NULL CFTypeRef.

   Return true iff accessGroup is allowable according to accessGroups.
 */
bool accessGroupsAllows(CFArrayRef accessGroups, CFStringRef accessGroup, SecurityClient* client) {
    /* NULL accessGroups is wildcard. */
    if (!accessGroups)
        return true;
    /* Make sure we have a string. */
    if (!isString(accessGroup))
        return false;

    /* Having the special accessGroup "*" allows access to all accessGroups. */
    CFRange range = { 0, CFArrayGetCount(accessGroups) };
    if (range.length &&
        (CFArrayContainsValue(accessGroups, range, accessGroup) ||
         CFArrayContainsValue(accessGroups, range, CFSTR("*")) ||
         accessGroupIsNetworkExtensionAndClientIsEntitled(accessGroup, client)))
        return true;

    return false;
}

bool itemInAccessGroup(CFDictionaryRef item, CFArrayRef accessGroups) {
    return accessGroupsAllows(accessGroups,
                              CFDictionaryGetValue(item, kSecAttrAccessGroup), NULL);
}


static CF_RETURNS_RETAINED CFDataRef SecServerExportBackupableKeychain(SecDbConnectionRef dbt,
    SecurityClient *client,
    keybag_handle_t src_keybag, keybag_handle_t dest_keybag, CFErrorRef *error) {
    CFDataRef data_out = NULL;

    SecSignpostStart(SecSignpostBackupKeychainBackupable);

    /* Export everything except the items for which SecItemIsSystemBound()
       returns true. */
    CFDictionaryRef keychain = SecServerCopyKeychainPlist(dbt, client,
        src_keybag, dest_keybag, kSecBackupableItemFilter,
        error);
    if (keychain) {
        data_out = CFPropertyListCreateData(kCFAllocatorDefault, keychain,
                                             kCFPropertyListBinaryFormat_v1_0,
                                             0, error);
        CFRelease(keychain);
    }
    SecSignpostStop(SecSignpostBackupKeychainBackupable);

    return data_out;
}

static bool SecServerImportBackupableKeychain(SecDbConnectionRef dbt,
                                              SecurityClient *client,
                                              keybag_handle_t src_keybag,
                                              keybag_handle_t dest_keybag,
                                              CFDataRef data,
                                              CFErrorRef *error)
{
    return kc_transaction(dbt, error, ^{
        bool ok = false;
        CFDictionaryRef keychain;

        SecSignpostStart(SecSignpostRestoreKeychainBackupable);

        keychain = CFPropertyListCreateWithData(kCFAllocatorDefault, data,
                                                kCFPropertyListImmutable, NULL,
                                                error);
        if (keychain) {
            if (isDictionary(keychain)) {
                ok = SecServerImportKeychainInPlist(dbt,
                                                    client,
                                                    src_keybag,
                                                    dest_keybag,
                                                    keychain,
                                                    kSecBackupableItemFilter,
                                                    false,  // Restoring backup should not remove stuff that got into the keychain before us
                                                    error);
            } else {
                ok = SecError(errSecParam, error, CFSTR("import: keychain is not a dictionary"));
            }
            CFRelease(keychain);
        }

        SecSignpostStop(SecSignpostRestoreKeychainBackupable);

        return ok;
    });
}

#if USE_KEYSTORE
/*
 * Similar to ks_open_keybag, but goes through MKB interface
 */
static bool mkb_open_keybag(CFDataRef keybag, CFDataRef password, MKBKeyBagHandleRef *handle, bool emcs, CFErrorRef *error) {
    kern_return_t rc;
    MKBKeyBagHandleRef mkbhandle = NULL;

    rc = MKBKeyBagCreateWithData(keybag, &mkbhandle);
    if (rc != kMobileKeyBagSuccess) {
        return SecKernError(rc, error, CFSTR("MKBKeyBagCreateWithData failed: %d"), rc);
    }

    if (!emcs) {
        rc = MKBKeyBagUnlock(mkbhandle, password);
        if (rc != kMobileKeyBagSuccess) {
            CFRelease(mkbhandle);
            return SecKernError(rc, error, CFSTR("failed to unlock bag: %d"), rc);
        }
    } else {
        secnotice("keychainbackup", "skipping keybag unlock for EMCS");
    }

    *handle = mkbhandle;

    return true;
}
#endif


static CFDataRef SecServerKeychainCreateBackup(SecDbConnectionRef dbt, SecurityClient *client, CFDataRef keybag,
    CFDataRef password, bool emcs, CFErrorRef *error) {
    CFDataRef backup = NULL;
    keybag_handle_t backup_keybag;

    SecSignpostStart(SecSignpostBackupOpenKeybag);

#if USE_KEYSTORE
    MKBKeyBagHandleRef mkbhandle = NULL;
    require(mkb_open_keybag(keybag, password, &mkbhandle, emcs, error), out);

    require_noerr(MKBKeyBagGetAKSHandle(mkbhandle, &backup_keybag), out);

#else
    backup_keybag = KEYBAG_NONE;
#endif
    SecSignpostStop(SecSignpostBackupOpenKeybag);
    SecSignpostStart(SecSignpostBackupKeychain);

    /* Export from system keybag to backup keybag. */
    backup = SecServerExportBackupableKeychain(dbt, client, KEYBAG_DEVICE, backup_keybag, error);

#if USE_KEYSTORE
out:
    SecSignpostStop(SecSignpostBackupOpenKeybag);

    if (mkbhandle)
        CFRelease(mkbhandle);
#endif
    return backup;
}

static bool SecServerKeychainRestore(SecDbConnectionRef dbt,
                                     SecurityClient *client,
                                     CFDataRef backup,
                                     CFDataRef keybag,
                                     CFDataRef password,
                                     CFErrorRef *error)
{
    bool ok = false;
    keybag_handle_t backup_keybag;

    secnotice("SecServerKeychainRestore", "Restoring keychain backup");

    SecSignpostStart(SecSignpostRestoreOpenKeybag);
#if USE_KEYSTORE
    MKBKeyBagHandleRef mkbhandle = NULL;
    require(mkb_open_keybag(keybag, password, &mkbhandle, false, error), out);

    require_noerr(MKBKeyBagGetAKSHandle(mkbhandle, &backup_keybag), out);
#else
    backup_keybag = KEYBAG_NONE;
#endif
    SecSignpostStop(SecSignpostRestoreOpenKeybag);
    SecSignpostStart(SecSignpostRestoreKeychain);

    /* Import from backup keybag to system keybag. */
    require(SecServerImportBackupableKeychain(dbt, client, backup_keybag, KEYBAG_DEVICE, backup, error), out);

    ok = true;
out:
    SecSignpostStop(SecSignpostRestoreKeychain);
#if USE_KEYSTORE
    if (mkbhandle)
        CFRelease(mkbhandle);
#endif

    if (ok) {
        secnotice("SecServerKeychainRestore", "Restore completed successfully");
    } else {
        secwarning("SecServerKeychainRestore: Restore failed with: %@", error ? *error : NULL);
    }

    return ok;
}

// MARK - External SPI support code.

CFStringRef __SecKeychainCopyPath(void) {
    CFStringRef kcRelPath = NULL;

    if (os_variant_is_recovery("securityd")) {
        kcRelPath = CFSTR("keychain-recovery-2.db");
    } else if (use_hwaes()) {
        kcRelPath = CFSTR("keychain-2.db");
    } else {
        kcRelPath = CFSTR("keychain-2-debug.db");
    }

    CFStringRef kcPath = NULL;
    CFURLRef kcURL = SecCopyURLForFileInKeychainDirectory(kcRelPath);
    if (kcURL) {
        kcPath = CFURLCopyFileSystemPath(kcURL, kCFURLPOSIXPathStyle);
        CFRelease(kcURL);
    }
    return kcPath;
}

// MARK; -
// MARK: kc_dbhandle init and reset

SecDbRef SecKeychainDbCreate(CFStringRef path, CFErrorRef* error) {
    __block CFErrorRef localerror = NULL;

    SecDbRef kc = SecDbCreate(path, 0600, true, true, true, true, kSecDbMaxIdleHandles,
        ^bool (SecDbRef db, SecDbConnectionRef dbconn, bool didCreate, bool *callMeAgainForNextConnection, CFErrorRef *createError)
    {
        // Upgrade from version 0 means create the schema in empty db.
        int version = 0;
        bool ok = true;
        if (!didCreate)
            ok = SecKeychainDbGetVersion(dbconn, &version, createError);

        ok = ok && SecKeychainDbUpgradeFromVersion(dbconn, version, callMeAgainForNextConnection, createError);
        if (!ok)
            secerror("Upgrade %sfailed: %@", didCreate ? "from v0 " : "", createError ? *createError : NULL);

        localerror = createError ? *createError : NULL;

        if(ok) {
            // This block might get called many, many times due to callMeAgainForNextConnection.
            // When we no longer want to be called, we believe we're done. Begin the rest of initialization.
            if( !callMeAgainForNextConnection || !(*callMeAgainForNextConnection)) {
                SecKeychainDbInitialize(db);
            }
        }

        return ok;
    });

    if (kc) {
        SecDbSetCorruptionReset(kc, ^{
            SecDbResetMetadataKeys();
        });
    }

    if(error) {
        *error = localerror;
    }

    return kc;
}

SecDbRef SecKeychainDbInitialize(SecDbRef db) {

#if OCTAGON
    if(SecCKKSIsEnabled()) {
        // This needs to be async, otherwise we get hangs between securityd, cloudd, and apsd
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            SecCKKSInitialize(db);

        });
    }

    if(OctagonIsEnabled() && OctagonShouldPerformInitialization()) {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            OctagonInitialize();
        });
    }

    if(EscrowRequestServerIsEnabled()) {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            EscrowRequestServerInitialize();
        });
    }
#endif

    return db;
}

static SecDbRef _kc_dbhandle = NULL;
static dispatch_queue_t _kc_dbhandle_dispatch = NULL;
static dispatch_once_t _kc_dbhandle_dispatch_onceToken = 0;
static dispatch_queue_t get_kc_dbhandle_dispatch() {
    dispatch_once(&_kc_dbhandle_dispatch_onceToken, ^{
        _kc_dbhandle_dispatch = dispatch_queue_create("sec_kc_dbhandle", DISPATCH_QUEUE_SERIAL);
    });

    return _kc_dbhandle_dispatch;
}

static bool kc_dbhandle_init(CFErrorRef* error) {
    SecDbRef oldHandle = _kc_dbhandle;
    _kc_dbhandle = NULL;
    CFStringRef dbPath = __SecKeychainCopyPath();
    if (dbPath) {
        _kc_dbhandle = SecKeychainDbCreate(dbPath, error);
        CFRelease(dbPath);
    } else {
        secerror("no keychain path available");
    }
    if (oldHandle) {
        secerror("replaced %@ with %@", oldHandle, _kc_dbhandle);
        CFRelease(oldHandle);
    }
    // Having a dbhandle means we succeeded.
    return !!_kc_dbhandle;
}

static SecDbRef kc_dbhandle(CFErrorRef* error)
{
    dispatch_sync(get_kc_dbhandle_dispatch(), ^{
        if(_kc_dbhandle == NULL) {
            _SecDbServerSetup();
            kc_dbhandle_init(error);
        }
    });
    return _kc_dbhandle;
}

/* For whitebox testing only */
void SecKeychainDbReset(dispatch_block_t inbetween)
{
    dispatch_sync(get_kc_dbhandle_dispatch(), ^{
        CFReleaseNull(_kc_dbhandle);
        SecDbResetMetadataKeys();

        if (inbetween)
            inbetween();
    });
}

static bool kc_acquire_dbt(bool writeAndRead, SecDbConnectionRef* dbconn, CFErrorRef *error) {
    SecDbRef db = kc_dbhandle(error);
    if (db == NULL) {
        if(error && !(*error)) {
            SecError(errSecDataNotAvailable, error, CFSTR("failed to get a db handle"));
        }
        return NULL;
    }

    return SecDbConnectionAcquireRefMigrationSafe(db, !writeAndRead, dbconn, error);
}

/* Return a per thread dbt handle for the keychain.  If create is true create
 the database if it does not yet exist.  If it is false, just return an
 error if it fails to auto-create. */
bool kc_with_dbt(bool writeAndRead, CFErrorRef *error, bool (^perform)(SecDbConnectionRef dbt))
{
    return kc_with_custom_db(writeAndRead, true, NULL, error, perform);
}

bool kc_with_dbt_non_item_tables(bool writeAndRead, CFErrorRef* error, bool (^perform)(SecDbConnectionRef dbt))
{
    return kc_with_custom_db(writeAndRead, false, NULL, error, perform);
}

bool kc_with_custom_db(bool writeAndRead, bool usesItemTables, SecDbRef db, CFErrorRef *error, bool (^perform)(SecDbConnectionRef dbt))
{
    if (db && db != kc_dbhandle(error)) {
        __block bool result = false;
        if (writeAndRead) {
            SecDbPerformWrite(db, error, ^(SecDbConnectionRef dbconn) {
                result = perform(dbconn);
            });
        }
        else {
            SecDbPerformRead(db, error, ^(SecDbConnectionRef dbconn) {
                result = perform(dbconn);
            });
        }
        return result;
    }

    if(threadDbt) {
        // The kc_with_dbt upthread will clean this up when it's done.
        return perform(threadDbt);
    }
    
    if (writeAndRead && usesItemTables) {
#if SECUREOBJECTSYNC
        SecItemDataSourceFactoryGetDefault();
#endif
    }

    bool ok = false;
    if (kc_acquire_dbt(writeAndRead, &threadDbt, error)) {
        ok = perform(threadDbt);
        SecDbConnectionRelease(threadDbt);
        threadDbt = NULL;
    }
    return ok;
}

static bool
items_matching_issuer_parent(SecDbConnectionRef dbt, CFArrayRef accessGroups, CFDataRef musrView,
                             CFDataRef issuer, CFArrayRef issuers, int recurse)
{
    Query *q;
    CFArrayRef results = NULL;
    CFIndex i, count;
    bool found = false;

    if (CFArrayContainsValue(issuers, CFRangeMake(0, CFArrayGetCount(issuers)), issuer))
        return true;

    /* XXX make musr supported */
    const void *keys[] = { kSecClass, kSecReturnRef, kSecAttrSubject };
    const void *vals[] = { kSecClassCertificate, kCFBooleanTrue, issuer };
    CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, array_size(keys), NULL, NULL);

    if (!query)
        return false;

    CFErrorRef localError = NULL;
    q = query_create_with_limit(query, musrView, kSecMatchUnlimited, &localError);
    CFRelease(query);
    if (q) {
        s3dl_copy_matching(dbt, q, (CFTypeRef*)&results, accessGroups, &localError);
        query_destroy(q, &localError);
    }
    if (localError) {
        secerror("items matching issuer parent: %@", localError);
        CFReleaseNull(localError);
        return false;
    }

    count = CFArrayGetCount(results);
    for (i = 0; (i < count) && !found; i++) {
        CFDictionaryRef cert_dict = (CFDictionaryRef)CFArrayGetValueAtIndex(results, i);
        CFDataRef cert_issuer = CFDictionaryGetValue(cert_dict, kSecAttrIssuer);
        if (CFEqual(cert_issuer, issuer))
            continue;
        if (recurse-- > 0)
            found = items_matching_issuer_parent(dbt, accessGroups, musrView, cert_issuer, issuers, recurse);
    }
    CFReleaseSafe(results);

    return found;
}

static bool
_FilterWithPolicy(SecPolicyRef policy, CFDateRef date, SecCertificateRef cert)
{
    CFDictionaryRef props = NULL;
    CFArrayRef keychains = NULL;
    CFArrayRef anchors = NULL;
    CFArrayRef certs = NULL;
    CFArrayRef chain = NULL;
    SecTrustRef trust = NULL;

    SecTrustResultType	trustResult;
    Boolean needChain = false;
    __block bool ok = false;

    if (!policy || !cert) return false;

    certs = CFArrayCreate(NULL, (const void **)&cert, (CFIndex)1, &kCFTypeArrayCallBacks);
    require_noerr_quiet(SecTrustCreateWithCertificates(certs, policy, &trust), cleanup);

    /* Set evaluation date, if specified (otherwise current date is implied) */
    if (date && (CFGetTypeID(date) == CFDateGetTypeID())) {
        require_noerr_quiet(SecTrustSetVerifyDate(trust, date), cleanup);
    }

    /* Check whether this is the X509 Basic policy, which means chain building */
    props = SecPolicyCopyProperties(policy);
    if (props) {
        CFTypeRef oid = (CFTypeRef) CFDictionaryGetValue(props, kSecPolicyOid);
        if (oid && (CFEqual(oid, kSecPolicyAppleX509Basic) ||
                    CFEqual(oid, kSecPolicyAppleRevocation))) {
            needChain = true;
        }
    }

    if (!needChain) {
        require_noerr_quiet(SecTrustEvaluateLeafOnly(trust, &trustResult), cleanup);
    } else {
        require_noerr_quiet(SecTrustEvaluate(trust, &trustResult), cleanup);
    }

    require_quiet((trustResult == kSecTrustResultProceed ||
                         trustResult == kSecTrustResultUnspecified ||
                         trustResult == kSecTrustResultRecoverableTrustFailure), cleanup);

    ok = true;
#if TARGET_OS_IPHONE
    CFArrayRef properties = SecTrustCopyProperties(trust);
#else
    CFArrayRef properties = SecTrustCopyProperties_ios(trust);
#endif
    if (properties) {
        CFArrayForEach(properties, ^(const void *property) {
            CFDictionaryForEach((CFDictionaryRef)property, ^(const void *key, const void *value) {
                if (CFEqual((CFTypeRef)key, kSecPropertyKeyType) && CFEqual((CFTypeRef)value, kSecPropertyTypeError))
                    ok = false;
            });
        });
        CFRelease(properties);
    }

cleanup:
    if(props) CFRelease(props);
    if(chain) CFRelease(chain);
    if(anchors) CFRelease(anchors);
    if(keychains) CFRelease(keychains);
    if(certs) CFRelease(certs);
    if(trust) CFRelease(trust);
    
    return ok;
}

static bool
_FilterWithDate(CFDateRef validOnDate, SecCertificateRef cert)
{
    if (!validOnDate || !cert) return false;

    CFAbsoluteTime at, nb, na;
    at = CFDateGetAbsoluteTime((CFDateRef)validOnDate);

    bool ok = true;
    nb = SecCertificateNotValidBefore(cert);
    na = SecCertificateNotValidAfter(cert);

    if (nb == 0 || na == 0 || nb == na) {
        ok = false;
        secnotice("FilterWithDate", "certificate cannot operate");
    }
    else if (at < nb) {
        ok = false;
        secnotice("FilterWithDate", "certificate is not valid yet");
    }
    else if (at > na) {
        ok = false;
        secnotice("FilterWithDate", "certificate expired");
    }
    
    return ok;
}

static bool
_FilterWithTrust(Boolean trustedOnly, SecCertificateRef cert)
{
    if (!cert) return false;
    if (!trustedOnly) return true;

    bool ok = false;
    CFArrayRef certArray = CFArrayCreate(NULL, (const void**)&cert, 1, &kCFTypeArrayCallBacks);
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    require_quiet(policy, out);

    require_noerr_quiet(SecTrustCreateWithCertificates(certArray, policy, &trust), out);
    SecTrustResultType trustResult;
    require_noerr_quiet(SecTrustEvaluate(trust, &trustResult), out);

    require_quiet((trustResult == kSecTrustResultProceed ||
                   trustResult == kSecTrustResultUnspecified), out);
    ok = true;
out:
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(certArray);
    return ok;
}

static SecCertificateRef
CopyCertificateFromItem(Query *q, CFDictionaryRef item) {
    SecCertificateRef certRef = NULL;
    CFDictionaryRef itemValue = NULL;

    CFTypeRef tokenID = NULL;
    CFDataRef certData = NULL;
    if (q->q_class == identity_class()) {
        certData = CFDictionaryGetValue(item, kSecAttrIdentityCertificateData);
        tokenID = CFDictionaryGetValue(item, kSecAttrIdentityCertificateTokenID);
    } else if (q->q_class == cert_class()) {
        certData = CFDictionaryGetValue(item, kSecValueData);
        tokenID = CFDictionaryGetValue(item, kSecAttrTokenID);
    }

    require_quiet(certData, out);
    if (tokenID != NULL) {
        CFErrorRef error = NULL;
        itemValue = SecTokenItemValueCopy(certData, &error);
        require_action_quiet(itemValue, out, { secerror("function SecTokenItemValueCopy failed with: %@", error); CFReleaseSafe(error); });
        CFDataRef tokenCertData = CFDictionaryGetValue(itemValue, kSecTokenValueDataKey);
        require_action_quiet(tokenCertData, out, { secerror("token item doesn't contain token value data");});
        certRef = SecCertificateCreateWithData(kCFAllocatorDefault, tokenCertData);
    }
    else
        certRef = SecCertificateCreateWithData(kCFAllocatorDefault, certData);

out:
    CFReleaseNull(itemValue);
    return certRef;
}

bool match_item(SecDbConnectionRef dbt, Query *q, CFArrayRef accessGroups, CFDictionaryRef item)
{
    bool ok = false;
    SecCertificateRef certRef = NULL;
    if (q->q_match_issuer) {
        CFDataRef issuer = CFDictionaryGetValue(item, kSecAttrIssuer);
        if (!items_matching_issuer_parent(dbt, accessGroups, q->q_musrView, issuer, q->q_match_issuer, 10 /*max depth*/))
            return ok;
    }

    if (q->q_match_policy && (q->q_class == identity_class() || q->q_class == cert_class())) {
        if (!certRef)
            certRef = CopyCertificateFromItem(q, item);
        require_quiet(certRef, out);
        require_quiet(_FilterWithPolicy(q->q_match_policy, q->q_match_valid_on_date, certRef), out);
    }

    if (q->q_match_valid_on_date && (q->q_class == identity_class() || q->q_class == cert_class())) {
        if (!certRef)
            certRef = CopyCertificateFromItem(q, item);
        require_quiet(certRef, out);
        require_quiet(_FilterWithDate(q->q_match_valid_on_date, certRef), out);
    }

    if (q->q_match_trusted_only && (q->q_class == identity_class() || q->q_class == cert_class())) {
        if (!certRef)
            certRef = CopyCertificateFromItem(q, item);
        require_quiet(certRef, out);
        require_quiet(_FilterWithTrust(CFBooleanGetValue(q->q_match_trusted_only), certRef), out);
    }

    /* Add future match checks here. */
    ok = true;
out:
    CFReleaseSafe(certRef);
    return ok;
}

/****************************************************************************
 **************** Beginning of Externally Callable Interface ****************
 ****************************************************************************/

static bool SecEntitlementError(CFErrorRef *error)
{
#if TARGET_OS_OSX
#define SEC_ENTITLEMENT_WARNING CFSTR("com.apple.application-identifier nor com.apple.security.application-groups nor keychain-access-groups")
#elif TARGET_OS_IOSMAC
#define SEC_ENTITLEMENT_WARNING CFSTR("com.apple.developer.associated-application-identifier nor application-identifier nor com.apple.security.application-groups nor keychain-access-groups")
#else
#define SEC_ENTITLEMENT_WARNING CFSTR("application-identifier nor keychain-access-groups")
#endif

    return SecError(errSecMissingEntitlement, error, CFSTR("Client has neither %@ entitlements"), SEC_ENTITLEMENT_WARNING);
}

static bool SecEntitlementErrorForExplicitAccessGroup(CFStringRef agrp, CFArrayRef clientGroups, CFErrorRef* error)
{
    return SecError(errSecMissingEntitlement, error, CFSTR("Client explicitly specifies access group %@ but is only entitled for %@"), agrp, clientGroups);
}

static CFStringRef CopyAccessGroupForRowID(sqlite_int64 rowID, CFStringRef itemClass)
{
    __block CFStringRef accessGroup = NULL;
    
    __block CFErrorRef error = NULL;
    bool ok = kc_with_dbt(false, &error, ^bool(SecDbConnectionRef dbt) {
        CFStringRef table = CFEqual(itemClass, kSecClassIdentity) ? kSecClassCertificate : itemClass;
        CFStringRef sql = CFStringCreateWithFormat(NULL, NULL, CFSTR("SELECT agrp FROM %@ WHERE rowid == %u"), table, (unsigned int)rowID);
        bool dbOk = SecDbWithSQL(dbt, sql, &error, ^bool(sqlite3_stmt *stmt) {
            bool rowOk = SecDbForEach(dbt, stmt, &error, ^bool(int row_index) {
                accessGroup = CFStringCreateWithBytes(NULL, sqlite3_column_blob(stmt, 0), sqlite3_column_bytes(stmt, 0), kCFStringEncodingUTF8, false);
                return accessGroup != NULL;
            });
            
            return (bool)(rowOk && accessGroup != NULL);
        });
        
        CFReleaseNull(sql);
        return (bool)(dbOk && accessGroup);
    });
    
    if (ok) {
        return accessGroup;
    }
    else {
        CFReleaseNull(accessGroup);
        return NULL;
    }
}

/* AUDIT[securityd](done):
   query (ok) is a caller provided dictionary, only its cf type has been checked.
 */
static bool
SecItemServerCopyMatching(CFDictionaryRef query, CFTypeRef *result,
    SecurityClient *client, CFErrorRef *error)
{
    CFArrayRef accessGroups = CFRetainSafe(client->accessGroups);

    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        CFReleaseNull(accessGroups);
        return SecEntitlementError(error);
    }

    SecSignpostStart(SecSignpostSecItemCopyMatching);

    if (client->canAccessNetworkExtensionAccessGroups) {
        CFDataRef persistentRef = CFDictionaryGetValue(query, kSecValuePersistentRef);
        CFStringRef itemClass = NULL;
        sqlite_int64 itemRowID = 0;
        if (persistentRef && _SecItemParsePersistentRef(persistentRef, &itemClass, &itemRowID, NULL)) {
            CFStringRef accessGroup = CopyAccessGroupForRowID(itemRowID, itemClass);
            if (accessGroup && CFStringHasSuffix(accessGroup, kSecNetworkExtensionAccessGroupSuffix) && !CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), accessGroup)) {
                CFMutableArrayRef mutableAccessGroups = CFArrayCreateMutableCopy(NULL, 0, accessGroups);
                CFArrayAppendValue(mutableAccessGroups, accessGroup);
                CFReleaseNull(accessGroups);
                accessGroups = mutableAccessGroups;
            }
            CFReleaseNull(accessGroup);
        }
    }

    if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*"))) {
        /* Having the special accessGroup "*" allows access to all accessGroups. */
        CFReleaseNull(accessGroups);
    }

    bool ok = false;
    Query *q = query_create_with_limit(query, client->musr, 1, error);
    if (q) {
        CFStringRef agrp = CFDictionaryGetValue(q->q_item, kSecAttrAccessGroup);
        if (agrp) {
            if (accessGroupsAllows(accessGroups, agrp, client)) {
                const void *val = agrp;
                CFReleaseNull(accessGroups);
                accessGroups = CFArrayCreate(0, &val, 1, &kCFTypeArrayCallBacks);
            } else {
                (void)SecEntitlementErrorForExplicitAccessGroup(agrp, accessGroups, error);
                CFReleaseNull(accessGroups);
                query_destroy(q, NULL);
                return false;
            }
        } else {
#if !TARGET_OS_OSX
            if (accessGroups != NULL) {
                // On iOS, drop 'com.apple.token' AG from allowed accessGroups, to avoid inserting token elements into
                // unsuspecting application's keychain. If the application on iOS wants to access token items, it needs
                // explicitly specify kSecAttrAccessGroup=kSecAttrAccessGroupToken in its query.
                CFMutableArrayRef mutableGroups =  CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, accessGroups);
                CFArrayRemoveAllValue(mutableGroups, kSecAttrAccessGroupToken);
                CFReleaseNull(accessGroups);
                accessGroups = mutableGroups;
            }
#endif
        }

#if TARGET_OS_IPHONE
        if (q->q_sync_bubble && client->inMultiUser) {
            CFReleaseNull(q->q_musrView);
            q->q_musrView = SecMUSRCreateSyncBubbleUserUUID(q->q_sync_bubble);
        } else if (client->inMultiUser && client->isNetworkExtension) {
            CFReleaseNull(q->q_musrView);
            q->q_musrView = SecMUSRCreateBothUserAndSystemUUID(client->uid);
        } else if (q->q_system_keychain && client->inMultiUser) {
            CFReleaseNull(q->q_musrView);
            q->q_musrView = SecMUSRCopySystemKeychainUUID();
        } else {
            q->q_system_keychain = false;
        }
#endif

        query_set_caller_access_groups(q, accessGroups);

        /* Sanity check the query. */
        if (q->q_system_keychain && !client->allowSystemKeychain) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for system keychain"));
        } else if (q->q_sync_bubble && !client->allowSyncBubbleKeychain) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for syncbubble keychain"));
        } else if (q->q_system_keychain && q->q_sync_bubble) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("can't do both system and syncbubble keychain"));
        } else if (q->q_use_item_list) {
            ok = SecError(errSecUseItemListUnsupported, error, CFSTR("use item list unsupported"));
        } else if (q->q_match_issuer && ((q->q_class != cert_class()) &&
                    (q->q_class != identity_class()))) {
            ok = SecError(errSecUnsupportedOperation, error, CFSTR("unsupported match attribute"));
        } else if (q->q_match_policy && ((q->q_class != cert_class()) &&
                    (q->q_class != identity_class()))) {
            ok = SecError(errSecUnsupportedOperation, error, CFSTR("unsupported kSecMatchPolicy attribute"));
        } else if (q->q_return_type != 0 && result == NULL) {
            ok = SecError(errSecReturnMissingPointer, error, CFSTR("missing pointer"));
        } else if (!q->q_error) {
            ok = kc_with_dbt(false, error, ^(SecDbConnectionRef dbt) {
                return s3dl_copy_matching(dbt, q, result, accessGroups, error);
            });
        }

        if (!query_destroy(q, error))
            ok = false;
    }
    CFReleaseNull(accessGroups);

    SecSignpostStop(SecSignpostSecItemCopyMatching);

	return ok;
}

bool
_SecItemCopyMatching(CFDictionaryRef query, SecurityClient *client, CFTypeRef *result, CFErrorRef *error) {
    return SecItemServerCopyMatching(query, result, client, error);
}

#if TARGET_OS_IPHONE
static bool
SecItemSynchronizable(CFDictionaryRef query)
{
    bool result = false;
    CFTypeRef value = CFDictionaryGetValue(query, kSecAttrSynchronizable);
    if (isBoolean(value))
        return CFBooleanGetValue(value);
    else if (isNumber(value)) {
        SInt32 number = 0;
        (void)CFNumberGetValue(value, kCFNumberSInt32Type, &number);
        result = !!number;
    }

    return result;
}
#endif

static CFArrayRef
SecurityClientCopyWritableAccessGroups(SecurityClient *client) {
    if (client == NULL || client->accessGroups == NULL) {
        return NULL;
    }
    CFIndex count = CFArrayGetCount(client->accessGroups);
    if (CFArrayContainsValue(client->accessGroups, CFRangeMake(0, count), kSecAttrAccessGroupToken)) {
        CFMutableArrayRef writableGroups = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, client->accessGroups);
        CFArrayRemoveAllValue(writableGroups, kSecAttrAccessGroupToken);
        return writableGroups;
    } else {
        return CFRetainSafe(client->accessGroups);
    }
}


/* AUDIT[securityd](done):
   attributes (ok) is a caller provided dictionary, only its cf type has
       been checked.
 */
bool
_SecItemAdd(CFDictionaryRef attributes, SecurityClient *client, CFTypeRef *result, CFErrorRef *error)
{
    CFArrayRef accessGroups = SecurityClientCopyWritableAccessGroups(client);

    bool ok = true;
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        CFReleaseNull(accessGroups);
        return SecEntitlementError(error);
    }

    SecSignpostStart(SecSignpostSecItemAdd);

    Query *q = query_create_with_limit(attributes, client->musr, 0, error);
    if (q) {
        /* Access group sanity checking. */
        CFStringRef agrp = (CFStringRef)CFDictionaryGetValue(attributes,
            kSecAttrAccessGroup);

        /* Having the special accessGroup "*" allows access to all accessGroups. */
        if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*")))
            CFReleaseNull(accessGroups);

        if (agrp) {
            /* The user specified an explicit access group, validate it. */
            if (!accessGroupsAllows(accessGroups, agrp, client))
                ok = SecEntitlementErrorForExplicitAccessGroup(agrp, accessGroups, error);
        } else {
            agrp = (CFStringRef)CFArrayGetValueAtIndex(client->accessGroups, 0);

            /* We are using an implicit access group, add it as if the user
               specified it as an attribute. */
            query_add_attribute(kSecAttrAccessGroup, agrp, q);
        }

        if (SecPLShouldLogRegisteredEvent(CFSTR("SecItem"))) {
            CFDictionaryRef dict = CFDictionaryCreateForCFTypes(NULL, CFSTR("operation"), CFSTR("add"), CFSTR("AccessGroup"), agrp, NULL);
            if (dict) {
                SecPLLogRegisteredEvent(CFSTR("SecItem"), dict);
                CFRelease(dict);
            }
        }
#if TARGET_OS_IPHONE
        if (q->q_system_keychain && client->inMultiUser) {
            CFReleaseNull(q->q_musrView);
            q->q_musrView = SecMUSRCopySystemKeychainUUID();
        } else {
            q->q_system_keychain = false;
        }
        query_add_attribute_with_desc(&v8musr, q->q_musrView, q);
#endif

        if (ok) {
            query_ensure_access_control(q, agrp);

#if OCTAGON
            void (^add_sync_callback)(bool, CFErrorRef) = CFDictionaryGetValue(attributes, CFSTR("f_ckkscallback"));
            if(add_sync_callback) {
                // The existence of this callback indicates that we need a predictable UUID for this item.
                q->q_uuid_from_primary_key = true;
                q->q_add_sync_callback = add_sync_callback;
            }
#endif

            if (q->q_system_keychain && !client->allowSystemKeychain) {
                ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for system keychain"));
            } else if (q->q_sync_bubble && !client->allowSyncBubbleKeychain) {
                ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for syncbubble keychain"));
#if TARGET_OS_IPHONE
            } else if (q->q_system_keychain && SecItemSynchronizable(attributes) && !client->inMultiUser) {
                ok = SecError(errSecInvalidKey, error, CFSTR("Can't store system keychain and synchronizable"));
#endif
            } else if (q->q_row_id || q->q_token_object_id) {
                ok = SecError(errSecValuePersistentRefUnsupported, error, CFSTR("q_row_id"));  // TODO: better error string
            } else if (!q->q_error) {
                ok = kc_with_dbt(true, error, ^(SecDbConnectionRef dbt){
                    return kc_transaction(dbt, error, ^{
                        query_pre_add(q, true);
                        return s3dl_query_add(dbt, q, result, error);
                    });
                });
            }
        }
        ok = query_notify_and_destroy(q, ok, error);
    } else {
        ok = false;
    }
    CFReleaseNull(accessGroups);

    SecSignpostStop(SecSignpostSecItemAdd);

    return ok;
}

/* AUDIT[securityd](done):
   query (ok) and attributesToUpdate (ok) are a caller provided dictionaries,
       only their cf types have been checked.
 */
bool
_SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate,
               SecurityClient *client, CFErrorRef *error)
{
    CFArrayRef accessGroups = SecurityClientCopyWritableAccessGroups(client);

    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        CFReleaseNull(accessGroups);
        return SecEntitlementError(error);
    }

    // Queries using implicit access groups which only find items that're inaccessible yield errSecItemNotFound,
    // but we can pre-emptively shut down queries which are clearly illegal
    CFTypeRef q_agrp = CFDictionaryGetValue(query, kSecAttrAccessGroup);
    if (q_agrp && !accessGroupsAllows(accessGroups, q_agrp, client)) {
        SecEntitlementErrorForExplicitAccessGroup(q_agrp, accessGroups, error);
        CFReleaseSafe(accessGroups);
        return false;
    }

    SecSignpostStart(SecSignpostSecItemUpdate);

    if (SecPLShouldLogRegisteredEvent(CFSTR("SecItem"))) {
        CFTypeRef agrp = CFArrayGetValueAtIndex(accessGroups, 0);
        CFDictionaryRef dict = CFDictionaryCreateForCFTypes(NULL, CFSTR("operation"), CFSTR("update"), CFSTR("AccessGroup"), agrp, NULL);
        if (dict) {
            SecPLLogRegisteredEvent(CFSTR("SecItem"), dict);
            CFRelease(dict);
        }
    }

    if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*"))) {
        /* Having the special accessGroup "*" allows access to all accessGroups. */
        CFReleaseNull(accessGroups);
    }

    bool ok = true;
    Query *q = query_create_with_limit(query, client->musr, kSecMatchUnlimited, error);
    if (!q) {
        ok = false;
    }
    if (ok) {
#if TARGET_OS_IPHONE
        if (q->q_system_keychain && client->inMultiUser) {
            CFReleaseNull(q->q_musrView);
            q->q_musrView = SecMUSRCopySystemKeychainUUID();
        } else {
            q->q_system_keychain = false;
        }
#endif

        /* Sanity check the query. */
        query_set_caller_access_groups(q, accessGroups);
        if (q->q_system_keychain && !client->allowSystemKeychain) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for system keychain"));
        } else if (q->q_sync_bubble && !client->allowSyncBubbleKeychain) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for syncbubble keychain"));
#if TARGET_OS_IPHONE
        } else if (q->q_system_keychain && SecItemSynchronizable(attributesToUpdate) && !client->inMultiUser) {
            ok = SecError(errSecInvalidKey, error, CFSTR("Can't update an system keychain item with synchronizable"));
#endif
        } else if (q->q_use_item_list) {
            ok = SecError(errSecUseItemListUnsupported, error, CFSTR("use item list not supported"));
        } else if (q->q_return_type & kSecReturnDataMask) {
            /* Update doesn't return anything so don't ask for it. */
            ok = SecError(errSecReturnDataUnsupported, error, CFSTR("return data not supported by update"));
        } else if (q->q_return_type & kSecReturnAttributesMask) {
            ok = SecError(errSecReturnAttributesUnsupported, error, CFSTR("return attributes not supported by update"));
        } else if (q->q_return_type & kSecReturnRefMask) {
            ok = SecError(errSecReturnRefUnsupported, error, CFSTR("return ref not supported by update"));
        } else if (q->q_return_type & kSecReturnPersistentRefMask) {
            ok = SecError(errSecReturnPersistentRefUnsupported, error, CFSTR("return persistent ref not supported by update"));
        } else {
            /* Access group sanity checking. */
            CFStringRef agrp = (CFStringRef)CFDictionaryGetValue(attributesToUpdate,
                kSecAttrAccessGroup);
            if (agrp) {
                /* The user is attempting to modify the access group column,
                   validate it to make sure the new value is allowable. */
                if (!accessGroupsAllows(accessGroups, agrp, client)) {
                    secerror("Cannot update keychain item to access group %@", agrp);
                    ok = SecEntitlementErrorForExplicitAccessGroup(agrp, accessGroups, error);
                }
            }
        }
    }
    if (ok) {
        ok = kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
            return kc_transaction(dbt, error, ^{
                return s3dl_query_update(dbt, q, attributesToUpdate, accessGroups, error);
            });
        });
    }
    if (q) {
        ok = query_notify_and_destroy(q, ok, error);
    }
    CFReleaseNull(accessGroups);

    SecSignpostStop(SecSignpostSecItemUpdate);

    return ok;
}


/* AUDIT[securityd](done):
   query (ok) is a caller provided dictionary, only its cf type has been checked.
 */
bool
_SecItemDelete(CFDictionaryRef query, SecurityClient *client, CFErrorRef *error)
{
    CFArrayRef accessGroups = SecurityClientCopyWritableAccessGroups(client);

    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        CFReleaseNull(accessGroups);
        return SecEntitlementError(error);
    }

    CFTypeRef q_agrp = CFDictionaryGetValue(query, kSecAttrAccessGroup);
    if (q_agrp && !accessGroupsAllows(accessGroups, q_agrp, client)) {
        SecEntitlementErrorForExplicitAccessGroup(q_agrp, accessGroups, error);
        CFReleaseSafe(accessGroups);
        return false;
    }

    SecSignpostStart(SecSignpostSecItemDelete);

    if (SecPLShouldLogRegisteredEvent(CFSTR("SecItem"))) {
        CFTypeRef agrp = CFArrayGetValueAtIndex(accessGroups, 0);
        CFDictionaryRef dict = CFDictionaryCreateForCFTypes(NULL, CFSTR("operation"), CFSTR("delete"), CFSTR("AccessGroup"), agrp, NULL);
        if (dict) {
            SecPLLogRegisteredEvent(CFSTR("SecItem"), dict);
            CFRelease(dict);
        }
    }

    if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*"))) {
        /* Having the special accessGroup "*" allows access to all accessGroups. */
        CFReleaseNull(accessGroups);
    }

    Query *q = query_create_with_limit(query, client->musr, kSecMatchUnlimited, error);
    bool ok;
    if (q) {
#if TARGET_OS_IPHONE
        if (q->q_system_keychain && client->inMultiUser) {
            CFReleaseNull(q->q_musrView);
            q->q_musrView = SecMUSRCopySystemKeychainUUID();
        } else {
            q->q_system_keychain = false;
        }
#endif

        query_set_caller_access_groups(q, accessGroups);
        /* Sanity check the query. */
        if (q->q_system_keychain && !client->allowSystemKeychain) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for system keychain"));
        } else if (q->q_sync_bubble && !client->allowSyncBubbleKeychain) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for syncbubble keychain"));
        } else if (q->q_limit != kSecMatchUnlimited) {
            ok = SecError(errSecMatchLimitUnsupported, error, CFSTR("match limit not supported by delete"));
        } else if (query_match_count(q) != 0) {
            ok = SecError(errSecItemMatchUnsupported, error, CFSTR("match not supported by delete"));
        } else if (q->q_ref) {
            ok = SecError(errSecValueRefUnsupported, error, CFSTR("value ref not supported by delete"));
        } else if (q->q_row_id && query_attr_count(q)) {
            ok = SecError(errSecItemIllegalQuery, error, CFSTR("rowid and other attributes are mutually exclusive"));
        } else if (q->q_token_object_id && query_attr_count(q) != 1) {
            ok = SecError(errSecItemIllegalQuery, error, CFSTR("token persistent ref and other attributes are mutually exclusive"));
        } else {
            ok = kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
                return kc_transaction(dbt, error, ^{
                    return s3dl_query_delete(dbt, q, accessGroups, error);
                });
            });
        }
        ok = query_notify_and_destroy(q, ok, error);
    } else {
        ok = false;
    }
    CFReleaseNull(accessGroups);

    SecSignpostStop(SecSignpostSecItemDelete);

    return ok;
}

static bool SecItemDeleteTokenItems(SecDbConnectionRef dbt, CFTypeRef classToDelete, CFTypeRef tokenID, CFArrayRef accessGroups, SecurityClient *client, CFErrorRef *error) {
    CFTypeRef keys[] = { kSecClass, kSecAttrTokenID };
    CFTypeRef values[] = { classToDelete, tokenID };
    
    CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    Query *q = query_create_with_limit(query, client->musr, kSecMatchUnlimited, error);
    CFRelease(query);
    bool ok;
    if (q) {
        query_set_caller_access_groups(q, accessGroups);
        ok = s3dl_query_delete(dbt, q, accessGroups, error);
        ok = query_notify_and_destroy(q, ok, error);
    } else {
        ok = false;
    }

    return ok;
}

static bool SecItemAddTokenItem(SecDbConnectionRef dbt, CFDictionaryRef attributes, CFArrayRef accessGroups, SecurityClient *client, CFErrorRef *error) {
    bool ok = true;
    Query *q = query_create_with_limit(attributes, client->musr, 0, error);
    if (q) {
        CFStringRef agrp = kSecAttrAccessGroupToken;
        query_add_attribute(kSecAttrAccessGroup, agrp, q);

        if (ok) {
            query_ensure_access_control(q, agrp);
            if (q->q_system_keychain && !client->allowSystemKeychain) {
                ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for system keychain"));
            } else if (q->q_sync_bubble && !client->allowSyncBubbleKeychain) {
                ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for syncbubble keychain"));
            } else if (q->q_row_id || q->q_token_object_id) {
                ok = SecError(errSecValuePersistentRefUnsupported, error, CFSTR("q_row_id"));  // TODO: better error string
            } else if (!q->q_error) {
                query_pre_add(q, true);
                ok = s3dl_query_add(dbt, q, NULL, error);
            }
        }
        ok = query_notify_and_destroy(q, ok, error);
    } else {
        return false;
    }
    return ok;
}

bool _SecItemUpdateTokenItems(CFStringRef tokenID, CFArrayRef items, SecurityClient *client, CFErrorRef *error) {
    bool ok = true;
    CFArrayRef accessGroups = client->accessGroups;
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        return SecEntitlementError(error);
    }

    ok = kc_with_dbt(true, error, ^bool (SecDbConnectionRef dbt) {
        return kc_transaction(dbt, error, ^bool {
            if (items) {
                const CFTypeRef classToDelete[] = { kSecClassGenericPassword, kSecClassInternetPassword, kSecClassCertificate, kSecClassKey };
                for (size_t i = 0; i < sizeof(classToDelete) / sizeof(classToDelete[0]); ++i) {
                    SecItemDeleteTokenItems(dbt, classToDelete[i], tokenID, accessGroups, client, NULL);
                }

                for (CFIndex i = 0; i < CFArrayGetCount(items); ++i) {
                    if (!SecItemAddTokenItem(dbt, CFArrayGetValueAtIndex(items, i), accessGroups, client, error))
                        return false;
                }
                return true;
            }
            else {
                const CFTypeRef classToDelete[] = { kSecClassGenericPassword, kSecClassInternetPassword, kSecClassCertificate, kSecClassKey };
                bool deleted = true;
                for (size_t i = 0; i < sizeof(classToDelete) / sizeof(classToDelete[0]); ++i) {
                    if (!SecItemDeleteTokenItems(dbt, classToDelete[i], tokenID, accessGroups, client, error) && error && CFErrorGetCode(*error) != errSecItemNotFound) {
                        deleted = false;
                        break;
                    }
                    else if (error && *error) {
                        CFReleaseNull(*error);
                    }
                }
                return deleted;
            }
        });
    });

    return ok;
}

static bool deleteNonSysboundItemsForItemClass(SecDbConnectionRef dbt, SecDbClass const* class, CFErrorRef* error) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutableForCFTypes(NULL);
    CFDictionaryAddValue(query, kSecMatchLimit, kSecMatchLimitAll);

    __block CFErrorRef localError = NULL;
    SecDbQueryRef q = query_create(class, NULL, query, &localError);
    if (q == NULL) {    // illegal query or out of memory
        secerror("SecItemServerDeleteAll: aborting because failed to initialize Query: %@", localError);
        abort();
    }
    SecDbItemSelect(q, dbt, &localError, ^bool(const SecDbAttr *attr) {
        return (attr->flags & kSecDbInFlag) && !CFEqual(attr->name, CFSTR("data"));
    }, NULL, NULL, NULL,
    ^(SecDbItemRef item, bool *stop) {
        if (!SecItemIsSystemBound(item->attributes, class, false) &&
            !CFEqual(CFDictionaryGetValue(item->attributes, kSecAttrAccessGroup), CFSTR("com.apple.bluetooth")))
        {
            SecDbItemDelete(item, dbt, kCFBooleanFalse, &localError);
        }
    });
    query_destroy(q, &localError);

    if (localError) {
        if (error) {
            CFReleaseNull(*error);
            *error = localError;
        } else {
            CFReleaseNull(localError);
        }
        return false;
    }
    return true;
}

// Delete all the items except sysbound ones because horrible things happen if you do, like bluetooth devices unpairing
static bool
SecItemServerDeleteAll(CFErrorRef *error) {
    secerror("SecItemServerDeleteAll");
    return kc_with_dbt(true, error, ^bool (SecDbConnectionRef dbt) {
        return (kc_transaction(dbt, error, ^bool {
            bool ok = true;
            ok &= SecDbExec(dbt, CFSTR("DELETE FROM genp WHERE sync=1;"), error);
            ok &= SecDbExec(dbt, CFSTR("DELETE FROM inet WHERE sync=1;"), error);
            ok &= SecDbExec(dbt, CFSTR("DELETE FROM cert WHERE sync=1;"), error);
            ok &= SecDbExec(dbt, CFSTR("DELETE FROM keys WHERE sync=1;"), error);

            ok &= deleteNonSysboundItemsForItemClass(dbt, genp_class(), error);
            ok &= deleteNonSysboundItemsForItemClass(dbt, inet_class(), error);
            ok &= deleteNonSysboundItemsForItemClass(dbt, cert_class(), error);
            ok &= deleteNonSysboundItemsForItemClass(dbt, keys_class(), error);

            return ok;
        }) && SecDbExec(dbt, CFSTR("VACUUM;"), error));
    });
}

bool
_SecItemDeleteAll(CFErrorRef *error) {
    return SecItemServerDeleteAll(error);
}

bool
_SecItemServerDeleteAllWithAccessGroups(CFArrayRef accessGroups, SecurityClient *client, CFErrorRef *error)
{
    __block bool ok = true;
    static dispatch_once_t onceToken;
    static CFSetRef illegalAccessGroups = NULL;

    dispatch_once(&onceToken, ^{
        const CFStringRef values[] = {
            CFSTR("*"),
            CFSTR("apple"),
            CFSTR("com.apple.security.sos"),
            CFSTR("lockdown-identities"),
        };
        illegalAccessGroups = CFSetCreate(NULL, (const void **)values, sizeof(values)/sizeof(values[0]), &kCFTypeSetCallBacks);
    });

    static CFTypeRef qclasses[] = {
        NULL,
        NULL,
        NULL,
        NULL
    };
    // strange construction needed for schema indirection
    static dispatch_once_t qclassesOnceToken;
    dispatch_once(&qclassesOnceToken, ^{
        qclasses[0] = inet_class();
        qclasses[1] = genp_class();
        qclasses[2] = keys_class();
        qclasses[3] = cert_class();
    });

    require_action_quiet(isArray(accessGroups), fail,
                         ok = false;
                         SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedType, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("accessGroups not CFArray, got %@"), accessGroups));

    // TODO: whitelist instead? look for dev IDs like 7123498YQX.com.somedev.app

    require_action(CFArrayGetCount(accessGroups) != 0, fail,
                   ok = false;
                   SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedType, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("accessGroups e empty")));


    // Pre-check accessGroups for prohibited values
    CFArrayForEach(accessGroups, ^(const void *value) {
        CFStringRef agrp = (CFStringRef)value;

        if (!isString(agrp)) {
            SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedType, sSecXPCErrorDomain, NULL, error, NULL,
                                       CFSTR("access not a string: %@"), agrp);
            ok &= false;
        } else if (CFSetContainsValue(illegalAccessGroups, agrp)) {
            SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedType, sSecXPCErrorDomain, NULL, error, NULL,
                                       CFSTR("illegal access group: %@"), accessGroups);
            ok &= false;
        }
    });
    require(ok,fail);

    ok = kc_with_dbt(true, error, ^bool(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, error, ^bool {
            CFErrorRef localError = NULL;
            bool ok1 = true;
            size_t n;

            for (n = 0; n < sizeof(qclasses)/sizeof(qclasses[0]) && ok1; n++) {
                Query *q;

                q = query_create(qclasses[n], client->musr, NULL, error);
                require(q, fail2);

                (void)s3dl_query_delete(dbt, q, accessGroups, &localError);
            fail2:
                query_destroy(q, error);
                CFReleaseNull(localError);
            }
            return ok1;
        }) && SecDbExec(dbt, CFSTR("VACUUM"), error);
    });

fail:
    return ok;
}


// MARK: -
// MARK: Shared web credentials

#if SHAREDWEBCREDENTIALS

/* constants */
#define SEC_CONST_DECL(k,v) const CFStringRef k = CFSTR(v);

SEC_CONST_DECL (kSecSafariAccessGroup, "com.apple.cfnetwork");
SEC_CONST_DECL (kSecSafariDefaultComment, "default");
SEC_CONST_DECL (kSecSafariPasswordsNotSaved, "Passwords not saved");
SEC_CONST_DECL (kSecSharedCredentialUrlScheme, "https://");
SEC_CONST_DECL (kSecSharedWebCredentialsService, "webcredentials");

#if !TARGET_OS_SIMULATOR
static SWCFlags
_SecAppDomainApprovalStatus(CFStringRef appID, CFStringRef fqdn, CFErrorRef *error)
{
    __block SWCFlags flags = kSWCFlags_None;
    OSStatus status;

    secnotice("swc", "Application %@ is requesting approval for %@", appID, fqdn);

    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    if (semaphore == NULL)
        return 0;

    status = SWCCheckService(kSecSharedWebCredentialsService, appID, fqdn, ^void (OSStatus inStatus, SWCFlags inFlags, CFDictionaryRef inDetails)
    {
        if (inStatus == 0) {
            flags = inFlags;
        } else {
            secerror("SWCCheckService failed with %d", (int)inStatus);
        }
        dispatch_semaphore_signal(semaphore);
    });

    if (status == 0) {
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    } else {
        secerror("SWCCheckService: failed to queue");
    }
    dispatch_release(semaphore);

    if (error) {
        if (!(flags & kSWCFlag_SiteApproved)) {
            SecError(errSecAuthFailed, error, CFSTR("\"%@\" failed to approve \"%@\""), fqdn, appID);
        } else if (flags & kSWCFlag_UserDenied) {
            SecError(errSecAuthFailed, error, CFSTR("User denied access to \"%@\" by \"%@\""), fqdn, appID);
        }
    }
    return flags;
}

static bool
_SecEntitlementContainsDomainForService(CFArrayRef domains, CFStringRef domain, CFStringRef service)
{
    bool result = false;
    CFIndex idx, count = (domains) ? CFArrayGetCount(domains) : (CFIndex) 0;
    if (!count || !domain || !service) {
        return result;
    }
    for (idx=0; idx < count; idx++) {
        CFStringRef str = (CFStringRef) CFArrayGetValueAtIndex(domains, idx);
        if (str && CFStringHasPrefix(str, kSecSharedWebCredentialsService)) {
            CFIndex prefix_len = CFStringGetLength(kSecSharedWebCredentialsService)+1;
            CFIndex substr_len = CFStringGetLength(str) - prefix_len;
            CFRange range = { prefix_len, substr_len };
            CFStringRef substr = CFStringCreateWithSubstring(kCFAllocatorDefault, str, range);
            if (substr && CFEqual(substr, domain)) {
                result = true;
            }
            CFReleaseSafe(substr);
            if (result) {
                break;
            }
        }
    }
    return result;
}
#endif /* !TARGET_OS_SIMULATOR */

static bool
_SecAddNegativeWebCredential(SecurityClient *client, CFStringRef fqdn, CFStringRef appID, bool forSafari)
{
#if !TARGET_OS_SIMULATOR
    bool result = false;
    if (!fqdn) { return result; }

    // update our database
    CFRetainSafe(appID);
    CFRetainSafe(fqdn);
    if (0 == SWCSetServiceFlags(kSecSharedWebCredentialsService, appID, fqdn, kSWCFlag_ExternalMask, kSWCFlag_UserDenied,
        ^void(OSStatus inStatus, SWCFlags inNewFlags){
            CFReleaseSafe(appID);
            CFReleaseSafe(fqdn);
        }))
    {
        result = true;
    }
    else // didn't queue the block
    {
        CFReleaseSafe(appID);
        CFReleaseSafe(fqdn);
    }

    if (!forSafari) { return result; }

    // below this point: create a negative Safari web credential item

    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!attrs) { return result; }

    CFErrorRef error = NULL;
    CFStringRef accessGroup = CFSTR("*");
    SecurityClient swcclient = {
        .task = NULL,
        .accessGroups =  CFArrayCreate(kCFAllocatorDefault, (const void **)&accessGroup, 1, &kCFTypeArrayCallBacks),
        .allowSystemKeychain = false,
        .allowSyncBubbleKeychain = false,
        .isNetworkExtension = false,
        .musr = client->musr,
    };

    CFDictionaryAddValue(attrs, kSecClass, kSecClassInternetPassword);
    CFDictionaryAddValue(attrs, kSecAttrAccessGroup, kSecSafariAccessGroup);
    CFDictionaryAddValue(attrs, kSecAttrAuthenticationType, kSecAttrAuthenticationTypeHTMLForm);
    CFDictionaryAddValue(attrs, kSecAttrProtocol, kSecAttrProtocolHTTPS);
    CFDictionaryAddValue(attrs, kSecAttrServer, fqdn);
    CFDictionaryAddValue(attrs, kSecAttrSynchronizable, kCFBooleanTrue);

    (void)_SecItemDelete(attrs, &swcclient, &error);
    CFReleaseNull(error);

    CFDictionaryAddValue(attrs, kSecAttrAccount, kSecSafariPasswordsNotSaved);
    CFDictionaryAddValue(attrs, kSecAttrComment, kSecSafariDefaultComment);

    CFStringRef label = CFStringCreateWithFormat(kCFAllocatorDefault,
                                                 NULL, CFSTR("%@ (%@)"), fqdn, kSecSafariPasswordsNotSaved);
    if (label) {
        CFDictionaryAddValue(attrs, kSecAttrLabel, label);
        CFReleaseSafe(label);
    }

    UInt8 space = ' ';
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, &space, 1);
    if (data) {
        CFDictionarySetValue(attrs, kSecValueData, data);
        CFReleaseSafe(data);
    }

    CFTypeRef addResult = NULL;
    result = _SecItemAdd(attrs, &swcclient, &addResult, &error);

    CFReleaseSafe(addResult);
    CFReleaseSafe(error);
    CFReleaseSafe(attrs);
    CFReleaseSafe(swcclient.accessGroups);

    return result;
#else
    return true;
#endif
}

/* Specialized version of SecItemAdd for shared web credentials */
bool
_SecAddSharedWebCredential(CFDictionaryRef attributes,
                           SecurityClient *client,
                           const audit_token_t *clientAuditToken,
                           CFStringRef appID,
                           CFArrayRef domains,
                           CFTypeRef *result,
                           CFErrorRef *error)
{

    SecurityClient swcclient = {};

    CFStringRef fqdn = CFRetainSafe(CFDictionaryGetValue(attributes, kSecAttrServer));
    CFStringRef account = CFDictionaryGetValue(attributes, kSecAttrAccount);
#if TARGET_OS_IOS && !TARGET_OS_BRIDGE
    CFStringRef password = CFDictionaryGetValue(attributes, kSecSharedPassword);
#else
    CFStringRef password = CFDictionaryGetValue(attributes, CFSTR("spwd"));
#endif
    CFStringRef accessGroup = CFSTR("*");
    CFMutableDictionaryRef query = NULL, attrs = NULL;
    SInt32 port = -1;
    bool ok = false;

    // check autofill enabled status
    if (!swca_autofill_enabled(clientAuditToken)) {
        SecError(errSecBadReq, error, CFSTR("Autofill is not enabled in Safari settings"));
        goto cleanup;
    }

    // parse fqdn with CFURL here, since it could be specified as domain:port
    if (fqdn) {
        CFStringRef urlStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@%@"), kSecSharedCredentialUrlScheme, fqdn);
        if (urlStr) {
            CFURLRef url = CFURLCreateWithString(kCFAllocatorDefault, urlStr, nil);
            if (url) {
                CFStringRef hostname = CFURLCopyHostName(url);
                if (hostname) {
                    CFReleaseSafe(fqdn);
                    fqdn = hostname;
                    port = CFURLGetPortNumber(url);
                }
                CFReleaseSafe(url);
            }
            CFReleaseSafe(urlStr);
        }
    }

    if (!account) {
        SecError(errSecParam, error, CFSTR("No account provided"));
        goto cleanup;
    }
    if (!fqdn) {
        SecError(errSecParam, error, CFSTR("No domain provided"));
        goto cleanup;
    }

#if TARGET_OS_SIMULATOR
    secerror("app/site association entitlements not checked in Simulator");
#else
    OSStatus status = errSecMissingEntitlement;
    // validate that fqdn is part of caller's shared credential domains entitlement
    if (!appID) {
        SecError(status, error, CFSTR("Missing application-identifier entitlement"));
        goto cleanup;
    }
    if (_SecEntitlementContainsDomainForService(domains, fqdn, kSecSharedWebCredentialsService)) {
        status = errSecSuccess;
    }
    if (errSecSuccess != status) {
        CFStringRef msg = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                   CFSTR("%@ not found in %@ entitlement"), fqdn, kSecEntitlementAssociatedDomains);
        if (!msg) {
            msg = CFRetain(CFSTR("Requested domain not found in entitlement"));
        }
        SecError(status, error, CFSTR("%@"), msg);
        CFReleaseSafe(msg);
        goto cleanup;
    }
#endif

#if TARGET_OS_SIMULATOR
    secerror("Ignoring app/site approval state in the Simulator.");
#else
    // get approval status for this app/domain pair
    SWCFlags flags = _SecAppDomainApprovalStatus(appID, fqdn, error);
    if (!(flags & kSWCFlag_SiteApproved)) {
        goto cleanup;
    }
#endif

    // give ourselves access to see matching items for kSecSafariAccessGroup
    swcclient.task = NULL;
    swcclient.accessGroups =  CFArrayCreate(kCFAllocatorDefault, (const void **)&accessGroup, 1, &kCFTypeArrayCallBacks);
    swcclient.allowSystemKeychain = false;
    swcclient.musr = client->musr;
    swcclient.allowSystemKeychain = false;
    swcclient.allowSyncBubbleKeychain = false;
    swcclient.isNetworkExtension = false;


    // create lookup query
    query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!query) {
        SecError(errSecAllocate, error, CFSTR("Unable to create query dictionary"));
        goto cleanup;
    }
    CFDictionaryAddValue(query, kSecClass, kSecClassInternetPassword);
    CFDictionaryAddValue(query, kSecAttrAccessGroup, kSecSafariAccessGroup);
    CFDictionaryAddValue(query, kSecAttrAuthenticationType, kSecAttrAuthenticationTypeHTMLForm);
    CFDictionaryAddValue(query, kSecAttrServer, fqdn);
    CFDictionaryAddValue(query, kSecAttrSynchronizable, kCFBooleanTrue);

    // check for presence of Safari's negative entry ('passwords not saved')
    CFDictionarySetValue(query, kSecAttrAccount, kSecSafariPasswordsNotSaved);
    ok = _SecItemCopyMatching(query, &swcclient, result, error);
    if(result) CFReleaseNull(*result);
    if (error) CFReleaseNull(*error);
    if (ok) {
        SecError(errSecDuplicateItem, error, CFSTR("Item already exists for this server"));
        goto cleanup;
    }

    // now use the provided account (and optional port number, if one was present)
    CFDictionarySetValue(query, kSecAttrAccount, account);
    if (port < -1 || port > 0) {
        SInt16 portValueShort = (port & 0xFFFF);
        CFNumberRef portNumber = CFNumberCreate(NULL, kCFNumberSInt16Type, &portValueShort);
        CFDictionaryAddValue(query, kSecAttrPort, portNumber);
        CFReleaseSafe(portNumber);
    }

    // look up existing password
    CFDictionaryAddValue(query, kSecReturnData, kCFBooleanTrue);
    bool matched = _SecItemCopyMatching(query, &swcclient, result, error);
    CFDictionaryRemoveValue(query, kSecReturnData);
    if (matched) {
        // found it, so this becomes either an "update password" or "delete password" operation
        bool update = (password != NULL);
        if (update) {
            attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFDataRef credential = CFStringCreateExternalRepresentation(kCFAllocatorDefault, password, kCFStringEncodingUTF8, 0);
            CFDictionaryAddValue(attrs, kSecValueData, credential);
            bool samePassword = result && *result && CFEqual(*result, credential);
            CFReleaseSafe(credential);
            CFDictionaryAddValue(attrs, kSecAttrComment, kSecSafariDefaultComment);
            
            ok = samePassword || swca_confirm_operation(swca_update_request_id, clientAuditToken, query, error,
                ^void (CFStringRef confirm_fqdn) {
                    _SecAddNegativeWebCredential(client, confirm_fqdn, appID, false);
                });
            if (ok) {
                ok = _SecItemUpdate(query, attrs, &swcclient, error);
            }
        }
        else {
            // confirm the delete
            // (per rdar://16676288 we always prompt, even if there was prior user approval)
            ok = /*approved ||*/ swca_confirm_operation(swca_delete_request_id, clientAuditToken, query, error,
                ^void (CFStringRef confirm_fqdn) {
                    _SecAddNegativeWebCredential(client, confirm_fqdn, appID, false);
                });
            if (ok) {
                ok = _SecItemDelete(query, &swcclient, error);
            }
        }
        
        if(result) CFReleaseNull(*result);
        if(error) CFReleaseNull(*error);

        goto cleanup;
    }
    if (result) CFReleaseNull(*result);
    if (error) CFReleaseNull(*error);

    // password does not exist, so prepare to add it
    if (!password) {
        // a NULL password value removes the existing credential. Since we didn't find it, this is a no-op.
        ok = true;
        goto cleanup;
    }
    else {
        CFStringRef label = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@ (%@)"), fqdn, account);
        if (label) {
            CFDictionaryAddValue(query, kSecAttrLabel, label);
            CFReleaseSafe(label);
        }
        // NOTE: we always expect to use HTTPS for web forms.
        CFDictionaryAddValue(query, kSecAttrProtocol, kSecAttrProtocolHTTPS);

        CFDataRef credential = CFStringCreateExternalRepresentation(kCFAllocatorDefault, password, kCFStringEncodingUTF8, 0);
        CFDictionarySetValue(query, kSecValueData, credential);
        CFReleaseSafe(credential);
        CFDictionarySetValue(query, kSecAttrComment, kSecSafariDefaultComment);

        CFReleaseSafe(swcclient.accessGroups);
        swcclient.accessGroups = CFArrayCreate(kCFAllocatorDefault, (const void **)&kSecSafariAccessGroup, 1, &kCFTypeArrayCallBacks);

        // mark the item as created by this function
        const int32_t creator_value = 'swca';
        CFNumberRef creator = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &creator_value);
        if (creator) {
            CFDictionarySetValue(query, kSecAttrCreator, creator);
            CFReleaseSafe(creator);
        }

        // confirm the add
        ok = swca_confirm_operation(swca_add_request_id, clientAuditToken, query, error, ^void (CFStringRef confirm_fqdn) {
            _SecAddNegativeWebCredential(client, confirm_fqdn, appID, false);
        });
    }
    if (ok) {
        ok = _SecItemAdd(query, &swcclient, result, error);
    }

cleanup:
    CFReleaseSafe(attrs);
    CFReleaseSafe(query);
    CFReleaseSafe(swcclient.accessGroups);
    CFReleaseSafe(fqdn);
    return ok;
}

/* Specialized version of SecItemCopyMatching for shared web credentials */
bool
_SecCopySharedWebCredential(CFDictionaryRef query,
			    SecurityClient *client,
			    const audit_token_t *clientAuditToken,
			    CFStringRef appID,
			    CFArrayRef domains,
			    CFTypeRef *result,
			    CFErrorRef *error)
{
    CFMutableArrayRef credentials = NULL;
    CFMutableArrayRef foundItems = NULL;
    CFMutableArrayRef fqdns = NULL;
    CFStringRef fqdn = NULL;
    CFStringRef account = NULL;
    SInt32 port = -1;
    bool ok = false;

    require_quiet(result, cleanup);
    credentials = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    foundItems = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    fqdns = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    // give ourselves access to see matching items for kSecSafariAccessGroup
    CFStringRef accessGroup = CFSTR("*");
    SecurityClient swcclient = {
        .task = NULL,
        .accessGroups =  CFArrayCreate(kCFAllocatorDefault, (const void **)&accessGroup, 1, &kCFTypeArrayCallBacks),
        .allowSystemKeychain = false,
        .allowSyncBubbleKeychain = false,
        .isNetworkExtension = false,
        .musr = client->musr,
    };

    // On input, the query dictionary contains optional fqdn and account entries.
    fqdn = CFDictionaryGetValue(query, kSecAttrServer);
    account = CFDictionaryGetValue(query, kSecAttrAccount);

    // Check autofill enabled status
    if (!swca_autofill_enabled(clientAuditToken)) {
        SecError(errSecBadReq, error, CFSTR("Autofill is not enabled in Safari settings"));
        goto cleanup;
    }

    // Check fqdn; if NULL, add domains from caller's entitlement.
    if (fqdn) {
        CFArrayAppendValue(fqdns, fqdn);
    }
    else if (domains) {
        CFIndex idx, count = CFArrayGetCount(domains);
        for (idx=0; idx < count; idx++) {
            CFStringRef str = (CFStringRef) CFArrayGetValueAtIndex(domains, idx);
            // Parse the entry for our service label prefix
            if (str && CFStringHasPrefix(str, kSecSharedWebCredentialsService)) {
                CFIndex prefix_len = CFStringGetLength(kSecSharedWebCredentialsService)+1;
                CFIndex substr_len = CFStringGetLength(str) - prefix_len;
                CFRange range = { prefix_len, substr_len };
                fqdn = CFStringCreateWithSubstring(kCFAllocatorDefault, str, range);
                if (fqdn) {
                    CFArrayAppendValue(fqdns, fqdn);
                    CFRelease(fqdn);
                }
            }
        }
    }
    CFIndex count, idx;
    
    count = CFArrayGetCount(fqdns);
    if (count < 1) {
        SecError(errSecParam, error, CFSTR("No domain provided"));
        goto cleanup;
    }

    // Aggregate search results for each domain
    for (idx = 0; idx < count; idx++) {
        CFMutableArrayRef items = NULL;
        CFMutableDictionaryRef attrs = NULL;
        fqdn = (CFStringRef) CFArrayGetValueAtIndex(fqdns, idx);
        CFRetainSafe(fqdn);
        port = -1;

        // Parse the fqdn for a possible port specifier.
        if (fqdn) {
            CFStringRef urlStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@%@"), kSecSharedCredentialUrlScheme, fqdn);
            if (urlStr) {
                CFURLRef url = CFURLCreateWithString(kCFAllocatorDefault, urlStr, nil);
                if (url) {
                    CFStringRef hostname = CFURLCopyHostName(url);
                    if (hostname) {
                        CFReleaseSafe(fqdn);
                        fqdn = hostname;
                        port = CFURLGetPortNumber(url);
                    }
                    CFReleaseSafe(url);
                }
                CFReleaseSafe(urlStr);
            }
        }

#if TARGET_OS_SIMULATOR
        secerror("app/site association entitlements not checked in Simulator");
#else
	    OSStatus status = errSecMissingEntitlement;
        if (!appID) {
            SecError(status, error, CFSTR("Missing application-identifier entitlement"));
            CFReleaseSafe(fqdn);
            goto cleanup;
        }
        // validate that fqdn is part of caller's entitlement
        if (_SecEntitlementContainsDomainForService(domains, fqdn, kSecSharedWebCredentialsService)) {
            status = errSecSuccess;
        }
        if (errSecSuccess != status) {
            CFStringRef msg = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                CFSTR("%@ not found in %@ entitlement"), fqdn, kSecEntitlementAssociatedDomains);
            if (!msg) {
                msg = CFRetain(CFSTR("Requested domain not found in entitlement"));
            }
            SecError(status, error, CFSTR("%@"), msg);
            CFReleaseSafe(msg);
            CFReleaseSafe(fqdn);
            goto cleanup;
        }
#endif

        attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (!attrs) {
            SecError(errSecAllocate, error, CFSTR("Unable to create query dictionary"));
            CFReleaseSafe(fqdn);
            goto cleanup;
        }
        CFDictionaryAddValue(attrs, kSecClass, kSecClassInternetPassword);
        CFDictionaryAddValue(attrs, kSecAttrAccessGroup, kSecSafariAccessGroup);
        CFDictionaryAddValue(attrs, kSecAttrProtocol, kSecAttrProtocolHTTPS);
        CFDictionaryAddValue(attrs, kSecAttrAuthenticationType, kSecAttrAuthenticationTypeHTMLForm);
        CFDictionaryAddValue(attrs, kSecAttrServer, fqdn);
        if (account) {
            CFDictionaryAddValue(attrs, kSecAttrAccount, account);
        }
        if (port < -1 || port > 0) {
            SInt16 portValueShort = (port & 0xFFFF);
            CFNumberRef portNumber = CFNumberCreate(NULL, kCFNumberSInt16Type, &portValueShort);
            CFDictionaryAddValue(attrs, kSecAttrPort, portNumber);
            CFReleaseSafe(portNumber);
        }
        CFDictionaryAddValue(attrs, kSecAttrSynchronizable, kCFBooleanTrue);
        CFDictionaryAddValue(attrs, kSecMatchLimit, kSecMatchLimitAll);
        CFDictionaryAddValue(attrs, kSecReturnAttributes, kCFBooleanTrue);
        CFDictionaryAddValue(attrs, kSecReturnData, kCFBooleanTrue);

        ok = _SecItemCopyMatching(attrs, &swcclient, (CFTypeRef*)&items, error);
        if (count > 1) {
            // ignore interim error since we have multiple domains to search
            CFReleaseNull(*error);
        }
        if (ok && items && CFGetTypeID(items) == CFArrayGetTypeID()) {
#if TARGET_OS_SIMULATOR
            secerror("Ignoring app/site approval state in the Simulator.");
            bool approved = true;
#else
            // get approval status for this app/domain pair
            SWCFlags flags = _SecAppDomainApprovalStatus(appID, fqdn, error);
            if (count > 1) {
                // ignore interim error since we have multiple domains to check
                CFReleaseNull(*error);
            }
            bool approved = (flags & kSWCFlag_SiteApproved);
#endif
            if (approved) {
                CFArrayAppendArray(foundItems, items, CFRangeMake(0, CFArrayGetCount(items)));
            }
        }
        CFReleaseSafe(items);
        CFReleaseSafe(attrs);
        CFReleaseSafe(fqdn);
    }

//  If matching credentials are found, the credentials provided to the completionHandler
//  will be a CFArrayRef containing CFDictionaryRef entries. Each dictionary entry will
//  contain the following pairs (see Security/SecItem.h):
//  key: kSecAttrServer     value: CFStringRef (the website)
//  key: kSecAttrAccount    value: CFStringRef (the account)
//  key: kSecSharedPassword value: CFStringRef (the password)
//  Optional keys:
//  key: kSecAttrPort       value: CFNumberRef (the port number, if non-standard for https)

    count = CFArrayGetCount(foundItems);
    for (idx = 0; idx < count; idx++) {
        CFDictionaryRef dict = (CFDictionaryRef) CFArrayGetValueAtIndex(foundItems, idx);
        CFMutableDictionaryRef newdict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (newdict && dict && CFGetTypeID(dict) == CFDictionaryGetTypeID()) {
            CFStringRef srvr = CFDictionaryGetValue(dict, kSecAttrServer);
            CFStringRef acct = CFDictionaryGetValue(dict, kSecAttrAccount);
            CFNumberRef pnum = CFDictionaryGetValue(dict, kSecAttrPort);
            CFStringRef icmt = CFDictionaryGetValue(dict, kSecAttrComment);
            CFDataRef data = CFDictionaryGetValue(dict, kSecValueData);
            if (srvr) {
                CFDictionaryAddValue(newdict, kSecAttrServer, srvr);
            }
            if (acct) {
                CFDictionaryAddValue(newdict, kSecAttrAccount, acct);
            }
            if (pnum) {
                SInt16 pval = -1;
                if (CFNumberGetValue(pnum, kCFNumberSInt16Type, &pval) &&
                    (pval < -1 || pval > 0)) {
                    CFDictionaryAddValue(newdict, kSecAttrPort, pnum);
                }
            }
            if (data) {
                CFStringRef password = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, data, kCFStringEncodingUTF8);
                if (password) {
                #if TARGET_OS_IPHONE && !TARGET_OS_WATCH && !TARGET_OS_TV
                    CFDictionaryAddValue(newdict, kSecSharedPassword, password);
                #else
                    CFDictionaryAddValue(newdict, CFSTR("spwd"), password);
                #endif
                    CFReleaseSafe(password);
                }
            }

            if (acct && CFEqual(acct, kSecSafariPasswordsNotSaved)) {
                // Do not add to credentials list!
                secwarning("copySWC: Skipping \"%@\" item", kSecSafariPasswordsNotSaved);
            } else if (icmt && CFEqual(icmt, kSecSafariDefaultComment)) {
                CFArrayInsertValueAtIndex(credentials, 0, newdict);
            } else {
                CFArrayAppendValue(credentials, newdict);
            }
        }
        CFReleaseSafe(newdict);
    }

    count = CFArrayGetCount(credentials);
    if (count) {
        ok = false;
        // create a new array of dictionaries (without the actual password) for picker UI
        CFMutableArrayRef items = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        for (idx = 0; idx < count; idx++) {
            CFDictionaryRef dict = (CFDictionaryRef) CFArrayGetValueAtIndex(credentials, idx);
            CFMutableDictionaryRef newdict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, dict);
        #if TARGET_OS_IPHONE && !TARGET_OS_WATCH && !TARGET_OS_TV
            CFDictionaryRemoveValue(newdict, kSecSharedPassword);
        #else
            CFDictionaryRemoveValue(newdict, CFSTR("spwd"));
        #endif
            CFArrayAppendValue(items, newdict);
            CFReleaseSafe(newdict);
        }

        // prompt user to select one of the dictionary items
        CFDictionaryRef selected = swca_copy_selected_dictionary(swca_select_request_id,
                                                                 clientAuditToken, items, error);
        if (selected) {
            // find the matching item in our credentials array
            CFStringRef srvr = CFDictionaryGetValue(selected, kSecAttrServer);
            CFStringRef acct = CFDictionaryGetValue(selected, kSecAttrAccount);
            CFNumberRef pnum = CFDictionaryGetValue(selected, kSecAttrPort);
            for (idx = 0; idx < count; idx++) {
                CFDictionaryRef dict = (CFDictionaryRef) CFArrayGetValueAtIndex(credentials, idx);
                CFStringRef srvr1 = CFDictionaryGetValue(dict, kSecAttrServer);
                CFStringRef acct1 = CFDictionaryGetValue(dict, kSecAttrAccount);
                CFNumberRef pnum1 = CFDictionaryGetValue(dict, kSecAttrPort);

                if (!srvr || !srvr1 || !CFEqual(srvr, srvr1)) continue;
                if (!acct || !acct1 || !CFEqual(acct, acct1)) continue;
                if ((pnum && pnum1) && !CFEqual(pnum, pnum1)) continue;

                // we have a match!
                CFReleaseSafe(selected);
                CFRetainSafe(dict);
                selected = dict;
                ok = true;
                break;
            }
        }
        CFReleaseSafe(items);
        CFArrayRemoveAllValues(credentials);
        if (selected && ok) {
#if TARGET_OS_IOS && !TARGET_OS_BRIDGE && !TARGET_OS_SIMULATOR
            fqdn = CFDictionaryGetValue(selected, kSecAttrServer);
#endif
            CFArrayAppendValue(credentials, selected);
        }

        if (ok) {
#if TARGET_OS_IOS && !TARGET_OS_BRIDGE && !TARGET_OS_SIMULATOR
            // register confirmation with database
            CFRetainSafe(appID);
            CFRetainSafe(fqdn);
            if (0 != SWCSetServiceFlags(kSecSharedWebCredentialsService,
                appID, fqdn, kSWCFlag_ExternalMask, kSWCFlag_UserApproved,
                ^void(OSStatus inStatus, SWCFlags inNewFlags){
                    CFReleaseSafe(appID);
                    CFReleaseSafe(fqdn);
                }))
            {
                 // we didn't queue the block
                CFReleaseSafe(appID);
                CFReleaseSafe(fqdn);
            }
#endif
        }
        CFReleaseSafe(selected);
    }
    else if (NULL == *error) {
        // found no items, and we haven't already filled in the error
        SecError(errSecItemNotFound, error, CFSTR("no matching items found"));
    }

cleanup:
    if (!ok) {
        CFArrayRemoveAllValues(credentials);
        CFReleaseNull(credentials);
    }
    CFReleaseSafe(foundItems);
    *result = credentials;
    CFReleaseSafe(swcclient.accessGroups);
    CFReleaseSafe(fqdns);

    return ok;
}

#endif /* SHAREDWEBCREDENTIALS */


// MARK: -
// MARK: Keychain backup

CF_RETURNS_RETAINED CFDataRef
_SecServerKeychainCreateBackup(SecurityClient *client, CFDataRef keybag, CFDataRef passcode, bool emcs, CFErrorRef *error) {
    __block CFDataRef backup;
    kc_with_dbt(false, error, ^bool (SecDbConnectionRef dbt) {

        LKABackupReportStart(!!keybag, !!passcode, emcs);

        return kc_transaction_type(dbt, kSecDbNormalTransactionType, error, ^bool{
            secnotice("SecServerKeychainCreateBackup", "Performing backup from %s keybag%s", keybag ? "provided" : "device", emcs ? ", EMCS mode" : "");

            if (keybag == NULL && passcode == NULL) {
#if USE_KEYSTORE
                backup = SecServerExportBackupableKeychain(dbt, client, KEYBAG_DEVICE, backup_keybag_handle, error);
#else /* !USE_KEYSTORE */
                (void)client;
                SecError(errSecParam, error, CFSTR("Why are you doing this?"));
                backup = NULL;
#endif /* USE_KEYSTORE */
            } else {
                backup = SecServerKeychainCreateBackup(dbt, client, keybag, passcode, emcs, error);
            }
            return (backup != NULL);
        });
    });

    secnotice("SecServerKeychainCreateBackup", "Backup result: %s (%@)", backup ? "success" : "fail", error ? *error : NULL);
    LKABackupReportEnd(!!backup, error ? *error : NULL);

    return backup;
}

bool
_SecServerKeychainRestore(CFDataRef backup, SecurityClient *client, CFDataRef keybag, CFDataRef passcode, CFErrorRef *error) {
    if (backup == NULL || keybag == NULL)
        return SecError(errSecParam, error, CFSTR("backup or keybag missing"));

    __block bool ok = true;
    ok &= kc_with_dbt(true, error, ^bool (SecDbConnectionRef dbconn) {
        return SecServerKeychainRestore(dbconn, client, backup, keybag, passcode, error);
    });

    if (ok) {
        SecKeychainChanged();
    }

    return ok;
}

CFStringRef
_SecServerBackupCopyUUID(CFDataRef data, CFErrorRef *error)
{
    CFStringRef uuid = NULL;
    CFDictionaryRef backup;

    backup = CFPropertyListCreateWithData(kCFAllocatorDefault, data,
                                          kCFPropertyListImmutable, NULL,
                                          error);
    if (isDictionary(backup)) {
        uuid = SecServerBackupGetKeybagUUID(backup, error);
        if (uuid)
            CFRetain(uuid);
    }
    CFReleaseNull(backup);

    return uuid;
}



// MARK: -
// MARK: SecItemDataSource

#if SECUREOBJECTSYNC

// Make sure to call this before any writes to the keychain, so that we fire
// up the engines to monitor manifest changes.
SOSDataSourceFactoryRef SecItemDataSourceFactoryGetDefault(void) {
    return SecItemDataSourceFactoryGetShared(kc_dbhandle(NULL));
}

/* AUDIT[securityd]:
   args_in (ok) is a caller provided, CFDictionaryRef.
 */

CF_RETURNS_RETAINED CFArrayRef
_SecServerKeychainSyncUpdateMessage(CFDictionaryRef updates, CFErrorRef *error) {
    // This never fails, trust us!
    return SOSCCHandleUpdateMessage(updates);
}

//
// Truthiness in the cloud backup/restore support.
//

static CFDictionaryRef
_SecServerCopyTruthInTheCloud(CFDataRef keybag, CFDataRef password,
    CFDictionaryRef backup, CFErrorRef *error)
{
    SOSManifestRef mold = NULL, mnow = NULL, mdelete = NULL, madd = NULL;
    __block CFMutableDictionaryRef backup_new = NULL;
    keybag_handle_t bag_handle;
    if (!ks_open_keybag(keybag, password, &bag_handle, error))
        return backup_new;

    // We need to have a datasource singleton for protection domain
    // kSecAttrAccessibleWhenUnlocked and keep a single shared engine
    // instance around which we create in the datasource constructor as well.
    SOSDataSourceFactoryRef dsf = SecItemDataSourceFactoryGetDefault();
    SOSDataSourceRef ds = SOSDataSourceFactoryCreateDataSource(dsf, kSecAttrAccessibleWhenUnlocked, error);
    if (ds) {
        backup_new = backup ? CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, backup) : CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        mold = SOSCreateManifestWithBackup(backup, error);
        SOSEngineRef engine = SOSDataSourceGetSharedEngine(ds, error);
        mnow = SOSEngineCopyManifest(engine, NULL);
        if (!mnow) {
            mnow = SOSDataSourceCopyManifestWithViewNameSet(ds, SOSViewsGetV0ViewSet(), error);
        }
        if (!mnow) {
            CFReleaseNull(backup_new);
            secerror("failed to obtain manifest for keychain: %@", error ? *error : NULL);
        } else {
            SOSManifestDiff(mold, mnow, &mdelete, &madd, error);
        }

        // Delete everything from the new_backup that is no longer in the datasource according to the datasources manifest.
        SOSManifestForEach(mdelete, ^(CFDataRef digest_data, bool *stop) {
            CFStringRef deleted_item_key = CFDataCopyHexString(digest_data);
            CFDictionaryRemoveValue(backup_new, deleted_item_key);
            CFRelease(deleted_item_key);
        });

        CFMutableArrayRef changes = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        SOSDataSourceForEachObject(ds, NULL, madd, error, ^void(CFDataRef digest, SOSObjectRef object, bool *stop) {
            CFErrorRef localError = NULL;
            CFDataRef digest_data = NULL;
            CFTypeRef value = NULL;
            if (!object) {
                // Key in our manifest can't be found in db, remove it from our manifest
                SOSChangesAppendDelete(changes, digest);
            } else if (!(digest_data = SOSObjectCopyDigest(ds, object, &localError))
                || !(value = SOSObjectCopyBackup(ds, object, bag_handle, &localError))) {
                if (SecErrorGetOSStatus(localError) == errSecDecode) {
                    // Ignore decode errors, pretend the objects aren't there
                    CFRelease(localError);
                    // Object undecodable, remove it from our manifest
                    SOSChangesAppendDelete(changes, digest);
                } else {
                    // Stop iterating and propagate out all other errors.
                    *stop = true;
                    *error = localError;
                    CFReleaseNull(backup_new);
                }
            } else {
                // TODO: Should we skip tombstones here?
                CFStringRef key = CFDataCopyHexString(digest_data);
                CFDictionarySetValue(backup_new, key, value);
                CFReleaseSafe(key);
            }
            CFReleaseSafe(digest_data);
            CFReleaseSafe(value);
        }) || CFReleaseNull(backup_new);

        if (CFArrayGetCount(changes)) {
            if (!SOSEngineUpdateChanges(engine, kSOSDataSourceSOSTransaction, changes, error)) {
                CFReleaseNull(backup_new);
            }
        }
        CFReleaseSafe(changes);

        SOSDataSourceRelease(ds, error) || CFReleaseNull(backup_new);
    }

    CFReleaseSafe(mold);
    CFReleaseSafe(mnow);
    CFReleaseSafe(madd);
    CFReleaseSafe(mdelete);
    ks_close_keybag(bag_handle, error) || CFReleaseNull(backup_new);

    return backup_new;
}

static bool
_SecServerRestoreTruthInTheCloud(CFDataRef keybag, CFDataRef password, CFDictionaryRef backup_in, CFErrorRef *error) {
    __block bool ok = true;
    keybag_handle_t bag_handle;
    if (!ks_open_keybag(keybag, password, &bag_handle, error))
        return false;

    SOSManifestRef mbackup = SOSCreateManifestWithBackup(backup_in, error);
    if (mbackup) {
        SOSDataSourceFactoryRef dsf = SecItemDataSourceFactoryGetDefault();
        SOSDataSourceRef ds = SOSDataSourceFactoryCreateDataSource(dsf, kSecAttrAccessibleWhenUnlocked, error);
        ok &= ds && SOSDataSourceWith(ds, error, ^(SOSTransactionRef txn, bool *commit) {
            SOSManifestRef mnow = SOSDataSourceCopyManifestWithViewNameSet(ds, SOSViewsGetV0BackupViewSet(), error);
            SOSManifestRef mdelete = NULL, madd = NULL;
            SOSManifestDiff(mnow, mbackup, &mdelete, &madd, error);
            
            // Don't delete everything in datasource not in backup.
            
            // Add items from the backup
            SOSManifestForEach(madd, ^void(CFDataRef e, bool *stop) {
                CFDictionaryRef item = NULL;
                CFStringRef sha1 = CFDataCopyHexString(e);
                if (sha1) {
                    item = CFDictionaryGetValue(backup_in, sha1);
                    CFRelease(sha1);
                }
                if (item) {
                    CFErrorRef localError = NULL;
                    
                    if (!SOSObjectRestoreObject(ds, txn, bag_handle, item, &localError)) {
                        OSStatus status = SecErrorGetOSStatus(localError);
                        if (status == errSecDuplicateItem) {
                            // Log and ignore duplicate item errors during restore
                            secnotice("titc", "restore %@ not replacing existing item", item);
                        } else if (status == errSecDecode) {
                            // Log and ignore corrupted item errors during restore
                            secnotice("titc", "restore %@ skipping corrupted item %@", item, localError);
                        } else {
                            if (status == errSecInteractionNotAllowed)
                                *stop = true;
                            // Propagate the first other error upwards (causing the restore to fail).
                            secerror("restore %@ failed %@", item, localError);
                            ok = false;
                            if (error && !*error) {
                                *error = localError;
                                localError = NULL;
                            }
                        }
                        CFReleaseSafe(localError);
                    }
                }
            });
            ok &= SOSDataSourceRelease(ds, error);
            CFReleaseNull(mdelete);
            CFReleaseNull(madd);
            CFReleaseNull(mnow);
        });
        CFRelease(mbackup);
    }

    ok &= ks_close_keybag(bag_handle, error);

    return ok;
}


CF_RETURNS_RETAINED CFDictionaryRef
_SecServerBackupSyncable(CFDictionaryRef backup, CFDataRef keybag, CFDataRef password, CFErrorRef *error) {
    require_action_quiet(isData(keybag), errOut, SecError(errSecParam, error, CFSTR("keybag %@ not a data"), keybag));
    require_action_quiet(!backup || isDictionary(backup), errOut, SecError(errSecParam, error, CFSTR("backup %@ not a dictionary"), backup));
    require_action_quiet(!password || isData(password), errOut, SecError(errSecParam, error, CFSTR("password %@ not a data"), password));

    return _SecServerCopyTruthInTheCloud(keybag, password, backup, error);

errOut:
    return NULL;
}

bool
_SecServerRestoreSyncable(CFDictionaryRef backup, CFDataRef keybag, CFDataRef password, CFErrorRef *error) {
    bool ok;
    require_action_quiet(isData(keybag), errOut, ok = SecError(errSecParam, error, CFSTR("keybag %@ not a data"), keybag));
    require_action_quiet(isDictionary(backup), errOut, ok = SecError(errSecParam, error, CFSTR("backup %@ not a dictionary"), backup));
    if (password) {
        
        require_action_quiet(isData(password), errOut, ok = SecError(errSecParam, error, CFSTR("password not a data")));
    }

    ok = _SecServerRestoreTruthInTheCloud(keybag, password, backup, error);

errOut:
    return ok;
}
#endif /* SECUREOBJECTSYNC */

bool _SecServerRollKeysGlue(bool force, CFErrorRef *error) {
    return _SecServerRollKeys(force, NULL, error);
}


bool _SecServerRollKeys(bool force, SecurityClient *client, CFErrorRef *error) {
#if USE_KEYSTORE
    uint32_t keystore_generation_status = 0;
    if (aks_generation(KEYBAG_DEVICE, generation_noop, &keystore_generation_status))
        return false;
    uint32_t current_generation = keystore_generation_status & generation_current;

    return kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
        bool up_to_date = s3dl_dbt_keys_current(dbt, current_generation, NULL);

        if (force && !up_to_date) {
            up_to_date = s3dl_dbt_update_keys(dbt, client, error);
            if (up_to_date) {
                secerror("Completed roll keys.");
                up_to_date = s3dl_dbt_keys_current(dbt, current_generation, NULL);
            }
            if (!up_to_date)
                secerror("Failed to roll keys.");
        }
        return up_to_date;
    });
#else
    return true;
#endif
}

static bool
InitialSyncItems(CFMutableArrayRef items, bool limitToCurrent, CFStringRef agrp, CFStringRef svce, const SecDbClass *qclass, CFErrorRef *error)
{
    bool result = false;
    Query *q = NULL;

    q = query_create(qclass, NULL, NULL, error);
    require(q, fail);

    q->q_return_type = kSecReturnDataMask | kSecReturnAttributesMask;
    q->q_limit = kSecMatchUnlimited;
    q->q_keybag = KEYBAG_DEVICE;

    query_add_attribute(kSecAttrAccessGroup, agrp, q);
    query_add_attribute(kSecAttrSynchronizable, kCFBooleanTrue, q);
    query_add_attribute(kSecAttrTombstone, kCFBooleanFalse, q);
    if (svce)
        query_add_attribute(kSecAttrService, svce, q);

    result = kc_with_dbt(false, error, ^(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, error, ^{
            CFErrorRef error2 = NULL;

            SecDbItemSelect(q, dbt, &error2, NULL, ^bool(const SecDbAttr *attr) {
                return CFDictionaryGetValue(q->q_item, attr->name);
            }, NULL, NULL, ^(SecDbItemRef item, bool *stop) {
                CFErrorRef error3 = NULL;
                secinfo("InitialSyncItems", "Copy item");

                CFMutableDictionaryRef attrs = SecDbItemCopyPListWithMask(item, kSecDbSyncFlag, &error3);
                if (attrs) {
                    int match = true;
                    CFStringRef itemvwht = CFDictionaryGetValue(attrs, kSecAttrSyncViewHint);
                    /*
                     * Saying its a SOS viewhint is really not the right answer post Triangle
                     */
                    if (isString(itemvwht) && !SOSViewInSOSSystem(itemvwht)) {
                        match = false;
                    }
                    /*
                     * Here we encode how PCS stores identities so that we only copy the
                     * current identites for performance reasons.
                     */
                    if (limitToCurrent) {
                        enum { PCS_CURRENT_IDENTITY_OFFSET		= 0x10000 };
                        int32_t s32;

                        CFNumberRef type = CFDictionaryGetValue(attrs, kSecAttrType);
                        if (!isNumber(type)) {
                            // still allow this case since its not a service identity ??
                        } else if (!CFNumberGetValue(type, kCFNumberSInt32Type, &s32)) {
                            match = false;
                        } else if ((s32 & PCS_CURRENT_IDENTITY_OFFSET) == 0) {
                            match = false;
                        }
                    }
                    if (match) {
                        CFDictionaryAddValue(attrs, kSecClass, SecDbItemGetClass(item)->name);
                        CFArrayAppendValue(items, attrs);
                    }

                    CFReleaseNull(attrs);
                }
                CFReleaseNull(error3);
            });
            CFReleaseNull(error2);

            return (bool)true;
        });
    });

fail:
    if (q)
        query_destroy(q, NULL);
    return result;
}

CFArrayRef
_SecServerCopyInitialSyncCredentials(uint32_t flags, CFErrorRef *error)
{
    CFMutableArrayRef items = CFArrayCreateMutableForCFTypes(NULL);

    if (flags & SecServerInitialSyncCredentialFlagTLK) {
        require_action(InitialSyncItems(items, false, CFSTR("com.apple.security.ckks"), NULL, inet_class(), error), fail,
                       secerror("failed to collect CKKS-inet keys: %@", error ? *error : NULL));
    }
    if (flags & SecServerInitialSyncCredentialFlagPCS) {
        bool onlyCurrent = !(flags & SecServerInitialSyncCredentialFlagPCSNonCurrent);

        require_action(InitialSyncItems(items, false, CFSTR("com.apple.ProtectedCloudStorage"), NULL, genp_class(), error), fail,
                       secerror("failed to collect PCS-genp keys: %@", error ? *error : NULL));
        require_action(InitialSyncItems(items, onlyCurrent, CFSTR("com.apple.ProtectedCloudStorage"), NULL, inet_class(), error), fail,
                       secerror("failed to collect PCS-inet keys: %@", error ? *error : NULL));
    }
    if (flags & SecServerInitialSyncCredentialFlagBluetoothMigration) {
        require_action(InitialSyncItems(items, false, CFSTR("com.apple.nanoregistry.migration"), NULL, genp_class(), error), fail,
                       secerror("failed to collect com.apple.nanoregistry.migration-genp item: %@", error ? *error : NULL));
        require_action(InitialSyncItems(items, false, CFSTR("com.apple.nanoregistry.migration2"), NULL, genp_class(), error), fail,
                       secerror("failed to collect com.apple.nanoregistry.migration2-genp item: %@", error ? *error : NULL));
        require_action(InitialSyncItems(items, false, CFSTR("com.apple.bluetooth"), CFSTR("BluetoothLESync"), genp_class(), error), fail,
                       secerror("failed to collect com.apple.bluetooth-genp item: %@", error ? *error : NULL));

    }

fail:
    return items;
}

bool
_SecServerImportInitialSyncCredentials(CFArrayRef array, CFErrorRef *error)
{
    return kc_with_dbt(true, error, ^bool(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, error, ^bool(void){
            CFIndex n, count = CFArrayGetCount(array);

            secinfo("ImportInitialSyncItems", "Importing %d items", (int)count);

            for (n = 0; n < count; n++) {
                CFErrorRef cferror = NULL;

                CFDictionaryRef item = CFArrayGetValueAtIndex(array, n);
                if (!isDictionary(item))
                    continue;

                CFStringRef className = CFDictionaryGetValue(item, kSecClass);
                if (className == NULL) {
                    secinfo("ImportInitialSyncItems", "Item w/o class");
                    continue;
                }

                const SecDbClass *cls = kc_class_with_name(className);
                if (cls == NULL) {
                    secinfo("ImportInitialSyncItems", "Item with unknown class: %@", className);
                    continue;
                }

                SecDbItemRef dbi = SecDbItemCreateWithAttributes(NULL, cls, item, KEYBAG_DEVICE, &cferror);
                if (dbi == NULL) {
                    secinfo("ImportInitialSyncItems", "Item creation failed with: %@", cferror);
                    CFReleaseNull(cferror);
                    continue;
                }

                if (!SecDbItemSetSyncable(dbi, true, &cferror)) {
                    secinfo("ImportInitialSyncItems", "Failed to set sync=1: %@ for item %@", cferror, dbi);
                    CFReleaseNull(cferror);
                    CFReleaseNull(dbi);
                    continue;
                }

                if (!SecDbItemInsert(dbi, dbt, &cferror)) {
                    secinfo("ImportInitialSyncItems", "Item store failed with: %@: %@", cferror, dbi);
                    CFReleaseNull(cferror);
                }
                CFReleaseNull(dbi);
            }
            return true;
        });
    });
}



#if TARGET_OS_IOS

/*
 * Sync bubble migration code
 */

struct SyncBubbleRule {
    CFStringRef attribute;
    CFTypeRef value;
};

static bool
TransmogrifyItemsToSyncBubble(SecurityClient *client, uid_t uid,
                              bool onlyDelete,
                              bool copyToo,
                              const SecDbClass *qclass,
                              struct SyncBubbleRule *items, CFIndex nItems,
                              CFErrorRef *error)
{
    CFMutableDictionaryRef updateAttributes = NULL;
    CFDataRef syncBubbleView = NULL;
    CFDataRef activeUserView = NULL;
    bool res = false;
    Query *q = NULL;
    CFIndex n;

    syncBubbleView = SecMUSRCreateSyncBubbleUserUUID(uid);
    require(syncBubbleView, fail);

    activeUserView = SecMUSRCreateActiveUserUUID(uid);
    require(activeUserView, fail);


    if ((onlyDelete && !copyToo) || !onlyDelete) {

        /*
         * Clean out items first
         */

        secnotice("syncbubble", "cleaning out old items");

        q = query_create(qclass, NULL, NULL, error);
        require(q, fail);

        q->q_limit = kSecMatchUnlimited;
        q->q_keybag = device_keybag_handle;

        for (n = 0; n < nItems; n++) {
            query_add_attribute(items[n].attribute, items[n].value, q);
        }
        q->q_musrView = CFRetain(syncBubbleView);
        require(q->q_musrView, fail);

        kc_with_dbt(false, error, ^(SecDbConnectionRef dbt) {
            return kc_transaction(dbt, error, ^{
                return s3dl_query_delete(dbt, q, NULL, error);
            });
        });

        query_destroy(q, NULL);
        q = NULL;
    }
    

    if (onlyDelete || !copyToo) {
        secnotice("syncbubble", "skip migration of items");
    } else {
        /*
         * Copy over items from EMCS to sync bubble
         */

        secnotice("syncbubble", "migrating sync bubble items");

        q = query_create(qclass, NULL, NULL, error);
        require(q, fail);

        q->q_return_type = kSecReturnDataMask | kSecReturnAttributesMask;
        q->q_limit = kSecMatchUnlimited;
        q->q_keybag = device_keybag_handle; /* XXX change to session key bag when it exists */

        for (n = 0; n < nItems; n++) {
            query_add_or_attribute(items[n].attribute, items[n].value, q);
        }
        query_add_or_attribute(CFSTR("musr"), activeUserView, q);
        q->q_musrView = CFRetain(activeUserView);

        updateAttributes = CFDictionaryCreateMutableForCFTypes(NULL);
        require(updateAttributes, fail);

        CFDictionarySetValue(updateAttributes, CFSTR("musr"), syncBubbleView); /* XXX should use kSecAttrMultiUser */


        kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
            return kc_transaction(dbt, error, ^{
                CFErrorRef error2 = NULL;

                SecDbItemSelect(q, dbt, &error2, NULL, ^bool(const SecDbAttr *attr) {
                    return CFDictionaryGetValue(q->q_item, attr->name);
                }, NULL, NULL, ^(SecDbItemRef item, bool *stop) {
                    CFErrorRef error3 = NULL;
                    secinfo("syncbubble", "migrating item");

                    SecDbItemRef new_item = SecDbItemCopyWithUpdates(item, updateAttributes, NULL);
                    if (new_item == NULL)
                        return;

                    SecDbItemClearRowId(new_item, NULL);

                    if (!SecDbItemSetKeybag(new_item, device_keybag_handle, NULL)) {
                        CFRelease(new_item);
                        return;
                    }

                    if (!SecDbItemInsert(new_item, dbt, &error3)) {
                        secnotice("syncbubble", "migration failed with %@ for item %@", error3, new_item);
                    }
                    CFRelease(new_item);
                    CFReleaseNull(error3);
                });
                CFReleaseNull(error2);
                
                return (bool)true;
            });
        });
    }
    res = true;

fail:
    CFReleaseNull(syncBubbleView);
    CFReleaseNull(activeUserView);
    CFReleaseNull(updateAttributes);
    if (q)
        query_destroy(q, NULL);

    return res;
}

static struct SyncBubbleRule PCSItems[] = {
    {
        .attribute = CFSTR("agrp"),
        .value = CFSTR("com.apple.ProtectedCloudStorage"),
    }
};
static struct SyncBubbleRule NSURLSesssiond[] = {
    {
        .attribute = CFSTR("agrp"),
        .value = CFSTR("com.apple.nsurlsessiond"),
    }
};
static struct SyncBubbleRule AccountsdItems[] = {
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.AppleAccount.token"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.AppleAccount.password"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.AppleAccount.rpassword"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.idms.token"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.idms.continuation-key"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.CloudKit.token"),
    },
};

static struct SyncBubbleRule MobileMailItems[] = {
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.IMAP.password"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.SMTP.password"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.Exchange.password"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.Hotmail.password"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.Google.password"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.Google.oauth-token"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.Google.oath-refresh-token"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.Yahoo.password"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.Yahoo.oauth-token"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.Yahoo.oauth-token-nosync"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.Yahoo.oath-refresh-token"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.IMAPNotes.password"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.IMAPMail.password"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.126.password"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.163.password"),
    },
    {
        .attribute = CFSTR("svce"),
        .value = CFSTR("com.apple.account.aol.password"),
    },
};

static bool
ArrayContains(CFArrayRef array, CFStringRef service)
{
    return CFArrayContainsValue(array, CFRangeMake(0, CFArrayGetCount(array)), service);
}

bool
_SecServerTransmogrifyToSyncBubble(CFArrayRef services, uid_t uid, SecurityClient *client, CFErrorRef *error)
{
    bool copyCloudAuthToken = false;
    bool copyMobileMail = false;
    bool res = true;
    bool copyPCS = false;
    bool onlyDelete = false;
    bool copyNSURLSesssion = false;

    if (!client->inMultiUser)
        return false;

    secnotice("syncbubble", "migration for uid %d uid for services %@", (int)uid, services);

#if TARGET_OS_SIMULATOR
    // no delete in sim
#elif TARGET_OS_IOS
    if (uid != (uid_t)client->activeUser)
        onlyDelete = true;
#else
#error "no sync bubble on other platforms"
#endif

    /*
     * First select that services to copy/delete
     */

    if (ArrayContains(services, CFSTR("com.apple.bird.usermanager.sync"))
        || ArrayContains(services, CFSTR("com.apple.cloudphotod.sync"))
        || ArrayContains(services, CFSTR("com.apple.cloudphotod.syncstakeholder"))
        || ArrayContains(services, CFSTR("com.apple.cloudd.usermanager.sync")))
    {
        copyCloudAuthToken = true;
        copyPCS = true;
    }

    if (ArrayContains(services, CFSTR("com.apple.nsurlsessiond.usermanager.sync")))
    {
        copyCloudAuthToken = true;
        copyNSURLSesssion = true;
    }

    if (ArrayContains(services, CFSTR("com.apple.syncdefaultsd.usermanager.sync"))) {
        copyCloudAuthToken = true;
    }
    if (ArrayContains(services, CFSTR("com.apple.mailq.sync")) || ArrayContains(services, CFSTR("com.apple.mailq.sync.xpc"))) {
        copyCloudAuthToken = true;
        copyMobileMail = true;
        copyPCS = true;
    }

    /* 
     * The actually copy/delete the items selected
     */

    res = TransmogrifyItemsToSyncBubble(client, uid, onlyDelete, copyPCS, inet_class(), PCSItems, sizeof(PCSItems)/sizeof(PCSItems[0]), error);
    require(res, fail);
    res = TransmogrifyItemsToSyncBubble(client, uid, onlyDelete, copyPCS, genp_class(), PCSItems, sizeof(PCSItems)/sizeof(PCSItems[0]), error);
    require(res, fail);

    /* mail */
    res = TransmogrifyItemsToSyncBubble(client, uid, onlyDelete, copyMobileMail, genp_class(), MobileMailItems, sizeof(MobileMailItems)/sizeof(MobileMailItems[0]), error);
    require(res, fail);

    /* accountsd */
    res = TransmogrifyItemsToSyncBubble(client, uid, onlyDelete, copyCloudAuthToken, genp_class(), AccountsdItems, sizeof(AccountsdItems)/sizeof(AccountsdItems[0]), error);
    require(res, fail);

    /* nsurlsessiond */
    res = TransmogrifyItemsToSyncBubble(client, uid, onlyDelete, copyNSURLSesssion, inet_class(), NSURLSesssiond, sizeof(NSURLSesssiond)/sizeof(NSURLSesssiond[0]), error);
    require(res, fail);

fail:
    return res;
}

/*
 * Migrate from user keychain to system keychain when switching to edu mode
 */

bool
_SecServerTransmogrifyToSystemKeychain(SecurityClient *client, CFErrorRef *error)
{
    __block bool ok = true;

    /*
     * we are not in multi user yet, about to switch, otherwise we would
     * check that for client->inMultiuser here
     */

    kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, error, ^{
            CFDataRef systemUUID = SecMUSRGetSystemKeychainUUID();

            const SecDbSchema *newSchema = current_schema();
            SecDbClass const *const *kcClass;

            for (kcClass = newSchema->classes; *kcClass != NULL; kcClass++) {
                CFErrorRef localError = NULL;
                Query *q = NULL;

                if (!((*kcClass)->itemclass)) {
                    continue;
                }

                q = query_create(*kcClass, SecMUSRGetSingleUserKeychainUUID(), NULL, error);
                if (q == NULL)
                    continue;

                ok &= SecDbItemSelect(q, dbt, error, ^bool(const SecDbAttr *attr) {
                    return (attr->flags & kSecDbInFlag) != 0;
                }, ^bool(const SecDbAttr *attr) {
                    // No filtering please.
                    return false;
                }, ^bool(CFMutableStringRef sql, bool *needWhere) {
                    SecDbAppendWhereOrAnd(sql, needWhere);
                    CFStringAppendFormat(sql, NULL, CFSTR("musr = ?"));
                    return true;
                }, ^bool(sqlite3_stmt *stmt, int col) {
                    return SecDbBindObject(stmt, col++, SecMUSRGetSingleUserKeychainUUID(), error);
                }, ^(SecDbItemRef item, bool *stop) {
                    CFErrorRef itemError = NULL;

                    if (!SecDbItemSetValueWithName(item, kSecAttrMultiUser, systemUUID, &itemError)) {
                        secerror("item: %@ update musr to system failed: %@", item, itemError);
                        ok = false;
                        goto out;
                    }

                    if (!SecDbItemDoUpdate(item, item, dbt, &itemError, ^bool (const SecDbAttr *attr) {
                        return attr->kind == kSecDbRowIdAttr;
                    })) {
                        secerror("item: %@ insert during UPDATE: %@", item, itemError);
                        ok = false;
                        goto out;
                    }

                out:
                    SecErrorPropagate(itemError, error);
                });

                if (q)
                    query_destroy(q, &localError);

            }
            return (bool)true;
        });
    });

    return ok;
}

/*
 * Delete account from local usage
 */

bool
_SecServerDeleteMUSERViews(SecurityClient *client, uid_t uid, CFErrorRef *error)
{
    return kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
        CFDataRef musrView = NULL, syncBubbleView = NULL;
        bool ok = false;

        syncBubbleView = SecMUSRCreateSyncBubbleUserUUID(uid);
        require(syncBubbleView, fail);

        musrView = SecMUSRCreateActiveUserUUID(uid);
        require(musrView, fail);

        require(ok = SecServerDeleteAllForUser(dbt, syncBubbleView, false, error), fail);
        require(ok = SecServerDeleteAllForUser(dbt, musrView, false, error), fail);

    fail:
        CFReleaseNull(syncBubbleView);
        CFReleaseNull(musrView);
        return ok;
    });
}


#endif /* TARGET_OS_IOS */

CFArrayRef _SecItemCopyParentCertificates(CFDataRef normalizedIssuer, CFArrayRef accessGroups, CFErrorRef *error) {
    const void *keys[] = {
        kSecClass,
        kSecReturnData,
        kSecMatchLimit,
        kSecAttrSubject
    },
    *values[] = {
        kSecClassCertificate,
        kCFBooleanTrue,
        kSecMatchLimitAll,
        normalizedIssuer
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, 4,
                                               NULL, NULL);
    CFTypeRef results = NULL;
    SecurityClient client = {
        .task = NULL,
        .accessGroups = accessGroups,
        .allowSystemKeychain = true,
        .allowSyncBubbleKeychain = false,
        .isNetworkExtension = false,
    };

    (void)_SecItemCopyMatching(query, &client, &results, error);
    CFRelease(query);
    return results;
}

bool _SecItemCertificateExists(CFDataRef normalizedIssuer, CFDataRef serialNumber, CFArrayRef accessGroups, CFErrorRef *error) {
    const void *keys[] = {
        kSecClass,
        kSecMatchLimit,
        kSecAttrIssuer,
        kSecAttrSerialNumber
    },
    *values[] = {
        kSecClassCertificate,
        kSecMatchLimitOne,
        normalizedIssuer,
        serialNumber
    };
    SecurityClient client = {
        .task = NULL,
        .accessGroups = accessGroups,
        .allowSystemKeychain = true,
        .allowSyncBubbleKeychain = false,
        .isNetworkExtension = false,
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, 4, NULL, NULL);
    CFTypeRef results = NULL;
    bool ok = _SecItemCopyMatching(query, &client, &results, error);
    CFReleaseSafe(query);
    CFReleaseSafe(results);
    if (!ok) {
        return false;
    }
    return true;
}
