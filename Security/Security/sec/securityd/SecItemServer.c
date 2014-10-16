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
#include <SecureObjectSync/SOSDigestVector.h>

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

#if TARGET_OS_IPHONE
#include <SharedWebCredentials/SharedWebCredentials.h>
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

static const char * const s3dl_upgrade_sql[] = {
    /* 0 */
    "",

    /* 1 */
    /* Create indices. */
    "CREATE INDEX igsha ON genp(sha1);"
    "CREATE INDEX iisha ON inet(sha1);"
    "CREATE INDEX icsha ON cert(sha1);"
    "CREATE INDEX iksha ON keys(sha1);"
    "CREATE INDEX ialis ON cert(alis);"
    "CREATE INDEX isubj ON cert(subj);"
    "CREATE INDEX iskid ON cert(skid);"
    "CREATE INDEX ipkhh ON cert(pkhh);"
    "CREATE INDEX ikcls ON keys(kcls);"
    "CREATE INDEX iklbl ON keys(klbl);"
    "CREATE INDEX iencr ON keys(encr);"
    "CREATE INDEX idecr ON keys(decr);"
    "CREATE INDEX idrve ON keys(drve);"
    "CREATE INDEX isign ON keys(sign);"
    "CREATE INDEX ivrfy ON keys(vrfy);"
    "CREATE INDEX iwrap ON keys(wrap);"
    "CREATE INDEX iunwp ON keys(unwp);",

    /* 2 */
    "",

    /* 3 */
    /* Rename version 2 or version 3 tables and drop version table since
       step 0 creates it. */
    "ALTER TABLE genp RENAME TO ogenp;"
    "ALTER TABLE inet RENAME TO oinet;"
    "ALTER TABLE cert RENAME TO ocert;"
    "ALTER TABLE keys RENAME TO okeys;"
    "DROP TABLE tversion;",

    /* 4 */
    "",

    /* 5 */
    "",

    /* 6 */
    "",

    /* 7 */
    /* Move data from version 5 tables to new ones and drop old ones. */
    "INSERT INTO genp (rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,svce,gena,data,agrp,pdmn) SELECT rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,svce,gena,data,agrp,pdmn from ogenp;"
    "INSERT INTO inet (rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,sdmn,srvr,ptcl,atyp,port,path,data,agrp,pdmn) SELECT rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,sdmn,srvr,ptcl,atyp,port,path,data,agrp,pdmn from oinet;"
    "INSERT INTO cert (rowid,cdat,mdat,ctyp,cenc,labl,alis,subj,issr,slnr,skid,pkhh,data,agrp,pdmn) SELECT rowid,cdat,mdat,ctyp,cenc,labl,alis,subj,issr,slnr,skid,pkhh,data,agrp,pdmn from ocert;"
    "INSERT INTO keys (rowid,cdat,mdat,kcls,labl,alis,perm,priv,modi,klbl,atag,crtr,type,bsiz,esiz,sdat,edat,sens,asen,extr,next,encr,decr,drve,sign,vrfy,snrc,vyrc,wrap,unwp,data,agrp,pdmn) SELECT rowid,cdat,mdat,kcls,labl,alis,perm,priv,modi,klbl,atag,crtr,type,bsiz,esiz,sdat,edat,sens,asen,extr,next,encr,decr,drve,sign,vrfy,snrc,vyrc,wrap,unwp,data,agrp,pdmn from okeys;"
    "DROP TABLE ogenp;"
    "DROP TABLE oinet;"
    "DROP TABLE ocert;"
    "DROP TABLE okeys;"
    "CREATE INDEX igsha ON genp(sha1);"
    "CREATE INDEX iisha ON inet(sha1);"
    "CREATE INDEX icsha ON cert(sha1);"
    "CREATE INDEX iksha ON keys(sha1);",
};

struct sql_stages {
    int pre;
    int main;
    int post;
    bool init_pdmn; // If true do a full export followed by an import of the entire database so all items are re-encoded.
};

/* On disk database format version upgrade scripts.
   If pre is 0, version is unsupported and db is considered corrupt for having that version.
   First entry creates the current db, each susequent entry upgrade to current from the version
   represented by the index of the slot.  Each script is either -1 (disabled) of the number of
   the script in the main table.
    {pre,main,post, reencode} */


