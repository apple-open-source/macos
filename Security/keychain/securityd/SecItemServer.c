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

#if (defined(TARGET_DARWINOS) && TARGET_DARWINOS) || (defined(SECURITYD_SYSTEM) && SECURITYD_SYSTEM)
#undef OCTAGON
#undef SECUREOBJECTSYNC
#undef SHAREDWEBCREDENTIALS
#endif

#include "keychain/securityd/SecItemServer.h"

#include <CoreFoundation/CFPriv.h>
#include <notify.h>
#include <os/lock_private.h>
#include "keychain/securityd/SecItemDataSource.h"
#include "keychain/securityd/SecItemDb.h"
#include "keychain/securityd/SecItemSchema.h"
#include <utilities/SecDb.h>
#include <utilities/SecDbInternal.h>
#import <utilities/SecCoreAnalytics.h>
#include "keychain/securityd/SecDbKeychainItem.h"
#include "keychain/securityd/SOSCloudCircleServer.h"
#include <Security/SecBasePriv.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemInternal.h>
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
#import "keychain/ot/Affordance_OTConstants.h"
#import "keychain/escrowrequest/EscrowRequestServerHelpers.h"

#if KCSHARING
#include "keychain/Sharing/KCSharingSupport.h"
#endif  // KCSHARING

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
#include <sys/time.h>
#include <unistd.h>
#endif

#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
#include <sys/stat.h>
#endif // KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER

#include <utilities/array_size.h>
#include <utilities/SecFileLocations.h>
#include <utilities/SecTrace.h>
#include <utilities/SecXPCError.h>
#include <utilities/sec_action.h>
#include <Security/SecuritydXPC.h>
#include "swcagent_client.h"
#include "SecPLWrappers.h"
#include "SecItemServer+SWC.h"
#include <ipc/server_entitlement_helpers.h>

#include <os/assumes.h>
#include <os/variant_private.h>


#include "Analytics/Clients/LocalKeychainAnalytics.h"

/* Changed the name of the keychain changed notification, for testing */
static const char *g_keychain_changed_notification = kSecServerKeychainChangedNotification;
static CFNumberRef lastRowIDHandled = NULL;
static CFErrorRef testError = NULL;
static CFDictionaryRef rowIDAndErrorDictionary = NULL; /* CFDictionaryRef ex. [1: errSecNotAvailable], [2 : errSecDecode]*/
#define  MAX_NUM_PERSISTENT_REF_ROWIDS    100

void SecItemServerSetKeychainChangedNotification(const char *notification_name)
{
    g_keychain_changed_notification = notification_name;
}

static os_unfair_lock sharedItemNotifierLock = OS_UNFAIR_LOCK_INIT;
static CFNotificationCenterRef sharedItemNotifier;

void SecServerSetSharedItemNotifier(CFNotificationCenterRef notifier) {
    os_unfair_lock_lock_scoped_guard(lock, &sharedItemNotifierLock);
    sharedItemNotifier = notifier;
}

static CFNotificationCenterRef SecServerGetSharedItemNotifier(void) {
    os_unfair_lock_lock_scoped_guard(lock, &sharedItemNotifierLock);
    return sharedItemNotifier ?: CFNotificationCenterGetDistributedCenter();
}

void SecSharedItemsChanged(void) {
    CFNotificationCenterPostNotificationWithOptions(SecServerGetSharedItemNotifier(), CFSTR(kSecServerSharedItemsChangedNotification), NULL, NULL, 0);
}

void SecKeychainChanged(void) {
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
    secnotice("upgr", "UID: %d  EUID: %d", getuid(), geteuid());
    CFReleaseSafe(localError);


    return ok;
}

#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
// Determine whether we've already transcrypted the database, stored in a bit in the DB user version.
static bool SecKeychainDbGetEduModeTranscrypted(SecDbConnectionRef dbt)
{
    int32_t userVersion = getDbUserVersion(dbt);
    bool done = (userVersion & KeychainDbUserVersion_Transcrypted) == KeychainDbUserVersion_Transcrypted;
    secnotice("edutranscrypted", "got: %{bool}d", done);
    return done;
}

// Set the bit in the DB user version, so we know we already transcrypted (or created a new DB).
static void SecKeychainDbSetEduModeTranscrypted(SecDbConnectionRef dbt)
{
    CFErrorRef localError = NULL;
    int32_t userVersion = getDbUserVersion(dbt);
    userVersion |= KeychainDbUserVersion_Transcrypted;

    if (!setDbUserVersion(userVersion, dbt, &localError)) {
        secnotice("edutranscrypted", "failed to set DB user version: %@", localError);
        CFReleaseNull(localError);
    }
}

static bool InternalTranscryptToSystemKeychainKeybag(SecDbConnectionRef dbt, SecurityClient *client, CFErrorRef *error) {
    return kc_transaction(dbt, error, ^{
        bool ok = true;

        CFDataRef systemUUID = SecMUSRGetSystemKeychainUUID();

        const SecDbSchema *schema = current_schema();

        for (SecDbClass const *const *kcClass = schema->classes; *kcClass != NULL; kcClass++) {
            Query *q = NULL;

            if (!((*kcClass)->itemclass)) {
                continue;
            }

            q = query_create(*kcClass, systemUUID, NULL, client, error);
            if (q == NULL) {
                secnotice("transcrypt", "could not create query for class %@: %@", (*kcClass)->name, *error);
                continue;
            }

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
                return SecDbBindObject(stmt, col++, systemUUID, error);
            }, ^(SecDbItemRef item, bool *stop) {
                secnotice("transcrypt", "handling item: " SECDBITEM_FMT, item);

                CFErrorRef localError = NULL;
                if (!SecDbItemSetKeybag(item, system_keychain_handle, &localError)) {
                    secnotice("transcrypt", "failed to set keybag, but continuing. Error: %@", localError);
                    CFReleaseNull(localError);
                    return;
                }
                if (!SecDbItemDoUpdate(item, item, dbt, &localError, ^bool (const SecDbAttr *attr) {
                    return attr->kind == kSecDbRowIdAttr;
                })) {
                    secnotice("transcrypt", "failed to update item, but continuing. Error: %@", localError);
                    CFReleaseNull(localError);
                }
            });

            query_destroy(q, NULL);
        }

        return ok;
    });
}
#endif // KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER

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
        SecCoreAnalyticsSendValue(CFSTR("com.apple.keychain.phase1.migrated-items-success"), itemsMigrated);
        SecCoreAnalyticsSendValue(CFSTR("com.apple.keychain.phase1.migrated-time-success"), duration);
    } else {
        SecCoreAnalyticsSendValue(CFSTR("com.apple.keychain.phase1.migrated-items-fail"), itemsMigrated);
        SecCoreAnalyticsSendValue(CFSTR("com.apple.keychain.phase1.migrated-time-fail"), duration);
    }
}

static void
measureUpgradePhase2(struct timeval *start, int64_t itemsMigrated)
{
    int64_t duration = measureDuration(start);

    SecCoreAnalyticsSendValue(CFSTR("com.apple.keychain.phase2.migrated-items"), itemsMigrated);
    SecCoreAnalyticsSendValue(CFSTR("com.apple.keychain.phase2.migrated-time"), duration);
}

