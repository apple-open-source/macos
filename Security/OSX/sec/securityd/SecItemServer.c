/*
 * Copyright (c) 2006-2015 Apple Inc. All Rights Reserved.
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

#include <securityd/SecItemServer.h>

#include <notify.h>
#include <securityd/SecItemDataSource.h>
#include <securityd/SecItemDb.h>
#include <securityd/SecItemSchema.h>
#include <securityd/SOSCloudCircleServer.h>
#include <Security/SecBasePriv.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemInternal.h>
#include <Security/SecureObjectSync/SOSChangeTracker.h>
#include <Security/SecureObjectSync/SOSDigestVector.h>
#include <Security/SecureObjectSync/SOSViews.h>

// TODO: Make this include work on both platforms. rdar://problem/16526848
#if TARGET_OS_EMBEDDED
#include <Security/SecEntitlements.h>
#else
/* defines from <Security/SecEntitlements.h> */
#define kSecEntitlementAssociatedDomains CFSTR("com.apple.developer.associated-domains")
#define kSecEntitlementPrivateAssociatedDomains CFSTR("com.apple.private.associated-domains")
#endif

#include <utilities/array_size.h>
#include <utilities/SecFileLocations.h>
#include <Security/SecuritydXPC.h>
#include "swcagent_client.h"

#if TARGET_OS_IPHONE && !TARGET_OS_NANO
#include <dlfcn.h>
#include <SharedWebCredentials/SharedWebCredentials.h>

typedef OSStatus (*SWCCheckService_f)(CFStringRef service, CFStringRef appID, CFStringRef domain, SWCCheckServiceCompletion_b completion);
typedef OSStatus (*SWCSetServiceFlags_f)(CFStringRef service, CFStringRef appID, CFStringRef domain, SWCFlags mask, SWCFlags flags, SWCSetServiceFlagsCompletion_b completion);
#else
typedef uint32_t SWCFlags;
#define kSWCFlags_None				0
#define kSWCFlag_Pending			( 1U << 0 )
#define kSWCFlag_SiteApproved		( 1U << 1 )
#define kSWCFlag_SiteDenied			( 1U << 2 )
#define kSWCFlag_UserApproved		( 1U << 3 )
#define kSWCFlag_UserDenied			( 1U << 4 )
#define kSWCFlag_ExternalMask		( kSWCFlag_UserApproved | kSWCFlag_UserDenied )
#endif

/* Changed the name of the keychain changed notification, for testing */
static const char *g_keychain_changed_notification = kSecServerKeychainChangedNotification;

void SecItemServerSetKeychainChangedNotification(const char *notification_name)
{
    g_keychain_changed_notification = notification_name;
}

void SecKeychainChanged(bool syncWithPeers) {
    uint32_t result = notify_post(g_keychain_changed_notification);
    if (syncWithPeers)
        SOSCCSyncWithAllPeers();
    if (result == NOTIFY_STATUS_OK)
        secnotice("item", "Sent %s%s", syncWithPeers ? "SyncWithAllPeers and " : "", g_keychain_changed_notification);
    else
        secerror("%snotify_post %s returned: %" PRIu32, syncWithPeers ? "Sent SyncWithAllPeers, " : "", g_keychain_changed_notification, result);
}

/* Return the current database version in *version. */
static bool SecKeychainDbGetVersion(SecDbConnectionRef dbt, int *version, CFErrorRef *error)
{
    __block bool ok = false;
    SecDbQueryRef query = NULL;
    __block CFNumberRef versionNumber = NULL;
    __block CFErrorRef localError = NULL;

    require_quiet(query = query_create(&tversion_class, NULL, &localError), out);
    require_quiet(SecDbItemSelect(query, dbt, &localError, ^bool(const SecDbAttr *attr) {
        // Bind all attributes.
        return true;
    }, ^bool(const SecDbAttr *attr) {
        // No filtering.
        return false;
    }, NULL, NULL, ^(SecDbItemRef item, bool *stop) {
        versionNumber = copyNumber(SecDbItemGetValue(item, tversion_class.attrs[0], &localError));
        *stop = true;
    }), out);

    require_action_quiet(versionNumber != NULL && CFNumberGetValue(versionNumber, kCFNumberIntType, version), out,
                         // We have a tversion table but we didn't find a single version
                         // value, now what? I suppose we pretend the db is corrupted
                         // since this isn't supposed to ever happen.
                         SecDbError(SQLITE_CORRUPT, error, CFSTR("Failed to read version table"));
                         secwarning("tversion read error: %@", error ? *error : NULL));
    ok = true;

out:
    if (!ok && CFErrorGetCode(localError) == SQLITE_ERROR) {
        // Most probably means that the version table does not exist at all.
        // TODO: Use "SELECT name FROM sqlite_master WHERE type='table' AND name='tversion'" to detect tversion presence.
        CFReleaseSafe(localError);
        version = 0;
        ok = true;
    }
    if (query)
        query_destroy(query, NULL);
    CFReleaseSafe(versionNumber);
    return ok || CFErrorPropagate(localError, error);
}