static struct sql_stages s3dl_upgrade_script[] = {
    { -1, 0, 1, false },/* 0->current: Create version 6*/
    {},                 /* 1->current: Upgrade to version 6 from version 1 -- Unsupported. */
    {},                 /* 2->current: Upgrade to version 6 from version 2 -- Unsupported */
    {},                 /* 3->current: Upgrade to version 6 from version 3 -- Unsupported */
    {},                 /* 4->current: Upgrade to version 6 from version 4 -- Unsupported */
    { 3, 0, 7, true },  /* 5->current: Upgrade to version 6 from version 5 */
};

static bool sql_run_script(SecDbConnectionRef dbt, int number, CFErrorRef *error)
{
    /* Script -1 == skip this step. */
    if (number < 0)
        return true;

    /* If we are attempting to run a script we don't have, fail. */
    if ((size_t)number >= array_size(s3dl_upgrade_sql))
        return SecDbError(SQLITE_CORRUPT, error, CFSTR("script %d exceeds maximum %d"),
                                number, (int)(array_size(s3dl_upgrade_sql)));
    __block bool ok = true;
    if (number == 0) {
        CFMutableStringRef sql = CFStringCreateMutable(0, 0);
        SecDbAppendCreateTableWithClass(sql, &genp_class);
        SecDbAppendCreateTableWithClass(sql, &inet_class);
        SecDbAppendCreateTableWithClass(sql, &cert_class);
        SecDbAppendCreateTableWithClass(sql, &keys_class);
        CFStringAppend(sql, CFSTR("CREATE TABLE tversion(version INTEGER);INSERT INTO tversion(version) VALUES(6);"));
        CFStringPerformWithCString(sql, ^(const char *sql_string) {
            ok = SecDbErrorWithDb(sqlite3_exec(SecDbHandle(dbt), sql_string, NULL, NULL, NULL),
                                     SecDbHandle(dbt), error, CFSTR("sqlite3_exec: %s"), sql_string);
        });
        CFReleaseSafe(sql);
    } else {
        ok = SecDbErrorWithDb(sqlite3_exec(SecDbHandle(dbt), s3dl_upgrade_sql[number], NULL, NULL, NULL),
                                 SecDbHandle(dbt), error, CFSTR("sqlite3_exec: %s"), s3dl_upgrade_sql[number]);
    }
    return ok;
}

/* Return the current database version in *version.  Returns a
 SQLITE error. */
static bool s3dl_dbt_get_version(SecDbConnectionRef dbt, int *version, CFErrorRef *error)
{
    CFStringRef sql = CFSTR("SELECT version FROM tversion LIMIT 1");
    return SecDbWithSQL(dbt, sql, error, ^(sqlite3_stmt *stmt) {
        __block bool found_version = false;
        bool step_ok = SecDbForEach(stmt, error, ^(int row_index __unused) {
            if (!found_version) {
                *version = sqlite3_column_int(stmt, 0);
                found_version = true;
            }
            return found_version;
        });
        if (!found_version) {
            /* We have a tversion table but we didn't find a single version
             value, now what? I suppose we pretend the db is corrupted
             since this isn't supposed to ever happen. */
            step_ok = SecDbError(SQLITE_CORRUPT, error, CFSTR("Failed to read version: database corrupt"));
            secwarning("SELECT version step: %@", error ? *error : NULL);
        }
        return step_ok;
    });
}


