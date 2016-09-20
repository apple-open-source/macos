/*
 * Copyright (c) 2007-2010,2012-2015 Apple Inc. All Rights Reserved.
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

#include <Security/SecCertificateInternal.h>
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
#include "SecBasePriv.h"
#include <Security/SecInternal.h>
#include <ipc/securityd_client.h>
#include <securityd/SecTrustStoreServer.h>
#include "utilities/sqlutils.h"
#include "utilities/SecDb.h"
#include <utilities/SecCFError.h>
#include "utilities/SecFileLocations.h"
#include <utilities/SecDispatchRelease.h>

/* uid of the _securityd user. */
#define SECURTYD_UID 64

static dispatch_once_t kSecTrustStoreUserOnce;
static SecTrustStoreRef kSecTrustStoreUser = NULL;

static const char copyParentsSQL[] = "SELECT data FROM tsettings WHERE subj=?";
static const char containsSQL[] = "SELECT tset FROM tsettings WHERE sha1=?";
static const char insertSQL[] = "INSERT INTO tsettings(sha1,subj,tset,data)VALUES(?,?,?,?)";
static const char updateSQL[] = "UPDATE tsettings SET tset=? WHERE sha1=?";
static const char deleteSQL[] = "DELETE FROM tsettings WHERE sha1=?";
static const char deleteAllSQL[] = "BEGIN EXCLUSIVE TRANSACTION; DELETE from tsettings; COMMIT TRANSACTION; VACUUM;";
static const char copyAllSQL[] = "SELECT data,tset FROM tsettings ORDER BY sha1";
static const char countAllSQL[] = "SELECT COUNT(*) FROM tsettings";

#define kSecTrustStoreName CFSTR("TrustStore")
#define kSecTrustStoreDbExtension CFSTR("sqlite3")

#define kTrustStoreFileName CFSTR("TrustStore.sqlite3")


struct __SecTrustStore {
    dispatch_queue_t queue;
	sqlite3 *s3h;
	sqlite3_stmt *copyParents;
	sqlite3_stmt *contains;
	bool readOnly;
	bool containsSettings;  // For optimization of high-use calls.
};

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
	s3e = sqlite3_open(db_name, s3h);
	if (s3e == SQLITE_CANTOPEN && create_path) {
		/* Make sure the path to db_name exists and is writable, then
		   try again. */
		s3e = sec_create_path(db_name);
		if (!s3e)
			s3e = sqlite3_open(db_name, s3h);
	}

	return s3e;
}

static int64_t SecTrustStoreCountAll(SecTrustStoreRef ts) {
    __block int64_t result = -1;
    require_quiet(ts, errOutNotLocked);
    dispatch_sync(ts->queue, ^{
        sqlite3_stmt *countAllStmt = NULL;
        int s3e = sqlite3_prepare(ts->s3h, countAllSQL, sizeof(countAllSQL),
                                      &countAllStmt, NULL);
        if (s3e == SQLITE_OK) {
            s3e = sqlite3_step(countAllStmt);
            if (s3e == SQLITE_ROW) {
                result = sqlite3_column_int64(countAllStmt, 0);
            }
        }

        if (countAllStmt) {
            verify_noerr(sqlite3_finalize(countAllStmt));
        }
    });

errOutNotLocked:
    return result;
}