// Goes through all tables represented by old_schema and tries to migrate all items from them into new (current version) tables.
static bool SecKeychainDbUpgradeFromSchema(SecDbConnectionRef dbt, const SecDbSchema *oldSchema, bool *inProgress, CFErrorRef *error) {
    __block bool ok = true;
    const SecDbSchema *newSchema = kc_schemas[0];
    SecDbClass const *const *oldClass;
    SecDbClass const *const *newClass;
    SecDbQueryRef query = NULL;
    CFMutableStringRef sql = NULL;

    // Rename existing tables to old names, as present in old schemas.
    sql = CFStringCreateMutable(NULL, 0);
    for (oldClass = oldSchema->classes, newClass = newSchema->classes;
         *oldClass != NULL && *newClass != NULL; oldClass++, newClass++) {
        if (!CFEqual((*oldClass)->name, (*newClass)->name)) {
            CFStringAppendFormat(sql, NULL, CFSTR("ALTER TABLE %@ RENAME TO %@;"),
                                 (*newClass)->name, (*oldClass)->name);
        } else {
            CFStringAppendFormat(sql, NULL, CFSTR("DROP TABLE %@;"), (*oldClass)->name);
        }
    }
    require_quiet(ok &= SecDbExec(dbt, sql, error), out);

    // Create tables for new schema.
    require_quiet(ok &= SecItemDbCreateSchema(dbt, newSchema, error), out);
    // Go through all classes of current schema to transfer all items to new tables.
    for (oldClass = oldSchema->classes, newClass = newSchema->classes;
         *oldClass != NULL && *newClass != NULL; oldClass++, newClass++) {
        if (CFEqual((*oldClass)->name, (*newClass)->name))
            continue;

        // Prepare query to iterate through all items in cur_class.
        if (query != NULL)
            query_destroy(query, NULL);
        require_quiet(query = query_create(*oldClass, NULL, error), out);

        ok &= SecDbItemSelect(query, dbt, error, ^bool(const SecDbAttr *attr) {
            // We are interested in all attributes which are physically present in the DB.
            return (attr->flags & kSecDbInFlag) != 0;
        }, ^bool(const SecDbAttr *attr) {
            // No filtering please.
            return false;
        }, NULL, NULL, ^(SecDbItemRef item, bool *stop) {
            CFErrorRef localError = NULL;

            // Switch item to the new class.
            item->class = *newClass;

            // Decrypt the item.
            if (SecDbItemEnsureDecrypted(item, &localError)) {
                // Delete SHA1 field from the item, so that it is newly recalculated before storing
                // the item into the new table.
                require_quiet(ok &= SecDbItemSetValue(item, SecDbClassAttrWithKind(item->class, kSecDbSHA1Attr, error),
                                                      kCFNull, error), out);
            } else {
                OSStatus status = SecErrorGetOSStatus(localError);

                // Items producing errSecDecode are silently dropped - they are not decodable and lost forever.
                require_quiet(status != errSecDecode, out);

                // errSecAuthNeeded means that it is an ACL-based item which requires authentication (or at least
                // ACM context, which we do not have).  Other errors should abort the migration completely.
                require_action_quiet(status == errSecAuthNeeded || status == errSecInteractionNotAllowed, out,
                                     ok &= CFErrorPropagate(localError, error); localError = NULL);

                // If we've hit item which could not be decoded because of locked keybag, store it into
                // new tables so that accessing it fails with errSecInteractionNotAllowed instead of errSecItemNotFound.
                // Next connection to the DB with opened keybag will properly decode and replace the item.
                if (status == errSecInteractionNotAllowed) {
                    *inProgress = true;
                }

                // Leave item encrypted, do not ever try to decrypt it since it will fail.
                item->_edataState = kSecDbItemAlwaysEncrypted;
            }

            // Insert new item into the new table.
            if (!SecDbItemInsert(item, dbt, &localError)) {
                secerror("item: %@ insert during upgrade: %@", item, localError);
                ok &= CFErrorPropagate(localError, error); localError = NULL;
            }

        out:
            CFReleaseSafe(localError);
            *stop = !ok;
        });
        require_quiet(ok, out);
    }

    // Remove old tables from the DB.
    CFAssignRetained(sql, CFStringCreateMutable(NULL, 0));
    for (oldClass = oldSchema->classes, newClass = newSchema->classes;
         *oldClass != NULL && *newClass != NULL; oldClass++, newClass++) {
        if (!CFEqual((*oldClass)->name, (*newClass)->name)) {
            CFStringAppendFormat(sql, NULL, CFSTR("DROP TABLE %@;"), (*oldClass)->name);
        }
    }
    require_quiet(ok &= SecDbExec(dbt, sql, error), out);

out:
    if (query != NULL) {
        query_destroy(query, NULL);
    }
    CFReleaseSafe(sql);
    return ok;
}

// Goes through all tables represented by old_schema and tries to migrate all items from them into new (current version) tables.
static bool SecKeychainDbUpgradeUnlockedItems(SecDbConnectionRef dbt, bool *inProgress, CFErrorRef *error) {
    __block bool ok = true;
    SecDbQueryRef query = NULL;

    // Go through all classes in new schema
    const SecDbSchema *newSchema = kc_schemas[0];
    for (const SecDbClass *const *class = newSchema->classes; *class != NULL && !*inProgress; class++) {
        const SecDbAttr *pdmn = SecDbClassAttrWithKind(*class, kSecDbAccessAttr, error);
        if (pdmn == nil) {
            continue;
        }

        // Prepare query to go through all non-DK|DKU items
        if (query != NULL) {
            query_destroy(query, NULL);
        }
        require_action_quiet(query = query_create(*class, NULL, error), out, ok = false);
        ok = SecDbItemSelect(query, dbt, error, NULL, ^bool(const SecDbAttr *attr) {
            // No simple per-attribute filtering.
            return false;
        }, ^bool(CFMutableStringRef sql, bool *needWhere) {
            // Select only non-D-class items
            SecDbAppendWhereOrAnd(sql, needWhere);
            CFStringAppendFormat(sql, NULL, CFSTR("NOT %@ IN (?,?)"), pdmn->name);
            return true;
        }, ^bool(sqlite3_stmt *stmt, int col) {
            return SecDbBindObject(stmt, col++, kSecAttrAccessibleAlways, error) &&
            SecDbBindObject(stmt, col++, kSecAttrAccessibleAlwaysThisDeviceOnly, error);
        }, ^(SecDbItemRef item, bool *stop) {
            CFErrorRef localError = NULL;

            // Decrypt the item.
            if (SecDbItemEnsureDecrypted(item, &localError)) {
                // Delete SHA1 field from the item, so that it is newly recalculated before storing
                // the item into the new table.
                require_quiet(ok = SecDbItemSetValue(item, SecDbClassAttrWithKind(item->class, kSecDbSHA1Attr, error),
                                                     kCFNull, error), out);

                // Replace item with the new value in the table; this will cause the item to be decoded and recoded back,
                // incl. recalculation of item's hash.
                ok = SecDbItemUpdate(item, item, dbt, false, error);
            } else {
                CFIndex status = CFErrorGetCode(localError);

                // Items producing errSecDecode are silently dropped - they are not decodable and lost forever.
                require_action_quiet(status != errSecDecode, out, ok = SecDbItemDelete(item, dbt, false, error));

                // If we are still not able to decrypt the item because the class key is not released yet,
                // remember that DB still needs phase2 migration to be run next time a connection is made.  Also
                // stop iterating next items, it would be just waste of time because the whole iteration will be run
                // next time when this phase2 will be rerun.
                if (status == errSecInteractionNotAllowed) {
                    *inProgress = true;
                    *stop = true;
                } else {
                    // errSecAuthNeeded means that it is an ACL-based item which requires authentication (or at least
                    // ACM context, which we do not have).  Other errors should abort the migration completely.
                    require_action_quiet(status == errSecAuthNeeded, out,
                                         ok = CFErrorPropagate(CFRetainSafe(localError), error));
                }
            }

        out:
            CFReleaseSafe(localError);
            *stop = *stop || !ok;

        });
        require(ok, out);
    }

out:
    if (query != NULL)
        query_destroy(query, NULL);
    return ok;
}

