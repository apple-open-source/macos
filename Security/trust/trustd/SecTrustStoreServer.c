/*
 * Copyright (c) 2007-2010,2012-2024 Apple Inc. All Rights Reserved.
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
 * SecTrustStoreServer.c - CertificateSource API to a system root certificate store
 */
#include "SecTrustStoreServer.h"
#include "SecTrustSettingsServer.h"

#include <Security/SecCertificateInternal.h>
#include <Security/SecTrustInternal.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecFramework.h>
#include <errno.h>
#include <limits.h>
#include <dispatch/dispatch.h>
#include <sqlite3.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFURL.h>
#include <AssertMacros.h>
#include <utilities/debugging.h>
#include <Security/SecBasePriv.h>
#include <Security/SecInternal.h>
#include <ipc/securityd_client.h>
#include "featureflags/featureflags.h"
#include "trust/trustd/SecTrustStoreServer.h"
#include "utilities/sqlutils.h"
#include "utilities/SecDb.h"
#include <utilities/SecCFError.h>
#include "utilities/SecFileLocations.h"
#include <utilities/SecDispatchRelease.h>
#include "trust/trustd/SecTrustLoggingServer.h"
#include <os/variant_private.h>
#include <dirent.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecInternalReleasePriv.h>
#include "trust/trustd/SecCertificateSource.h"
#include "trust/trustd/trustdFileLocations.h"
#include "trust/trustd/trustdVariants.h"


static dispatch_once_t kSecTrustStoreUserOnce;
static dispatch_once_t kSecTrustStoreAdminOnce;
static dispatch_once_t kSecTrustStoreSystemOnce;
static SecTrustStoreRef kSecTrustStoreUser = NULL;
static SecTrustStoreRef kSecTrustStoreAdmin = NULL;
static SecTrustStoreRef kSecTrustStoreSystem = NULL;

static const char beginExclusiveTxnSQL[] = "BEGIN EXCLUSIVE TRANSACTION;";
static const char endExclusiveTxnSQL[] = "COMMIT TRANSACTION; VACUUM;";
#if !TARGET_OS_OSX
static const char copyAllOldSQL[] = "SELECT data,tset FROM tsettings ORDER BY sha1";
#endif // !TARGET_OS_OSX

static const char copyParentsSQL[] = "SELECT data FROM tsettings WHERE subj=? AND uuid=?";
static const char containsSQL[] = "SELECT tset FROM tsettings WHERE sha256=? AND uuid=?";
static const char insertSQL[] = "INSERT OR REPLACE INTO tsettings(sha256,subj,tset,data,uuid)VALUES(?,?,?,?,?)";
static const char deleteSQL[] = "DELETE FROM tsettings WHERE sha256=? AND uuid=?";
static const char deleteAllSQL[] = "DELETE from tsettings WHERE uuid=?";
#if TARGET_OS_IPHONE
static const char copyAllSQL[] = "SELECT data,tset FROM tsettings WHERE uuid=? ORDER BY sha256";
#endif // TARGET_OS_IPHONE
static const char countAllSQL[] = "SELECT COUNT(*) FROM tsettings WHERE uuid=?";
#if !TARGET_OS_OSX
static const char countAllV1SQL[] = "SELECT COUNT(*) FROM tsettings";
static const char findUUIDColSQL[] = "SELECT INSTR(sql,'uuid') FROM sqlite_master WHERE type='table' AND name='tsettings'";
static const char copyToTmpSQL[] = "INSERT OR REPLACE INTO tmp_tsettings(sha256,subj,tset,data) SELECT sha256,subj,tset,data FROM tsettings";
static const char updateUUIDSQL[] = "UPDATE tsettings SET uuid=? WHERE uuid=''";
#endif // !TARGET_OS_OSX

#define kSecTrustStoreName CFSTR("TrustStore")
#define kSecTrustStoreDbExtension CFSTR("sqlite3")

#define kTrustStoreFileName CFSTR("TrustStore.sqlite3")

#define TRUSTD_ROLE_ACCOUNT 282


struct __SecTrustStore {
    dispatch_queue_t queue;
    sqlite3 *s3h;
    sqlite3_stmt *copyParents;
    sqlite3_stmt *contains;
    bool readOnly;
    bool containsSettings;  // For optimization of high-use calls.
    SecTrustStoreDomain domain;
};

// MARK: -
// MARK: Corporate Root functions

// MARK: -
// MARK: Trust store functions

static int sec_create_path(const char *path)
{
	char pathbuf[PATH_MAX];
	size_t pos, len = strlen(path);
	if (len == 0 || len > PATH_MAX)
		return SQLITE_CANTOPEN;
	memcpy(pathbuf, path, len);
	for (pos = len-1; pos > 0; --pos)
	{
		/* Search backwards for trailing '/'. */
		if (pathbuf[pos] == '/')
		{
			pathbuf[pos] = '\0';
			/* Attempt to create parent directories of the database. */
			if (!mkdir(pathbuf, 0777))
				break;
			else
			{
				int err = errno;
				if (err == EEXIST)
					return 0;
				if (err == ENOTDIR)
					return SQLITE_CANTOPEN;
				if (err == EROFS)
					return SQLITE_READONLY;
				if (err == EACCES)
					return SQLITE_PERM;
				if (err == ENOSPC || err == EDQUOT)
					return SQLITE_FULL;
				if (err == EIO)
					return SQLITE_IOERR;

				/* EFAULT || ELOOP | ENAMETOOLONG || something else */
				return SQLITE_INTERNAL;
			}
		}
	}
	return SQLITE_OK;
}

static int sec_sqlite3_open(const char *db_name, sqlite3 **s3h,
	bool create_path)
{
	int s3e;
#if TARGET_OS_IPHONE
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FILEPROTECTION_NONE;
#else
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
#endif
	s3e = sqlite3_open_v2(db_name, s3h, flags, NULL);
	if (s3e == SQLITE_CANTOPEN && create_path) {
		/* Make sure the path to db_name exists and is writable, then
		   try again. */
		s3e = sec_create_path(db_name);
		if (!s3e)
			s3e = sqlite3_open(db_name, s3h);
	}

	return s3e;
}

static bool SecExtractFilesystemPathForPrivateTrustdFile(CFStringRef file, UInt8 *buffer, CFIndex maxBufLen)
{
    bool translated = false;
    CFURLRef fileURL = SecCopyURLForFileInPrivateTrustdDirectory(file);

    if (fileURL && CFURLGetFileSystemRepresentation(fileURL, false, buffer, maxBufLen))
        translated = true;
    CFReleaseSafe(fileURL);

    return translated;
}