static void
measureUpgradePhase3(struct timeval *start, int64_t itemsMigrated)
{
    int64_t duration = measureDuration(start);

    SecCoreAnalyticsSendValue(CFSTR("com.apple.keychain.phase3.add-uuid-persistentref-to-items"), itemsMigrated);
    SecCoreAnalyticsSendValue(CFSTR("com.apple.keychain.phase3.migrated-time"), duration);
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

static void bounceLKAReportKeychainUpgradeOutcomeWithError(int oldVersion, int newVersion, LKAKeychainUpgradeOutcome outcome, CFErrorRef error) {
    // Once we start upgrading system keychain DBs, we should change this. But we need LKA to use the correct keybag. See rdar://88044320
#if !(defined(SECURITYD_SYSTEM) && SECURITYD_SYSTEM)
    LKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, outcome, error);
#endif
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
    SecDbQueryRef query = NULL;
    CFMutableStringRef sql = NULL;
    CFArrayRef classNamesToMigrateAsArray = NULL;
    SecDbClass* renamedOldClass = NULL;
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    __block int64_t itemsMigrated = 0;
    struct timeval start;

    gettimeofday(&start, NULL);
#endif

    CFCharacterSetRef asciiDigits = CFCharacterSetCreateWithCharactersInRange(NULL, CFRangeMake('0', '9'));

    // Make a mapping of normalized class names to their indexes in the schema's
    // class list for the old and new schemas.
    CFMutableDictionaryRef oldClasses = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    {
        int classIndex = 0;
        const SecDbClass * const *oldClass = oldSchema->classes;
        for (; *oldClass != NULL; classIndex++, oldClass++) {
            // Trim the numbers from the class name, so that we can match up
            // pre-version 10.0 and post-version 10.0 classes. Before schema
            // version 10.0, whenever we added a new schema, we would also need
            // to manually rename the previous schema's item classes with the
            // previous schema's version number. Since version 10.0, we add
            // `_old` to the previous schema's table names instead, so it's no
            // longer necessary to rename the old classes by hand.
            CFStringRef oldClassName = CFStringCreateByTrimmingCharactersInSet((*oldClass)->name, asciiDigits);
            os_assert(!CFDictionaryContainsKey(oldClasses, oldClassName));
            CFDictionarySetValue(oldClasses, oldClassName, (void *)(intptr_t)classIndex);
            CFReleaseNull(oldClassName);
        }
    }
    CFMutableDictionaryRef newClasses = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    {
        int classIndex = 0;
        const SecDbClass * const *newClass = newSchema->classes;
        for (; *newClass != NULL; classIndex++, newClass++) {
            CFStringRef newClassName = CFStringCreateByTrimmingCharactersInSet((*newClass)->name, asciiDigits);
            os_assert(!CFDictionaryContainsKey(newClasses, newClassName));
            CFDictionarySetValue(newClasses, newClassName, (void *)(intptr_t)classIndex);
            CFReleaseNull(newClassName);
        }
    }

    CFReleaseNull(asciiDigits);

    // Class names that exist in both the old and new schemas, and are not
    // equal, must be migrated.
    CFMutableSetRef classNamesToMigrate = CFSetCreateMutableForCFTypes(NULL);
    CFDictionaryForEach(oldClasses, ^(const void *className, const void *oldClassIndexPtr) {
        const void *newClassIndexPtr;
        if (CFDictionaryGetValueIfPresent(newClasses, className, &newClassIndexPtr)) {
            int oldClassIndex = (int)(intptr_t)oldClassIndexPtr;
            const SecDbClass *oldClass = oldSchema->classes[oldClassIndex];

            int newClassIndex = (int)(intptr_t)newClassIndexPtr;
            const SecDbClass *newClass = newSchema->classes[newClassIndex];

            if (!DBClassesAreEqual(oldClass, newClass)) {
                CFSetAddValue(classNamesToMigrate, className);
            }
        }
    });

    // Rename existing tables to names derived from old schema names
    sql = CFStringCreateMutable(NULL, 0);
    CFMutableArrayRef classIndexesForNewTables = CFArrayCreateMutable(NULL, 0, NULL);
    CFSetForEach(classNamesToMigrate, ^(const void *className) {
        int oldClassIndex = (int)(intptr_t)CFDictionaryGetValue(oldClasses, className);
        const SecDbClass *oldClass = oldSchema->classes[oldClassIndex];

        int newClassIndex = (int)(intptr_t)CFDictionaryGetValue(newClasses, className);
        const SecDbClass *newClass = newSchema->classes[newClassIndex];

        CFStringAppendFormat(sql, NULL, CFSTR("ALTER TABLE %@ RENAME TO %@_old;"), newClass->name, oldClass->name);
        CFArrayAppendValue(classIndexesForNewTables, (void*)(long)newClassIndex);
    });

    // Drop any tables for classes in the old schema that have been removed from
    // the new schema.
    CFDictionaryForEach(oldClasses, ^(const void *oldClassName, const void *oldClassIndexPtr) {
        if (!CFSetContainsValue(classNamesToMigrate, oldClassName) && !CFDictionaryContainsKey(newClasses, oldClassName)) {
            int oldClassIndex = (int)(intptr_t)oldClassIndexPtr;
            const SecDbClass *oldClass = oldSchema->classes[oldClassIndex];
            CFStringAppendFormat(sql, NULL, CFSTR("DROP TABLE IF EXISTS %@;"), oldClass->name);
        }
    });

    // Drop any tables for the new schema that already exist. These should be
    // no-ops, unless you're upgrading a previously-upgraded database with an
    // invalid version number.
    CFDictionaryForEach(newClasses, ^(const void *newClassName, const void *newClassIndexPtr) {
        if (!CFSetContainsValue(classNamesToMigrate, newClassName) && !CFDictionaryContainsKey(oldClasses, newClassName)) {
            int newClassIndex = (int)(intptr_t)newClassIndexPtr;
            const SecDbClass *newClass = newSchema->classes[newClassIndex];
            CFStringAppendFormat(sql, NULL, CFSTR("DROP TABLE IF EXISTS %@;"), newClass->name);
            CFArrayAppendValue(classIndexesForNewTables, (void*)(long)newClassIndex);
        }
    });

    if(CFStringGetLength(sql) > 0) {
        require_action_quiet(ok &= SecDbExec(dbt, sql, error), out,
                             bounceLKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, LKAKeychainUpgradeOutcomePhase1AlterTables, error ? *error : NULL));
    }
    CFReleaseNull(sql);

    // Drop indices that that new schemas will use
    sql = CFStringCreateMutable(NULL, 0);
    for (const SecDbClass * const *newClass = newSchema->classes; *newClass != NULL; newClass++) {
        SecDbForEachAttrWithMask((*newClass), desc, kSecDbIndexFlag | kSecDbInFlag) {
            CFStringAppendFormat(sql, 0, CFSTR("DROP INDEX IF EXISTS %@%@;"), (*newClass)->name, desc->name);
            if (desc->kind == kSecDbSyncAttr) {
                CFStringAppendFormat(sql, 0, CFSTR("DROP INDEX IF EXISTS %@%@0;"), (*newClass)->name, desc->name);
            }
        }
    }
    require_action_quiet(ok &= SecDbExec(dbt, sql, error), out,
                         bounceLKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, LKAKeychainUpgradeOutcomePhase1DropIndices, error ? *error : NULL));
    CFReleaseNull(sql);

    // Create tables for new schema.
    require_action_quiet(ok &= SecItemDbCreateSchema(dbt, newSchema, classIndexesForNewTables, false, error), out,
                         bounceLKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, LKAKeychainUpgradeOutcomePhase1CreateSchema, error ? *error : NULL));

    // Go through all classes of current schema to transfer all items to new tables.
    classNamesToMigrateAsArray = CFSetCopyValues(classNamesToMigrate);
    for (CFIndex i = 0; i < CFArrayGetCount(classNamesToMigrateAsArray); i++) {
        CFStringRef className = CFArrayGetValueAtIndex(classNamesToMigrateAsArray, i);

        int oldClassIndex = (int)(intptr_t)CFDictionaryGetValue(oldClasses, className);
        const SecDbClass *oldClass = oldSchema->classes[oldClassIndex];

        int newClassIndex = (int)(intptr_t)CFDictionaryGetValue(newClasses, className);
        const SecDbClass *newClass = newSchema->classes[newClassIndex];

        secnotice("upgr", "Upgrading table %@", oldClass->name);

        // Create a new 'old' class with a new 'old' name.
        int count = 0;
        SecDbForEachAttr(oldClass, attr) {
            count++;
        }
        if(renamedOldClass) {
            CFReleaseNull(renamedOldClass->name);
            free(renamedOldClass);
        }
        renamedOldClass = (SecDbClass*) malloc(sizeof(SecDbClass) + sizeof(SecDbAttr*)*(count+1));
        renamedOldClass->name = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@_old"), oldClass->name);
        renamedOldClass->itemclass = oldClass->itemclass;
        for(; count >= 0; count--) {
            renamedOldClass->attrs[count] = oldClass->attrs[count];
        }

        // SecDbItemSelect only works for item classes.
        if(oldClass->itemclass) {
            // Prepare query to iterate through all items in cur_class.
            if (query != NULL)
                query_destroy(query, NULL);
            require_quiet(query = query_create(renamedOldClass, SecMUSRGetAllViews(), NULL, NULL, error), out);

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
                item->class = newClass;

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
                    secnotice("upgr", "dropping item during schema upgrade due to agrp=com.apple.token: " SECDBITEM_FMT, item);
                } else {
                    // Insert new item into the new table.
                    if (!SecDbItemInsert(item, dbt, false, false, &localError)) {
                        secerror("item: " SECDBITEM_FMT " insert during upgrade: %@", item, localError);
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
                            [[fallthrough]];
                        default:
                            ok &= CFErrorPropagate(CFRetainSafe(localError), error);
                            break;
                    }
                    CFReleaseSafe(localError);
                }

                *stop = !ok;

            });

            require_action_quiet(ok, out,
                                 bounceLKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, LKAKeychainUpgradeOutcomePhase1Items, error ? *error : NULL));
        } else {
            // This table does not contain secdb items, and must be transferred without using SecDbItemSelect.
            // For now, this code does not support removing or renaming any columns, or adding any new non-null columns.
            CFReleaseNull(sql);
            sql = CFStringCreateMutable(NULL, 0);
            bool comma = false;

            CFMutableStringRef columns = CFStringCreateMutable(NULL, 0);

            SecDbForEachAttrWithMask(renamedOldClass, attr, kSecDbInFlag) {
                if(comma) {
                    CFStringAppendFormat(columns, NULL, CFSTR(","));
                }
                CFStringAppendFormat(columns, NULL, CFSTR("%@"), attr->name);
                comma = true;
            }

            CFStringAppendFormat(sql, NULL, CFSTR("INSERT OR REPLACE INTO %@ (%@) SELECT %@ FROM %@;"), newClass->name, columns, columns, renamedOldClass->name);

            CFReleaseNull(columns);
            require_action_quiet(ok &= SecDbExec(dbt, sql, error), out,
                                 bounceLKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, LKAKeychainUpgradeOutcomePhase1NonItems, error ? *error : NULL));
        }
    }

    // Remove old tables from the DB.
    CFReleaseNull(sql);
    sql = CFStringCreateMutable(NULL, 0);
    CFSetForEach(classNamesToMigrate, ^(const void *className) {
        int oldClassIndex = (int)(intptr_t)CFDictionaryGetValue(oldClasses, className);
        const SecDbClass *oldClass = oldSchema->classes[oldClassIndex];

        CFStringAppendFormat(sql, NULL, CFSTR("DROP TABLE %@_old;"), oldClass->name);
    });

    if(CFStringGetLength(sql) > 0) {
        require_action_quiet(ok &= SecDbExec(dbt, sql, error), out,
                             bounceLKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, LKAKeychainUpgradeOutcomePhase1DropOld, error ? *error : NULL));
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
    CFReleaseNull(classNamesToMigrateAsArray);
    CFReleaseNull(classNamesToMigrate);
    CFReleaseNull(oldClasses);
    CFReleaseNull(newClasses);
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
        require_action_quiet(query = query_create(*class, SecMUSRGetAllViews(), NULL, NULL, error), out, ok = false);
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
                    secnotice("upgr", "dropping item during item upgrade due to agrp=com.apple.token: " SECDBITEM_FMT, item);
                    ok = SecDbItemDelete(item, threadDbt, kCFBooleanFalse, false, &localError);
                } else {
                    // Replace item with the new value in the table; this will cause the item to be decoded and recoded back,
                    // incl. recalculation of item's hash.
// radar:87784851
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wbool-conversion"
                    ok = SecDbItemUpdate(item, item, threadDbt, false, query->q_uuid_from_primary_key, &localError);
#pragma clang diagnostic pop
                }
            }

            if (localError) {
                CFIndex status = CFErrorGetCode(localError);

                switch (status) {
                    case errSecDecode: {
                        // Items producing errSecDecode are silently dropped - they are not decodable and lost forever.
                        // This also happens if we can't re-encode the item for a now non-existent persona.
                        // Make sure we use a local error so that it's not propagated upward, which would cause a
                        // migration failure.
                        CFErrorRef deleteError = NULL;
                        // Don't create a tombstone, just hard delete the item.
                        (void)SecDbItemDelete(item, threadDbt, kCFBooleanFalse, false, &deleteError);
                        CFReleaseNull(deleteError);
                        ok = true;
                        break;
                    }
                    case errSecInteractionNotAllowed:
                        // If we are still not able to decrypt the item because the class key is not released yet,
                        // remember that DB still needs phase2 migration to be run next time a connection is made.  Also
                        // stop iterating next items, it would be just waste of time because the whole iteration will be run
                        // next time when this phase2 will be rerun.
                        bounceLKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, LKAKeychainUpgradeOutcomeLocked, NULL);
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
                            secerror("Received SQLITE_CONSTRAINT with wrong error domain. Huh? Item: " SECDBITEM_FMT ", error: %@", item, localError);
                        } else {
                            secnotice("upgr", "Received SQLITE_CONSTRAINT -- ignoring: " SECDBITEM_FMT, item);
                            ok = true;
                        }
                        break;
                    case errSecDuplicateItem:
                        // continue to upgrade and don't propagate errors for insert failures
                        // that are typical of a single item failure
                        secnotice("upgr", "Ignoring duplicate item: " SECDBITEM_FMT, item);
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
                        [[fallthrough]];
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
        require_action(ok, out, bounceLKAReportKeychainUpgradeOutcomeWithError(oldVersion, newVersion, LKAKeychainUpgradeOutcomePhase2, error ? *error : NULL));
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


static bool phase3EvaluateErrorAndStop(CFErrorRef updateError, CFErrorRef *error) {
    bool shouldStop = false;
    CFIndex status = CFErrorGetCode(updateError);

    switch (status) {
        case errSecDecode:
            // Items producing errSecDecode are not decodable and lost forever.
            // We should probably consider deleting them here.
            secnotice("upgr-phase3", "failed to decode keychain item");
            break;
        case errSecInteractionNotAllowed:
            secnotice("upgr-phase3", "interaction not allowed: %@", updateError);
            shouldStop = true;
            CFErrorPropagate(CFRetainSafe(updateError), error);
            break;
        case errSecAuthNeeded:
            // This particular item requires authentication, attempt to continue iterating through the keychain db
            secnotice("upgr-phase3", "authentication needed: %@", updateError);
            break;
#if USE_KEYSTORE
        case kAKSReturnNotReady:
        case kAKSReturnTimeout:
            secnotice("upgr-phase3", "AKS is not ready/timing out: %@", updateError);
            shouldStop = true;
            CFErrorPropagate(CFRetainSafe(updateError), error);
            break;
#endif
        case errSecNotAvailable:
            secnotice("upgr-phase3", "AKS is unavailable: %@", updateError);
            break;
        default:
            CFErrorPropagate(CFRetainSafe(updateError), error);
            break;
    }
    return shouldStop;
}

static CFErrorRef errorForRowID(CFNumberRef rowID) {
    if (!rowIDAndErrorDictionary) {
        return NULL;
    }
    
    CFErrorRef matching = NULL;

    if (CFDictionaryContainsKey(rowIDAndErrorDictionary, rowID)) {
        matching = (CFErrorRef)CFDictionaryGetValue(rowIDAndErrorDictionary, rowID);
    }
    
    return matching;
}

CFDataRef UUIDDataCreate(void)
{
    CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
    CFUUIDBytes uuidBytes = CFUUIDGetUUIDBytes(uuid);
    CFDataRef uuidData = CFDataCreate(kCFAllocatorDefault, (const void *)&uuidBytes, sizeof(uuidBytes));
    CFReleaseNull(uuid);
    return uuidData;
}