static bool SecKeychainDbUpgradeFromVersion(SecDbConnectionRef dbt, int version, bool *inProgress, CFErrorRef *error) {
    __block bool ok = true;

    // The schema we want to have is the first in the list of schemas.
    const SecDbSchema *newSchema = kc_schemas[0];

    // If DB schema is the one we want, we are done.
    require_quiet(newSchema->version != version, out);

    if (version < 6) {
        // Pre v6 keychains need to have WAL enabled, since SecDb only does this at db creation time.
        // NOTE: This has to be run outside of a transaction.
        require_action_quiet(ok = (SecDbExec(dbt, CFSTR("PRAGMA auto_vacuum = FULL"), error) &&
                                   SecDbExec(dbt, CFSTR("PRAGMA journal_mode = WAL"), error)),
                             out, secerror("unable to enable WAL or auto vacuum, marking DB as corrupt: %@",
                                           error ? *error : NULL));
    }

    ok &= SecDbTransaction(dbt, kSecDbExclusiveTransactionType, error, ^(bool *commit) {
        CFStringRef sql = NULL;

        // Get version again once we start a transaction, someone else might change the migration state.
        int version = 0;
        require_quiet(ok = SecKeychainDbGetVersion(dbt, &version, error), out);
        require_quiet(newSchema->version != version, out);

        // If this is empty database, just create table according to schema and be done with it.
        require_action_quiet(version != 0, out, ok = SecItemDbCreateSchema(dbt, newSchema, error));

        int oldVersion = (version >> 16) & 0xffff;
        version &= 0xffff;
        require_action_quiet(version == newSchema->version || oldVersion == 0, out,
                             ok = SecDbError(SQLITE_CORRUPT, error,
                                             CFSTR("Half migrated but obsolete DB found: found %d(%d) but %d is needed"),
                                             version, oldVersion, newSchema->version));

        // Check whether we have both old and new tables in the DB.
        if (oldVersion == 0) {
            // Pure old-schema migration attempt, with full blown table renames etc (a.k.a. phase1)
            oldVersion = version;
            version = newSchema->version;

            // Find schema for old database.
            const SecDbSchema *oldSchema = NULL;
            for (const SecDbSchema * const *pschema = kc_schemas; *pschema; ++pschema) {
                if ((*pschema)->version == oldVersion) {
                    oldSchema = *pschema;
                    break;
                }
            }

            // If we are attempting to upgrade from a version for which we have no schema, fail.
            require_action_quiet(oldSchema != NULL, out,
                                 ok = SecDbError(SQLITE_CORRUPT, error, CFSTR("no schema for version: %d"), oldVersion);
                                 secerror("no schema for version %d", oldVersion));

            require(ok = SecKeychainDbUpgradeFromSchema(dbt, oldSchema, inProgress, error), out);
        } else {
            // Just go through non-D-class items in new tables and apply decode/encode on them, because
            // they were not recoded completely during some previous old-schema migration attempt (a.k.a. phase2)
            require(ok = SecKeychainDbUpgradeUnlockedItems(dbt, inProgress, error), out);
        }

        if (!*inProgress) {
            // If either migration path we did reported that the migration was complete, signalize that
            // in the version database by cleaning oldVersion (which is stored in upper halfword of the version)
            oldVersion = 0;
        }

        // Update database version table.
        version |= oldVersion << 16;
        sql = CFStringCreateWithFormat(NULL, NULL, CFSTR("UPDATE %@ SET %@ = %d"),
                                       tversion_class.name, tversion_class.attrs[0]->name, version);
        require_quiet(ok = SecDbExec(dbt, sql, error), out);

    out:
        CFReleaseSafe(sql);
        *commit = ok;
    });

out:
    if (!ok) {
        secerror("unable to complete upgrade, marking DB as corrupt: %@", error ? *error : NULL);
        SecDbCorrupt(dbt);
    }

    return ok;
}

/* AUDIT[securityd](done):
   accessGroup (ok) is a caller provided, non NULL CFTypeRef.

   Return true iff accessGroup is allowable according to accessGroups.
 */
static bool accessGroupsAllows(CFArrayRef accessGroups,
    CFStringRef accessGroup) {
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
         CFArrayContainsValue(accessGroups, range, CFSTR("*"))))
        return true;

    return false;
}

bool itemInAccessGroup(CFDictionaryRef item, CFArrayRef accessGroups) {
    return accessGroupsAllows(accessGroups,
                              CFDictionaryGetValue(item, kSecAttrAccessGroup));
}


static CF_RETURNS_RETAINED CFDataRef SecServerExportBackupableKeychain(SecDbConnectionRef dbt,
    keybag_handle_t src_keybag, keybag_handle_t dest_keybag, CFErrorRef *error) {
    CFDataRef data_out = NULL;
    /* Export everything except the items for which SecItemIsSystemBound()
       returns true. */
    CFDictionaryRef keychain = SecServerExportKeychainPlist(dbt,
        src_keybag, dest_keybag, kSecBackupableItemFilter,
        error);
    if (keychain) {
        data_out = CFPropertyListCreateData(kCFAllocatorDefault, keychain,
                                             kCFPropertyListBinaryFormat_v1_0,
                                             0, error);
        CFRelease(keychain);
    }

    return data_out;
}

static bool SecServerImportBackupableKeychain(SecDbConnectionRef dbt,
    keybag_handle_t src_keybag,
    keybag_handle_t dest_keybag, CFDataRef data, CFErrorRef *error) {
    return kc_transaction(dbt, error, ^{
        bool ok = false;
        CFDictionaryRef keychain;
        keychain = CFPropertyListCreateWithData(kCFAllocatorDefault, data,
                                                kCFPropertyListImmutable, NULL,
                                                error);
        if (keychain) {
            if (isDictionary(keychain)) {
                ok = SecServerImportKeychainInPlist(dbt, src_keybag,
                                                    dest_keybag, keychain,
                                                    kSecBackupableItemFilter,
                                                    error);
            } else {
                ok = SecError(errSecParam, error, CFSTR("import: keychain is not a dictionary"));
            }
            CFRelease(keychain);
        }
        return ok;
    });
}

static CF_RETURNS_RETAINED CFDataRef SecServerKeychainBackup(SecDbConnectionRef dbt, CFDataRef keybag,
    CFDataRef password, CFErrorRef *error) {
    CFDataRef backup = NULL;
    keybag_handle_t backup_keybag;
    if (ks_open_keybag(keybag, password, &backup_keybag, error)) {
        /* Export from system keybag to backup keybag. */
        backup = SecServerExportBackupableKeychain(dbt, KEYBAG_DEVICE, backup_keybag, error);
        if (!ks_close_keybag(backup_keybag, error)) {
            CFReleaseNull(backup);
        }
    }
    return backup;
}

static bool SecServerKeychainRestore(SecDbConnectionRef dbt, CFDataRef backup,
    CFDataRef keybag, CFDataRef password, CFErrorRef *error) {
    keybag_handle_t backup_keybag;
    if (!ks_open_keybag(keybag, password, &backup_keybag, error))
        return false;

    /* Import from backup keybag to system keybag. */
    bool ok = SecServerImportBackupableKeychain(dbt, backup_keybag, KEYBAG_DEVICE,
                                      backup, error);
    ok &= ks_close_keybag(backup_keybag, error);

    return ok;
}