static bool s3dl_dbt_upgrade_from_version(SecDbConnectionRef dbt, int version, CFErrorRef *error)
{
    /* We need to go from db version to CURRENT_DB_VERSION, let's do so. */
    __block bool ok = true;
    /* O, guess we're done already. */
    if (version == CURRENT_DB_VERSION)
        return ok;

    if (ok && version < 6) {
        // Pre v6 keychains need to have WAL enabled, since SecDb only
        // does this at db creation time.
        // NOTE: This has to be run outside of a transaction.
        ok = (SecDbExec(dbt, CFSTR("PRAGMA auto_vacuum = FULL"), error) &&
              SecDbExec(dbt, CFSTR("PRAGMA journal_mode = WAL"), error));
    }

    // Start a transaction to do the upgrade within
    if (ok) { ok = SecDbTransaction(dbt, kSecDbExclusiveTransactionType, error, ^(bool *commit) {
        // Be conservative and get the version again once we start a transaction.
        int cur_version = version;
        s3dl_dbt_get_version(dbt, &cur_version, NULL);

        /* If we are attempting to upgrade to a version greater than what we have
         an upgrade script for, fail. */
        if (ok && (cur_version < 0 ||
            (size_t)cur_version >= array_size(s3dl_upgrade_script))) {
            ok = SecDbError(SQLITE_CORRUPT, error, CFSTR("no upgrade script for version: %d"), cur_version);
            secerror("no upgrade script for version %d", cur_version);
        }

        struct sql_stages *script;
        if (ok) {
            script = &s3dl_upgrade_script[cur_version];
            if (script->pre == 0)
                ok = SecDbError(SQLITE_CORRUPT, error, CFSTR("unsupported db version %d"), cur_version);
        }
        if (ok)
            ok = sql_run_script(dbt, script->pre, error);
        if (ok)
            ok = sql_run_script(dbt, script->main, error);
        if (ok)
            ok = sql_run_script(dbt, script->post, error);
        if (ok && script->init_pdmn) {
            CFErrorRef localError = NULL;
            CFDictionaryRef backup = SecServerExportKeychainPlist(dbt,
                                                                  KEYBAG_DEVICE, KEYBAG_NONE, kSecNoItemFilter, &localError);
            if (backup) {
                if (localError) {
                    secerror("Ignoring export error: %@ during upgrade export", localError);
                    CFReleaseNull(localError);
                }
                ok = SecServerImportKeychainInPlist(dbt, KEYBAG_NONE,
                                                    KEYBAG_DEVICE, backup, kSecNoItemFilter, &localError);
                CFRelease(backup);
            } else {
                ok = false;

                if (localError && SecErrorGetOSStatus(localError) == errSecInteractionNotAllowed) {
                    SecError(errSecUpgradePending, error,
                         CFSTR("unable to complete upgrade due to device lock state"));
                    secerror("unable to complete upgrade due to device lock state");
                } else {
                    secerror("unable to complete upgrade for unknown reason, marking DB as corrupt: %@", localError);
                    SecDbCorrupt(dbt);
                }
            }

            if (localError) {
                if (error && !*error)
                    *error = localError;
                else
                    CFRelease(localError);
            }
        } else if (!ok) {
            secerror("unable to complete upgrade scripts, marking DB as corrupt: %@", error ? *error : NULL);
            SecDbCorrupt(dbt);
        }
        *commit = ok;
    }); } else {
        secerror("unable to complete upgrade scripts, marking DB as corrupt: %@", error ? *error : NULL);
        SecDbCorrupt(dbt);
    }

    return ok;
}


/* This function is called if the db doesn't have the proper version.  We
   start an exclusive transaction and recheck the version, and then perform
   the upgrade within that transaction. */