// Goes through all items for each table and assigns a persistent ref UUID
bool UpgradeItemPhase3(SecDbConnectionRef inDbt, bool *inProgress, CFErrorRef *error) {
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    __block int64_t itemsMigrated = 0;
    struct timeval start;

    gettimeofday(&start, NULL);
#endif

    // Go through all classes in new schema
    const SecDbSchema *newSchema = current_schema();
    for (const SecDbClass *const *class = newSchema->classes; *class != NULL; class++) {
        if(!((*class)->itemclass)) {
            //Don't try to decrypt non-item 'classes'
            continue;
        }

        const char* tableCPtr = CFStringGetCStringPtr((*class)->name, kCFStringEncodingUTF8);

        char *quotedMaxField = sqlite3_mprintf("%q", tableCPtr);
        if (quotedMaxField == NULL) {
            continue;
        }
        CFStringRef tableCF = CFStringCreateWithCString(kCFAllocatorDefault, quotedMaxField, kCFStringEncodingUTF8);
        sqlite3_free(quotedMaxField);

        __block CFStringRef sql = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("SELECT * FROM %@ WHERE persistref IS ''"), tableCF);
        __block CFErrorRef cferror = NULL;
        __block CFMutableArrayRef rowIDsToBeUpdated = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

        //evaluating only items that have an empty persistref field
        SecDbPrepare(inDbt, sql, &cferror, ^void (sqlite3_stmt *stmt) {
            SecDbStep(inDbt, stmt, &cferror, ^(bool *stop) {
                int64_t rowid = sqlite3_column_int64(stmt, 0);
                secnotice("upgr-phase3", "picked up rowid: %lld that needs a persistref", rowid);
                CFNumberRef rowid_cf = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &rowid);

                //only add rowids that are greater than the last 'highest' processed rowID.  This is to prevent infinite looping.
                if (lastRowIDHandled == NULL || CFNumberCompare(rowid_cf, lastRowIDHandled, NULL) == kCFCompareGreaterThan) {
                    if (CFArrayGetCount(rowIDsToBeUpdated) < MAX_NUM_PERSISTENT_REF_ROWIDS) {
                        CFArrayAppendValue(rowIDsToBeUpdated, rowid_cf);
                    } else {
                        *stop = true;
                        *inProgress = true;
                    }
                }
                CFReleaseNull(rowid_cf);
            });
        });

        CFReleaseNull(sql);

        __block bool shouldStopIteratingArray = false;

        CFArrayForEach(rowIDsToBeUpdated, ^(const void *row) {
    
            if (shouldStopIteratingArray) { //stop processing items in the array
                return;
            }
            
            CFErrorRef queryError = NULL;
            CFNumberRef row_cf = (CFNumberRef)row;
            SecDbQueryRef query = query_create(*class, SecMUSRGetAllViews(), NULL, NULL, &queryError);
            CFReleaseNull(queryError);
            
            SecDbItemSelect(query, inDbt, &queryError, ^bool(const SecDbAttr *attr) {
                return (attr->flags & kSecDbInFlag) != 0;
            }, ^bool(const SecDbAttr *attr) {
                return attr->kind == kSecDbRowIdAttr;
            }, NULL, ^bool(sqlite3_stmt *stmt, int col) {
                return SecDbBindObject(stmt, 1, row_cf, NULL);
            }, ^(SecDbItemRef item, bool *stop) {
                CFErrorRef localError = NULL;
                CFErrorRef fetchError = NULL;
                CFNumberRef previousRowIDHandled = NULL;

                //keep track of the highest rowID checked.
                //if items are corrupt we will never be able to unwrap them
                //or if the items require authentication they can't be upgraded at this point
                //in both cases, these items need to be ignored for now
                CFTransferRetained(previousRowIDHandled, lastRowIDHandled);
                
                CFReleaseNull(lastRowIDHandled);
                
                lastRowIDHandled = CFRetainSafe(row_cf);
                
                //only update items that do not have a persistent ref UUID
                CFDataRef persistRef = SecDbItemGetPersistentRef(item, &localError);
                if (localError) {
                    secerror("upgr-phase3: failed to get persistent ref error: %@", localError);
                    if (phase3EvaluateErrorAndStop(localError, error)) {
                        shouldStopIteratingArray = true;
                        *inProgress = true;
                        CFTransferRetained(lastRowIDHandled, previousRowIDHandled);
                        CFReleaseNull(localError);
                        return;
                    }
                    CFReleaseNull(localError);
                }

                sqlite_int64 itemRowID = SecDbItemGetRowId(item, &fetchError);
                if (fetchError) {
                    secerror("upgr-phase3: failed to get rowID error: %@", fetchError);
                }

                CFStringRef itemClass = SecDbItemGetClass(item)->name;

                CFStringRef shouldPerformUpgrade = (persistRef && CFDataGetLength(persistRef) == PERSISTENT_REF_UUID_BYTES_LENGTH) ? CFSTR("NO") : CFSTR("YES");

                secnotice("upgr-phase3", "inspecting item at row %lld in table %@, should add persistref uuid?: %@", itemRowID, itemClass, shouldPerformUpgrade);
                CFReleaseNull(fetchError);
                CFReleaseNull(localError);

                if (CFStringCompare(shouldPerformUpgrade, CFSTR("YES"), 0) == kCFCompareEqualTo) {
                    secnotice("upgr-phase3", "upgrading item persistentref at row id %lld", itemRowID);

                    //update item to have a UUID
                    CFDataRef uuidData = UUIDDataCreate();

                    //set the UUID on the item's persistent ref attribute
                    bool setResult = SecDbItemSetValueWithName(item, kSecAttrPersistentReference, uuidData, &localError);
                    CFReleaseNull(uuidData);

                    if (!setResult || localError || testError || errorForRowID(row_cf)) {
                        secerror("upgr-phase3: failed to set persistentref for item:" SECDBITEM_FMT ", error:%@", item, localError);
                        if (localError) {
                            if (phase3EvaluateErrorAndStop(localError, error)) {
                                shouldStopIteratingArray = true;
                                *inProgress = true;
                                CFTransferRetained(lastRowIDHandled, previousRowIDHandled);
                                CFReleaseNull(localError);
                                return;
                            }
                            CFReleaseNull(localError);
                        } else if (testError) {
                            secerror("upgr-phase3: TEST ERROR PATH:" SECDBITEM_FMT ", error:%@", item, testError);
                            if (phase3EvaluateErrorAndStop(testError, error)) {
                                shouldStopIteratingArray = true;
                                *inProgress = true;
                                CFTransferRetained(lastRowIDHandled, previousRowIDHandled);
                                return;
                            }
                        } else if (errorForRowID(row_cf)) {
                            CFErrorRef errorForRow = errorForRowID(row_cf);
                            secerror("upgr-phase3: TEST ERROR FOR ROWID PATH:" SECDBITEM_FMT ", error:%@", item, errorForRow);
                            if (phase3EvaluateErrorAndStop(errorForRow, error)) {
                                shouldStopIteratingArray = true;
                                *inProgress = true;
                                CFTransferRetained(lastRowIDHandled, previousRowIDHandled);
                                return;
                            }
                        } else if (!setResult) {
                            secerror("upgr-phase3: SecDbItemSetValueWithName returned false");
                            shouldStopIteratingArray = true;
                            *inProgress = true;
                            CFTransferRetained(lastRowIDHandled, previousRowIDHandled);
                            return;
                        }
                    } else {
                        CFErrorRef updateError = NULL;
// radar:87784851
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wbool-conversion"
                        bool updateResult = SecDbItemUpdate(item, item, inDbt, false, query->q_uuid_from_primary_key, &updateError);
#pragma clang diagnostic pop
                        if (!updateResult || updateError) {
                            secnotice("upgr-phase3", "phase3: failed to update item " SECDBITEM_FMT ": %d, error: %@", item, updateResult, updateError);
                            if (updateError) {
                                if (phase3EvaluateErrorAndStop(updateError, error)) {
                                    shouldStopIteratingArray = true;
                                    *inProgress = true;
                                    CFTransferRetained(lastRowIDHandled, previousRowIDHandled);
                                    CFReleaseNull(updateError);
                                    return;
                                }
                            }

                            CFReleaseNull(updateError);
                        } else {
                            secnotice("upgr-phase3", "updated item " SECDBITEM_FMT ": %d", item, updateResult);
                        }
                    }
                    CFReleaseNull(localError);
                    CFReleaseNull(previousRowIDHandled);
                }
            });
            if (query != NULL) {
                query_destroy(query, NULL);
            }
            CFReleaseNull(queryError);
        });

        CFReleaseNull(tableCF);
        CFReleaseNull(rowIDsToBeUpdated);
        CFReleaseNull(cferror);
    }

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    measureUpgradePhase3(&start, SecBucket2Significant(itemsMigrated));
#endif
    return true;
}

void clearLastRowIDHandledForTests(void) {
    CFReleaseNull(lastRowIDHandled);
}

CFNumberRef lastRowIDHandledForTests(void) {
    return lastRowIDHandled;
}

void setExpectedErrorForTests(CFErrorRef error) {
    testError = error;
}

void clearTestError(void) {
    testError = NULL;
}

void setRowIDToErrorDictionary(CFDictionaryRef dictionary) {
    rowIDAndErrorDictionary = CFRetainSafe(dictionary);
}

void clearRowIDAndErrorDictionary(void) {
    CFReleaseNull(rowIDAndErrorDictionary);
}