// MARK - External SPI support code.

CFStringRef __SecKeychainCopyPath(void) {
    CFStringRef kcRelPath = NULL;
    if (use_hwaes()) {
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

SecDbRef SecKeychainDbCreate(CFStringRef path) {
    return SecDbCreate(path, ^bool (SecDbConnectionRef dbconn, bool didCreate, bool *callMeAgainForNextConnection, CFErrorRef *error) {
        // Upgrade from version 0 means create the schema in empty db.
        int version = 0;
        bool ok = true;
        if (!didCreate)
            ok = SecKeychainDbGetVersion(dbconn, &version, error);

        ok = ok && SecKeychainDbUpgradeFromVersion(dbconn, version, callMeAgainForNextConnection, error);
        if (!ok)
            secerror("Upgrade %sfailed: %@", didCreate ? "from v0 " : "", error ? *error : NULL);

        return ok;
    });
}

static SecDbRef _kc_dbhandle = NULL;

static void kc_dbhandle_init(void) {
    SecDbRef oldHandle = _kc_dbhandle;
    _kc_dbhandle = NULL;
    CFStringRef dbPath = __SecKeychainCopyPath();
    if (dbPath) {
        _kc_dbhandle = SecKeychainDbCreate(dbPath);
        CFRelease(dbPath);
    } else {
        secerror("no keychain path available");
    }
    if (oldHandle) {
        secerror("replaced %@ with %@", oldHandle, _kc_dbhandle);
        CFRelease(oldHandle);
    }
}

// A callback for the sqlite3_log() interface.
static void sqlite3Log(void *pArg, int iErrCode, const char *zMsg){
    secinfo("sqlite3", "(%d) %s", iErrCode, zMsg);
}

static void setup_sqlite3_defaults_settings() {
    int rx = sqlite3_config(SQLITE_CONFIG_LOG, sqlite3Log, NULL);
    if (SQLITE_OK != rx) {
        secwarning("Could not set up sqlite global error logging to syslog: %d", rx);
    }
}

static dispatch_once_t _kc_dbhandle_once;

static SecDbRef kc_dbhandle(void) {
    dispatch_once(&_kc_dbhandle_once, ^{
        setup_sqlite3_defaults_settings();
        kc_dbhandle_init();
    });
    return _kc_dbhandle;
}

/* For whitebox testing only */
void kc_dbhandle_reset(void);
void kc_dbhandle_reset(void)
{
    __block bool done = false;
    dispatch_once(&_kc_dbhandle_once, ^{
        kc_dbhandle_init();
        done = true;
    });
    // TODO: Not thread safe at all! - FOR DEBUGGING ONLY
    if (!done)
        kc_dbhandle_init();
}

static SecDbConnectionRef kc_aquire_dbt(bool writeAndRead, CFErrorRef *error) {
    SecDbRef db = kc_dbhandle();
    if (db == NULL) {
        SecError(errSecDataNotAvailable, error, CFSTR("failed to get a db handle"));
        return NULL;
    }
    return SecDbConnectionAquire(db, !writeAndRead, error);
}

/* Return a per thread dbt handle for the keychain.  If create is true create
 the database if it does not yet exist.  If it is false, just return an
 error if it fails to auto-create. */
static bool kc_with_dbt(bool writeAndRead, CFErrorRef *error, bool (^perform)(SecDbConnectionRef dbt))
{
    // Make sure we initialize our engines before writing to the keychain
    if (writeAndRead)
        SecItemDataSourceFactoryGetDefault();

    bool ok = false;
    SecDbConnectionRef dbt = kc_aquire_dbt(writeAndRead, error);
    if (dbt) {
        ok = perform(dbt);
        SecDbConnectionRelease(dbt);
    }
    return ok;
}

static bool
items_matching_issuer_parent(SecDbConnectionRef dbt, CFArrayRef accessGroups,
                             CFDataRef issuer, CFArrayRef issuers, int recurse)
{
    Query *q;
    CFArrayRef results = NULL;
    CFIndex i, count;
    bool found = false;

    if (CFArrayContainsValue(issuers, CFRangeMake(0, CFArrayGetCount(issuers)), issuer))
        return true;

    const void *keys[] = { kSecClass, kSecReturnRef, kSecAttrSubject };
    const void *vals[] = { kSecClassCertificate, kCFBooleanTrue, issuer };
    CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, array_size(keys), NULL, NULL);

    if (!query)
        return false;

    CFErrorRef localError = NULL;
    q = query_create_with_limit(query, kSecMatchUnlimited, &localError);
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
            found = items_matching_issuer_parent(dbt, accessGroups, cert_issuer, issuers, recurse);
    }
    CFReleaseSafe(results);

    return found;
}

bool match_item(SecDbConnectionRef dbt, Query *q, CFArrayRef accessGroups, CFDictionaryRef item)
{
    if (q->q_match_issuer) {
        CFDataRef issuer = CFDictionaryGetValue(item, kSecAttrIssuer);
        if (!items_matching_issuer_parent(dbt, accessGroups, issuer, q->q_match_issuer, 10 /*max depth*/))
            return false;
    }

    /* Add future match checks here. */

    return true;
}

/****************************************************************************
 **************** Beginning of Externally Callable Interface ****************
 ****************************************************************************/

#if 0
// TODO Use as a safety wrapper
static bool SecErrorWith(CFErrorRef *in_error, bool (^perform)(CFErrorRef *error)) {
    CFErrorRef error = in_error ? *in_error : NULL;
    bool ok;
    if ((ok = perform(&error))) {
        assert(error == NULL);
        if (error)
            secerror("error + success: %@", error);
    } else {
        assert(error);
        OSStatus status = SecErrorGetOSStatus(error);
        if (status != errSecItemNotFound)           // Occurs in normal operation, so exclude
            secerror("error:[%" PRIdOSStatus "] %@", status, error);
        if (in_error) {
            *in_error = error;
        } else {
            CFReleaseNull(error);
        }
    }
    return ok;
}
#endif

void (*SecTaskDiagnoseEntitlements)(CFArrayRef accessGroups) = NULL;

/* AUDIT[securityd](done):
   query (ok) is a caller provided dictionary, only its cf type has been checked.
 */