static bool SecTrustStoreOpenSharedConnection(SecTrustStoreRef ts) {
    __block int s3e = SQLITE_ERROR;
    require(ts && ts->queue, errOut);
    dispatch_sync(ts->queue, ^{
        static sqlite3* dbhandle = NULL;
        if (dbhandle) {
            s3e = SQLITE_OK;
        } else {
            const char path[MAXPATHLEN];
            bool pathResult = SecExtractFilesystemPathForPrivateTrustdFile(kTrustStoreFileName, (UInt8*) path, (CFIndex) sizeof(path));
            if (pathResult) {
                s3e = sec_sqlite3_open(path, &dbhandle, true);
            }
        }
        ts->s3h = dbhandle;
    });
    require_noerr(s3e, errOut);
    return true;
errOut:
    secerror("Unable to open shared db connection (error %d)", s3e);
    return false;
}

static CFDataRef SecTrustStoreCopyUUID(SecTrustStoreRef ts) {
    if (ts == NULL) { return NULL; }
    /* If this is not the user domain, effective uid for this entry is TRUSTD_ROLE_ACCOUNT */
    uid_t euid = (ts->domain == kSecTrustStoreDomainUser) ? geteuid() : TRUSTD_ROLE_ACCOUNT;
    return SecCopyUUIDDataForUID(euid);
}

static int64_t SecTrustStoreCountAll(SecTrustStoreRef ts) {
    __block int64_t result = -1;
    __block CFDataRef uuid = SecTrustStoreCopyUUID(ts);

    require_quiet(ts && uuid, errOutNotLocked);
    dispatch_sync(ts->queue, ^{
        sqlite3_stmt *countAllStmt = NULL;
        int s3e = sqlite3_prepare_v2(ts->s3h,
                                     countAllSQL,
                                     sizeof(countAllSQL),
                                     &countAllStmt, NULL);
        if (s3e == SQLITE_OK) {
            /* "count all" means only those entries matching the user or admin uuid */
            require_noerr_quiet(s3e = sqlite3_bind_blob_wrapper(countAllStmt, 1,
                                                                 CFDataGetBytePtr(uuid), (size_t)CFDataGetLength(uuid),
                                                                 SQLITE_STATIC), errOutSql);
        }
        if (s3e == SQLITE_OK) {
            s3e = sqlite3_step(countAllStmt);
            if (s3e == SQLITE_ROW) {
                result = sqlite3_column_int64(countAllStmt, 0);
            }
        }
    errOutSql:
        if (countAllStmt) {
            verify_noerr(sqlite3_finalize(countAllStmt));
        }
    });

errOutNotLocked:
    CFReleaseNull(uuid);
    return result;
}

static SecTrustStoreRef SecTrustStoreCreate(const char *db_name,
    bool create,
    SecTrustStoreDomain domain) {
    SecTrustStoreRef ts;
    int s3e = SQLITE_OK;

    require(ts = (SecTrustStoreRef)malloc(sizeof(struct __SecTrustStore)), errOut);
    memset(ts, 0, sizeof(struct __SecTrustStore));
    ts->queue = dispatch_queue_create("truststore", DISPATCH_QUEUE_SERIAL);
    ts->domain = domain;


    require(TrustdVariantAllowsFileWrite(), errOut);
    require(SecTrustStoreOpenSharedConnection(ts), errOut);

    s3e = sqlite3_prepare_v3(ts->s3h,
                             copyParentsSQL,
                             sizeof(copyParentsSQL),
                             SQLITE_PREPARE_PERSISTENT, &ts->copyParents, NULL);
	if (create && s3e == SQLITE_ERROR) {
		/* sqlite3_prepare returns SQLITE_ERROR if the table we are
		   compiling this statement for doesn't exist. */
		char *errmsg = NULL;
        s3e = sqlite3_exec(ts->s3h,
                           "CREATE TABLE tsettings("
                           "sha256 BLOB NOT NULL DEFAULT '',"
                           "subj BLOB NOT NULL DEFAULT '',"
                           "tset BLOB,"
                           "data BLOB,"
                           "uuid BLOB NOT NULL DEFAULT '',"
                           "UNIQUE(sha256,uuid)"
                           ");"
                           "CREATE INDEX isubj ON tsettings(subj);"
                           , NULL, NULL, &errmsg);
		if (errmsg) {
			secwarning("CREATE TABLE tsettings: %s", errmsg);
			sqlite3_free(errmsg);
		}
		require_noerr(s3e, errOut);
        s3e = sqlite3_prepare_v3(ts->s3h,
                                 copyParentsSQL,
                                 sizeof(copyParentsSQL),
                                 SQLITE_PREPARE_PERSISTENT, &ts->copyParents, NULL);
	}
	require_noerr(s3e, errOut);
	require_noerr(s3e = sqlite3_prepare_v3(ts->s3h,
                                           containsSQL,
                                           sizeof(containsSQL),
                                           SQLITE_PREPARE_PERSISTENT,
                                           &ts->contains, NULL), errOut);

    if (SecTrustStoreCountAll(ts) == 0) {
        ts->containsSettings = false;
    } else {
        /* In the error case where SecTrustStoreCountAll returns a negative result,
         * we'll pretend there are contents in the trust store so that we still do
         * DB operations */
        ts->containsSettings = true;
    }

    return ts;

errOut:
    if (ts) {
        /* Don't call sqlite3_close(ts->s3h) since this is a shared connection */
        dispatch_release_safe(ts->queue);
        free(ts);
    }
    secerror("Failed to create trust store database: %d", s3e);
    TrustdHealthAnalyticsLogErrorCodeForDatabase(TATrustStore, TAOperationCreate, TAFatalError, s3e);

    return NULL;
}

static void SecTrustStoreInitUser(void) {
	const char path[MAXPATHLEN];
    bool pathResult = false;
    pathResult = SecExtractFilesystemPathForPrivateTrustdFile(kTrustStoreFileName, (UInt8*) path, (CFIndex) sizeof(path));
    secdebug("truststore", "Creating user trust store at %s (%d)", path, (pathResult) ? 1 : 0);
    if (pathResult) {
        kSecTrustStoreUser = SecTrustStoreCreate(path, true, kSecTrustStoreDomainUser);
        if (kSecTrustStoreUser) {
            if (kSecTrustStoreUser->s3h) {
                kSecTrustStoreUser->readOnly = false;
            }
        }
    }
}

static void SecTrustStoreInitAdmin(void) {
    const char path[MAXPATHLEN];
    bool pathResult = false;
    pathResult = SecExtractFilesystemPathForPrivateTrustdFile(kTrustStoreFileName, (UInt8*) path, (CFIndex) sizeof(path));
    secdebug("config", "Creating admin trust store at %s (%d)", path, (pathResult) ? 1 : 0);
    if (pathResult) {
        kSecTrustStoreAdmin = SecTrustStoreCreate(path, true, kSecTrustStoreDomainAdmin);
        if (kSecTrustStoreAdmin) {
            if (kSecTrustStoreAdmin->s3h) {
                kSecTrustStoreAdmin->readOnly = false;
            }
        }
    }
}