// There's no data-driven approach for this. Let's think about it more if it gets unwieldy
static void performCustomIndexProcessing(SecDbConnectionRef dbt) {
    CFErrorRef cfErr = NULL;
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS genpagrp; DROP INDEX IF EXISTS genpsync;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS inetagrp; DROP INDEX IF EXISTS inetsync;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS certagrp; DROP INDEX IF EXISTS certsync;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS keysagrp; DROP INDEX IF EXISTS keyssync;"), &cfErr), errhandler);

    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS genpsync0;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS inetsync0;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS certsync0;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS keyssync0;"), &cfErr), errhandler);

    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS genpmusr;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS inetmusr;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS certmusr;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS keysmusr;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS item_backupmusr;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS backup_keybagmusr;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS backup_keyarchivemusr;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS archived_key_backupmusr;"), &cfErr), errhandler);

    require(SecDbExec(dbt, CFSTR("CREATE INDEX IF NOT EXISTS agrp_musr_tomb_svce ON genp(agrp, musr, tomb, svce);"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("CREATE INDEX IF NOT EXISTS agrp_musr_tomb_srvr ON inet(agrp, musr, tomb, srvr);"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("CREATE INDEX IF NOT EXISTS agrp_musr_tomb_subj ON cert(agrp, musr, tomb, subj);"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("CREATE INDEX IF NOT EXISTS agrp_musr_tomb_atag ON keys(agrp, musr, tomb, atag);"), &cfErr), errhandler);

    require(SecDbExec(dbt, CFSTR("CREATE INDEX IF NOT EXISTS synckeys_contextID_ckzone_keyclass_state ON synckeys(contextID, ckzone, keyclass, state);"), &cfErr), errhandler);

    require(SecDbExec(dbt, CFSTR("CREATE INDEX IF NOT EXISTS incomingqueue_contextID_ckzone_UUID ON incomingqueue(contextID, ckzone, UUID);"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("CREATE INDEX IF NOT EXISTS incomingqueue_contextID_ckzone_state ON incomingqueue(contextID, ckzone, state);"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("CREATE INDEX IF NOT EXISTS incomingqueue_contextID_ckzone_parentkeyUUID ON incomingqueue(contextID, ckzone, parentKeyUUID);"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("CREATE INDEX IF NOT EXISTS outgoingqueue_contextID_ckzone_UUID ON outgoingqueue(contextID, ckzone, UUID);"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("CREATE INDEX IF NOT EXISTS outgoingqueue_contextID_ckzone_state ON outgoingqueue(contextID, ckzone, state);"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("CREATE INDEX IF NOT EXISTS outgoingqueue_contextID_ckzone_parentkeyUUID ON outgoingqueue(contextID, ckzone, parentKeyUUID);"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("CREATE INDEX IF NOT EXISTS ckmirror_contextID_ckzone_UUID ON ckmirror(contextID, ckzone, UUID);"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("CREATE INDEX IF NOT EXISTS ckmirror_contextID_ckzone_parentkeyUUID ON ckmirror(contextID, ckzone, parentKeyUUID);"), &cfErr), errhandler);

    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS tlksharecontextID;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS currentitemscontextID;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS ckdevicestatecontextID;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS outgoingqueuecontextID;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS incomingqueuecontextID;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS synckeyscontextID;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS ckmirrorcontextID;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS currentkeyscontextID;"), &cfErr), errhandler);
    require(SecDbExec(dbt, CFSTR("DROP INDEX IF EXISTS ckstatecontextID;"), &cfErr), errhandler);

    
    
    secnotice("upgr", "processed custom indexes (now or in the past)");
    CFReleaseNull(cfErr);   // Should be nil but belt and suspenders
    return;

errhandler:
    secerror("upgr: failed to process custom indexes: %@", cfErr);
    CFReleaseNull(cfErr);
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
                             performCustomIndexProcessing(dbt);
                             bounceLKAReportKeychainUpgradeOutcomeWithError(version2, newVersion, LKAKeychainUpgradeOutcomeNewDb, localError));

        int oldVersion = VERSION_OLD(version2);
        version2 = VERSION_NEW(version2);

        require_action_quiet(version2 == SCHEMA_VERSION(newSchema) || oldVersion == 0, out,
                             ok = SecDbError(SQLITE_CORRUPT, &localError,
                                             CFSTR("Half migrated but obsolete DB found: found 0x%x(0x%x) but 0x%x is needed"),
                                             version2, oldVersion, SCHEMA_VERSION(newSchema));
                             bounceLKAReportKeychainUpgradeOutcomeWithError(version2, newVersion, LKAKeychainUpgradeOutcomeObsoleteDb, NULL));

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
                                 bounceLKAReportKeychainUpgradeOutcomeWithError(version2, newVersion, LKAKeychainUpgradeOutcomeNoSchema, NULL));

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
        SecCoreAnalyticsSendValue(CFSTR("com.apple.keychain.migration-success"), 1);
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
            SecCoreAnalyticsSendValue(CFSTR("com.apple.keychain.migration-failure"), 1);
#endif
        }
    } else {
        // Things seemed to go okay!
        if (didPhase2) {
            bounceLKAReportKeychainUpgradeOutcomeWithError(version, newVersion, LKAKeychainUpgradeOutcomeSuccess, NULL);
        }

        //If we're done here, we should opportunistically re-add all indices (just in case)
        if(skipped_upgrade || didPhase2) {
            // Create indices, ignoring all errors
            performCustomIndexProcessing(dbt);
            for (SecDbClass const* const* newClass = newSchema->classes; *newClass; ++newClass) {
                SecDbForEachAttrWithMask((*newClass), desc, kSecDbIndexFlag | kSecDbInFlag) {
                    CFStringRef sql = NULL;
                    CFErrorRef classLocalError = NULL;
                    bool localOk = true;
                    sql = CFStringCreateWithFormat(NULL, NULL, CFSTR("CREATE INDEX IF NOT EXISTS %@%@ ON %@(%@);"), (*newClass)->name, desc->name, (*newClass)->name, desc->name);
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


static CF_RETURNS_RETAINED CFDataRef SecServerExportBackupableKeychain(SecDbConnectionRef dbt, SecurityClient *client, keybag_handle_t* dest_keybag, CFErrorRef *error) {
    CFDataRef data_out = NULL;

    SecSignpostStart(SecSignpostBackupKeychainBackupable);

    /* Export everything except the items for which SecItemIsSystemBound()
       returns true. */
    CFDictionaryRef keychain = SecServerCopyKeychainPlist(dbt, client,
        dest_keybag, kSecBackupableItemFilter,
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
                                              struct backup_keypair* src_bkp,
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
                                                    src_bkp,
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
    backup = SecServerExportBackupableKeychain(dbt, client, &backup_keybag, error);

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
    keybag_handle_t backup_keybag = bad_keybag_handle;
    struct backup_keypair bkp;

    secnotice("SecServerKeychainRestore", "Restoring keychain backup");

    SecSignpostStart(SecSignpostRestoreOpenKeybag);
#if USE_KEYSTORE
    kern_return_t ret = aks_kc_backup_open_keybag(CFDataGetBytePtr(keybag), CFDataGetLength(keybag), password ? CFDataGetBytePtr(password) : NULL, password ? CFDataGetLength(password) : 0, &backup_keybag, &bkp);
    if (ret != 0) {
        secwarning("SecServerKeychainRestore: aks_kc_backup_open_keybag failed: %d", ret);
        if (error) {
            SecKernError(ret, error, CFSTR("aks_kc_backup_open_keybag failed: %d"), ret);
        }
        goto out;
    }
    secnotice("SecServerKeychainRestore", "aks_kc_backup_open_keybag got backup_keybag:%d", backup_keybag);
#else
    backup_keybag = KEYBAG_NONE;
#endif
    SecSignpostStop(SecSignpostRestoreOpenKeybag);
    SecSignpostStart(SecSignpostRestoreKeychain);

    /* Import from backup keybag to system keybag. */
    struct backup_keypair* src_bkp = backup_keybag == bad_keybag_handle ? &bkp : NULL;
    require(SecServerImportBackupableKeychain(dbt, client, backup_keybag, src_bkp, KEYBAG_DEVICE, backup, error), out);

    ok = true;
out:
    SecSignpostStop(SecSignpostRestoreKeychain);

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
    bool useSystemPath = false;

#if defined(SECURITYD_SYSTEM) && SECURITYD_SYSTEM
    kcRelPath = CFSTR("system-keychain-2.db");
    useSystemPath = true;
#else
    if (os_variant_is_recovery("securityd")) {
        kcRelPath = CFSTR("keychain-recovery-2.db");
#if !TARGET_OS_OSX
        // In embedded recovery, don't use user homedir in embedded recovery, to maintain existing behavior.
        //
        // In macOS, SecCopyURLForFileInUserScopedKeychainDirectory() is the same as SecCopyURLForFileInKeychainDirectory(),
        // it will try to use the user homedir. And we do NOT want to use the system path.
        useSystemPath = true;
#endif
    } else if (use_hwaes()) {
        kcRelPath = CFSTR("keychain-2.db");
    } else {
        kcRelPath = CFSTR("keychain-2-debug.db");
    }
#endif // SECURITYD_SYSTEM

    CFStringRef kcPath = NULL;
    CFURLRef kcURL = useSystemPath ? SecCopyURLForFileInSystemKeychainDirectory(kcRelPath) : SecCopyURLForFileInUserScopedKeychainDirectory(kcRelPath);
    if (kcURL) {
        kcPath = CFURLCopyFileSystemPath(kcURL, kCFURLPOSIXPathStyle);
        CFRelease(kcURL);
    }
    secnotice("__SecKeychainCopyPath", "path: %s", kcPath ? (CFStringGetCStringPtr(kcPath, kCFStringEncodingUTF8) ?: "<unknown>") : "<null>");
    return kcPath;
}

// MARK; -
// MARK: kc_dbhandle init and reset

#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
static bool SystemKeybagUnlocked() {
#if DEBUG
    struct stat st;
    int result = stat("/Library/Keychains/force_system_keybag_locked", &st);
    if (result == 0) {
        secnotice("upgr", "forcing system keybag locked");
        return false;
    }
#endif // DEBUG

    CFErrorRef aksError = NULL;
    bool locked = true;

    if(!SecAKSGetIsLocked(system_keychain_handle, &locked, &aksError)) {
        secnotice("upgr", "error querying system keybag lock state: %@", aksError);
        CFReleaseNull(aksError);
    }

    return !locked;
}
#endif

SecDbRef SecKeychainDbCreate(CFStringRef path, CFErrorRef* error) {
    // rdar://112992329
    // localerror is only used in the block passed to SecDbCreate(), but that block is copied, not run synchronously.
    // So localerror will _not_ be modified by the time SecKeychainDbCreate() returns.
    // And because of that, the error parameter will never be filled in with anything but NULL.
    __block CFErrorRef localerror = NULL;

    SecDbRef kc = SecDbCreate(path, 0600, true, true, true, true, kSecDbMaxIdleHandles,
        ^bool (SecDbRef db, SecDbConnectionRef dbconn, bool didCreate, bool *callMeAgainForNextConnection, CFErrorRef *createError)
    {
        // Upgrade from version 0 means create the schema in empty db.
        int version = 0;
        bool ok = true;

#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
        bool needEduBagTranscryption = false;
        bool needEduModeWorkaround = SecSupportsEnhancedApfs() && !SecSeparateUserKeychain() && SecIsEduMode();
#endif // KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER

        if (!didCreate) {
            ok = SecKeychainDbGetVersion(dbconn, &version, createError);
#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
            if (ok && needEduModeWorkaround) {
                if (!SecKeychainDbGetEduModeTranscrypted(dbconn)) {
                    secnotice("upgr", "must transcrypt");
                    if (SystemKeybagUnlocked()) {
                        secnotice("upgr", "using default keybag");
                        needEduBagTranscryption = true;
                        // Set the keybag to the default, instead of the system keychain keybag, so
                        // we can decrypt items in order to upgrade the DB.
                        SecItemServerSetKeychainKeybagToDefault();
                    } else {
                        secerror("Cannot transcrypt because system keybag not (yet) unlocked!! 🫸");
                        ok = false;
                        SecError(errSecNotAvailable, createError, CFSTR("transcryption error: system keybag not (yet) unlocked"));
                    }
                } else {
                    secnotice("upgr", "already transcrypted");
                }
            }
        } else if (needEduModeWorkaround) {
            secnotice("upgr", "created new db, setting edu bag version");
            // We don't expect to create a new DB when already in edu mode.
            // But if we do, we don't need to transcrypt, because it is already protected by the system keychain keybag.
            SecKeychainDbSetEduModeTranscrypted(dbconn);
#endif // KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
        }

        ok = ok && SecKeychainDbUpgradeFromVersion(dbconn, version, callMeAgainForNextConnection, createError);
        if (!ok)
            secerror("Upgrade %sfailed: %@", didCreate ? "from v0 " : "", createError ? *createError : NULL);

        localerror = createError ? *createError : NULL;

#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
        if (needEduBagTranscryption) {
            CFStringRef accessGroup = CFSTR("*");
            SecurityClient client = {
                .task = NULL,
                .accessGroups =  CFArrayCreate(kCFAllocatorDefault, (const void **)&accessGroup, 1, &kCFTypeArrayCallBacks),
                .allowSystemKeychain = true,
                .allowSyncBubbleKeychain = false,
                .isNetworkExtension = false,
            };
            InternalTranscryptToSystemKeychainKeybag(dbconn, &client, createError);
            CFReleaseNull(client.accessGroups);

            secnotice("upgr", "transcrypted, setting flag to remember we've already done so");
            SecKeychainDbSetEduModeTranscrypted(dbconn);

            secnotice("upgr", "transcrypted, using system keychain handle");
            SecItemServerSetKeychainKeybag(system_keychain_handle);
        }
#endif // KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER

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

bool SecKeychainUpgradePersistentReferences(bool *inProgress, CFErrorRef *error)
{
    __block bool success = false;
    
    if (SecKeychainIsStaticPersistentRefsEnabled()) {
        __block CFErrorRef kcError = NULL;
        kc_with_dbt(true, &kcError, ^(SecDbConnectionRef dbt) {
            return kc_transaction(dbt, &kcError, ^bool{
                CFErrorRef phase3Error = NULL;
                success = UpgradeItemPhase3(dbt, inProgress, &phase3Error); //this always returns true
                if (phase3Error) {
                    secerror("upgr-phase3: failed to perform persistent ref upgrade for keychain item(s): %@", phase3Error);
                    CFErrorPropagate(CFRetainSafe(phase3Error), error);
                    CFReleaseNull(phase3Error);
                } else {
                    secnotice("upgr-phase3", "finished upgrading keychain items' persistent refs");
                }
                return true;
            });
        });
    }
    return success;
}

SecDbRef SecKeychainDbInitialize(SecDbRef db) {

#if OCTAGON
    // This needs to be async, otherwise we get hangs between securityd, cloudd, and apsd
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        if(OctagonShouldPerformInitialization()) {
            OctagonInitialize();
        }

        if(SecCKKSIsEnabled()) {
            SecCKKSInitialize(db);
        }
    });

    if(EscrowRequestServerIsEnabled()) {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            EscrowRequestServerInitialize();
        });
    }

#if KCSHARING
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        // Unconditionally register for events; the event handler will check if
        // the feature is enabled.
        KCSharingRegisterForDatabaseEvents(db);

        KCSharingPreflight();
    });
#endif  // KCSHARING
#endif  // OCTAGON

    return db;
}

static SecDbRef _kc_dbhandle = NULL;
static dispatch_queue_t _kc_dbhandle_dispatch = NULL;
static dispatch_once_t _kc_dbhandle_dispatch_onceToken = 0;
static dispatch_queue_t get_kc_dbhandle_dispatch(void) {
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

static dispatch_queue_t _async_db_dispatch = NULL;
static dispatch_once_t _async_db_dispatch_onceToken = 0;
static dispatch_queue_t get_async_db_dispatch(void) {
    dispatch_once(&_async_db_dispatch_onceToken, ^{
        _async_db_dispatch = dispatch_queue_create("sec_async_db", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    });

    return _async_db_dispatch;
}

/* open box testing only, and I really hope you call DbReset soon */
void SecKeychainDbForceClose(void)
{
    SecKeychainDbWaitForAsyncBlocks();

    dispatch_sync(get_kc_dbhandle_dispatch(), ^{
        if(_kc_dbhandle) {
            SecDbForceClose(_kc_dbhandle);
        }
    });
    LKAForceClose();
}

// for open box testing only
dispatch_group_t groupDelayAsyncBlocks;
void SecKeychainDelayAsyncBlocks(bool yorn)
{
    if (groupDelayAsyncBlocks) {
        dispatch_release(groupDelayAsyncBlocks);
    }
    groupDelayAsyncBlocks = yorn ? dispatch_group_create() : nullptr;
}

// for open box testing only
void SecKeychainDbWaitForAsyncBlocks(void)
{
    if (groupDelayAsyncBlocks) {
        dispatch_group_wait(groupDelayAsyncBlocks, DISPATCH_TIME_FOREVER);
    } else {
        // even if we're not delaying the blocks, we still need to wait for them (e.g. secd_05_corrupted_items())
        dispatch_group_t g = dispatch_group_create();
        dispatch_group_async(g, get_async_db_dispatch(), ^{});
        dispatch_group_wait(g, DISPATCH_TIME_FOREVER);
        dispatch_release(g);
    }
}

/* For open box testing only */

SecDbRef SecKeychainDbGetDb(CFErrorRef* error)
{
    return kc_dbhandle(error);
}

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

#if SECUREOBJECTSYNC
    if (writeAndRead && usesItemTables) {
        SecItemDataSourceFactoryGetDefault();
    }
#endif

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
    q = query_create_with_limit(query, musrView, kSecMatchUnlimited, NULL, &localError);
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
                         trustResult == kSecTrustResultUnspecified), cleanup);
    ok = true;

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
_FilterWithEmailAddress(CFStringRef emailAddrToMatch, SecCertificateRef cert)
{
    if (!cert || !emailAddrToMatch || CFGetTypeID(emailAddrToMatch) != CFStringGetTypeID()) {
        return false;
    }
    CFArrayRef emailAddresses = NULL;
    OSStatus status = SecCertificateCopyEmailAddresses(cert, &emailAddresses);
    if (status != errSecSuccess) {
        return false;
    }
    CFIndex idx, count = (emailAddresses) ? CFArrayGetCount(emailAddresses) : 0;
    status = (count > 0) ? errSecSMIMEEmailAddressesNotFound : errSecSMIMENoEmailAddress;
    for (idx = 0; idx < count; idx++) {
        CFStringRef emailAddr = (CFStringRef) CFArrayGetValueAtIndex(emailAddresses, idx);
        if (emailAddr && kCFCompareEqualTo == CFStringCompare(emailAddrToMatch, emailAddr, kCFCompareCaseInsensitive)) {
            status = errSecSuccess;
            break;
        }
    }
    if (emailAddresses) {
        CFRelease(emailAddresses);
    }
    return (status == errSecSuccess) ? true : false;
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

static bool isHostOrSubdomainOfHost(CFStringRef data, CFStringRef query) {
    if (data && query){
        if (CFEqualSafe(data, query)){
            return true;
        }
        CFMutableStringRef queryS = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFStringAppend(queryS, CFSTR("."));
        CFStringAppend(queryS, query);
        
        bool ok = CFStringHasSuffix(data, queryS);
        CFReleaseSafe(queryS);
        return ok;
    }
    return false;
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

    if (q->q_match_email_address && (q->q_class == identity_class() || q->q_class == cert_class())) {
        if (!certRef) {
            certRef = CopyCertificateFromItem(q, item);
        }
        require_quiet(certRef, out);
        require_quiet(_FilterWithEmailAddress(q->q_match_email_address, certRef), out);
    }

    if (q->q_match_host_or_subdomain && (q->q_class == inet_class())){
        CFTypeRef server = (CFTypeRef)CFDictionaryGetValue(item, kSecAttrServer);
        if (server && CFGetTypeID(server) == CFStringGetTypeID()){
            if(!isHostOrSubdomainOfHost((CFStringRef)server, q->q_match_host_or_subdomain)) {
                return ok;
            }
        } else {
            // server was not a string type, so filter it out since we're matching against a string
            return ok;
        }
    }

    /* Add future match checks here. */
    ok = true;
out:
    CFReleaseSafe(certRef);
    return ok;
}

//Mark: -

void deleteCorruptedItemAsync(SecDbConnectionRef dbt, CFStringRef tablename, sqlite_int64 rowid)
{
    // should really get db from dbt, but I don't know much much we should poke holes thought the boundaries.
    SecDbRef db = kc_dbhandle(NULL);
    if (db == NULL) {
        return;
    }

    CFRetain(db);
    CFRetain(tablename);

    dispatch_block_t deleteAsync = ^{
        __block CFErrorRef localErr = NULL;
        kc_with_custom_db(true, true, db, &localErr, ^bool(SecDbConnectionRef dbt2) {
            CFStringRef sql = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("DELETE FROM %@ WHERE rowid=%lli"), tablename, rowid);
            __block bool ok = true;
            ok &= SecDbPrepare(dbt2, sql, &localErr, ^(sqlite3_stmt *stmt) {
                ok &= SecDbStep(dbt2, stmt, &localErr, NULL);
            });

            if (!ok || localErr) {
                secerror("Failed to delete corrupt item, %@ row %lli: %@", tablename, rowid, localErr);
            } else {
                secnotice("item", "Deleted corrupt rowid %lli from table %@", rowid, tablename);
            }
            CFReleaseNull(localErr);
            CFReleaseNull(sql);
            return true;
        });

        CFRelease(tablename); // can't be a CFReleaseNull() because of scope
        CFRelease(db);
    };

    // local variable to obviate races
    dispatch_group_t theGroup = groupDelayAsyncBlocks;
    if (theGroup) {
        // This is the case for open box testing, not the normal case.
        dispatch_group_enter(theGroup);
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC*250), get_async_db_dispatch(), ^{deleteAsync(); dispatch_group_leave(theGroup);});
    } else {
        dispatch_async(get_async_db_dispatch(), deleteAsync);
    }
}



/****************************************************************************
 **************** Beginning of Externally Callable Interface ****************
 ****************************************************************************/

static bool SecEntitlementError(CFErrorRef *error)
{
#if TARGET_OS_OSX
#define SEC_ENTITLEMENT_WARNING CFSTR("com.apple.application-identifier nor com.apple.security.application-groups nor keychain-access-groups")
#elif TARGET_OS_MACCATALYST
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

static CFStringRef CopyAccessGroupForPersistentRef(CFStringRef itemClass, CFDataRef uuidData)
{
    __block CFStringRef accessGroup = NULL;
    
    __block CFErrorRef error = NULL;
    bool ok = kc_with_dbt(false, &error, ^bool(SecDbConnectionRef dbt) {
        CFStringRef table = CFEqual(itemClass, kSecClassIdentity) ? kSecClassCertificate : itemClass;
        CFStringRef sql = CFStringCreateWithFormat(NULL, NULL, CFSTR("SELECT agrp FROM %@ WHERE persistref = ?"), table);
      
        bool dbOk = SecDbWithSQL(dbt, sql, &error, ^bool(sqlite3_stmt *stmt) {
            bool bindOk = SecDbBindBlob(stmt, 1,  CFDataGetBytePtr(uuidData),
                                (size_t)CFDataGetLength(uuidData), SQLITE_TRANSIENT, &error);
            bool rowOk = SecDbForEach(dbt, stmt, &error, ^bool(int row_index) {
                accessGroup = CFStringCreateWithBytes(NULL, sqlite3_column_blob(stmt, 0), sqlite3_column_bytes(stmt, 0), kCFStringEncodingUTF8, false);
                return accessGroup != NULL;
            });
            
            return (bool)(rowOk && bindOk && accessGroup != NULL);
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
    } else if (isString(value)) {
        SInt32 number = CFStringGetIntValue((CFStringRef)value);
        CFStringRef t = CFStringCreateWithFormat(0, 0, CFSTR("%ld"), (long)number);
        // If a string converted to an int isn't equal to the int printed as
        // a string, return false
        if (CFEqual(t, value)) {
            result = !!number;
        }
        CFRelease(t);
    }

    return result;
}

// Expand this, rdar://problem/59297616
// Known attributes which are not API/SPI should not be permitted in queries
typedef CF_ENUM(CFIndex, QueryAttributesDescriptor) {
    QueryAttributesForAdd,
    QueryAttributesForSearch,
    QueryAttributesForUpdate,
    QueryAttributesForDelete,
};

static bool queryHasValidAttributes(CFDictionaryRef attrs, QueryAttributesDescriptor desc, CFErrorRef *error) {
    if (CFDictionaryContainsKey(attrs, kSecAttrAppClipItem)) {
        return SecError(errSecParam, error, CFSTR("Non-API attributes present in query"));
    }

    bool hasSharingGroup = CFDictionaryContainsKey(attrs, kSecAttrSharingGroup);
    if (hasSharingGroup) {
        if (SecItemSynchronizable(attrs)) {
            // Shared credentials are never synced, so querying the synced keychain
            // for them is likely a programmer error.
            return SecError(errSecParam, error, CFSTR("Can't query the synced keychain with a sharing group"));
        }
        switch (desc) {
            case QueryAttributesForAdd:
                // SecItemAdd into a group is allowed. This allows both sidecar creation alongside existing items,
                // and creating a new password directly into a group.
                break;

            case QueryAttributesForUpdate:
                return SecError(errSecParam, error, CFSTR("Can't update an item's sharing group"));

            case QueryAttributesForSearch:
            case QueryAttributesForDelete:
                break;
        }
    }

    return true;
}

static bool appClipHasAcceptableAccessGroups(SecurityClient* client) {
    if (!client || !client->applicationIdentifier || !client->accessGroups) {
        secerror("item: no app clip client or attributes not set, cannot verify restrictions");
        return false;
    }

    CFIndex count = CFArrayGetCount(client->accessGroups);
    if (count == 1 && CFEqualSafe(client->applicationIdentifier, CFArrayGetValueAtIndex(client->accessGroups, 0))) {
        return true;
    }

    // sigh, alright is the _other_ access group the application identifier?
    if (count == 2) {
        CFIndex tokenIdx = CFArrayGetFirstIndexOfValue(client->accessGroups, CFRangeMake(0, count), kSecAttrAccessGroupToken);
        if (tokenIdx != kCFNotFound) {
            return CFEqualSafe(client->applicationIdentifier, CFArrayGetValueAtIndex(client->accessGroups, tokenIdx == 0 ? 1 : 0));
        }
    }

    return false;
}

static bool
SecItemServerCopyMatching(CFDictionaryRef query, CFTypeRef *result,
    SecurityClient *client, CFErrorRef *error)
{
    if (!queryHasValidAttributes(query, QueryAttributesForSearch, error)) {
        return false;
    }

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
        CFDataRef uuidData = NULL;
        sqlite_int64 itemRowID = 0;
        if (persistentRef && _SecItemParsePersistentRef(persistentRef, &itemClass, &itemRowID, &uuidData, NULL)) {
            CFStringRef accessGroup = NULL;
            if (SecKeychainIsStaticPersistentRefsEnabled() && uuidData && CFDataGetLength(uuidData) == PERSISTENT_REF_UUID_BYTES_LENGTH) {
                accessGroup = CopyAccessGroupForPersistentRef(itemClass, uuidData);
            } else {
                accessGroup = CopyAccessGroupForRowID(itemRowID, itemClass);
            }
            if (accessGroup && CFStringHasSuffix(accessGroup, kSecNetworkExtensionAccessGroupSuffix) && !CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), accessGroup)) {
                CFMutableArrayRef mutableAccessGroups = CFArrayCreateMutableCopy(NULL, 0, accessGroups);
                CFArrayAppendValue(mutableAccessGroups, accessGroup);
                CFReleaseNull(accessGroups);
                accessGroups = mutableAccessGroups;
            }
            
            CFReleaseNull(accessGroup);
        }
        CFReleaseNull(uuidData);
    }

    if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*"))) {
        /* Having the special accessGroup "*" allows access to all accessGroups. */
        CFReleaseNull(accessGroups);
    }

    bool ok = false;
    Query *q = query_create_with_limit(query, client->musr, 1, client, error);
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
        }

#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
        if (q->q_sync_bubble && client->inEduMode) {
            CFReleaseNull(q->q_musrView);
            q->q_musrView = SecMUSRCreateSyncBubbleUserUUID(q->q_sync_bubble);
        } else if (client->inEduMode && client->isNetworkExtension) {
            CFReleaseNull(q->q_musrView);
            q->q_musrView = SecMUSRCreateBothUserAndSystemUUID(client->uid);
        } else if ((q->q_system_keychain == SystemKeychainFlag_EDUMODE && client->inEduMode) || q->q_system_keychain == SystemKeychainFlag_ALWAYS) {
#else // KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
        if (q->q_system_keychain == SystemKeychainFlag_ALWAYS) {
#endif // !KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
            CFReleaseNull(q->q_musrView);
            q->q_musrView = SecMUSRCopySystemKeychainUUID();
        } else {
            q->q_system_keychain = SystemKeychainFlag_NEITHER;
        }
#endif // KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN

        query_set_caller_access_groups(q, accessGroups);
        if (client->isAppClip && !appClipHasAcceptableAccessGroups(client)) {
            ok = SecError(errSecRestrictedAPI, error, CFSTR("App clips are not permitted to use access groups other than application identifier"));
#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
        } else if (q->q_system_keychain != SystemKeychainFlag_NEITHER && !client->allowSystemKeychain) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for system keychain"));
#endif // KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
        } else if (q->q_sync_bubble && !client->allowSyncBubbleKeychain) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for syncbubble keychain"));
#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
        } else if (q->q_system_keychain != SystemKeychainFlag_NEITHER && q->q_sync_bubble) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("can't do both system and syncbubble keychain"));
#endif // KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
#endif // KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
        } else if (q->q_use_item_list) {
            ok = SecError(errSecUseItemListUnsupported, error, CFSTR("use item list unsupported"));
        } else if (q->q_match_issuer && ((q->q_class != cert_class()) &&
                    (q->q_class != identity_class()))) {
            ok = SecError(errSecUnsupportedOperation, error, CFSTR("unsupported match attribute"));
        } else if (q->q_match_host_or_subdomain && ((q->q_class != inet_class()))) {
            ok = SecError(errSecUnsupportedOperation, error, CFSTR("unsupported kSecMatchHostOrSubdomainOfHost attribute"));
        } else if (q->q_match_policy && ((q->q_class != cert_class()) &&
                    (q->q_class != identity_class()))) {
            ok = SecError(errSecUnsupportedOperation, error, CFSTR("unsupported kSecMatchPolicy attribute"));
        } else if (q->q_return_type != 0 && result == NULL) {
            ok = SecError(errSecReturnMissingPointer, error, CFSTR("missing pointer"));
        } else if (q->q_skip_shared_items && CFDictionaryContainsKey(q->q_item, kSecAttrSharingGroup)) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("can't copy shared items without Keychain Sharing client entitlement"));
        } else if (!q->q_error) {
            ok = kc_with_dbt(false, error, ^(SecDbConnectionRef dbt) {
                return s3dl_copy_matching(dbt, q, result, accessGroups, error);
            });
        }

        if (!query_destroy(q, error)) {
            ok = false;
        }
    }
    CFReleaseNull(accessGroups);

    SecSignpostStop(SecSignpostSecItemCopyMatching);

	return ok;
}