static bool
SecItemServerCopyMatching(CFDictionaryRef query, CFTypeRef *result,
    CFArrayRef accessGroups, CFErrorRef *error)
{
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        if (SecTaskDiagnoseEntitlements)
            SecTaskDiagnoseEntitlements(accessGroups);
        return SecError(errSecMissingEntitlement, error,
                         CFSTR("client has neither application-identifier nor keychain-access-groups entitlements"));
    }

    if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*"))) {
        /* Having the special accessGroup "*" allows access to all accessGroups. */
        accessGroups = NULL;
    }

    bool ok = false;
    Query *q = query_create_with_limit(query, 1, error);
    if (q) {
        CFStringRef agrp = CFDictionaryGetValue(q->q_item, kSecAttrAccessGroup);
        if (agrp && accessGroupsAllows(accessGroups, agrp)) {
            // TODO: Return an error if agrp is not NULL and accessGroupsAllows() fails above.
            const void *val = agrp;
            accessGroups = CFArrayCreate(0, &val, 1, &kCFTypeArrayCallBacks);
        } else {
            CFRetainSafe(accessGroups);
        }

        query_set_caller_access_groups(q, accessGroups);

        /* Sanity check the query. */
        if (q->q_use_item_list) {
            ok = SecError(errSecUseItemListUnsupported, error, CFSTR("use item list unsupported"));
#if defined(MULTIPLE_KEYCHAINS)
        } else if (q->q_use_keychain) {
            ok = SecError(errSecUseKeychainUnsupported, error, CFSTR("use keychain list unsupported"));
#endif
        } else if (q->q_match_issuer && ((q->q_class != &cert_class) &&
                    (q->q_class != &identity_class))) {
            ok = SecError(errSecUnsupportedOperation, error, CFSTR("unsupported match attribute"));
        } else if (q->q_return_type != 0 && result == NULL) {
            ok = SecError(errSecReturnMissingPointer, error, CFSTR("missing pointer"));
        } else if (!q->q_error) {
            ok = kc_with_dbt(false, error, ^(SecDbConnectionRef dbt) {
                return s3dl_copy_matching(dbt, q, result, accessGroups, error);
            });
        }

        CFReleaseSafe(accessGroups);
        if (!query_destroy(q, error))
            ok = false;
    }

	return ok;
}

bool
_SecItemCopyMatching(CFDictionaryRef query, CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef *error) {
    return SecItemServerCopyMatching(query, result, accessGroups, error);
}

/* AUDIT[securityd](done):
   attributes (ok) is a caller provided dictionary, only its cf type has
       been checked.
 */
bool
_SecItemAdd(CFDictionaryRef attributes, CFArrayRef accessGroups,
            CFTypeRef *result, CFErrorRef *error)
{
    bool ok = true;
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        if (SecTaskDiagnoseEntitlements)
            SecTaskDiagnoseEntitlements(accessGroups);
        return SecError(errSecMissingEntitlement, error,
                           CFSTR("client has neither application-identifier nor keychain-access-groups entitlements"));
    }

    Query *q = query_create_with_limit(attributes, 0, error);
    if (q) {
        /* Access group sanity checking. */
        CFStringRef agrp = (CFStringRef)CFDictionaryGetValue(attributes,
            kSecAttrAccessGroup);

        CFArrayRef ag = accessGroups;
        /* Having the special accessGroup "*" allows access to all accessGroups. */
        if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*")))
            accessGroups = NULL;

        if (agrp) {
            /* The user specified an explicit access group, validate it. */
            if (!accessGroupsAllows(accessGroups, agrp))
                ok = SecError(errSecNoAccessForItem, error, CFSTR("NoAccessForItem"));
        } else {
            agrp = (CFStringRef)CFArrayGetValueAtIndex(ag, 0);

            /* We are using an implicit access group, add it as if the user
               specified it as an attribute. */
            query_add_attribute(kSecAttrAccessGroup, agrp, q);
        }

        if (ok) {
            query_ensure_access_control(q, agrp);

            if (q->q_row_id)
                ok = SecError(errSecValuePersistentRefUnsupported, error, CFSTR("q_row_id"));  // TODO: better error string
        #if defined(MULTIPLE_KEYCHAINS)
            else if (q->q_use_keychain_list)
                ok = SecError(errSecUseKeychainListUnsupported, error, CFSTR("q_use_keychain_list"));  // TODO: better error string;
        #endif
            else if (!q->q_error) {
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
    return ok;
}

/* AUDIT[securityd](done):
   query (ok) and attributesToUpdate (ok) are a caller provided dictionaries,
       only their cf types have been checked.
 */
bool
_SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate,
               CFArrayRef accessGroups, CFErrorRef *error)
{
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        if (SecTaskDiagnoseEntitlements)
            SecTaskDiagnoseEntitlements(accessGroups);
        return SecError(errSecMissingEntitlement, error,
                         CFSTR("client has neither application-identifier nor keychain-access-groups entitlements"));
    }

    if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*"))) {
        /* Having the special accessGroup "*" allows access to all accessGroups. */
        accessGroups = NULL;
    }

    bool ok = true;
    Query *q = query_create_with_limit(query, kSecMatchUnlimited, error);
    if (!q) {
        ok = false;
    }
    if (ok) {
        /* Sanity check the query. */
        query_set_caller_access_groups(q, accessGroups);
        if (q->q_use_item_list) {
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
                if (!accessGroupsAllows(accessGroups, agrp)) {
                    ok = SecError(errSecNoAccessForItem, error, CFSTR("accessGroup %@ not in %@"), agrp, accessGroups);
                }
            }
        }
    }
    if (ok) {
        ok = kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
            return s3dl_query_update(dbt, q, attributesToUpdate, accessGroups, error);
        });
    }
    if (q) {
        ok = query_notify_and_destroy(q, ok, error);
    }
    return ok;
}


/* AUDIT[securityd](done):
   query (ok) is a caller provided dictionary, only its cf type has been checked.
 */
bool
_SecItemDelete(CFDictionaryRef query, CFArrayRef accessGroups, CFErrorRef *error)
{
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        if (SecTaskDiagnoseEntitlements)
            SecTaskDiagnoseEntitlements(accessGroups);
        return SecError(errSecMissingEntitlement, error,
                           CFSTR("client has neither application-identifier nor keychain-access-groups entitlements"));
    }

    if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*"))) {
        /* Having the special accessGroup "*" allows access to all accessGroups. */
        accessGroups = NULL;
    }

    Query *q = query_create_with_limit(query, kSecMatchUnlimited, error);
    bool ok;
    if (q) {
        query_set_caller_access_groups(q, accessGroups);
        /* Sanity check the query. */
        if (q->q_limit != kSecMatchUnlimited)
            ok = SecError(errSecMatchLimitUnsupported, error, CFSTR("match limit not supported by delete"));
        else if (query_match_count(q) != 0)
            ok = SecError(errSecItemMatchUnsupported, error, CFSTR("match not supported by delete"));
        else if (q->q_ref)
            ok = SecError(errSecValueRefUnsupported, error, CFSTR("value ref not supported by delete"));
        else if (q->q_row_id && query_attr_count(q))
            ok = SecError(errSecItemIllegalQuery, error, CFSTR("rowid and other attributes are mutually exclusive"));
        else {
            ok = kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
                return s3dl_query_delete(dbt, q, accessGroups, error);
            });
        }
        ok = query_notify_and_destroy(q, ok, error);
    } else {
        ok = false;
    }
    return ok;
}


/* AUDIT[securityd](done):
   No caller provided inputs.
 */