static SecTrustStoreRef SecTrustStoreCreate(const char *db_name,
	bool create) {
	SecTrustStoreRef ts;
	int s3e;

	require(ts = (SecTrustStoreRef)malloc(sizeof(struct __SecTrustStore)), errOut);
    ts->queue = dispatch_queue_create("truststore", DISPATCH_QUEUE_SERIAL);
	require_noerr(s3e = sec_sqlite3_open(db_name, &ts->s3h, create), errOut);

	s3e = sqlite3_prepare(ts->s3h, copyParentsSQL, sizeof(copyParentsSQL),
		&ts->copyParents, NULL);
	if (create && s3e == SQLITE_ERROR) {
		/* sqlite3_prepare returns SQLITE_ERROR if the table we are
		   compiling this statement for doesn't exist. */
		char *errmsg = NULL;
		s3e = sqlite3_exec(ts->s3h,
			"CREATE TABLE tsettings("
			"sha1 BLOB NOT NULL DEFAULT '',"
			"subj BLOB NOT NULL DEFAULT '',"
			"tset BLOB,"
			"data BLOB,"
			"PRIMARY KEY(sha1)"
			");"
			"CREATE INDEX isubj ON tsettings(subj);"
			, NULL, NULL, &errmsg);
		if (errmsg) {
			secwarning("CREATE TABLE cert: %s", errmsg);
			sqlite3_free(errmsg);
		}
		require_noerr(s3e, errOut);
		s3e = sqlite3_prepare(ts->s3h, copyParentsSQL, sizeof(copyParentsSQL),
			&ts->copyParents, NULL);
	}
	require_noerr(s3e, errOut);
	require_noerr(s3e = sqlite3_prepare(ts->s3h, containsSQL, sizeof(containsSQL),
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
		sqlite3_close(ts->s3h);
        dispatch_release_safe(ts->queue);
		free(ts);
	}

	return NULL;
}

static bool SecExtractFilesystemPathForKeychainFile(CFStringRef file, UInt8 *buffer, CFIndex maxBufLen)
{
    bool translated = false;
    CFURLRef fileURL = SecCopyURLForFileInKeychainDirectory(file);
    
    if (fileURL && CFURLGetFileSystemRepresentation(fileURL, false, buffer, maxBufLen))
        translated = true;
    CFReleaseSafe(fileURL);
    
    return translated;
}

static void SecTrustStoreInitUser(void) {
	const char path[MAXPATHLEN];
    
    if (SecExtractFilesystemPathForKeychainFile(kTrustStoreFileName, (UInt8*) path, (CFIndex) sizeof(path)))
    {
        kSecTrustStoreUser = SecTrustStoreCreate(path, true);
        if (kSecTrustStoreUser)
            kSecTrustStoreUser->readOnly = false;
    }    
}

/* AUDIT[securityd](done):
   domainName (ok) is a caller provided string of any length (might be 0), only
       its cf type has been checked.
 */
SecTrustStoreRef SecTrustStoreForDomainName(CFStringRef domainName, CFErrorRef *error) {
	if (CFEqual(CFSTR("user"), domainName)) {
		dispatch_once(&kSecTrustStoreUserOnce, ^{ SecTrustStoreInitUser(); });
		return kSecTrustStoreUser;
	} else {
        SecError(errSecParam, error, CFSTR("unknown domain: %@"), domainName);
		return NULL;
	}
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
	require_action_quiet(ts, errOutNotLocked, ok = SecError(errSecParam, error, CFSTR("truststore is NULL")));
    require_action_quiet(!ts->readOnly, errOutNotLocked, ok = SecError(errSecReadOnly, error, CFSTR("truststore is readOnly")));
    dispatch_sync(ts->queue, ^{
        CFTypeRef trustSettingsDictOrArray = tsdoa;
        sqlite3_stmt *insert = NULL, *update = NULL;
        CFDataRef xmlData = NULL;
        CFArrayRef array = NULL;

        CFDataRef subject;
        require_action_quiet(subject = SecCertificateGetNormalizedSubjectContent(certificate),
                             errOut, ok = SecError(errSecParam, error, CFSTR("get normalized subject failed")));
        CFDataRef digest;
        require_action_quiet(digest = SecCertificateGetSHA1Digest(certificate), errOut, ok = SecError(errSecParam, error, CFSTR("get sha1 digest failed")));

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

        int s3e = sqlite3_exec(ts->s3h, "BEGIN EXCLUSIVE TRANSACTION", NULL, NULL, NULL);
        require_action_quiet(s3e == SQLITE_OK, errOut, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));

        /* Parameter order is sha1,subj,tset,data. */
        require_noerr_action_quiet(sqlite3_prepare(ts->s3h, insertSQL, sizeof(insertSQL),
                                                   &insert, NULL), errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
        require_noerr_action_quiet(sqlite3_bind_blob_wrapper(insert, 1,
                                                             CFDataGetBytePtr(digest), CFDataGetLength(digest), SQLITE_STATIC),
                                   errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
        require_noerr_action_quiet(sqlite3_bind_blob_wrapper(insert, 2,
                                                             CFDataGetBytePtr(subject), CFDataGetLength(subject),
                                                             SQLITE_STATIC), errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
        require_noerr_action_quiet(sqlite3_bind_blob_wrapper(insert, 3,
                                                             CFDataGetBytePtr(xmlData), CFDataGetLength(xmlData),
                                                             SQLITE_STATIC), errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
        require_noerr_action_quiet(sqlite3_bind_blob_wrapper(insert, 4,
                                                             SecCertificateGetBytePtr(certificate),
                                                             SecCertificateGetLength(certificate), SQLITE_STATIC), errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
        s3e = sqlite3_step(insert);
        if (s3e == SQLITE_DONE) {
            /* Great the insert worked. */
            ok = true;
            ts->containsSettings = true;
        } else if (s3e == SQLITE_ERROR) {
            /* Try update. */
            require_noerr_action_quiet(s3e = sqlite3_prepare(ts->s3h, updateSQL, sizeof(updateSQL),
                                                             &update, NULL), errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
            require_noerr_action_quiet(s3e = sqlite3_bind_blob_wrapper(update, 1,
                                                                       CFDataGetBytePtr(xmlData), CFDataGetLength(xmlData),
                                                                       SQLITE_STATIC), errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
            require_noerr_action_quiet(s3e = sqlite3_bind_blob_wrapper(update, 2,
                                                                       CFDataGetBytePtr(digest), CFDataGetLength(digest), SQLITE_STATIC),
                                       errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
            s3e = sqlite3_step(update);
            require_action_quiet(s3e == SQLITE_DONE, errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
            s3e = SQLITE_OK;
            ok = true;
        } else {
            require_noerr_action_quiet(s3e, errOutSql, ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e));
            ok = true;
        }

    errOutSql:
        if (insert)
            s3e = sqlite3_finalize(insert);
        if (update)
            s3e = sqlite3_finalize(update);

        if (ok && s3e == SQLITE_OK)
            s3e = sqlite3_exec(ts->s3h, "COMMIT TRANSACTION", NULL, NULL, NULL);

        if (!ok || s3e != SQLITE_OK) {
            sqlite3_exec(ts->s3h, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
            if (ok) {
                ok = SecError(errSecInternal, error, CFSTR("sqlite3 error: %d"), s3e);
            }
        }

    errOut:
        CFReleaseSafe(xmlData);
        CFReleaseSafe(array);
    });
errOutNotLocked:
	return ok;
}

/* AUDIT[securityd](done):
   ts (ok) might be NULL.
   digest (ok) is a data of any length (might be 0).
 */
bool SecTrustStoreRemoveCertificateWithDigest(SecTrustStoreRef ts,
    CFDataRef digest, CFErrorRef *error) {
	require_quiet(ts, errOutNotLocked);
	require(!ts->readOnly, errOutNotLocked);
    dispatch_sync(ts->queue, ^{
        sqlite3_stmt *deleteStmt = NULL;
        require_noerr(sqlite3_prepare(ts->s3h, deleteSQL, sizeof(deleteSQL),
                                      &deleteStmt, NULL), errOut);
        require_noerr(sqlite3_bind_blob_wrapper(deleteStmt, 1,
                                                CFDataGetBytePtr(digest), CFDataGetLength(digest), SQLITE_STATIC),
                      errOut);
        sqlite3_step(deleteStmt);

    errOut:
        if (deleteStmt) {
            verify_noerr(sqlite3_finalize(deleteStmt));
        }
    });
errOutNotLocked:
	return true;
}

bool _SecTrustStoreRemoveAll(SecTrustStoreRef ts, CFErrorRef *error)
{
    __block bool removed_all = false;
	require(ts, errOutNotLocked);
	require(!ts->readOnly, errOutNotLocked);
    dispatch_sync(ts->queue, ^{
        if (SQLITE_OK == sqlite3_exec(ts->s3h, deleteAllSQL, NULL, NULL, NULL)) {
            removed_all = true;
            ts->containsSettings = false;
        }

        /* prepared statements become unusable after deleteAllSQL, reset them */
        if (ts->copyParents)
            sqlite3_finalize(ts->copyParents);
        sqlite3_prepare(ts->s3h, copyParentsSQL, sizeof(copyParentsSQL),
                        &ts->copyParents, NULL);
        if (ts->contains)
            sqlite3_finalize(ts->contains);
        sqlite3_prepare(ts->s3h, containsSQL, sizeof(containsSQL),
                        &ts->contains, NULL);
    });
errOutNotLocked:
	return removed_all;
}

CFArrayRef SecTrustStoreCopyParents(SecTrustStoreRef ts,
	SecCertificateRef certificate, CFErrorRef *error) {
	__block CFMutableArrayRef parents = NULL;
	require(ts, errOutNotLocked);
    dispatch_sync(ts->queue, ^{
        require_quiet(ts->containsSettings, errOut);
        CFDataRef issuer;
        require(issuer = SecCertificateGetNormalizedIssuerContent(certificate),
            errOut);
        /* @@@ Might have to use SQLITE_TRANSIENT */
        require_noerr(sqlite3_bind_blob_wrapper(ts->copyParents, 1,
            CFDataGetBytePtr(issuer), CFDataGetLength(issuer),
            SQLITE_STATIC), errOut);

        require(parents = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks), errOut);
        for (;;) {
            int s3e = sqlite3_step(ts->copyParents);
            if (s3e == SQLITE_ROW) {
                SecCertificateRef cert;
                require(cert = SecCertificateCreateWithBytes(kCFAllocatorDefault,
                    sqlite3_column_blob(ts->copyParents, 0),
                    sqlite3_column_bytes(ts->copyParents, 0)), errOut);
                CFArrayAppendValue(parents, cert);
                CFRelease(cert);
            } else {
                require(s3e == SQLITE_DONE, errOut);
                break;
            }
        }

        goto ok;
    errOut:
        if (parents) {
            CFRelease(parents);
            parents = NULL;
        }
    ok:
        verify_noerr(sqlite3_reset(ts->copyParents));
        verify_noerr(sqlite3_clear_bindings(ts->copyParents));
    });
errOutNotLocked:
	return parents;
}

static bool SecTrustStoreQueryCertificateWithDigest(SecTrustStoreRef ts,
	CFDataRef digest, bool *contains, CFArrayRef *usageConstraints, CFErrorRef *error) {
    if (contains)
        *contains = false;
    __block bool ok = true;
	require_action_quiet(ts, errOutNotLocked, ok = SecError(errSecParam, error, CFSTR("ts is NULL")));
    dispatch_sync(ts->queue, ^{
        CFDataRef xmlData = NULL;
        CFPropertyListRef trustSettings = NULL;
        require_action_quiet(ts->containsSettings, errOut, ok = true);
        int s3e;
        require_noerr_action(s3e = sqlite3_bind_blob_wrapper(ts->contains, 1,
            CFDataGetBytePtr(digest), CFDataGetLength(digest), SQLITE_STATIC),
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
            require_action(s3e == SQLITE_DONE, errOut, ok = SecDbErrorWithStmt(s3e, ts->contains, error, CFSTR("sqlite3_step failed")));
        }

    errOut:
        verify_noerr(sqlite3_reset(ts->contains));
        verify_noerr(sqlite3_clear_bindings(ts->contains));
        CFReleaseNull(xmlData);
        CFReleaseNull(trustSettings);
    });
errOutNotLocked:
	return ok;
}

bool SecTrustStoreContainsCertificateWithDigest(SecTrustStoreRef ts,
    CFDataRef digest, bool *contains, CFErrorRef *error) {
    return SecTrustStoreQueryCertificateWithDigest(ts, digest, contains, NULL, error);
}

bool _SecTrustStoreCopyUsageConstraints(SecTrustStoreRef ts,
    CFDataRef digest, CFArrayRef *usageConstraints, CFErrorRef *error) {
    return SecTrustStoreQueryCertificateWithDigest(ts, digest, NULL, usageConstraints, error);
}

bool _SecTrustStoreCopyAll(SecTrustStoreRef ts, CFArrayRef *trustStoreContents, CFErrorRef *error) {
    __block bool ok = true;
    __block CFMutableArrayRef CertsAndSettings = NULL;
    require_action_quiet(ts, errOutNotLocked, ok = SecError(errSecParam, error, CFSTR("ts is NULL")));
    require_action_quiet(trustStoreContents, errOutNotLocked, ok = SecError(errSecParam, error, CFSTR("trustStoreContents is NULL")));
    dispatch_sync(ts->queue, ^{
        sqlite3_stmt *copyAllStmt = NULL;
        CFDataRef cert = NULL;
        CFDataRef xmlData = NULL;
        CFPropertyListRef trustSettings = NULL;
        CFArrayRef certSettingsPair = NULL;
        require_noerr(sqlite3_prepare(ts->s3h, copyAllSQL, sizeof(copyAllSQL),
                                      &copyAllStmt, NULL), errOut);
        require(CertsAndSettings = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks), errOut);

        for(;;) {
            int s3e = sqlite3_step(copyAllStmt);
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
                require_action(s3e == SQLITE_DONE, errOut, ok = SecDbErrorWithStmt(s3e, copyAllStmt, error, CFSTR("sqlite3_step failed")));
                break;
            }
        }
        goto ok;

    errOut:
        CFReleaseNull(cert);
        CFReleaseNull(xmlData);
        CFReleaseNull(trustSettings);
        CFReleaseNull(certSettingsPair);
    ok:
        if (copyAllStmt) {
            verify_noerr(sqlite3_finalize(copyAllStmt));
        }
        if (CertsAndSettings) {
            *trustStoreContents = CertsAndSettings;
        }
    });
errOutNotLocked:
    return ok;
}