bool
_SecItemCopyMatching(CFDictionaryRef query, SecurityClient *client, CFTypeRef *result, CFErrorRef *error) {
    return SecItemServerCopyMatching(query, result, client, error);
}

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

bool
_SecItemAdd(CFDictionaryRef attributes, SecurityClient *client, CFTypeRef *result, CFErrorRef *error)
{
    if (!queryHasValidAttributes(attributes, QueryAttributesForAdd, error)) {
        return false;
    }

    CFArrayRef accessGroups = SecurityClientCopyWritableAccessGroups(client);

    bool ok = true;
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        CFReleaseNull(accessGroups);
        return SecEntitlementError(error);
    }

    SecSignpostStart(SecSignpostSecItemAdd);

    Query *q = query_create_with_limit(attributes, client->musr, 0, client, error);
    if (q) {
        CFStringRef agrp = (CFStringRef)CFDictionaryGetValue(attributes,
            kSecAttrAccessGroup);

        /* Having the special accessGroup "*" allows access to all accessGroups. */
        if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*"))) {
            CFReleaseNull(accessGroups);
        }

        if (agrp) {
            /* The user specified an explicit access group, validate it. */
            if (!accessGroupsAllows(accessGroups, agrp, client)) {
                ok = SecEntitlementErrorForExplicitAccessGroup(agrp, accessGroups, error);
            }
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
#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
        if ((q->q_system_keychain == SystemKeychainFlag_EDUMODE && client->inEduMode) || q->q_system_keychain == SystemKeychainFlag_ALWAYS) {
            CFReleaseNull(q->q_musrView);
            q->q_musrView = SecMUSRCopySystemKeychainUUID();
        } else {
            q->q_system_keychain = SystemKeychainFlag_NEITHER;
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
            if (SecKeychainIsStaticPersistentRefsEnabled()) {
                CFDataRef uuidData = UUIDDataCreate();
                query_add_attribute(v10itempersistentref.name, uuidData, q);
                CFRetainAssign(q->q_uuid_pref, uuidData);
                CFReleaseNull(uuidData);
            }
            if (client->isAppClip && !appClipHasAcceptableAccessGroups(client)) {
                ok = SecError(errSecRestrictedAPI, error, CFSTR("App clips are not permitted to use access groups other than application identifier"));
            } else if (client->isAppClip && CFEqualSafe(CFDictionaryGetValue(attributes, kSecAttrSynchronizable), kCFBooleanTrue)) {
                ok = SecError(errSecRestrictedAPI, error, CFSTR("App clips are not permitted to add synchronizable items to the keychain"));
#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
            } else if (q->q_system_keychain != SystemKeychainFlag_NEITHER && !client->allowSystemKeychain) {
                ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for system keychain"));
            } else if (q->q_system_keychain == SystemKeychainFlag_ALWAYS && SecItemSynchronizable(attributes)) {
                ok = SecError(errSecItemInvalidKey, error, CFSTR("Can't store system keychain (always) and synchronizable"));
#endif // KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
            } else if (q->q_sync_bubble && !client->allowSyncBubbleKeychain) {
                ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for syncbubble keychain"));
#endif // KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
            } else if (q->q_row_id || q->q_token_object_id) {
                ok = SecError(errSecValuePersistentRefUnsupported, error, CFSTR("q_row_id"));  // TODO: better error string
            } else if (q->q_skip_shared_items && CFDictionaryContainsKey(q->q_item, kSecAttrSharingGroup)) {
                ok = SecError(errSecMissingEntitlement, error, CFSTR("can't add shared item without Keychain Sharing client entitlement"));
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

/// Clones a database item into the given group. This function _mutates the
/// original item_.
static bool
shareDatabaseItemWithGroup(SecDbItemRef item, SecDbConnectionRef dbt, CFStringRef sharingGroup, CFErrorRef *error) {
    const SecDbAttr *uuidAttr = NULL;
    const SecDbAttr *sharingGroupAttr = NULL;
    const SecDbAttr *synchronizableAttr = NULL;
    const SecDbAttr *persistentRefAttr = NULL;
    SecDbForEachAttr(item->class, attr) {
        if (CFEqual(attr->name, kSecAttrUUID)) {
            uuidAttr = attr;
        } else if (CFEqual(attr->name, kSecAttrSharingGroup)) {
            sharingGroupAttr = attr;
        } else if (CFEqual(attr->name, kSecAttrSynchronizable)) {
            synchronizableAttr = attr;
        } else if (attr->kind == kSecDbUUIDAttr) {
            persistentRefAttr = attr;
        }
        if (uuidAttr && sharingGroupAttr && synchronizableAttr && persistentRefAttr) {
            break;
        }
    }
    if (!sharingGroupAttr) {
        return SecError(errSecParam, error, CFSTR("Items of class '%@' can't be shared"), item->class->name);
    }

    // We want to insert a fresh copy of the original item, so forget its old
    // row ID and UUID, and give it a new persistent reference.
    if (!SecDbItemClearRowId(item, error) ||
        (uuidAttr && !SecDbItemSetValue(item, uuidAttr, NULL, error))) {
        return false;
    }
    if (persistentRefAttr) {
        CFDataRef uuidData = UUIDDataCreate();
        bool ret = SecDbItemSetValue(item, persistentRefAttr, uuidData, error);
        CFReleaseNull(uuidData);
        if (!ret) {
            return false;
        }
    }

    // Set the new sharing group for the clone.
    if (!SecDbItemSetValue(item, sharingGroupAttr, sharingGroup, error)) {
        return false;
    }

    // Make sure the clone isn't synced via iCloud Keychain.
    if (synchronizableAttr && !SecDbItemSetValue(item, synchronizableAttr, kCFBooleanFalse, error)) {
        return false;
    }

    CFErrorRef insertError = NULL;
    if (!SecDbItemInsert(item, dbt, false /* always_use_uuid_from_new_item */, false /* always_use_persistentref_from_backup */, &insertError)) {
        if (SecErrorIsSqliteDuplicateItemError(insertError)) {
            CFReleaseNull(insertError);
            return SecError(errSecDuplicateItem, error, CFSTR("Item is already shared with this group"));
        }
        return SecErrorPropagate(insertError, error);
    }

    return true;
}

CFTypeRef
_SecItemShareWithGroup(CFDictionaryRef query, CFStringRef sharingGroup, SecurityClient *client, CFErrorRef *error)
{
    if (sharingGroup == NULL || CFEqualSafe(sharingGroup, kSecAttrSharingGroupNone)) {
        (void)SecError(errSecParam, error, CFSTR("A group must be specified to share the item"));
        return NULL;
    }

    if (!client->allowKeychainSharing) {
        (void)SecError(errSecMissingEntitlement, error, CFSTR("Client doesn't have Keychain Sharing client entitlement"));
        return NULL;
    }

    if (!queryHasValidAttributes(query, QueryAttributesForSearch, error)) {
        return NULL;
    }

    if (CFDictionaryContainsKey(query, kSecAttrTombstone)) {
        (void)SecError(errSecParam, error, CFSTR("Tombstones can't be shared"));
        return NULL;
    }

#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
    if (client->inEduMode) {
        (void)SecError(errSecBadReq, error, CFSTR("This client can't share items"));
        return NULL;
    }
#endif

    // If we were given an explicit access group, use that. If not, we'll use
    // all access groups the client can write to (`*` means the client is
    // entitled to all of them). Since we clone items into the same access
    // groups as their originals, the client must be entitled to write to those
    // groups.
    CFArrayRef accessGroups = SecurityClientCopyWritableAccessGroups(client);
    if (!accessGroups || CFArrayGetCount(accessGroups) == 0) {
        CFReleaseNull(accessGroups);
        (void)SecEntitlementError(error);
        return NULL;
    }
    if (CFArrayContainsValue(accessGroups, CFRangeMake(0, CFArrayGetCount(accessGroups)), CFSTR("*"))) {
        CFReleaseNull(accessGroups);
    }
    CFStringRef explicitAccessGroup = CFDictionaryGetValue(query, kSecAttrAccessGroup);
    if (explicitAccessGroup) {
        if (!accessGroupsAllows(accessGroups, explicitAccessGroup, client)) {
            CFReleaseNull(accessGroups);
            (void)SecEntitlementErrorForExplicitAccessGroup(explicitAccessGroup, accessGroups, error);
            return NULL;
        }
        CFReleaseNull(accessGroups);
        accessGroups = CFArrayCreateForCFTypes(kCFAllocatorDefault, explicitAccessGroup, NULL);
    }

    // First, search for the original items we want to share. The default
    // behavior is to share all matching items if the query doesn't specify a
    // limit.
    Query *q = query_create_with_limit(query, client->musr, kSecMatchUnlimited, client, error);
    if (!q) {
        CFReleaseNull(accessGroups);
        return NULL;
    }
    query_set_caller_access_groups(q, accessGroups);
    bool queryIsValid = true;
    if (!SecMUSRIsSingleUserView(q->q_musrView)) {
        queryIsValid = SecError(errSecBadReq, error, CFSTR("Items from a multi-user view can't be shared"));
    }
#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
    if (queryIsValid && q->q_system_keychain != SystemKeychainFlag_NEITHER) {
        // System keychain items aren't syncable, so it doesn't make sense
        // to share them.
        queryIsValid = SecError(errSecBadReq, error, CFSTR("System keychain items can't be shared"));
    }
#endif
#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
    if (queryIsValid && q->q_sync_bubble) {
        queryIsValid = SecError(errSecBadReq, error, CFSTR("Items in a sync bubble can't be shared"));
    }
#endif

    // The return result keys in the input query apply to the _new shared
    // copies_, not the originals. When searching for the originals, we want to
    // fetch all data and attributes, so that we can copy them.
    ReturnTypeMask originalReturnType = q->q_return_type;
    q->q_return_type = kSecReturnDataMask | kSecReturnAttributesMask;

    CFIndex originalMatchLimit = q->q_limit;

    // Exclude existing clones when searching for the originals, unless the
    // input query explicitly specified a group. This avoids a corner case where
    // omitting the group for an item that hasn't been shared yet will work, but
    // omitting the group for an item that's already shared with a different
    // group will try to copy both the original and the existing clone to the
    // new group, and fail with a duplicate item error.
    if (!CFDictionaryContainsKey(q->q_item, kSecAttrSharingGroup)) {
        query_add_attribute(kSecAttrSharingGroup, kSecAttrSharingGroupNone, q);
    }

    if (queryIsValid && q->q_error) {
        queryIsValid = query_error(q, error);
    }
    if (!queryIsValid) {
        (void)query_destroy(q, NULL);
        CFReleaseNull(accessGroups);
        return NULL;
    }

    __block bool ok = true;
    __block CFErrorRef localError = NULL;
    __block CFMutableArrayRef results = NULL;
    bool didCommit = kc_with_dbt(true, error, ^bool(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, error, ^bool{
            CFMutableArrayRef itemsToShare = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
            ok = SecDbItemQuery(q, accessGroups, dbt, &localError, ^(SecDbItemRef item, bool *stop) {
                CFArrayAppendValue(itemsToShare, item);
            });
            if (!ok) {
                CFReleaseNull(itemsToShare);
                return false;
            }
            if (CFArrayGetCount(itemsToShare) == 0) {
                ok = SecError(errSecItemNotFound, error, CFSTR("No items matched the query"));
                CFReleaseNull(itemsToShare);
                return false;
            }

            results = CFArrayCreateMutableForCFTypesWithCapacity(kCFAllocatorDefault, CFArrayGetCount(itemsToShare));

            for (CFIndex i = 0; i < CFArrayGetCount(itemsToShare); i++) {
                SecDbItemRef item = (SecDbItemRef)CFArrayGetValueAtIndex(itemsToShare, i);

                // Insert a clone of the original item into the database. This
                // call mutates `item` into the cloned item.
                ok = shareDatabaseItemWithGroup(item, dbt, sharingGroup, &localError);
                if (!ok) {
                    break;
                }

                CFTypeRef result = SecDbItemCopyResult(item, originalReturnType, &localError);
                if (!result) {
                    ok = false;
                    break;
                }
                if (!CFEqual(result, kCFNull)) {
                    CFArrayAppendValue(results, result);
                }
                CFReleaseNull(result);
            }

            CFReleaseNull(itemsToShare);
            return ok;
        });
    });

    (void)query_destroy(q, error);
    q = NULL;
    CFReleaseNull(accessGroups);

    if (!ok) {
        CFReleaseNull(results);
        (void)SecErrorPropagate(localError, error);
        return NULL;
    }

    if (!didCommit) {
        // Transaction errors set `error` directly, not `localError`, so we
        // don't use `SecErrorPropagate` here.
        CFReleaseNull(results);
        return NULL;
    }

    SecKeychainChanged();
    SecSharedItemsChanged();

    CFTypeRef result = NULL;
    if (originalReturnType) {
        // If the query specified a return result key, the return type depends
        // on the match limit: for `kSecMatchLimitOne`, return the (one) result;
        // for `kSecMatchLimitAll`, return an immutable copy of the entire
        // array.
        if (originalMatchLimit == 1) {
            assert(CFArrayGetCount(results) == 1);
            result = CFArrayGetCount(results) == 1 ? CFRetain(CFArrayGetValueAtIndex(results, 0)) : NULL;
        } else {
            result = CFArrayCreateCopy(kCFAllocatorDefault, results);
        }
    } else {
        // If the query didn't specify a return result key, don't return
        // anything.
        result = kCFNull;
    }
    CFReleaseNull(results);

    return result;
}

bool
_SecDeleteItemsOnSignOut(SecurityClient *client, CFErrorRef *error)
{
    bool ok = kc_with_dbt(true, error, ^bool (SecDbConnectionRef dbt) {
        return kc_transaction(dbt, error, ^bool {
            __block bool didDelete = true;

            CFDataRef musr = client->musr ?: SecMUSRGetSingleUserKeychainUUID();
            const SecDbClass *classes[] = {
                inet_class(),
                genp_class(),
                cert_class(),
                keys_class(),
            };

            CFArrayRef accessGroups = CFArrayCreateForCFTypes(
                NULL,
                CFSTR("com.apple.safari.credit-cards"),
                CFSTR("com.apple.cfnetwork"),
                CFSTR("com.apple.cfnetwork-recently-deleted"),
                CFSTR("com.apple.password-manager"),
                CFSTR("com.apple.password-manager-recently-deleted"),
                CFSTR("com.apple.password-manager.personal"),
                CFSTR("com.apple.password-manager.personal-recently-deleted"),
                CFSTR("com.apple.webkit.webauthn"),
                CFSTR("com.apple.webkit.webauthn-recently-deleted"),
                CFSTR("com.apple.password-manager.generated-passwords"),
                CFSTR("com.apple.password-manager.generated-passwords-recently-deleted"),
                NULL
            );

            for (size_t classIndex = 0; didDelete && classIndex < array_size(classes); classIndex++) {
                secinfo("SecDeleteItemsOnSignOut", "Deleting items from class=%@ with multi-user view=%@", classes[classIndex]->name, musr);

                Query *query = query_create(classes[classIndex], musr, NULL, client, error);
                if (!query) {
                    didDelete = false;
                    break;
                }
                query_add_attribute(kSecAttrMultiUser, musr, query);
                query_add_attribute(kSecAttrSynchronizable, kCFBooleanTrue, query);
                for (CFIndex groupIndex = 0; groupIndex < CFArrayGetCount(accessGroups); groupIndex++) {
                    query_add_or_attribute(kSecAttrAccessGroup, CFArrayGetValueAtIndex(accessGroups, groupIndex), query);
                }
                didDelete &= SecDbItemSelect(query, dbt, error, NULL, ^bool(const SecDbAttr *attr) {
                    return CFDictionaryContainsKey(query->q_item, attr->name);
                }, NULL, NULL, ^(SecDbItemRef item, bool *stop) {
                    didDelete &= SecDbItemDelete(item, dbt, kCFBooleanFalse, false, error);
                    if (!didDelete) {
                        *stop = true;
                    }
                });
                (void)query_destroy(query, NULL);
            }

            CFReleaseNull(accessGroups);

            return didDelete;
        });
    });

    if (ok) {
        SecKeychainChanged();
        SecSharedItemsChanged();
    }

    return ok;
}

bool
_SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate,
               SecurityClient *client, CFErrorRef *error)
{
    if (!queryHasValidAttributes(query, QueryAttributesForSearch, error) || !queryHasValidAttributes(attributesToUpdate, QueryAttributesForUpdate, error)) {
        return false;
    }

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
    Query *q = query_create_with_limit(query, client->musr, kSecMatchUnlimited, client, error);
    if (!q) {
        ok = false;
    }
    if (ok) {
#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
        if ((q->q_system_keychain == SystemKeychainFlag_EDUMODE && client->inEduMode) || q->q_system_keychain == SystemKeychainFlag_ALWAYS) {
            CFReleaseNull(q->q_musrView);
            q->q_musrView = SecMUSRCopySystemKeychainUUID();
        } else {
            q->q_system_keychain = SystemKeychainFlag_NEITHER;
        }
#endif // KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
        query_set_caller_access_groups(q, accessGroups);
        if (client->isAppClip && !appClipHasAcceptableAccessGroups(client)) {
            ok = SecError(errSecRestrictedAPI, error, CFSTR("App clips are not permitted to use access groups other than application identifier"));
        } else if (client->isAppClip && (CFEqualSafe(CFDictionaryGetValue(query, kSecAttrSynchronizable), kCFBooleanTrue) ||
            CFEqualSafe(CFDictionaryGetValue(attributesToUpdate, kSecAttrSynchronizable), kCFBooleanTrue)))
        {
            ok = SecError(errSecRestrictedAPI, error, CFSTR("App clips are not permitted to make items synchronizable"));
#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
        } else if (q->q_system_keychain != SystemKeychainFlag_NEITHER && !client->allowSystemKeychain) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for system keychain"));
        } else if (q->q_system_keychain == SystemKeychainFlag_ALWAYS && SecItemSynchronizable(attributesToUpdate)) {
            ok = SecError(errSecItemInvalidKey, error, CFSTR("Can't update a system keychain (always) item with synchronizable"));
#endif // KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
        } else if (q->q_sync_bubble && !client->allowSyncBubbleKeychain) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for syncbubble keychain"));
#endif // KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
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
        } else if (q->q_skip_shared_items && CFDictionaryContainsKey(q->q_item, kSecAttrSharingGroup)) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("can't update shared items without Keychain Sharing client entitlement"));
        } else {
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

bool
_SecItemDelete(CFDictionaryRef query, SecurityClient *client, CFErrorRef *error)
{
    if (!queryHasValidAttributes(query, QueryAttributesForDelete, error)) {
        return false;
    }

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

    Query *q = query_create_with_limit(query, client->musr, kSecMatchUnlimited, client, error);
    bool ok;
    if (q) {
#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
        if ((q->q_system_keychain == SystemKeychainFlag_EDUMODE && client->inEduMode) || q->q_system_keychain == SystemKeychainFlag_ALWAYS) {
            CFReleaseNull(q->q_musrView);
            q->q_musrView = SecMUSRCopySystemKeychainUUID();
        } else {
            q->q_system_keychain = SystemKeychainFlag_NEITHER;
        }
#endif // KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN

        query_set_caller_access_groups(q, accessGroups);
        if (client->isAppClip && !appClipHasAcceptableAccessGroups(client)) {
            ok = SecError(errSecRestrictedAPI, error, CFSTR("App clips are not permitted to use access groups other than application identifier"));
#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
        } else if (q->q_system_keychain != SystemKeychainFlag_NEITHER && !client->allowSystemKeychain) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for system keychain"));
#endif // KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
        } else if (q->q_sync_bubble && !client->allowSyncBubbleKeychain) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("client doesn't have entitlement for syncbubble keychain"));
#endif // KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
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
        } else if (q->q_skip_shared_items && CFDictionaryContainsKey(q->q_item, kSecAttrSharingGroup)) {
            ok = SecError(errSecMissingEntitlement, error, CFSTR("can't delete shared items without Keychain Sharing client entitlement"));
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
    CFDictionaryRef query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, kSecClass, classToDelete, kSecAttrTokenID, tokenID, NULL);
    Query *q = query_create_with_limit(query, client->musr, kSecMatchUnlimited, client, error);
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