static bool
SecItemServerDeleteAll(CFErrorRef *error) {
    return kc_with_dbt(true, error, ^bool (SecDbConnectionRef dbt) {
        return (kc_transaction(dbt, error, ^bool {
            return (SecDbExec(dbt, CFSTR("DELETE from genp;"), error) &&
                    SecDbExec(dbt, CFSTR("DELETE from inet;"), error) &&
                    SecDbExec(dbt, CFSTR("DELETE from cert;"), error) &&
                    SecDbExec(dbt, CFSTR("DELETE from keys;"), error));
        }) && SecDbExec(dbt, CFSTR("VACUUM;"), error));
    });
}

bool
_SecItemDeleteAll(CFErrorRef *error) {
    return SecItemServerDeleteAll(error);
}


// MARK: -
// MARK: Shared web credentials

/* constants */
#define SEC_CONST_DECL(k,v) const CFStringRef k = CFSTR(v);

SEC_CONST_DECL (kSecSafariAccessGroup, "com.apple.cfnetwork");
SEC_CONST_DECL (kSecSafariDefaultComment, "default");
SEC_CONST_DECL (kSecSafariPasswordsNotSaved, "Passwords not saved");
SEC_CONST_DECL (kSecSharedCredentialUrlScheme, "https://");
SEC_CONST_DECL (kSecSharedWebCredentialsService, "webcredentials");

#if TARGET_OS_IPHONE && !TARGET_OS_WATCH
static dispatch_once_t			sSecSWCInitializeOnce	= 0;
static void *					sSecSWCLibrary			= NULL;
static SWCCheckService_f		sSWCCheckService_f		= NULL;
static SWCSetServiceFlags_f		sSWCSetServiceFlags_f	= NULL;

static OSStatus _SecSWCEnsuredInitialized(void);

static OSStatus _SecSWCEnsuredInitialized(void)
{
    __block OSStatus status = errSecNotAvailable;

    dispatch_once(&sSecSWCInitializeOnce, ^{
        sSecSWCLibrary = dlopen("/System/Library/PrivateFrameworks/SharedWebCredentials.framework/SharedWebCredentials", RTLD_LAZY | RTLD_LOCAL);
        assert(sSecSWCLibrary);
        if (sSecSWCLibrary) {
            sSWCCheckService_f = (SWCCheckService_f)(uintptr_t) dlsym(sSecSWCLibrary, "SWCCheckService");
            sSWCSetServiceFlags_f = (SWCSetServiceFlags_f)(uintptr_t) dlsym(sSecSWCLibrary, "SWCSetServiceFlags");
        }
    });

    if (sSWCCheckService_f && sSWCSetServiceFlags_f) {
        status = noErr;
    }
    return status;
}
#endif

#if !TARGET_IPHONE_SIMULATOR
static SWCFlags
_SecAppDomainApprovalStatus(CFStringRef appID, CFStringRef fqdn, CFErrorRef *error)
{
    __block SWCFlags flags = kSWCFlags_None;

#if TARGET_OS_IPHONE && !TARGET_OS_WATCH
    OSStatus status = _SecSWCEnsuredInitialized();
    if (status) {
        SecError(status, error, CFSTR("SWC initialize failed"));
        return flags;
    }
    CFRetainSafe(appID);
    CFRetainSafe(fqdn);
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    dispatch_retain(semaphore);
    if (0 == sSWCCheckService_f(kSecSharedWebCredentialsService, appID, fqdn,
        ^void (OSStatus inStatus, SWCFlags inFlags, CFDictionaryRef inDetails) {
            if (!inStatus) { flags = inFlags; }
            CFReleaseSafe(appID);
            CFReleaseSafe(fqdn);
            dispatch_semaphore_signal(semaphore);
            dispatch_release(semaphore);
            //secerror("SWCCheckService: inStatus=%d, flags=%0X", inStatus, flags);
        }))
    {
        // wait for the block to complete, as we need its answer
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    }
    else // didn't queue the block
    {
        CFReleaseSafe(appID);
        CFReleaseSafe(fqdn);
        dispatch_release(semaphore);
    }
    dispatch_release(semaphore);
#else
    flags |= (kSWCFlag_SiteApproved);
#endif

    if (!error) { return flags; }
    *error = NULL;

    // check website approval status
    if (!(flags & kSWCFlag_SiteApproved)) {
        if (flags & kSWCFlag_Pending) {
            SecError(errSecAuthFailed, error, CFSTR("Approval is pending for \"%@\", try later"), fqdn);
        } else {
            SecError(errSecAuthFailed, error, CFSTR("\"%@\" failed to approve \"%@\""), fqdn, appID);
        }
        return flags;
    }

    // check user approval status
    if (flags & kSWCFlag_UserDenied) {
        SecError(errSecAuthFailed, error, CFSTR("User denied access to \"%@\" by \"%@\""), fqdn, appID);
    }
    return flags;
}
#endif

#if !TARGET_IPHONE_SIMULATOR
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
#endif

