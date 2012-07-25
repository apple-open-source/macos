/*
 * Copyright (c) 2007-2010 Apple Inc. All Rights Reserved.
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
#include <pthread.h>
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
#include <security_utilities/debugging.h>
#include "SecBasePriv.h"
#include <Security/SecInternal.h>
#include "securityd_client.h"
#include "securityd_server.h"
#include "sqlutils.h"

/* uid of the _securityd user. */
#define SECURTYD_UID 64

#if 0
#include <CoreFoundation/CFPriv.h>
#else
CF_EXPORT
CFURLRef CFCopyHomeDirectoryURLForUser(CFStringRef uName);	/* Pass NULL for the current user's home directory */
#endif

static pthread_once_t kSecTrustStoreUserOnce = PTHREAD_ONCE_INIT;
static SecTrustStoreRef kSecTrustStoreUser = NULL;

static const char copyParentsSQL[] = "SELECT data FROM tsettings WHERE subj=?";
static const char containsSQL[] = "SELECT tset FROM tsettings WHERE sha1=?";
static const char insertSQL[] = "INSERT INTO tsettings(sha1,subj,tset,data)VALUES(?,?,?,?)";
static const char updateSQL[] = "UPDATE tsettings SET tset=? WHERE sha1=?";
static const char deleteSQL[] = "DELETE FROM tsettings WHERE sha1=?";
static const char deleteAllSQL[] = "BEGIN EXCLUSIVE TRANSACTION; DELETE from tsettings; COMMIT TRANSACTION; VACUUM;";

#define kSecTrustStoreName CFSTR("TrustStore")
#define kSecTrustStoreDbExtension CFSTR("sqlite3")

#define kSecTrustStoreUserPath "/Library/Keychains/TrustStore.sqlite3"


struct __SecTrustStore {
	pthread_mutex_t lock;
	sqlite3 *s3h;
	sqlite3_stmt *copyParents;
	sqlite3_stmt *contains;
	bool readOnly;
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

static SecTrustStoreRef SecTrustStoreCreate(const char *db_name,
	bool create) {
	SecTrustStoreRef ts;
	int s3e;

	require(ts = (SecTrustStoreRef)malloc(sizeof(struct __SecTrustStore)), errOut);
	pthread_mutex_init(&ts->lock, NULL);
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

	return ts;

errOut:
	if (ts) {
		sqlite3_close(ts->s3h);
		pthread_mutex_destroy(&ts->lock);
		free(ts);
	}

	return NULL;
}

static void SecTrustStoreInitUser(void) {
	static const char * path = kSecTrustStoreUserPath;
#if NO_SERVER
    /* Added this block of code back to keep the tests happy for now. */
	const char *home = getenv("HOME");
	char buffer[PATH_MAX];
	size_t homeLen;
	size_t pathLen = strlen(path);
	if (home) {
		homeLen = strlen(home);
		if (homeLen + pathLen >= sizeof(buffer)) {
			return;
		}

		strlcpy(buffer, home, sizeof(buffer));
	} else {
		CFURLRef homeURL = CFCopyHomeDirectoryURLForUser(NULL);
		if (!homeURL)
			return;

		CFURLGetFileSystemRepresentation(homeURL, true, (uint8_t *)buffer,
			sizeof(buffer));
		CFRelease(homeURL);
		homeLen = strlen(buffer);
		buffer[homeLen] = '\0';
		if (homeLen + pathLen >= sizeof(buffer)) {
			return;
		}
	}

	strlcat(buffer, path, sizeof(buffer));

    path = buffer;

#endif

	kSecTrustStoreUser = SecTrustStoreCreate(path, true);
    if (kSecTrustStoreUser)
		kSecTrustStoreUser->readOnly = false;
}

/* AUDIT[securityd](done):
   domainName (ok) is a caller provided string of any length (might be 0), only
       its cf type has been checked.
 */
SecTrustStoreRef SecTrustStoreForDomainName(CFStringRef domainName) {
	if (CFEqual(CFSTR("user"), domainName)) {
		pthread_once(&kSecTrustStoreUserOnce, SecTrustStoreInitUser);
		return kSecTrustStoreUser;
	} else {
		return NULL;
	}
}

/* AUDIT[securityd](done):
   ts (ok) might be NULL.
   certificate (ok) is a valid SecCertificateRef.
   trustSettingsDictOrArray (checked by CFPropertyListCreateXMLData) is either
   NULL, a dictionary or an array, but its contents have not been checked.
 */
OSStatus _SecTrustStoreSetTrustSettings(SecTrustStoreRef ts,
	SecCertificateRef certificate,
    CFTypeRef trustSettingsDictOrArray) {
	OSStatus status = errSecParam;

	require_quiet(ts, errOutNotLocked);
	if (ts->readOnly) {
		status = errSecReadOnly;
        goto errOutNotLocked;
    }
	require_noerr(pthread_mutex_lock(&ts->lock), errOutNotLocked);
	sqlite3_stmt *insert = NULL, *update = NULL;
    CFDataRef xmlData = NULL;
    CFArrayRef array = NULL;

	CFDataRef subject;
	require(subject = SecCertificateGetNormalizedSubjectContent(certificate),
		errOut);
    CFDataRef digest;
	require(digest = SecCertificateGetSHA1Digest(certificate), errOut);

	/* Do some basic checks on the trust settings passed in. */
	if(trustSettingsDictOrArray == NULL) {
		require(array = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks), errOut);
		trustSettingsDictOrArray = array;
	}
	else if(CFGetTypeID(trustSettingsDictOrArray) == CFDictionaryGetTypeID()) {
		/* array-ize it */
		array = CFArrayCreate(NULL, &trustSettingsDictOrArray, 1,
			&kCFTypeArrayCallBacks);
		trustSettingsDictOrArray = array;
	}
	else {
		require(CFGetTypeID(trustSettingsDictOrArray) == CFArrayGetTypeID(), errOut);
	}