static bool SecItemAddTokenItemToAccessGroups(SecDbConnectionRef dbt, CFDictionaryRef attributes, CFArrayRef accessGroups, SecurityClient *client, CFErrorRef *error) {
    Query *q;
    bool ok = false;
    for (CFIndex i = 0; i < CFArrayGetCount(accessGroups); ++i) {
        require_quiet(q = query_create_with_limit(attributes, client->musr, 0, client, error), out);
        CFStringRef agrp = CFArrayGetValueAtIndex(accessGroups, i);
        query_add_attribute(kSecAttrAccessGroup, agrp, q);
        query_ensure_access_control(q, agrp);
        bool added = false;
        if (!q->q_error) {
            query_pre_add(q, true);
            added = s3dl_query_add(dbt, q, NULL, error);
        }
        require_quiet(query_notify_and_destroy(q, added, error), out);
    }
    ok = true;
out:
    return ok;
}

bool _SecItemUpdateTokenItemsForAccessGroups(CFStringRef tokenID, CFArrayRef accessGroups, CFArrayRef items, SecurityClient *client, CFErrorRef *error) {
    // This is SPI for CTK only, don't even listen to app clips.
    if (client->isAppClip) {
        return SecError(errSecRestrictedAPI, error, CFSTR("App Clips may not call this API"));
    }

    return kc_with_dbt(true, error, ^bool (SecDbConnectionRef dbt) {
        return kc_transaction(dbt, error, ^bool {
            bool ok = false;
            CFErrorRef localError = NULL;
            const CFTypeRef classToDelete[] = { kSecClassGenericPassword, kSecClassInternetPassword, kSecClassCertificate, kSecClassKey };
            for (size_t i = 0; i < sizeof(classToDelete) / sizeof(classToDelete[0]); ++i) {
                if (!SecItemDeleteTokenItems(dbt, classToDelete[i], tokenID, accessGroups, client, &localError)) {
                    require_action_quiet(CFErrorGetCode(localError) == errSecItemNotFound, out, CFErrorPropagate(localError, error));
                    CFReleaseNull(localError);
                }
            }

            if (items) {
                for (CFIndex i = 0; i < CFArrayGetCount(items); ++i) {
                    require_quiet(SecItemAddTokenItemToAccessGroups(dbt, CFArrayGetValueAtIndex(items, i), accessGroups, client, error), out);
                }
            }

            ok = true;
        out:
            return ok;
        });
    });
}