static bool
_SecAddNegativeWebCredential(CFStringRef fqdn, CFStringRef appID, bool forSafari)
{
    bool result = false;
    if (!fqdn) { return result; }

#if TARGET_OS_IPHONE && !TARGET_OS_WATCH
    OSStatus status = _SecSWCEnsuredInitialized();
    if (status) { return false; }

    // update our database
    CFRetainSafe(appID);
    CFRetainSafe(fqdn);
    if (0 == sSWCSetServiceFlags_f(kSecSharedWebCredentialsService,
        appID, fqdn, kSWCFlag_ExternalMask, kSWCFlag_UserDenied,
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
#endif
    if (!forSafari) { return result; }

    // below this point: create a negative Safari web credential item

    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!attrs) { return result; }

    CFErrorRef error = NULL;
    CFStringRef accessGroup = CFSTR("*");
    CFArrayRef accessGroups = CFArrayCreate(kCFAllocatorDefault, (const void **)&accessGroup, 1, &kCFTypeArrayCallBacks);

    CFDictionaryAddValue(attrs, kSecClass, kSecClassInternetPassword);
    CFDictionaryAddValue(attrs, kSecAttrAccessGroup, kSecSafariAccessGroup);
    CFDictionaryAddValue(attrs, kSecAttrAuthenticationType, kSecAttrAuthenticationTypeHTMLForm);
    CFDictionaryAddValue(attrs, kSecAttrProtocol, kSecAttrProtocolHTTPS);
    CFDictionaryAddValue(attrs, kSecAttrServer, fqdn);
    CFDictionaryAddValue(attrs, kSecAttrSynchronizable, kCFBooleanTrue);

    (void)_SecItemDelete(attrs, accessGroups, &error);
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
    result = _SecItemAdd(attrs, accessGroups, &addResult, &error);

    CFReleaseSafe(addResult);
    CFReleaseSafe(error);
    CFReleaseSafe(attrs);
    CFReleaseSafe(accessGroups);

    return result;
}

/* Specialized version of SecItemAdd for shared web credentials */
bool
_SecAddSharedWebCredential(CFDictionaryRef attributes,
    const audit_token_t *clientAuditToken,
    CFStringRef appID,
    CFArrayRef domains,
    CFTypeRef *result,
    CFErrorRef *error) {

    CFStringRef fqdn = CFRetainSafe(CFDictionaryGetValue(attributes, kSecAttrServer));
    CFStringRef account = CFRetainSafe(CFDictionaryGetValue(attributes, kSecAttrAccount));
#if TARGET_OS_IPHONE && !TARGET_OS_WATCH
    CFStringRef password = CFRetainSafe(CFDictionaryGetValue(attributes, kSecSharedPassword));
#else
    CFStringRef password = CFRetainSafe(CFDictionaryGetValue(attributes, CFSTR("spwd")));
#endif
    CFStringRef accessGroup = CFSTR("*");
    CFArrayRef accessGroups = NULL;
    CFMutableDictionaryRef query = NULL, attrs = NULL;
    SInt32 port = -1;
    bool ok = false, update = false;
    //bool approved = false;

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

#if TARGET_IPHONE_SIMULATOR
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

#if TARGET_IPHONE_SIMULATOR
    secerror("Ignoring app/site approval state in the Simulator.");
#else
    // get approval status for this app/domain pair
    SWCFlags flags = _SecAppDomainApprovalStatus(appID, fqdn, error);
    //approved = ((flags & kSWCFlag_SiteApproved) && (flags & kSWCFlag_UserApproved));
    if (!(flags & kSWCFlag_SiteApproved)) {
        goto cleanup;
    }
#endif

    // give ourselves access to see matching items for kSecSafariAccessGroup
    accessGroups = CFArrayCreate(kCFAllocatorDefault, (const void **)&accessGroup, 1, &kCFTypeArrayCallBacks);

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
    ok = _SecItemCopyMatching(query, accessGroups, result, error);
    if(result) CFReleaseNull(*result);
    CFReleaseNull(*error);
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
    if (_SecItemCopyMatching(query, accessGroups, result, error)) {
        // found it, so this becomes either an "update password" or "delete password" operation
        if(result) CFReleaseNull(*result);
        CFReleaseNull(*error);
        update = (password != NULL);
        if (update) {
            attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFDataRef credential = CFStringCreateExternalRepresentation(kCFAllocatorDefault, password, kCFStringEncodingUTF8, 0);
            CFDictionaryAddValue(attrs, kSecValueData, credential);
            CFReleaseSafe(credential);
            CFDictionaryAddValue(attrs, kSecAttrComment, kSecSafariDefaultComment);

            // confirm the update
            // (per rdar://16676310 we always prompt, even if there was prior user approval)
            ok = /*approved ||*/ swca_confirm_operation(swca_update_request_id, clientAuditToken, query, error,
                ^void (CFStringRef fqdn) { _SecAddNegativeWebCredential(fqdn, appID, false); });
            if (ok) {
                ok = _SecItemUpdate(query, attrs, accessGroups, error);
            }
        }
        else {
            // confirm the delete
            // (per rdar://16676288 we always prompt, even if there was prior user approval)
            ok = /*approved ||*/ swca_confirm_operation(swca_delete_request_id, clientAuditToken, query, error,
                ^void (CFStringRef fqdn) { _SecAddNegativeWebCredential(fqdn, appID, false); });
            if (ok) {
                ok = _SecItemDelete(query, accessGroups, error);
            }
        }
        if (ok) {
            CFReleaseNull(*error);
        }
        goto cleanup;
    }
    if(result) CFReleaseNull(*result);
    CFReleaseNull(*error);

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

        CFReleaseSafe(accessGroups);
        accessGroups = CFArrayCreate(kCFAllocatorDefault, (const void **)&kSecSafariAccessGroup, 1, &kCFTypeArrayCallBacks);

        // mark the item as created by this function
        const int32_t creator_value = 'swca';
        CFNumberRef creator = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &creator_value);
        if (creator) {
            CFDictionarySetValue(query, kSecAttrCreator, creator);
            CFReleaseSafe(creator);
            ok = true;
        }
        else {
            // confirm the add
            // (per rdar://16680019, we won't prompt here in the normal case)
            ok = /*approved ||*/ swca_confirm_operation(swca_add_request_id, clientAuditToken, query, error,
                ^void (CFStringRef fqdn) { _SecAddNegativeWebCredential(fqdn, appID, false); });
        }
    }
    if (ok) {
        ok = _SecItemAdd(query, accessGroups, result, error);
    }

cleanup:
#if 0 /* debugging */
{
    const char *op_str = (password) ? ((update) ? "updated" : "added") : "deleted";
    const char *result_str = (ok) ? "true" : "false";
    secerror("result=%s, %s item %@, error=%@", result_str, op_str, *result, *error);
}
#else
    (void)update;
#endif
    CFReleaseSafe(attrs);
    CFReleaseSafe(query);
    CFReleaseSafe(accessGroups);
    CFReleaseSafe(fqdn);
    CFReleaseSafe(account);
    CFReleaseSafe(password);
    return ok;
}

/* Specialized version of SecItemCopyMatching for shared web credentials */
bool
_SecCopySharedWebCredential(CFDictionaryRef query,
    const audit_token_t *clientAuditToken,
    CFStringRef appID,
    CFArrayRef domains,
    CFTypeRef *result,
    CFErrorRef *error) {

    CFMutableArrayRef credentials = NULL;
    CFMutableArrayRef foundItems = NULL;
    CFMutableArrayRef fqdns = NULL;
    CFArrayRef accessGroups = NULL;
    CFStringRef fqdn = NULL;
    CFStringRef account = NULL;
    CFIndex idx, count;
    SInt32 port = -1;
    bool ok = false;

    require_quiet(result, cleanup);
    credentials = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    foundItems = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    fqdns = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    // give ourselves access to see matching items for kSecSafariAccessGroup
    CFStringRef accessGroup = CFSTR("*");
    accessGroups = CFArrayCreate(kCFAllocatorDefault, (const void **)&accessGroup, 1, &kCFTypeArrayCallBacks);

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

    #if TARGET_IPHONE_SIMULATOR
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

        ok = _SecItemCopyMatching(attrs, accessGroups, (CFTypeRef*)&items, error);
        if (count > 1) {
            // ignore interim error since we have multiple domains to search
            CFReleaseNull(*error);
        }
        if (ok && items && CFGetTypeID(items) == CFArrayGetTypeID()) {
    #if TARGET_IPHONE_SIMULATOR
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
                #if TARGET_OS_IPHONE && !TARGET_OS_WATCH
                    CFDictionaryAddValue(newdict, kSecSharedPassword, password);
                #else
                    CFDictionaryAddValue(newdict, CFSTR("spwd"), password);
                #endif
                    CFReleaseSafe(password);
                }
            }
            if (icmt && CFEqual(icmt, kSecSafariDefaultComment)) {
                CFArrayInsertValueAtIndex(credentials, 0, newdict);
            } else {
                CFArrayAppendValue(credentials, newdict);
            }
        }
        CFReleaseSafe(newdict);
    }

    if (count) {

        ok = false;

        // create a new array of dictionaries (without the actual password) for picker UI
        count = CFArrayGetCount(credentials);
        CFMutableArrayRef items = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        for (idx = 0; idx < count; idx++) {
            CFDictionaryRef dict = (CFDictionaryRef) CFArrayGetValueAtIndex(credentials, idx);
            CFMutableDictionaryRef newdict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, dict);
        #if TARGET_OS_IPHONE && !TARGET_OS_WATCH
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
#if TARGET_OS_IPHONE && !TARGET_OS_WATCH
            fqdn = CFDictionaryGetValue(selected, kSecAttrServer);
#endif
            CFArrayAppendValue(credentials, selected);
        }