static void SecTrustStoreInitSystem(void) {
    if (TrustdVariantHasCertificatesBundle()) {
        kSecTrustStoreSystem = (SecTrustStoreRef)malloc(sizeof(struct __SecTrustStore));
        if (kSecTrustStoreSystem) {
            memset(kSecTrustStoreSystem, 0, sizeof(struct __SecTrustStore));
            kSecTrustStoreSystem->readOnly = true;
            kSecTrustStoreSystem->domain = kSecTrustStoreDomainSystem;
        }
    }
}

/* AUDIT[securityd](done):
   domainName (ok) is a caller provided string of any length (might be 0), only
       its cf type has been checked.
 */
SecTrustStoreRef SecTrustStoreForDomainName(CFStringRef domainName, CFErrorRef *error) {
    SecTrustStoreRef ts = NULL;
	if (CFEqualSafe(CFSTR("user"), domainName)) {
		dispatch_once(&kSecTrustStoreUserOnce, ^{ SecTrustStoreInitUser(); });
		ts = kSecTrustStoreUser;
    } else if (CFEqualSafe(CFSTR("admin"), domainName)) {
        dispatch_once(&kSecTrustStoreAdminOnce, ^{ SecTrustStoreInitAdmin(); });
        ts = kSecTrustStoreAdmin;
    } else if (CFEqualSafe(CFSTR("system"), domainName)) {
        dispatch_once(&kSecTrustStoreSystemOnce, ^{ SecTrustStoreInitSystem(); });
        ts = kSecTrustStoreSystem;
	} else {
        SecError(errSecParam, error, CFSTR("unknown domain: %@"), domainName);
		return NULL;
	}
    if (ts == NULL) {
        SecError(errSecInternal, error, CFSTR("unable to initialize trust store for %@ domain"), domainName);
    }
    return ts;
}

/* AUDIT[securityd](done):
   ts (ok) might be NULL.
   certificate (ok) is a valid SecCertificateRef.
   trustSettingsDictOrArray (checked by CFPropertyListCreateXMLData) is either
   NULL, a dictionary or an array, but its contents have not been checked.
 */