	require(xmlData = CFPropertyListCreateXMLData(kCFAllocatorDefault,
		trustSettingsDictOrArray), errOut);

	int s3e = sqlite3_exec(ts->s3h, "BEGIN EXCLUSIVE TRANSACTION", NULL, NULL, NULL);
	if (s3e != SQLITE_OK) {
        status = errSecInternal;
        goto errOut;
    }

	/* Parameter order is sha1,subj,tset,data. */
	require_noerr(sqlite3_prepare(ts->s3h, insertSQL, sizeof(insertSQL),
		&insert, NULL), errOutSql);
	require_noerr(sqlite3_bind_blob_wrapper(insert, 1,
		CFDataGetBytePtr(digest), CFDataGetLength(digest), SQLITE_STATIC),
		errOutSql);
	require_noerr(sqlite3_bind_blob_wrapper(insert, 2,
		CFDataGetBytePtr(subject), CFDataGetLength(subject),
		SQLITE_STATIC), errOutSql);
	require_noerr(sqlite3_bind_blob_wrapper(insert, 3,
		CFDataGetBytePtr(xmlData), CFDataGetLength(xmlData),
		SQLITE_STATIC), errOutSql);
	require_noerr(sqlite3_bind_blob_wrapper(insert, 4,
		SecCertificateGetBytePtr(certificate),
		SecCertificateGetLength(certificate), SQLITE_STATIC), errOutSql);
	s3e = sqlite3_step(insert);
	if (s3e == SQLITE_DONE) {
		/* Great the insert worked. */
		status = noErr;
	} else if (s3e == SQLITE_ERROR) {
        /* Try update. */
        require_noerr(sqlite3_prepare(ts->s3h, updateSQL, sizeof(updateSQL),
            &update, NULL), errOutSql);
        require_noerr(sqlite3_bind_blob_wrapper(update, 1,
            CFDataGetBytePtr(xmlData), CFDataGetLength(xmlData),
            SQLITE_STATIC), errOutSql);
        require_noerr(sqlite3_bind_blob_wrapper(update, 2,
            CFDataGetBytePtr(digest), CFDataGetLength(digest), SQLITE_STATIC),
            errOutSql);
        s3e = sqlite3_step(update);
		require(s3e == SQLITE_DONE, errOutSql);
        status = noErr;
    } else {
		require_noerr(s3e, errOutSql);
	}

errOutSql:
	if (insert)
		s3e = sqlite3_finalize(insert);
	if (update)
		s3e = sqlite3_finalize(update);