static bool deleteNonSysboundItemsForItemClass(SecDbConnectionRef dbt, SecDbClass const* class, CFErrorRef* error) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutableForCFTypes(NULL);
    CFDictionaryAddValue(query, kSecMatchLimit, kSecMatchLimitAll);

    __block CFErrorRef localError = NULL;
    SecDbQueryRef q = query_create(class, NULL, query, NULL, &localError);
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
            SecDbItemDelete(item, dbt, kCFBooleanFalse, false, &localError);
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

    // strange construction needed for schema indirection
    const CFTypeRef qclassesPtrs[] = {
        inet_class(),
        genp_class(),
        keys_class(),
        cert_class(),
    };
    // have to do another layer of indirection because the block below cannot refer to a declaration with an array type
    const CFTypeRef* qclasses = qclassesPtrs;

    require_action_quiet(isArray(accessGroups), fail,
                         ok = false;
                         SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedType, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("accessGroups not CFArray, got %@"), accessGroups));

    // TODO: allowlist instead? look for dev IDs like 7123498YQX.com.somedev.app

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

            for (n = 0; n < array_size(qclassesPtrs) && ok1; n++) {
                Query *q;

                q = query_create(qclasses[n], client->musr, NULL, client, error);
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

// OSX now has SWC enabled, but cannot link SharedWebCredentials framework: rdar://59958701
#if TARGET_OS_IOS && !TARGET_OS_BRIDGE && !TARGET_OS_WATCH && !TARGET_OS_TV

/* constants */
#define SEC_CONST_DECL(k,v) const CFStringRef k = CFSTR(v);

SEC_CONST_DECL (kSecSafariAccessGroup, "com.apple.cfnetwork");
SEC_CONST_DECL (kSecSafariDefaultComment, "default");
SEC_CONST_DECL (kSecSafariPasswordsNotSaved, "Passwords not saved");

static bool
_SecAddNegativeWebCredential(SecurityClient *client, CFStringRef fqdn, CFStringRef appID, bool forSafari)
{
#if !TARGET_OS_SIMULATOR
    bool result = false;
    if (!fqdn) { return result; }

    // update our database
	_SecSetAppDomainApprovalStatus(appID, fqdn, kCFBooleanFalse);

    if (!forSafari) { return result; }

    // below this point: create a negative Safari web credential item

    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!attrs) { return result; }

    CFErrorRef error = NULL;
    CFStringRef accessGroup = CFSTR("*");
    SecurityClient swcclient = {
        .task = NULL,
        .accessGroups =  CFArrayCreate(kCFAllocatorDefault, (const void **)&accessGroup, 1, &kCFTypeArrayCallBacks),
#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
        .allowSystemKeychain = false,
#endif
#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
        .allowSyncBubbleKeychain = false,
#endif
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
        SecError(errSecBadReq, error, CFSTR("Password AutoFill for iCloud Keychain must be enabled in Settings > Passwords to save passwords"));
        goto cleanup;
    }

    // parse fqdn with CFURL here, since it could be specified as domain:port
    if (fqdn) {
		CFTypeRef fqdnObject = _SecCopyFQDNObjectFromString(fqdn);
		if (fqdnObject) {
			CFReleaseSafe(fqdn);
			fqdn = _SecGetFQDNFromFQDNObject(fqdnObject, &port);
			CFRetainSafe(fqdn);
			CFReleaseSafe(fqdnObject);
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
    if (_SecEntitlementContainsDomainForService(domains, fqdn, port)) {
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
    SecSWCFlags flags = _SecAppDomainApprovalStatus(appID, fqdn, error);
    if (!(flags & kSecSWCFlag_SiteApproved)) {
        goto cleanup;
    }
#endif

    // give ourselves access to see matching items for kSecSafariAccessGroup
    swcclient.task = NULL;
    swcclient.accessGroups =  CFArrayCreate(kCFAllocatorDefault, (const void **)&accessGroup, 1, &kCFTypeArrayCallBacks);
    swcclient.musr = client->musr;
#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
    swcclient.allowSystemKeychain = false;
#endif
#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
    swcclient.allowSyncBubbleKeychain = false;
#endif
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

#else /* !(TARGET_OS_IOS && !TARGET_OS_BRIDGE && !TARGET_OS_WATCH && !TARGET_OS_TV) */

bool _SecAddSharedWebCredential(CFDictionaryRef attributes, SecurityClient *client, const audit_token_t *clientAuditToken, CFStringRef appID, CFArrayRef domains, CFTypeRef *result, CFErrorRef *error) {
    if (error) {
        SecError(errSecUnimplemented, error, CFSTR("_SecAddSharedWebCredential not supported on this platform"));
    }
    return false;
}

#endif /* !(TARGET_OS_IOS && !TARGET_OS_BRIDGE && !TARGET_OS_WATCH && !TARGET_OS_TV) */
#endif /* SHAREDWEBCREDENTIALS */


// MARK: -
// MARK: Keychain backup

CF_RETURNS_RETAINED CFDataRef
_SecServerKeychainCreateBackup(SecurityClient *client, CFDataRef keybag, CFDataRef passcode, bool emcs, CFErrorRef *error) {
    __block CFDataRef backup = NULL;
    kc_with_dbt(false, error, ^bool (SecDbConnectionRef dbt) {

        LKABackupReportStart(!!keybag, !!passcode, emcs);

        return kc_transaction_type(dbt, kSecDbNormalTransactionType, error, ^bool{
            secnotice("SecServerKeychainCreateBackup", "Performing backup from %s keybag%s", keybag ? "provided" : "device", emcs ? ", EMCS mode" : "");

            if (keybag == NULL && passcode == NULL) {
#if USE_KEYSTORE
                // destination keybag pointer is NULL, which means to ask AKS to use the currently-configured backup bag
                backup = SecServerExportBackupableKeychain(dbt, client, NULL, error);
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
        SecSharedItemsChanged();
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
                            secnotice("titc", "restore " SECDBITEM_FMT " not replacing existing item", item);
                        } else if (status == errSecDecode) {
                            // Log and ignore corrupted item errors during restore
                            secnotice("titc", "restore " SECDBITEM_FMT " skipping corrupted item %@", item, localError);
                        } else {
                            if (status == errSecInteractionNotAllowed)
                                *stop = true;
                            // Propagate the first other error upwards (causing the restore to fail).
                            secerror("restore " SECDBITEM_FMT " failed %@", item, localError);
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

#else /* SECUREOBJECTSYNC */

SOSDataSourceFactoryRef SecItemDataSourceFactoryGetDefault(void) {
    return NULL;
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

    q = query_create(qclass, NULL, NULL, NULL, error);
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
_SecServerCopyInitialSyncCredentials(uint32_t flags, uint64_t* tlks, uint64_t* pcs, uint64_t* bluetooth, CFErrorRef *error)
{
    CFMutableArrayRef items = CFArrayCreateMutableForCFTypes(NULL);

    uint64_t numberOfTlks = 0;
    uint64_t numberOfPCS = 0;
    uint64_t numberOfBluetooth = 0;

    if (flags & SecServerInitialSyncCredentialFlagTLK) {
        require_action(InitialSyncItems(items, false, CFSTR("com.apple.security.ckks"), NULL, inet_class(), error), fail,
                       secerror("failed to collect CKKS-inet keys: %@", error ? *error : NULL));
        numberOfTlks = CFArrayGetCount(items);
    }
    if (flags & SecServerInitialSyncCredentialFlagPCS) {
        bool onlyCurrent = !(flags & SecServerInitialSyncCredentialFlagPCSNonCurrent);

        require_action(InitialSyncItems(items, false, CFSTR("com.apple.ProtectedCloudStorage"), NULL, genp_class(), error), fail,
                       secerror("failed to collect PCS-genp keys: %@", error ? *error : NULL));
        require_action(InitialSyncItems(items, onlyCurrent, CFSTR("com.apple.ProtectedCloudStorage"), NULL, inet_class(), error), fail,
                       secerror("failed to collect PCS-inet keys: %@", error ? *error : NULL));
        numberOfPCS = CFArrayGetCount(items) - numberOfTlks;

    }
    if (flags & SecServerInitialSyncCredentialFlagBluetoothMigration) {
        require_action(InitialSyncItems(items, false, CFSTR("com.apple.nanoregistry.migration"), NULL, genp_class(), error), fail,
                       secerror("failed to collect com.apple.nanoregistry.migration-genp item: %@", error ? *error : NULL));
        require_action(InitialSyncItems(items, false, CFSTR("com.apple.nanoregistry.migration2"), NULL, genp_class(), error), fail,
                       secerror("failed to collect com.apple.nanoregistry.migration2-genp item: %@", error ? *error : NULL));
        require_action(InitialSyncItems(items, false, CFSTR("com.apple.bluetooth"), CFSTR("BluetoothLESync"), genp_class(), error), fail,
                       secerror("failed to collect com.apple.bluetooth-genp item: %@", error ? *error : NULL));

        numberOfBluetooth = CFArrayGetCount(items) - numberOfTlks - numberOfPCS;
    }

    *tlks = numberOfTlks;
    *pcs = numberOfPCS;
    *bluetooth = numberOfBluetooth;
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
                    secinfo("ImportInitialSyncItems", "Failed to set sync=1: %@ for item " SECDBITEM_FMT, cferror, dbi);
                    CFReleaseNull(cferror);
                    CFReleaseNull(dbi);
                    continue;
                }

                if (!SecDbItemInsert(dbi, dbt, false, false, &cferror)) {
                    secinfo("ImportInitialSyncItems", "Item store failed with: %@: " SECDBITEM_FMT, cferror, dbi);
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

        q = query_create(qclass, NULL, NULL, client, error);
        require(q, fail);

        q->q_limit = kSecMatchUnlimited;
        q->q_keybag = KEYBAG_DEVICE; //keybag when querying in the normal user keychain in edu mode

        for (n = 0; n < nItems; n++) {
            query_add_attribute(items[n].attribute, items[n].value, q);
        }
        q->q_musrView = CFRetain(syncBubbleView);
        require(q->q_musrView, fail);

        kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
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

        q = query_create(qclass, NULL, NULL, client, error);
        require(q, fail);

        q->q_return_type = kSecReturnDataMask | kSecReturnAttributesMask;
        q->q_limit = kSecMatchUnlimited;
        q->q_keybag = KEYBAG_DEVICE; // keybag when querying in the normal user keychain in edu mode

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

                    SecDbItemRef new_item = SecDbItemCopyWithUpdates(item, updateAttributes, &error3);
                    if (new_item == NULL)
                    {
                        secnotice("syncbubble", "migration failed, no new_item %@", error3);
                        CFReleaseNull(error3);
                        return;
                    }

                    SecDbItemClearRowId(new_item, NULL);

                    if (!SecDbItemSetKeybag(new_item, KEYBAG_DEVICE, NULL)) { // keybag when storing the transmogrified item into the syncbubble keychain
                        CFRelease(new_item);
                        return;
                    }

                    if (!SecDbItemInsert(new_item, dbt, false, false, &error3)) {
                        secnotice("syncbubble", "migration failed with %@ for item " SECDBITEM_FMT, error3, new_item);
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

    if (!client->inEduMode)
        return false;

    SecSignpostStart(SecSignpostSecSyncBubbleTransfer);

    secnotice("syncbubble", "migration for uid %d for services %@", (int)uid, services);

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
    secnotice("syncbubble", "migration for uid %d complete", (int)uid);
    SecSignpostStop(SecSignpostSecSyncBubbleTransfer);
    return res;
}

/*
 * Migrate from user keychain to system keychain when switching to edu mode
 */

bool
_SecServerTransmogrifyToSystemKeychain(SecurityClient *client, CFErrorRef *error)
{
    __block bool ok = true;

    SecSignpostStart(SecSignpostSecSystemTransfer);
    secnotice("transmogrify", "begin");

    /*
     * we are not in multi user yet, about to switch, otherwise we would
     * check that for client->inEduMode here
     */

    kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, error, ^{
            CFDataRef systemUUID = SecMUSRGetSystemKeychainUUID();

            const SecDbSchema *newSchema = current_schema();
            SecDbClass const *const *kcClass;

            for (kcClass = newSchema->classes; *kcClass != NULL; kcClass++) {
                Query *q = NULL;

                if (!((*kcClass)->itemclass)) {
                    continue;
                }

                q = query_create(*kcClass, SecMUSRGetSingleUserKeychainUUID(), NULL, client, error);
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
                        secerror("item: " SECDBITEM_FMT " update musr to system failed: %@", item, itemError);
                        ok = false;
                        goto out;
                    }

                    if (!SecDbItemDoUpdate(item, item, dbt, &itemError, ^bool (const SecDbAttr *attr) {
                        return attr->kind == kSecDbRowIdAttr;
                    })) {
                        secerror("item: " SECDBITEM_FMT " insert during UPDATE: %@", item, itemError);
                        ok = false;
                        goto out;
                    }

                out:
                    SecErrorPropagate(itemError, error);
                });

                query_destroy(q, NULL);

            }
            return (bool)true;
        });
    });

#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
    // If Enhanced APFS is already enabled, then we also need to transcrypt the DB, as the current keybag will go away soon.
    if (SecSupportsEnhancedApfs() && !SecSeparateUserKeychain()) {
        kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
            if (SecKeychainDbGetEduModeTranscrypted(dbt)) {
                secnotice("transmogrify", "Unexpectedly already transcrypted??");
                return true;
            }
            kc_transaction(dbt, error, ^{
                secnotice("transmogrify", "must transcrypt, using default keybag");
                // Since we are transmogrifying, the keybag will already be set to the default, not the system keychain keybag.
                ok = InternalTranscryptToSystemKeychainKeybag(dbt, client, error) && ok;
                return true;
            });

            secnotice("transmogrify", "transcrypted, setting flag to remember we've already done so");
            SecKeychainDbSetEduModeTranscrypted(dbt);

            // Now we need to use the system keychain keybag.
            // Otherwise, all item decryptions will fail, causing accessed items to be dropped on the floor.
            secnotice("transmogrify", "transcrypted, using system keychain handle");
            SecItemServerSetKeychainKeybag(system_keychain_handle);

            return true;
        });
    }
#endif // KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER

    secnotice("transmogrify", "end");
    SecSignpostStop(SecSignpostSecSystemTransfer);

    return ok;
}

/*
 * Transcrypt system keychain items from user keychain to system keychain when switching to edu mode
 */

bool
_SecServerTranscryptToSystemKeychainKeybag(SecurityClient *client, CFErrorRef *error)
{
    __block bool ok = false;

#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
    SecSignpostStart(SecSignpostSecSystemTranscrypt);
    secnotice("transcrypt", "begin");

    kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
        ok = InternalTranscryptToSystemKeychainKeybag(dbt, client, error);
        return ok;
    });

    secnotice("transcrypt", "end");
    SecSignpostStop(SecSignpostSecSystemTranscrypt);
#endif // KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER

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
        kSecAttrSubject,
        kSecAttrSynchronizable
    };
    const void *values[] = {
        kSecClassCertificate,
        kCFBooleanTrue,
        kSecMatchLimitAll,
        normalizedIssuer,
        kSecAttrSynchronizableAny
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, sizeof(keys)/sizeof(*keys),
                                               &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFTypeRef results = NULL;
    SecurityClient client = {
        .task = NULL,
        .accessGroups = accessGroups,
#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
        .allowSystemKeychain = true,
#endif
#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
        .allowSyncBubbleKeychain = false,
#endif
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
        kSecAttrSerialNumber,
        kSecAttrSynchronizable
    };
    const void *values[] = {
        kSecClassCertificate,
        kSecMatchLimitOne,
        normalizedIssuer,
        serialNumber,
        kSecAttrSynchronizableAny
    };
    SecurityClient client = {
        .task = NULL,
        .accessGroups = accessGroups,
#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
        .allowSystemKeychain = true,
#endif
#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
        .allowSyncBubbleKeychain = false,
#endif
        .isNetworkExtension = false,
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, sizeof(keys)/sizeof(*keys),
                                               &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFTypeRef results = NULL;
    bool ok = _SecItemCopyMatching(query, &client, &results, error);
    CFReleaseSafe(query);
    CFReleaseSafe(results);
    if (!ok) {
        return false;
    }
    return true;
}