static bool s3dl_dbt_upgrade(SecDbConnectionRef dbt, CFErrorRef *error)
{
    // Already in a transaction
    //return kc_transaction(dbt, error, ^{
        int version = 0; // Upgrade from version 0 == create new db
        s3dl_dbt_get_version(dbt, &version, NULL);
        return s3dl_dbt_upgrade_from_version(dbt, version, error);
    //});
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


static CF_RETURNS_RETAINED CFDataRef SecServerExportKeychain(SecDbConnectionRef dbt,
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

static bool SecServerImportKeychain(SecDbConnectionRef dbt,
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
        backup = SecServerExportKeychain(dbt, KEYBAG_DEVICE, backup_keybag, error);
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
    bool ok = SecServerImportKeychain(dbt, backup_keybag, KEYBAG_DEVICE,
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
    return SecDbCreate(path, ^bool (SecDbConnectionRef dbconn, bool didCreate, CFErrorRef *localError) {
        bool ok;
        if (didCreate)
            ok = s3dl_dbt_upgrade_from_version(dbconn, 0, localError);
        else
            ok = s3dl_dbt_upgrade(dbconn, localError);

        if (!ok)
            secerror("Upgrade %sfailed: %@", didCreate ? "from v0 " : "", localError ? *localError : NULL);

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

static dispatch_once_t _kc_dbhandle_once;

static SecDbRef kc_dbhandle(void) {
    dispatch_once(&_kc_dbhandle_once, ^{
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

/* AUDIT[securityd](done):
   query (ok) is a caller provided dictionary, only its cf type has been checked.
 */
static bool
SecItemServerCopyMatching(CFDictionaryRef query, CFTypeRef *result,
    CFArrayRef accessGroups, CFErrorRef *error)
{
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
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

        query_enable_interactive(q);
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
            do {
                ok = kc_with_dbt(false, error, ^(SecDbConnectionRef dbt) {
                    return s3dl_copy_matching(dbt, q, result, accessGroups, error);
                });
            } while (query_needs_authentication(q) && (ok = query_authenticate(q, &error)));
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
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups)))
        return SecError(errSecMissingEntitlement, error,
                           CFSTR("client has neither application-identifier nor keychain-access-groups entitlements"));

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
                return SecError(errSecNoAccessForItem, error, CFSTR("NoAccessForItem"));
        } else {
            agrp = (CFStringRef)CFArrayGetValueAtIndex(ag, 0);

            /* We are using an implicit access group, add it as if the user
               specified it as an attribute. */
            query_add_attribute(kSecAttrAccessGroup, agrp, q);
        }

        query_ensure_access_control(q, agrp);
        query_enable_interactive(q);

        if (q->q_row_id)
            ok = SecError(errSecValuePersistentRefUnsupported, error, CFSTR("q_row_id"));  // TODO: better error string
    #if defined(MULTIPLE_KEYCHAINS)
        else if (q->q_use_keychain_list)
            ok = SecError(errSecUseKeychainListUnsupported, error, CFSTR("q_use_keychain_list"));  // TODO: better error string;
    #endif
        else if (!q->q_error) {
            do {
                ok = kc_with_dbt(true, error, ^(SecDbConnectionRef dbt){
                    return kc_transaction(dbt, error, ^{
                        query_pre_add(q, true);
                        return s3dl_query_add(dbt, q, result, error);
                    });
                });
            } while (query_needs_authentication(q) && (ok = query_authenticate(q, &error)));
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
        if (!q->q_use_tomb && SOSCCThisDeviceDefinitelyNotActiveInCircle()) {
            q->q_use_tomb = kCFBooleanFalse;
        }
        query_enable_interactive(q);
        do {
            ok = kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
                return s3dl_query_update(dbt, q, attributesToUpdate, accessGroups, error);
            });
        } while (query_needs_authentication(q) && (ok = query_authenticate(q, &error)));
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
        q->q_crypto_op = kSecKsDelete;
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
            if (!q->q_use_tomb && SOSCCThisDeviceDefinitelyNotActiveInCircle()) {
                q->q_use_tomb = kCFBooleanFalse;
            }
            query_enable_interactive(q);
            do {
                ok = kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
                    return s3dl_query_delete(dbt, q, accessGroups, error);
                });
            } while (query_needs_authentication(q) && (ok = query_authenticate(q, &error)));
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
#define SEC_CONST_DECL(k,v) CFTypeRef k = (CFTypeRef)(CFSTR(v));

SEC_CONST_DECL (kSecSafariAccessGroup, "com.apple.cfnetwork");
SEC_CONST_DECL (kSecSafariDefaultComment, "default");
SEC_CONST_DECL (kSecSafariPasswordsNotSaved, "Passwords not saved");
SEC_CONST_DECL (kSecSharedCredentialUrlScheme, "https://");
SEC_CONST_DECL (kSecSharedWebCredentialsService, "webcredentials");

#if !TARGET_IPHONE_SIMULATOR
static SWCFlags
_SecAppDomainApprovalStatus(CFStringRef appID, CFStringRef fqdn, CFErrorRef *error)
{
    __block SWCFlags flags = kSWCFlags_None;

#if TARGET_OS_IPHONE
    CFRetainSafe(appID);
    CFRetainSafe(fqdn);
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    dispatch_retain(semaphore);
    if (0 == SWCCheckService(kSecSharedWebCredentialsService, appID, fqdn,
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

#if TARGET_OS_IPHONE
    // update our database
    CFRetainSafe(appID);
    CFRetainSafe(fqdn);
    if (0 == SWCSetServiceFlags(kSecSharedWebCredentialsService,
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

    CFStringRef fqdn = CFDictionaryGetValue(attributes, kSecAttrServer);
    CFStringRef account = CFDictionaryGetValue(attributes, kSecAttrAccount);
#if TARGET_OS_IPHONE
    CFStringRef password = CFDictionaryGetValue(attributes, kSecSharedPassword);
#else
    CFStringRef password = CFDictionaryGetValue(attributes, CFSTR("spwd"));
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
        CFRetainSafe(fqdn);
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
    CFReleaseNull(*result);
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
        CFReleaseNull(*result);
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
    CFReleaseNull(*result);
    CFReleaseNull(*error);

    // password does not exist, so prepare to add it
    if (true) {
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
                #if TARGET_OS_IPHONE
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
        #if TARGET_OS_IPHONE
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
#if TARGET_OS_IPHONE
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
            #if TARGET_OS_IPHONE
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
        backup = SecServerExportKeychain(dbt, KEYBAG_DEVICE, backup_keybag_handle, error);
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
_SecServerKeychainSyncUpdateKeyParameter(CFDictionaryRef updates, CFErrorRef *error) {
    // This never fails, trust us!
    return SOSCCHandleUpdateKeyParameter(updates);
}


CF_RETURNS_RETAINED CFArrayRef
_SecServerKeychainSyncUpdateCircle(CFDictionaryRef updates, CFErrorRef *error) {
    // This never fails, trust us!
    return SOSCCHandleUpdateCircle(updates);
}

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
    SOSDataSourceRef ds = dsf->create_datasource(dsf, kSecAttrAccessibleWhenUnlocked, error);
    if (ds) {
        backup_new = backup ? CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, backup) : CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        mold = SOSCreateManifestWithBackup(backup, error);
        SOSEngineRef engine = SOSDataSourceGetSharedEngine(ds, error);
        mnow = SOSEngineCopyManifest(engine, error);
        if (!mnow) {
            mnow = SOSDataSourceCopyManifest(ds, error);
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

        __block struct SOSDigestVector dvdel = SOSDigestVectorInit;
        SOSDataSourceForEachObject(ds, madd, error, ^void(CFDataRef digest, SOSObjectRef object, bool *stop) {
            CFErrorRef localError = NULL;
            CFDataRef digest_data = NULL;
            CFTypeRef value = NULL;
            if (!object) {
                // Key in our manifest can't be found in db, remove it from our manifest
                SOSDigestVectorAppend(&dvdel, CFDataGetBytePtr(digest));
            } else if (!(digest_data = SOSObjectCopyDigest(ds, object, &localError))
                || !(value = SOSObjectCopyBackup(ds, object, bag_handle, &localError))) {
                if (SecErrorGetOSStatus(localError) == errSecDecode) {
                    // Ignore decode errors, pretend the objects aren't there
                    CFRelease(localError);
                    // Object undecodable, remove it from our manifest
                    SOSDigestVectorAppend(&dvdel, CFDataGetBytePtr(digest));
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

        if (dvdel.count) {
            struct SOSDigestVector dvadd = SOSDigestVectorInit;
            if (!SOSEngineUpdateLocalManifest(engine, kSOSDataSourceSOSTransaction, &dvdel, &dvadd, error)) {
                CFReleaseNull(backup_new);
            }
            SOSDigestVectorFree(&dvdel);
        }

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
        SOSDataSourceRef ds = dsf->create_datasource(dsf, kSecAttrAccessibleWhenUnlocked, error);
        if (ds) {
            ok = SOSDataSourceWith(ds, error, ^(SOSTransactionRef txn, bool *commit) {
                SOSManifestRef mnow = SOSDataSourceCopyManifest(ds, error);
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
        } else {
            ok = false;
        }
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