    if (s3e == SQLITE_OK)
        sqlite3_exec(ts->s3h, "COMMIT TRANSACTION", NULL, NULL, NULL);
    else
        sqlite3_exec(ts->s3h, "ROLLBACK TRANSACTION", NULL, NULL, NULL);

errOut:
	CFReleaseSafe(xmlData);
	CFReleaseSafe(array);
	verify_noerr(pthread_mutex_unlock(&ts->lock));
errOutNotLocked:
	return status;
}

/* AUDIT[securityd](done):
   ts (ok) might be NULL.
   digest (ok) is a data of any length (might be 0).
 */
OSStatus SecTrustStoreRemoveCertificateWithDigest(SecTrustStoreRef ts,
    CFDataRef digest) {
	sqlite3_stmt *deleteStmt = NULL;

	require_quiet(ts, errOutNotLocked);
	require(!ts->readOnly, errOutNotLocked);
	require_noerr(pthread_mutex_lock(&ts->lock), errOutNotLocked);
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
	verify_noerr(pthread_mutex_unlock(&ts->lock));
errOutNotLocked:
	return noErr;
}

bool _SecTrustStoreRemoveAll(SecTrustStoreRef ts)
{
    bool removed_all = false;
	require(ts, errOutNotLocked);
	require(!ts->readOnly, errOutNotLocked);
	require_noerr(pthread_mutex_lock(&ts->lock), errOutNotLocked);
    if (SQLITE_OK == sqlite3_exec(ts->s3h, deleteAllSQL, NULL, NULL, NULL))
        removed_all = true;

    /* prepared statements become unusable after deleteAllSQL, reset them */
    if (ts->copyParents)
        sqlite3_finalize(ts->copyParents);
    sqlite3_prepare(ts->s3h, copyParentsSQL, sizeof(copyParentsSQL),
        &ts->copyParents, NULL);
    if (ts->contains)
        sqlite3_finalize(ts->contains);
    sqlite3_prepare(ts->s3h, containsSQL, sizeof(containsSQL),
        &ts->contains, NULL);

	verify_noerr(pthread_mutex_unlock(&ts->lock));
errOutNotLocked:
	return removed_all;
}

CFArrayRef SecTrustStoreCopyParents(SecTrustStoreRef ts,
	SecCertificateRef certificate) {
	CFMutableArrayRef parents = NULL;

	require(ts, errOutNotLocked);
	require_noerr(pthread_mutex_lock(&ts->lock), errOutNotLocked);

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
	verify_noerr(pthread_mutex_unlock(&ts->lock));
errOutNotLocked:
	return parents;
}

/* AUDIT[securityd](done):
   ts (ok) might be NULL.
   digest (ok) is a data of any length (might be 0), only its cf type has
   been checked.
*/
bool SecTrustStoreContainsCertificateWithDigest(SecTrustStoreRef ts,
	CFDataRef digest) {
	bool contains = false;

	require_quiet(ts, errOutNotLocked);
	require_noerr(pthread_mutex_lock(&ts->lock), errOutNotLocked);

	/* @@@ Might have to use SQLITE_TRANSIENT */
	require_noerr(sqlite3_bind_blob_wrapper(ts->contains, 1,
		CFDataGetBytePtr(digest), CFDataGetLength(digest), SQLITE_STATIC),
		errOut);
	int s3e = sqlite3_step(ts->contains);
	if (s3e == SQLITE_ROW) {
		contains = true;
	} else {
		require(s3e == SQLITE_DONE, errOut);
	}

errOut:
	verify_noerr(sqlite3_reset(ts->contains));
	verify_noerr(sqlite3_clear_bindings(ts->contains));
	verify_noerr(pthread_mutex_unlock(&ts->lock));
errOutNotLocked:
	return contains;
}