#if 0
        // confirm the access
        ok = swca_confirm_operation(swca_copy_request_id, clientAuditToken, query, error,
                    ^void (CFStringRef fqdn) { _SecAddNegativeWebCredential(fqdn, appID, false); });
#endif
        if (ok) {
            #if TARGET_OS_IPHONE && !TARGET_OS_WATCH
            // register confirmation with database
            OSStatus status = _SecSWCEnsuredInitialized();
            if (status) {
                SecError(status, error, CFSTR("SWC initialize failed"));
                ok = false;
                CFReleaseSafe(selected);
                goto cleanup;
            }
            CFRetainSafe(appID);
            CFRetainSafe(fqdn);
            if (0 != sSWCSetServiceFlags_f(kSecSharedWebCredentialsService,
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
    }
    CFReleaseSafe(foundItems);
    *result = credentials;
    CFReleaseSafe(accessGroups);
    CFReleaseSafe(fqdns);
#if 0 /* debugging */
    secerror("result=%s, copied items %@, error=%@", (ok) ? "true" : "false", *result, *error);
#endif
    return ok;
}

// MARK: -
// MARK: Keychain backup

CF_RETURNS_RETAINED CFDataRef
_SecServerKeychainBackup(CFDataRef keybag, CFDataRef passcode, CFErrorRef *error) {
    CFDataRef backup;
	SecDbConnectionRef dbt = SecDbConnectionAquire(kc_dbhandle(), false, error);

	if (!dbt)
		return NULL;

    if (keybag == NULL && passcode == NULL) {
#if USE_KEYSTORE
        backup = SecServerExportBackupableKeychain(dbt, KEYBAG_DEVICE, backup_keybag_handle, error);
#else /* !USE_KEYSTORE */
        SecError(errSecParam, error, CFSTR("Why are you doing this?"));
        backup = NULL;
#endif /* USE_KEYSTORE */
    } else {
        backup = SecServerKeychainBackup(dbt, keybag, passcode, error);
    }

    SecDbConnectionRelease(dbt);

    return backup;
}

bool
_SecServerKeychainRestore(CFDataRef backup, CFDataRef keybag, CFDataRef passcode, CFErrorRef *error) {
    if (backup == NULL || keybag == NULL)
        return SecError(errSecParam, error, CFSTR("backup or keybag missing"));

    __block bool ok = true;
    ok &= SecDbPerformWrite(kc_dbhandle(), error, ^(SecDbConnectionRef dbconn) {
        ok = SecServerKeychainRestore(dbconn, backup, keybag, passcode, error);
    });

    if (ok) {
        SecKeychainChanged(true);
    }

    return ok;
}


// MARK: -
// MARK: SecItemDataSource

// Make sure to call this before any writes to the keychain, so that we fire
// up the engines to monitor manifest changes.
SOSDataSourceFactoryRef SecItemDataSourceFactoryGetDefault(void) {
    return SecItemDataSourceFactoryGetShared(kc_dbhandle());
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
        SOSDataSourceForEachObject(ds, madd, error, ^void(CFDataRef digest, SOSObjectRef object, bool *stop) {
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

bool _SecServerRollKeys(bool force, CFErrorRef *error) {
#if USE_KEYSTORE
    uint32_t keystore_generation_status = 0;
    if (aks_generation(KEYBAG_DEVICE, generation_noop, &keystore_generation_status))
        return false;
    uint32_t current_generation = keystore_generation_status & generation_current;

    return kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
        bool up_to_date = s3dl_dbt_keys_current(dbt, current_generation, NULL);

        if (force && !up_to_date) {
            up_to_date = s3dl_dbt_update_keys(dbt, error);
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

bool
_SecServerGetKeyStats(const SecDbClass *qclass,
                      struct _SecServerKeyStats *stats)
{
    __block CFErrorRef error = NULL;
    bool res = false;

    Query *q = query_create(qclass, NULL, &error);
    require(q, fail);

    q->q_return_type = kSecReturnDataMask | kSecReturnAttributesMask;
    q->q_limit = kSecMatchUnlimited;
    q->q_keybag = KEYBAG_DEVICE;
    query_add_or_attribute(kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked, q);
    query_add_or_attribute(kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlock, q);
    query_add_or_attribute(kSecAttrAccessible, kSecAttrAccessibleAlways, q);
    query_add_or_attribute(kSecAttrAccessible, kSecAttrAccessibleWhenUnlockedThisDeviceOnly, q);
    query_add_or_attribute(kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly, q);
    query_add_or_attribute(kSecAttrAccessible, kSecAttrAccessibleAlwaysThisDeviceOnly, q);
    query_add_attribute(kSecAttrTombstone, kCFBooleanFalse, q);

    kc_with_dbt(false, &error, ^(SecDbConnectionRef dbconn) {
        CFErrorRef error2 = NULL;
        __block CFIndex totalSize = 0;
        stats->maxDataSize = 0;

        SecDbItemSelect(q, dbconn, &error2, NULL, ^bool(const SecDbAttr *attr) {
            return CFDictionaryContainsKey(q->q_item, attr->name);
        }, NULL, NULL, ^(SecDbItemRef item, bool *stop) {
            CFErrorRef error3 = NULL;
            CFDataRef data = SecDbItemGetValue(item, &v6v_Data, &error3);
            if (isData(data)) {
                CFIndex size = CFDataGetLength(data);
                if (size > stats->maxDataSize)
                    stats->maxDataSize = size;
                totalSize += size;
                stats->items++;
            }
            CFReleaseNull(error3);
        });
        CFReleaseNull(error2);
        if (stats->items)
            stats->averageSize = totalSize / stats->items;

        return (bool)true;
    });


    res = true;

fail:
    CFReleaseNull(error);
    if (q)
        query_destroy(q, NULL);
    return res;
}