bool _SecTrustStoreSetTrustSettings(SecTrustStoreRef ts,
    SecCertificateRef certificate,
    CFTypeRef tsdoa, CFErrorRef *error) {
    __block bool ok;
    secdebug("truststore", "[%s trustd] SecTrustStoreSetTrustSettings for domain %d", (geteuid() == TRUSTD_ROLE_ACCOUNT) ? "system" : "user", ts->domain);
    require_action_quiet(ts, errOutNotLocked, ok = SecError(errSecParam, error, CFSTR("truststore is NULL")));
    require_action_quiet(!ts->readOnly, errOutNotLocked, ok = SecError(errSecReadOnly, error, CFSTR("truststore is readOnly")));
    dispatch_sync(ts->queue, ^{
        CFTypeRef trustSettingsDictOrArray = tsdoa;
        sqlite3_stmt *insert = NULL;
        CFDataRef xmlData = NULL;
        CFArrayRef array = NULL;
        CFDataRef subject = NULL;
        CFDataRef digest = NULL;
        CFDataRef uuid = NULL;

        require_action_quiet(subject = SecCertificateGetNormalizedSubjectContent(certificate),
                             errOut, ok = SecError(errSecParam, error, CFSTR("get normalized subject failed")));
        require_action_quiet(digest = SecCertificateCopySHA256Digest(certificate), errOut, ok = SecError(errSecParam, error, CFSTR("get sha256 digest failed")));
        require_action_quiet(uuid = SecTrustStoreCopyUUID(ts), errOut, ok = SecError(errSecInternal, error, CFSTR("get uuid failed")));

        /* Do some basic checks on the trust settings passed in. */
        if (trustSettingsDictOrArray == NULL) {
            require_action_quiet(array = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks), errOut, ok = SecError(errSecAllocate, error, CFSTR("CFArrayCreate failed")));
            trustSettingsDictOrArray = array;
        }
        else if(CFGetTypeID(trustSettingsDictOrArray) == CFDictionaryGetTypeID()) {
            /* array-ize it */
            array = CFArrayCreate(NULL, &trustSettingsDictOrArray, 1,
                                  &kCFTypeArrayCallBacks);
            trustSettingsDictOrArray = array;
        }
        else {
            require_action_quiet(CFGetTypeID(trustSettingsDictOrArray) == CFArrayGetTypeID(), errOut, ok = SecError(errSecParam, error, CFSTR("trustSettingsDictOrArray neither dict nor array")));
        }

        require_action_quiet(xmlData = CFPropertyListCreateXMLData(kCFAllocatorDefault,
                                                                   trustSettingsDictOrArray), errOut, ok = SecError(errSecParam, error, CFSTR("xml encode failed")));

        int s3e = sqlite3_exec(ts->s3h, beginExclusiveTxnSQL, NULL, NULL, NULL);
        require_action_quiet(s3e == SQLITE_OK, errOut, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
        require_action_quiet(CFDataGetLength(digest) > 0 &&
                             CFDataGetLength(subject) > 0 &&
                             CFDataGetLength(xmlData) > 0 &&
                             SecCertificateGetLength(certificate) > 0,
                             errOut, ok = SecError(errSecInternal, error, CFSTR("size error")));

        /* Parameter order is sha256,subj,tset,data{,uuid}. */
        require_noerr_action_quiet(s3e = sqlite3_prepare_v2(ts->s3h,
                                            insertSQL,
                                            sizeof(insertSQL),
                                            &insert, NULL),
                                   errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
        require_noerr_action_quiet(s3e = sqlite3_bind_blob_wrapper(insert, 1,
                                                             CFDataGetBytePtr(digest), (size_t)CFDataGetLength(digest), SQLITE_STATIC),
                                   errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
        require_noerr_action_quiet(s3e = sqlite3_bind_blob_wrapper(insert, 2,
                                                             CFDataGetBytePtr(subject), (size_t)CFDataGetLength(subject),
                                                             SQLITE_STATIC),
                                   errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
        require_noerr_action_quiet(s3e = sqlite3_bind_blob_wrapper(insert, 3,
                                                             CFDataGetBytePtr(xmlData), (size_t)CFDataGetLength(xmlData),
                                                             SQLITE_STATIC),
                                   errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
        require_noerr_action_quiet(s3e = sqlite3_bind_blob_wrapper(insert, 4,
                                                             SecCertificateGetBytePtr(certificate),
                                                             (size_t)SecCertificateGetLength(certificate), SQLITE_STATIC),
                                   errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
        require_noerr_action_quiet(s3e = sqlite3_bind_blob_wrapper(insert, 5,
                                                            CFDataGetBytePtr(uuid), (size_t)CFDataGetLength(uuid),
                                                                   SQLITE_STATIC),
                                   errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
        s3e = sqlite3_step(insert);
        if (s3e == SQLITE_DONE) {
            /* Great the insert worked. */
            ok = true;
            ts->containsSettings = true;
        } else {
            require_noerr_action_quiet(s3e, errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
            ok = true;
        }

    errOutSql:
        if (insert) {
            s3e = sqlite3_finalize(insert);
        }

        if (ok && s3e == SQLITE_OK) {
            s3e = sqlite3_exec(ts->s3h, "COMMIT TRANSACTION", NULL, NULL, NULL);
        }

        if (!ok || s3e != SQLITE_OK) {
            secerror("Failed to update trust store: (%d) %@", s3e, error ? *error : NULL);
            TrustdHealthAnalyticsLogErrorCodeForDatabase(TATrustStore, TAOperationWrite, TAFatalError, s3e);
            if (ok) {
                /* we have an error in s3e but haven't propagated it yet; do so now */
                ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e);
            }
            s3e = sqlite3_exec(ts->s3h, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
            if (s3e != SQLITE_OK) {
                secerror("Failed to rollback transaction (%d) %@", s3e, error ? *error : NULL);
            }
        }

    errOut:
        CFReleaseSafe(xmlData);
        CFReleaseSafe(array);
        CFReleaseNull(digest);
        CFReleaseNull(uuid);
    });
errOutNotLocked:
    return ok;
}

bool _SecTrustStoreRemoveCertificate(SecTrustStoreRef ts, SecCertificateRef cert, CFErrorRef *error) {
    bool ok = true;
    __block CFDataRef uuid = SecTrustStoreCopyUUID(ts);
    CFDataRef digest = NULL;
    require_action_quiet(ts, errOutNotLocked, ok = SecError(errSecParam, error, CFSTR("truststore is NULL")));
    require_action_quiet(!ts->readOnly, errOutNotLocked, ok = SecError(errSecReadOnly, error, CFSTR("truststore is readOnly")));
    require_action_quiet(uuid, errOutNotLocked, ok = SecError(errSecInternal, error, CFSTR("uuid is NULL")));
    require_action_quiet(digest = SecCertificateCopySHA256Digest(cert), errOutNotLocked, ok = SecError(errSecAllocate, error, CFSTR("failed to get cert sha256 digest")));
    require_action_quiet(CFDataGetLength(digest) > 0, errOutNotLocked, ok = SecError(errSecAllocate, error, CFSTR("cert digest of bad length")));
    dispatch_sync(ts->queue, ^{
        int s3e = SQLITE_OK;
        sqlite3_stmt *deleteStmt = NULL;

        require_noerr(s3e = sqlite3_prepare_v2(ts->s3h,
                                               deleteSQL,
                                               sizeof(deleteSQL),
                                               &deleteStmt, NULL), errOut);
        require_noerr(s3e = sqlite3_bind_blob_wrapper(deleteStmt, 1,
                                                CFDataGetBytePtr(digest), (size_t)CFDataGetLength(digest), SQLITE_STATIC), errOut);
        require_noerr(s3e = sqlite3_bind_blob_wrapper(deleteStmt, 2,
                                                CFDataGetBytePtr(uuid), (size_t)CFDataGetLength(uuid), SQLITE_STATIC), errOut);
        s3e = sqlite3_step(deleteStmt);

    errOut:
        if (deleteStmt) {
            verify_noerr(sqlite3_finalize(deleteStmt));
        }
        if (s3e != SQLITE_OK && s3e != SQLITE_DONE) {
            secerror("Removal of certificate from trust store failed: %d", s3e);
            TrustdHealthAnalyticsLogErrorCodeForDatabase(TATrustStore, TAOperationWrite, TAFatalError, s3e);
        }
    });
errOutNotLocked:
    CFReleaseNull(uuid);
    CFReleaseNull(digest);
	return ok;
}

bool _SecTrustStoreRemoveAll(SecTrustStoreRef ts, CFErrorRef *error)
{
    __block bool removed_all = false;
    __block CFDataRef uuid = SecTrustStoreCopyUUID(ts);
    require_action_quiet(ts, errOutNotLocked, removed_all = SecError(errSecParam, error, CFSTR("truststore is NULL")));
    require_action_quiet(!ts->readOnly, errOutNotLocked, removed_all = SecError(errSecReadOnly, error, CFSTR("truststore is readOnly")));
    require_action_quiet(uuid, errOutNotLocked, removed_all = SecError(errSecInternal, error, CFSTR("uuid is NULL")));
    dispatch_sync(ts->queue, ^{
        int s3e = SQLITE_OK;
        sqlite3_stmt *deleteAllStmt = NULL;
        require_noerr(s3e = sqlite3_prepare_v2(ts->s3h,
                                        deleteAllSQL,
                                        sizeof(deleteAllSQL),
                                        &deleteAllStmt, NULL), errOut);
        require_noerr(s3e = sqlite3_bind_blob_wrapper(deleteAllStmt, 1,
                                            CFDataGetBytePtr(uuid), (size_t)CFDataGetLength(uuid), SQLITE_STATIC), errOut);
        // begin exclusive transaction
        s3e =sqlite3_exec(ts->s3h, beginExclusiveTxnSQL, NULL, NULL, NULL);
        if (s3e == SQLITE_OK) {
            s3e = sqlite3_step(deleteAllStmt);
            (void)s3e;
        }
        s3e = sqlite3_exec(ts->s3h, endExclusiveTxnSQL, NULL, NULL, NULL);

    errOut:
        if (deleteAllStmt) {
            verify_noerr(sqlite3_finalize(deleteAllStmt));
        }
        if (s3e == SQLITE_OK) {
            removed_all = true;
            ts->containsSettings = false;
        } else {
            secerror("Clearing of trust store failed: %d", s3e);
            TrustdHealthAnalyticsLogErrorCodeForDatabase(TATrustStore, TAOperationWrite, TAFatalError, s3e);
        }

        /* prepared statements become unusable after deleteAllSQL, reset them */
        if (ts->copyParents) {
            sqlite3_finalize(ts->copyParents);
        }
        sqlite3_prepare_v3(ts->s3h,
                           copyParentsSQL,
                           sizeof(copyParentsSQL),
                           SQLITE_PREPARE_PERSISTENT, &ts->copyParents, NULL);

        if (ts->contains) {
            sqlite3_finalize(ts->contains);
        }
        sqlite3_prepare_v3(ts->s3h,
                           containsSQL,
                           sizeof(containsSQL),
                           SQLITE_PREPARE_PERSISTENT, &ts->contains, NULL);
    });
errOutNotLocked:
    CFReleaseNull(uuid);
	return removed_all;
}

CFArrayRef SecTrustStoreCopyParents(SecTrustStoreRef ts, SecCertificateRef certificate, CFErrorRef *error) {
    __block CFMutableArrayRef parents = NULL;
#if TARGET_OS_IPHONE
    __block CFDataRef uuid = SecTrustStoreCopyUUID(ts);
    CFDataRef issuer = NULL;
    require(issuer = SecCertificateGetNormalizedIssuerContent(certificate), errOutNotLocked);
    require(CFDataGetLength(issuer) > 0, errOutNotLocked);
    require(ts && ts->s3h, errOutNotLocked);
    /* Since only trustd uses the CopyParents interface and only for the CertificateSource, it should never call
     * this with the system domain. */
    require(ts->domain != kSecTrustStoreDomainSystem, errOutNotLocked);
    require(uuid, errOutNotLocked);
    dispatch_sync(ts->queue, ^{
        int s3e = SQLITE_OK;

        require_quiet(ts->containsSettings, ok);
        /* @@@ Might have to use SQLITE_TRANSIENT */
        require_noerr(s3e = sqlite3_bind_blob_wrapper(ts->copyParents, 1,
                                                      CFDataGetBytePtr(issuer), (size_t)CFDataGetLength(issuer),
                                                      SQLITE_STATIC), errOut);
        require_noerr(s3e = sqlite3_bind_blob_wrapper(ts->copyParents, 2,
                                                      CFDataGetBytePtr(uuid), (size_t)CFDataGetLength(uuid),
                                                      SQLITE_STATIC), errOut);
        require(parents = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                               &kCFTypeArrayCallBacks), errOut);
        for (;;) {
            s3e = sqlite3_step(ts->copyParents);
            if (s3e == SQLITE_ROW) {
                SecCertificateRef cert;
                require(cert = SecCertificateCreateWithBytes(kCFAllocatorDefault,
                                                             sqlite3_column_blob(ts->copyParents, 0),
                                                             sqlite3_column_bytes(ts->copyParents, 0)), errOut);
                CFArrayAppendValue(parents, cert);
                CFRelease(cert);
            } else {
                require(s3e == SQLITE_DONE || s3e == SQLITE_OK, errOut);
                break;
            }
        }

        goto ok;
    errOut:
        secerror("Failed to read parents from trust store: %d", s3e);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TATrustStore, TAOperationRead, TAFatalError, s3e);
        if (parents) {
            CFRelease(parents);
            parents = NULL;
        }
    ok:
        verify_noerr(sqlite3_reset(ts->copyParents));
        verify_noerr(sqlite3_clear_bindings(ts->copyParents));
    });
errOutNotLocked:
    CFReleaseNull(uuid);
    return parents;
#else // !TARGET_OS_IPHONE
    /* Since only trustd uses the CopyParents interface and only for the CertificateSource, it should never call
     * this with the system domain. */
    if (ts && ts->domain == kSecTrustStoreDomainSystem) {
        return NULL;
    }
    return parents;
#endif // !TARGET_OS_IPHONE
}

#if TARGET_OS_IPHONE
static bool SecTrustStoreQueryCertificate(SecTrustStoreRef ts, SecCertificateRef cert, bool *contains, CFArrayRef *usageConstraints, CFErrorRef *error) {
    if (contains) {
        *contains = false;
    }
    __block bool ok = true;
    __block CFDataRef uuid = SecTrustStoreCopyUUID(ts);
    __block CFDataRef digest = SecCertificateCopySHA256Digest(cert);
    require_action_quiet(digest, errOutNotLocked, ok = SecError(errSecAllocate, error, CFSTR("failed to get cert sha256 digest")));
    require_action_quiet(CFDataGetLength(digest) > 0, errOutNotLocked, ok = SecError(errSecAllocate, error, CFSTR("cert digest of bad length")));
    require_action_quiet(ts && ts->s3h, errOutNotLocked, ok = SecError(errSecParam, error, CFSTR("ts is NULL")));
    require_action_quiet(uuid, errOutNotLocked, ok = SecError(errSecInternal, error, CFSTR("failed to get uuid")));
    dispatch_sync(ts->queue, ^{
        CFDataRef xmlData = NULL;
        CFPropertyListRef trustSettings = NULL;
        int s3e = SQLITE_OK;
        require_action_quiet(ts->containsSettings, errOut, ok = true);
        require_noerr_action(s3e = sqlite3_bind_blob_wrapper(ts->contains, 1,
                                                             CFDataGetBytePtr(digest), (size_t)CFDataGetLength(digest), SQLITE_STATIC),
                             errOut, ok = SecDbErrorWithStmt(s3e, ts->contains, error, CFSTR("sqlite3_bind_blob failed")));
        require_noerr_action(s3e = sqlite3_bind_blob_wrapper(ts->contains, 2,
                                                             CFDataGetBytePtr(uuid), (size_t)CFDataGetLength(uuid), SQLITE_STATIC),
                             errOut, ok = SecDbErrorWithStmt(s3e, ts->contains, error, CFSTR("sqlite3_bind_blob failed")));
        s3e = sqlite3_step(ts->contains);
        if (s3e == SQLITE_ROW) {
            if (contains)
                *contains = true;
            if (usageConstraints) {
                require_action(xmlData = CFDataCreate(NULL,
                                                      sqlite3_column_blob(ts->contains, 0),
                                                      sqlite3_column_bytes(ts->contains, 0)), errOut, ok = false);
                require_action(trustSettings = CFPropertyListCreateWithData(NULL,
                                                                            xmlData,
                                                                            kCFPropertyListImmutable,
                                                                            NULL, error), errOut, ok = false);
                require_action(CFGetTypeID(trustSettings) == CFArrayGetTypeID(), errOut, ok = false);
                *usageConstraints = CFRetain(trustSettings);
            }
        } else {
            require_action(s3e == SQLITE_DONE || s3e == SQLITE_OK, errOut,
                           ok = SecDbErrorWithStmt(s3e, ts->contains, error, CFSTR("sqlite3_step failed")));
        }

    errOut:
        if (!ok) {
            secerror("Failed to query for cert in trust store: %d", s3e);
            TrustdHealthAnalyticsLogErrorCodeForDatabase(TATrustStore, TAOperationRead, TAFatalError, s3e);
        }
        verify_noerr(sqlite3_reset(ts->contains));
        verify_noerr(sqlite3_clear_bindings(ts->contains));
        CFReleaseNull(xmlData);
        CFReleaseNull(trustSettings);
    });
errOutNotLocked:
    CFReleaseNull(uuid);
    CFReleaseNull(digest);
    return ok;
}
#endif // TARGET_OS_IPHONE

bool _SecTrustStoreContainsCertificate(SecTrustStoreRef ts, SecCertificateRef cert, bool *contains, CFErrorRef *error) {
    if (contains) {
        *contains = false;
    }
    if (ts && ts->domain == kSecTrustStoreDomainSystem) {
        // For the system domain, use the system anchor source
        if (contains) {
            *contains = SecCertificateSourceContains(kSecSystemAnchorSource, cert);
        }
        return true;
    }
#if TARGET_OS_IPHONE
    return SecTrustStoreQueryCertificate(ts, cert, contains, NULL, error);
#else // !TARGET_OS_IPHONE
    return true;
#endif // !TARGET_OS_IPHONE
}

bool _SecTrustStoreCopyUsageConstraints(SecTrustStoreRef ts, SecCertificateRef cert, CFArrayRef *usageConstraints, CFErrorRef *error) {
    if (ts && ts->domain == kSecTrustStoreDomainSystem) {
        // For the system domain, use the system anchor source
        if (usageConstraints) {
            *usageConstraints = SecCertificateSourceCopyUsageConstraints(kSecSystemAnchorSource, cert);
        }
        return true;
    }
#if TARGET_OS_IPHONE
    return SecTrustStoreQueryCertificate(ts, cert, NULL, usageConstraints, error);
#else // !TARGET_OS_IPHONE
    return true;
#endif // !TARGET_OS_IPHONE
}

bool _SecTrustStoreCopyAll(SecTrustStoreRef ts, CFArrayRef *trustStoreContents, CFErrorRef *error) {
#if TARGET_OS_IPHONE
    __block bool ok = true;
    __block CFMutableArrayRef CertsAndSettings = NULL;
    __block CFDataRef uuid = SecTrustStoreCopyUUID(ts);
    require(CertsAndSettings = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks), errOutNotLocked);
    require_action_quiet(trustStoreContents, errOutNotLocked, ok = SecError(errSecParam, error, CFSTR("trustStoreContents is NULL")));
    require_action_quiet(ts, errOutNotLocked, ok = SecError(errSecParam, error, CFSTR("ts is NULL")));
#if NO_SYSTEM_STORE
    require_action_quiet(ts->domain != kSecTrustStoreDomainSystem, errOutNotLocked, ok = SecError(errSecUnimplemented, error, CFSTR("Cannot copy system trust store contents"))); // Not allowing system trust store enumeration
#else
    if (ts->domain == kSecTrustStoreDomainSystem) {
        CFArrayRef certs = SecSystemAnchorSourceCopyCertificates();
        if (certs && CFArrayGetCount(certs) > 0) {
            //%%% need to return as CertsAndSettings instead of just certs
            *trustStoreContents = CFRetainSafe(certs);
        }
        CFReleaseNull(certs);
        CFReleaseNull(CertsAndSettings);
        CFReleaseNull(uuid);
        return ok;
    }
#endif // NO_SYSTEM_STORE
    require_action_quiet(ts->s3h, errOutNotLocked, ok = SecError(errSecParam, error, CFSTR("ts DB is NULL")));
    require_action_quiet(uuid, errOutNotLocked, ok = SecError(errSecInternal, error, CFSTR("uuid is NULL")));
    dispatch_sync(ts->queue, ^{
        sqlite3_stmt *copyAllStmt = NULL;
        CFDataRef cert = NULL;
        CFDataRef xmlData = NULL;
        CFPropertyListRef trustSettings = NULL;
        CFArrayRef certSettingsPair = NULL;
        int s3e = SQLITE_OK;
        require_noerr(s3e = sqlite3_prepare_v2(ts->s3h,
                                            copyAllSQL,
                                            sizeof(copyAllSQL),
                                            &copyAllStmt, NULL), errOut);
        require_noerr_action(s3e = sqlite3_bind_blob_wrapper(copyAllStmt, 1,
                                                             CFDataGetBytePtr(uuid), (size_t)CFDataGetLength(uuid), SQLITE_STATIC),
                             errOut, ok = SecDbErrorWithStmt(s3e, copyAllStmt, error, CFSTR("sqlite3_bind_blob failed")));
        for(;;) {
            s3e = sqlite3_step(copyAllStmt);
            if (s3e == SQLITE_ROW) {
                require(cert = CFDataCreate(kCFAllocatorDefault,
                                            sqlite3_column_blob(copyAllStmt, 0),
                                            sqlite3_column_bytes(copyAllStmt, 0)), errOut);
                require(xmlData = CFDataCreate(NULL,
                                               sqlite3_column_blob(copyAllStmt, 1),
                                               sqlite3_column_bytes(copyAllStmt, 1)), errOut);
                require(trustSettings = CFPropertyListCreateWithData(NULL,
                                                                     xmlData,
                                                                     kCFPropertyListImmutable,
                                                                     NULL, error), errOut);
                const void *pair[] = { cert , trustSettings };
                require(certSettingsPair = CFArrayCreate(NULL, pair, 2, &kCFTypeArrayCallBacks), errOut);
                CFArrayAppendValue(CertsAndSettings, certSettingsPair);

                CFReleaseNull(cert);
                CFReleaseNull(xmlData);
                CFReleaseNull(trustSettings);
                CFReleaseNull(certSettingsPair);
            } else {
                require_action(s3e == SQLITE_DONE || s3e == SQLITE_OK, errOut, ok = SecDbErrorWithStmt(s3e, copyAllStmt, error, CFSTR("sqlite3_step failed")));
                break;
            }
        }
        goto ok;

    errOut:
        secerror("Failed to query for all certs in trust store: %d", s3e);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TATrustStore, TAOperationRead, TAFatalError, s3e);
        CFReleaseNull(cert);
        CFReleaseNull(xmlData);
        CFReleaseNull(trustSettings);
        CFReleaseNull(certSettingsPair);
    ok:
        if (copyAllStmt) {
            verify_noerr(sqlite3_finalize(copyAllStmt));
        }
    });
errOutNotLocked:
    if (CertsAndSettings) {
        if (CFArrayGetCount(CertsAndSettings) > 0) {
            *trustStoreContents = CFRetainSafe(CertsAndSettings);
        }
    }
    CFReleaseNull(CertsAndSettings);
    CFReleaseNull(uuid);
    return ok;
#else // !TARGET_OS_IPHONE
    if (ts && ts->domain == kSecTrustStoreDomainSystem) {
#if NO_SYSTEM_STORE
        return SecError(errSecUnimplemented, error, CFSTR("Cannot copy system trust store contents"));
#else
        CFArrayRef certs = SecSystemAnchorSourceCopyCertificates();
        if (certs && CFArrayGetCount(certs) > 0) {
            //%%% need to return as CertsAndSettings instead of just certs
            *trustStoreContents = CFRetainSafe(certs);
        }
        CFReleaseNull(certs);
        return true;
#endif // NO_SYSTEM_STORE
    }
    CFMutableArrayRef CertsAndSettings = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (CertsAndSettings) {
        *trustStoreContents = CertsAndSettings;
    }
    return true;
#endif // !TARGET_OS_IPHONE
}

#if !TARGET_OS_OSX
static bool _SecTrustStoreUpdateSchema(const char *path, CFErrorRef *error)
{
    bool ok = false;
    int s3e = SQLITE_OK;
    int64_t rows = -1;
    char *errmsg = NULL;
    sqlite3 *s3h = NULL;
    sqlite3_stmt *findColStmt = NULL;
    sqlite3_stmt *countAllStmt = NULL;
    sqlite3_stmt *updateUUIDStmt = NULL;
    CFDataRef uuid = NULL;

    // open database connection (caller has previously checked that path exists)
    require_noerr(s3e = sec_sqlite3_open(path, &s3h, true), errOut);
    // check whether the 'uuid' column exists
    require_noerr_action(s3e = sqlite3_prepare_v3(s3h, findUUIDColSQL, sizeof(findUUIDColSQL),
                         0, &findColStmt, NULL), errOut,
                         ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to prepare findColStmt")));
    s3e = sqlite3_step(findColStmt);
    if (s3e == SQLITE_ROW) {
        int64_t result = sqlite3_column_int64(findColStmt, 0);
        if (result > 0) {
            // found the 'uuid' column, so we don't need to update the schema
            ok = true;
            goto errOut;
        }
        secnotice("config", "trust store schema not current, will update");
    } else {
        require_action(s3e == SQLITE_DONE || s3e == SQLITE_OK, errOut, ok = SecDbErrorWithStmt(s3e, findColStmt, error, CFSTR("check for uuid column failed")));
    }
    if (findColStmt) {
        // must finalize select statements before we can drop the table this statement was bound to
        sqlite3_stmt *tmpFindColStmt = findColStmt;
        findColStmt = NULL;
        require_noerr_action(s3e = sqlite3_finalize(tmpFindColStmt), errOut,
                             ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to finalize findColStmt")));
    }
    // previous entries (prior to schema update) are placed in admin domain owned by _trustd
    require_action_quiet(uuid = SecCopyUUIDDataForUID(TRUSTD_ROLE_ACCOUNT), errOut,
                         ok = SecError(errSecParam, error, CFSTR("get uuid failed")));

    // ======================================================================================
    // note: to update the schema of the existing tsettings table, we follow the guidance in
    // https://www.sqlite.org/lang_altertable.html#otheralter, which boils down to:
    // 1. Create new table
    // 2. Copy data
    // 3. Drop old table
    // 4. Rename new into old
    // ======================================================================================

    // start transaction
    require_noerr(s3e = sqlite3_exec(s3h, beginExclusiveTxnSQL, NULL, NULL, NULL), errOut);
    // create new tmp_tsettings table with current schema
    s3e = sqlite3_exec(s3h,
            "CREATE TABLE tmp_tsettings("
            "sha256 BLOB NOT NULL DEFAULT '',"
            "subj BLOB NOT NULL DEFAULT '',"
            "tset BLOB,"
            "data BLOB,"
            "uuid BLOB NOT NULL DEFAULT '',"
            "UNIQUE(sha256,uuid)"
            ");"
            , NULL, NULL, &errmsg);
    if (errmsg) {
        secwarning("CREATE TABLE tmp_tsettings: %s", errmsg);
        sqlite3_free(errmsg);
    }
    require_noerr(s3e, errSql);
    // are there any existing rows in old table to copy?
    require_noerr_action(s3e = sqlite3_prepare_v3(s3h, countAllV1SQL, sizeof(countAllV1SQL),
                         0, &countAllStmt, NULL), errSql,
                         ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to prepare countAllStmt")));
    s3e = sqlite3_step(countAllStmt);
    if (s3e == SQLITE_ROW) {
        rows = sqlite3_column_int64(countAllStmt, 0);
    }
    if (countAllStmt) {
        // must finalize select statements before we can drop the table this statement was bound to
        sqlite3_stmt *tmpCountAllStmt = countAllStmt;
        countAllStmt = NULL;
        require_noerr_action(s3e = sqlite3_finalize(tmpCountAllStmt), errSql,
                             ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to finalize countAllStmt")));
    }
    if (rows > 0) {
        // copy data
        secnotice("config", "copying %lld rows from tsettings", (long long)rows);
        require_noerr_action(s3e = sqlite3_exec(s3h, copyToTmpSQL, NULL, NULL, NULL), errSql,
                             ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to copy table data")));
    } else {
        secnotice("config", "no existing tsettings (%lld rows)", (long long)rows);
    }
    // drop old table
    require_noerr_action(s3e = sqlite3_exec(s3h, "DROP TABLE tsettings;", NULL, NULL, NULL), errSql,
                         ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to drop old table")));
    // rename new into old
    require_noerr_action(s3e = sqlite3_exec(s3h, "ALTER TABLE tmp_tsettings RENAME TO tsettings;", NULL, NULL, NULL), errSql,
                         ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to rename new table")));
    if (rows > 0) {
        // fix up default (empty) uuid values
        require_noerr_action(s3e = sqlite3_prepare_v3(s3h, updateUUIDSQL, sizeof(updateUUIDSQL),
                                                      SQLITE_PREPARE_PERSISTENT, &updateUUIDStmt, NULL), errSql,
                             ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to prepare updateUUIDStmt")));
        require_noerr_action(s3e = sqlite3_bind_blob_wrapper(updateUUIDStmt, 1,
                                                             CFDataGetBytePtr(uuid), (size_t)CFDataGetLength(uuid), SQLITE_STATIC), errSql,
                             ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to bind uuid value")));
        s3e = sqlite3_step(updateUUIDStmt);
        if (!(s3e == SQLITE_OK || s3e == SQLITE_DONE)) {
            // note SQLITE_DONE means the statement ran to completion, i.e. a successful result
            require_noerr_action(s3e, errSql,
            ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to update uuid column")));
        }
    }
    // drop and recreate index
    require_noerr_action(s3e = sqlite3_exec(s3h, "DROP INDEX IF EXISTS isubj;", NULL, NULL, NULL),
                         errSql, ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to drop old index")));
    require_noerr_action(s3e = sqlite3_exec(s3h, "CREATE INDEX isubj ON tsettings(subj);", NULL, NULL, NULL),
                         errSql, ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to recreate index")));
    // success
    ok = true;
    s3e = sqlite3_exec(s3h, "COMMIT TRANSACTION", NULL, NULL, NULL);
    if (s3e != SQLITE_OK) {
        secerror("Failed to commit transaction (%d), will attempt rollback", s3e);
    } else {
        secnotice("config", "successfully updated trust store schema");
    }
errSql:
    if (!ok || s3e != SQLITE_OK) {
        secerror("Failed to update trust store: (%d) %@", s3e, error ? *error : NULL);
        TrustdHealthAnalyticsLogErrorCodeForDatabase(TATrustStore, TAOperationWrite, TAFatalError, s3e);
        if (ok) {
            // we have an error in s3e but haven't propagated it yet; do so now
            ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e);
        }
        s3e = sqlite3_exec(s3h, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        if (s3e != SQLITE_OK) {
            secerror("Failed to rollback transaction (%d) %@", s3e, error ? *error : NULL);
        }
    }
errOut:
    if (updateUUIDStmt) {
        require_noerr_action(s3e = sqlite3_finalize(updateUUIDStmt), errExit,
                             ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to finalize updateUUIDStmt")));
    }
    if (countAllStmt) {
        require_noerr_action(s3e = sqlite3_finalize(countAllStmt), errExit,
                             ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to finalize countAllStmt")));
    }
    if (findColStmt) {
        require_noerr_action(s3e = sqlite3_finalize(findColStmt), errExit,
                             ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to finalize findColStmt")));
    }
errExit:
    if (!ok) {
        secerror("Failed to update schema (uuid %@)", uuid);
        // We won't be able to add entries to the table. Remove it so it can be recreated from scratch.
        require_noerr_action(s3e = sqlite3_exec(s3h, "DROP TABLE tsettings;", NULL, NULL, NULL), errSql,
                             ok = SecDbErrorWithDb(s3e, s3h, error, CFSTR("failed to drop tsettings table")));
    }
    if (s3h) {
        require_noerr_action(s3e = sqlite3_close(s3h), errExit,
                             ok = SecDbError(s3e, error, CFSTR("failed to close trust store after schema update")));
    }
    CFReleaseNull(uuid);

    return ok;
}
#endif // !TARGET_OS_OSX

bool _SecTrustStoreMigrateUserStore(CFErrorRef *error)
{
#if TARGET_OS_OSX
    // There was no old trust store database location on macOS.
    // Migration from plist is handled in _SecTrustStoreMigrateTrustSettingsPropertyList.
    return true;
#else
    /* If new trust store file already exists, we don't need to migrate, but we may need a schema update. */
    const char path[MAXPATHLEN];
    bool pathResult = false;
    pathResult = SecExtractFilesystemPathForPrivateTrustdFile(kTrustStoreFileName, (UInt8*) path, (CFIndex) sizeof(path));
    if (pathResult) {
        FILE* file = fopen(path, "r");
        if (file != NULL) {
            secdebug("config", "already migrated user trust store");
            fclose(file);
            return _SecTrustStoreUpdateSchema(path, error);
        }
    }

    secnotice("config", "migrating trust store");
    bool ok = false;
    sqlite3 *old_db = NULL;
    int s3e = SQLITE_OK;
    SecCertificateRef cert = NULL;
    CFDataRef xmlData = NULL;
    CFArrayRef tsArray = NULL;
    sqlite3_stmt *copyAllStmt = NULL;

    /* Open old Trust Store */
    CFURLRef oldURL = SecCopyURLForFileInKeychainDirectory(kTrustStoreFileName);
    require_action(oldURL, errOut, ok = SecError(errSecIO, error, CFSTR("failed to get old DB file URL")));
    require_action(CFURLGetFileSystemRepresentation(oldURL, false, (UInt8*) path, (CFIndex) sizeof(path)), errOut,
                   ok= SecError(errSecIO, error, CFSTR("failed to get old DB file path")));
    require_noerr_action(s3e = sqlite3_open_v2(path, &old_db, SQLITE_OPEN_READONLY, NULL), errOut,
                         ok = SecDbError(s3e, error, CFSTR("failed to open old trust store database; new trust store will be empty")));
    require_noerr_action(s3e = sqlite3_prepare_v2(old_db, copyAllOldSQL, sizeof(copyAllOldSQL), &copyAllStmt, NULL), errOut,
                         ok = SecDbErrorWithDb(s3e, old_db, error, CFSTR("failed to prepare old trust store read")));

    /* Open new Trust Store */
    SecTrustStoreRef new_db = SecTrustStoreForDomainName(CFSTR("user"), error);
    require_action(new_db, errOut, ok = SecError(errSecAllocate, error, CFSTR("failed to open new trust store")));

    /* Read each row of the old trust store and set it in the new trust store */
    for(;;) {
        s3e = sqlite3_step(copyAllStmt);
        if (s3e == SQLITE_ROW) {
            require_action(cert = SecCertificateCreateWithBytes(NULL,
                                                         sqlite3_column_blob(copyAllStmt, 0),
                                                         sqlite3_column_bytes(copyAllStmt, 0)), errOut,
                           ok = SecError(errSecDecode, error, CFSTR("failed to decode cert in old DB")));
            require_action(xmlData = CFDataCreate(NULL,
                                           sqlite3_column_blob(copyAllStmt, 1),
                                           sqlite3_column_bytes(copyAllStmt, 1)), errOut,
                           ok = SecError(errSecParam, error, CFSTR("no tset data in old DB")));
            require(tsArray = CFPropertyListCreateWithData(NULL,
                                                           xmlData,
                                                           kCFPropertyListImmutable,
                                                           NULL, error), errOut);
            require_action(isArray(tsArray), errOut,
                           ok = SecError(errSecDecode, error, CFSTR("tset is not an array in old DB")));
            OSStatus status = errSecSuccess;
            require(status = _SecTrustStoreSetTrustSettings(new_db, cert, tsArray, error), errOut);

            CFReleaseNull(cert);
            CFReleaseNull(xmlData);
            CFReleaseNull(tsArray);
        } else {
            require_action(s3e == SQLITE_DONE || s3e == SQLITE_OK, errOut, ok = SecDbErrorWithStmt(s3e, copyAllStmt, error, CFSTR("sqlite3_step failed")));
            break;
        }
    }
    require_noerr_action(s3e = sqlite3_finalize(copyAllStmt), errOut,
                         ok = SecDbErrorWithDb(s3e, old_db, error, CFSTR("failed to finalize old trust store read")));
    copyAllStmt = NULL;
    require_noerr_action(s3e = sqlite3_close(old_db), errOut,
                         ok = SecDbError(s3e, error, CFSTR("failed to close old trust store")));
    old_db = NULL;
    ok = true;
    secdebug("config", "successfully migrated existing trust store");

    /* Delete the old trust store database */
    WithPathInKeychainDirectory(kTrustStoreFileName, ^(const char *utf8String) {
        remove(utf8String);
    });
errOut:
    if (copyAllStmt) {
        require_noerr_action(s3e = sqlite3_finalize(copyAllStmt), errOut,
                             ok = SecDbErrorWithDb(s3e, old_db, error, CFSTR("failed to finalize old trust store read")));
    }
    if (old_db) {
        require_noerr_action(s3e = sqlite3_close(old_db), errOut,
                             ok = SecDbError(s3e, error, CFSTR("failed to close old trust store")));
    }
    CFReleaseNull(cert);
    CFReleaseNull(xmlData);
    CFReleaseNull(tsArray);
    CFReleaseNull(oldURL);
    return ok;
#endif // !TARGET_OS_OSX
}

bool _SecTrustStoreMigrateTrustSettingsPropertyList(CFErrorRef *error)
{
#if !TARGET_OS_OSX
    // no trust settings property lists on iOS.
    return true;
#else
    // The user trustd instances have read access to user keychains and user plists,
    // and will need to look up each cert based on its hash in the trust settings plist.
    // The system trustd instance is responsible for migrating the Admin plist settings.
    // Presence of the new trust store file is not sufficient to indicate migration has
    // occurred, since user trustd instances may come up after it has been created; a new
    // table of migrated UUIDs can be added to the database for this purpose.

    bool ok = false;
    CFPropertyListRef plist = NULL;
    CFMutableDictionaryRef certificates = NULL;
    //
    // rdar://106132782
    //     - read file into data, then into plist
    //     - for each entry in plist, get key (sha1 hash) and call SecItemCopyMatching
    //       to look up the cert which matches that digest
    //     - copy certificate data
    //     - add certificate data to the certificates dictionary, keyed by hash
    //

    if (errSecSuccess == SecTrustSettingsXPCMigrate(plist, certificates)) {
        ok = true;
    }
    CFReleaseNull(certificates);
    CFReleaseNull(plist);
    return ok;
#endif
}


