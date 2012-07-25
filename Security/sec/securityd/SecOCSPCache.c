/*
 * Copyright (c) 2009-2010 Apple Inc. All Rights Reserved.
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
 *  SecOCSPCache.c - securityd
 */

#include <securityd/SecOCSPCache.h>
#include <security_utilities/debugging.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecFramework.h>
#include <Security/SecInternal.h>
#include <sqlite3.h>
#include <AssertMacros.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <asl.h>
#include "sqlutils.h"

#define ocspErrorLog(args...)     asl_log(NULL, NULL, ASL_LEVEL_ERR, ## args)

static const char expireSQL[] = "DELETE FROM responses WHERE expires<?";
static const char beginTxnSQL[] = "BEGIN EXCLUSIVE TRANSACTION";
static const char endTxnSQL[] = "COMMIT TRANSACTION";
static const char insertResponseSQL[] = "INSERT INTO responses "
    "(ocspResponse,responderURI,expires,lastUsed) VALUES (?,?,?,?)";
static const char insertLinkSQL[] = "INSERT INTO ocsp (hashAlgorithm,"
    "issuerNameHash,issuerPubKeyHash,serialNum,responseId) VALUES (?,?,?,?,?)";
static const char selectHashAlgorithmSQL[] = "SELECT DISTINCT hashAlgorithm "
    "FROM ocsp WHERE serialNum=?";
static const char selectResponseSQL[] = "SELECT ocspResponse,responseId FROM "
    "responses WHERE responseId=(SELECT responseId FROM ocsp WHERE "
    "issuerNameHash=? AND issuerPubKeyHash=? AND serialNum=? AND hashAlgorithm=?)"
    " ORDER BY expires DESC";

#if NO_SERVER
CF_EXPORT
CFURLRef CFCopyHomeDirectoryURLForUser(CFStringRef uName);	/* Pass NULL for the current user's home directory */
#endif

#define kSecOCSPCachePath "/Library/Keychains/ocspcache.sqlite3";

typedef struct __SecOCSPCache *SecOCSPCacheRef;
struct __SecOCSPCache {
	sqlite3 *s3h;
	sqlite3_stmt *expire;
	sqlite3_stmt *beginTxn;
	sqlite3_stmt *endTxn;
	sqlite3_stmt *insertResponse;
	sqlite3_stmt *insertLink;
	sqlite3_stmt *selectHashAlgorithm;
	sqlite3_stmt *selectResponse;
    bool in_transaction;
};

static pthread_once_t kSecOCSPCacheOnce = PTHREAD_ONCE_INIT;
static SecOCSPCacheRef kSecOCSPCache = NULL;

/* @@@ Duplicated from SecTrustStore.c */
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

static int sec_sqlite3_reset(sqlite3_stmt *stmt, int s3e) {
    int s3e2;
    if (s3e == SQLITE_ROW || s3e == SQLITE_DONE)
        s3e = SQLITE_OK;
    s3e2 = sqlite3_reset(stmt);
    if (s3e2 && !s3e)
        s3e = s3e2;
    s3e2 = sqlite3_clear_bindings(stmt);
    if (s3e2 && !s3e)
        s3e = s3e2;
    return s3e;
}

static int SecOCSPCacheEnsureTxn(SecOCSPCacheRef this) {
    int s3e, s3e2;

    if (this->in_transaction)
        return SQLITE_OK;

    s3e = sqlite3_step(this->beginTxn);
    if (s3e == SQLITE_DONE) {
        this->in_transaction = true;
        s3e = SQLITE_OK;
    } else {
        secdebug("ocspcache", "sqlite3_step returned [%d]: %s", s3e,
            sqlite3_errmsg(this->s3h));
    }
    s3e2 = sqlite3_reset(this->beginTxn);
    if (s3e2 && !s3e)
        s3e = s3e2;

    return s3e;
}

static int SecOCSPCacheCommitTxn(SecOCSPCacheRef this) {
    int s3e, s3e2;

    if (!this->in_transaction)
        return SQLITE_OK;

    s3e = sqlite3_step(this->endTxn);
    if (s3e == SQLITE_DONE) {
        this->in_transaction = false;
        s3e = SQLITE_OK;
    } else {
        secdebug("ocspcache", "sqlite3_step returned [%d]: %s", s3e,
            sqlite3_errmsg(this->s3h));
    }
    s3e2 = sqlite3_reset(this->endTxn);
    if (s3e2 && !s3e)
        s3e = s3e2;

    return s3e;
}

static SecOCSPCacheRef SecOCSPCacheCreate(const char *db_name) {
	SecOCSPCacheRef this;
	int s3e;
    bool create = true;

	require(this = (SecOCSPCacheRef)malloc(sizeof(struct __SecOCSPCache)), errOut);
	require_noerr(s3e = sec_sqlite3_open(db_name, &this->s3h, create), errOut);
    this->in_transaction = false;

	s3e = sqlite3_prepare_v2(this->s3h, beginTxnSQL, sizeof(beginTxnSQL),
		&this->beginTxn, NULL);
	require_noerr(s3e, errOut);
	s3e = sqlite3_prepare_v2(this->s3h, endTxnSQL, sizeof(endTxnSQL),
		&this->endTxn, NULL);
	require_noerr(s3e, errOut);

	s3e = sqlite3_prepare_v2(this->s3h, expireSQL, sizeof(expireSQL),
		&this->expire, NULL);
	if (create && s3e == SQLITE_ERROR) {
        s3e = SecOCSPCacheEnsureTxn(this);
		require_noerr(s3e, errOut);

		/* sqlite3_prepare returns SQLITE_ERROR if the table we are
		   compiling this statement for doesn't exist. */
		char *errmsg = NULL;
		s3e = sqlite3_exec(this->s3h,
			"CREATE TABLE ocsp("
			"issuerNameHash BLOB NOT NULL,"
			"issuerPubKeyHash BLOB NOT NULL,"
			"serialNum BLOB NOT NULL,"
			"hashAlgorithm BLOB NOT NULL,"
			"responseId INTEGER NOT NULL"
			");"
			"CREATE INDEX iResponseId ON ocsp(responseId);"
			"CREATE INDEX iserialNum ON ocsp(serialNum);"
			"CREATE INDEX iSNumDAlg ON ocsp(serialNum,hashAlgorithm);"
			"CREATE TABLE responses("
            "responseId INTEGER PRIMARY KEY,"
			"ocspResponse BLOB NOT NULL,"
			"responderURI BLOB,"
            "expires DOUBLE NOT NULL,"
            "lastUsed DOUBLE NOT NULL"
			");"
			"CREATE INDEX iexpires ON responses(expires);"
            "CREATE TRIGGER tocspdel BEFORE DELETE ON responses FOR EACH ROW "
            "BEGIN "
            "DELETE FROM ocsp WHERE responseId=OLD.responseId;"
            " END;"
			, NULL, NULL, &errmsg);
		if (errmsg) {
			ocspErrorLog("ocsp db CREATE TABLES: %s", errmsg);
			sqlite3_free(errmsg);
		}
		require_noerr(s3e, errOut);
        s3e = sqlite3_prepare_v2(this->s3h, expireSQL, sizeof(expireSQL),
            &this->expire, NULL);
	}
	require_noerr(s3e, errOut);
	s3e = sqlite3_prepare_v2(this->s3h, insertResponseSQL, sizeof(insertResponseSQL),
		&this->insertResponse, NULL);
	require_noerr(s3e, errOut);
	s3e = sqlite3_prepare_v2(this->s3h, insertLinkSQL, sizeof(insertLinkSQL),
		&this->insertLink, NULL);
	require_noerr(s3e, errOut);
	s3e = sqlite3_prepare_v2(this->s3h, selectHashAlgorithmSQL, sizeof(selectHashAlgorithmSQL),
		&this->selectHashAlgorithm, NULL);
	require_noerr(s3e, errOut);
	s3e = sqlite3_prepare_v2(this->s3h, selectResponseSQL, sizeof(selectResponseSQL),
		&this->selectResponse, NULL);
	require_noerr(s3e, errOut);

	return this;

errOut:
	if (this) {
		sqlite3_close(this->s3h);
		free(this);
	}

	return NULL;
}

static void SecOCSPCacheInit(void) {
	static const char *path = kSecOCSPCachePath;
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

    kSecOCSPCache = SecOCSPCacheCreate(path);
    if (kSecOCSPCache)
        atexit(SecOCSPCacheGC);
}

/* Instance implemenation. */

static void _SecOCSPCacheAddResponse(SecOCSPCacheRef this,
    SecOCSPResponseRef ocspResponse, CFURLRef localResponderURI) {
    int s3e;

    secdebug("ocspcache", "adding response from %@", localResponderURI);
    require_noerr(s3e = SecOCSPCacheEnsureTxn(this), errOut);

    /* responses.ocspResponse */
    CFDataRef responseData = SecOCSPResponseGetData(ocspResponse);
    s3e = sqlite3_bind_blob_wrapper(this->insertResponse, 1,
        CFDataGetBytePtr(responseData),
        CFDataGetLength(responseData), SQLITE_TRANSIENT);

    /* responses.responderURI */
    if (!s3e) {
        CFDataRef uriData = NULL;
        if (localResponderURI) {
            uriData = CFURLCreateData(kCFAllocatorDefault, localResponderURI,
                kCFStringEncodingUTF8, false);
        }
        if (uriData) {
            s3e = sqlite3_bind_blob_wrapper(this->insertResponse, 2,
                    CFDataGetBytePtr(uriData),
                    CFDataGetLength(uriData), SQLITE_TRANSIENT);
            CFRelease(uriData);
        } else {
            s3e = sqlite3_bind_null(this->insertResponse, 2);
        }
    }
    /* responses.expires */
    if (!s3e) s3e = sqlite3_bind_double(this->insertResponse, 3,
            SecOCSPResponseGetExpirationTime(ocspResponse));
    /* responses.lastUsed */
    if (!s3e) s3e = sqlite3_bind_double(this->insertResponse, 4,
            SecOCSPResponseVerifyTime(ocspResponse));

    /* Execute the insert statement. */
    if (!s3e) s3e = sqlite3_step(this->insertResponse);
    require_noerr(s3e = sec_sqlite3_reset(this->insertResponse, s3e), errOut);

    sqlite3_int64 responseId = sqlite3_last_insert_rowid(this->s3h);

    /* Now add a link record for every singleResponse in the ocspResponse. */
    SecAsn1OCSPSingleResponse **responses;
    for (responses = ocspResponse->responseData.responses;
        *responses; ++responses) {
		SecAsn1OCSPSingleResponse *resp = *responses;
        SecAsn1OCSPCertID *certId = &resp->certID;

        s3e = sqlite3_bind_blob_wrapper(this->insertLink, 1,
            certId->algId.algorithm.Data, certId->algId.algorithm.Length,
            SQLITE_TRANSIENT);
        if (!s3e) s3e = sqlite3_bind_blob_wrapper(this->insertLink, 2,
            certId->issuerNameHash.Data, certId->issuerNameHash.Length,
            SQLITE_TRANSIENT);
        if (!s3e) s3e = sqlite3_bind_blob_wrapper(this->insertLink, 3,
            certId->issuerPubKeyHash.Data, certId->issuerPubKeyHash.Length,
            SQLITE_TRANSIENT);
        if (!s3e) s3e = sqlite3_bind_blob_wrapper(this->insertLink, 4,
            certId->serialNumber.Data, certId->serialNumber.Length,
            SQLITE_TRANSIENT);
        if (!s3e) s3e = sqlite3_bind_int64(this->insertLink, 5,
            responseId);

        /* Execute the insert statement. */
        if (!s3e) s3e = sqlite3_step(this->insertLink);
        require_noerr(s3e = sec_sqlite3_reset(this->insertLink, s3e), errOut);
    }

errOut:
    if (s3e) {
        ocspErrorLog("ocsp cache add failed: %s", sqlite3_errmsg(this->s3h));
        /* @@@ Blow away the cache and create a new db. */
    }
}

static SecOCSPResponseRef _SecOCSPCacheCopyMatching(SecOCSPCacheRef this,
    SecOCSPRequestRef request, CFURLRef responderURI) {
    SecOCSPResponseRef response = NULL;
    const DERItem *publicKey;
    CFDataRef issuer = NULL;
    CFDataRef serial = NULL;
    int s3e = SQLITE_ERROR;

    require(publicKey = SecCertificateGetPublicKeyData(request->issuer), errOut);
    require(issuer = SecCertificateCopyIssuerSequence(request->certificate), errOut);
    require(serial = SecCertificateCopySerialNumber(request->certificate), errOut);
    s3e = sqlite3_bind_blob_wrapper(this->selectHashAlgorithm, 1,
        CFDataGetBytePtr(serial), CFDataGetLength(serial), SQLITE_TRANSIENT);
    while (!s3e && !response &&
        (s3e = sqlite3_step(this->selectHashAlgorithm)) == SQLITE_ROW) {
        SecAsn1Oid algorithm;
        algorithm.Data = (uint8_t *)sqlite3_column_blob(this->selectHashAlgorithm, 0);
        algorithm.Length = sqlite3_column_bytes(this->selectHashAlgorithm, 0);

        /* Calcluate the issuerKey and issuerName digests using the returned
           hashAlgorithm. */
        CFDataRef issuerNameHash = SecDigestCreate(kCFAllocatorDefault,
            &algorithm, NULL, CFDataGetBytePtr(issuer), CFDataGetLength(issuer));
        CFDataRef issuerPubKeyHash = SecDigestCreate(kCFAllocatorDefault,
            &algorithm, NULL, publicKey->data, publicKey->length);

        require(issuerNameHash && issuerPubKeyHash, nextResponse);

        /* Now we have the serial, algorithm, issuerNameHash and
           issuerPubKeyHash so let's lookup the db entry. */
        s3e = sqlite3_bind_blob_wrapper(this->selectResponse, 1, CFDataGetBytePtr(issuerNameHash),
            CFDataGetLength(issuerNameHash), SQLITE_TRANSIENT);
        if (!s3e) s3e = sqlite3_bind_blob_wrapper(this->selectResponse, 2, CFDataGetBytePtr(issuerPubKeyHash),
            CFDataGetLength(issuerPubKeyHash), SQLITE_TRANSIENT);
        if (!s3e) s3e = sqlite3_bind_blob_wrapper(this->selectResponse, 3, CFDataGetBytePtr(serial),
            CFDataGetLength(serial), SQLITE_TRANSIENT);
        if (!s3e) s3e = sqlite3_bind_blob_wrapper(this->selectResponse, 4, algorithm.Data,
            algorithm.Length, SQLITE_TRANSIENT);

        if (!s3e) s3e = sqlite3_step(this->selectResponse);
        if (s3e == SQLITE_ROW) {
            /* Found an entry! */
            secdebug("ocspcache", "found cached response");

            const void *respData = sqlite3_column_blob(this->selectResponse, 0);
            int respLen = sqlite3_column_bytes(this->selectResponse, 0);
            CFDataRef resp = CFDataCreate(kCFAllocatorDefault, respData, respLen);
            if (resp) {
                response = SecOCSPResponseCreate(resp, NULL_TIME);
                CFRelease(resp);
            }
            if (response) {
                //sqlite3_int64 responseId = sqlite3_column_int64(this->selectResponse, 1);
                /* @@@ Update the lastUsed field in the db. */
            }
        }

nextResponse:
        s3e = sec_sqlite3_reset(this->selectResponse, s3e);
        CFReleaseSafe(issuerNameHash);
        CFReleaseSafe(issuerPubKeyHash);
    }
    require_noerr(s3e = sec_sqlite3_reset(this->selectHashAlgorithm, s3e), errOut);

errOut:
    CFReleaseSafe(serial);
    CFReleaseSafe(issuer);

    if (s3e) {
        ocspErrorLog("ocsp cache lookup failed: %s", sqlite3_errmsg(this->s3h));
        /* @@@ Blow away the cache and create a new db. */

        if (response) {
            SecOCSPResponseFinalize(response);
            response = NULL;
        }
    }

    secdebug("ocspcache", "returning %s", (response ? "cached response" : "NULL"));

    return response;
}

static void _SecOCSPCacheGC(SecOCSPCacheRef this) {
    int s3e;

    require_noerr(s3e = SecOCSPCacheEnsureTxn(this), errOut);
    secdebug("ocspcache", "expiring stale responses");
    s3e = sqlite3_bind_double(this->expire, 1, CFAbsoluteTimeGetCurrent());
    if (!s3e) s3e = sqlite3_step(this->expire);
    require_noerr(s3e = sec_sqlite3_reset(this->expire, s3e), errOut);
    require_noerr(s3e = SecOCSPCacheCommitTxn(this), errOut);

errOut:
    if (s3e) {
        ocspErrorLog("ocsp cache expire failed: %s", sqlite3_errmsg(this->s3h));
        /* @@@ Blow away the cache and create a new db. */
    }
}

static void _SecOCSPCacheFlush(SecOCSPCacheRef this) {
    int s3e;
    secdebug("ocspcache", "flushing pending changes");
    s3e = SecOCSPCacheCommitTxn(this);

    if (s3e) {
        ocspErrorLog("ocsp cache flush failed: %s", sqlite3_errmsg(this->s3h));
        /* @@@ Blow away the cache and create a new db. */
    }
}

/* Public API */

void SecOCSPCacheAddResponse(SecOCSPResponseRef response,
    CFURLRef localResponderURI) {
    pthread_once(&kSecOCSPCacheOnce, SecOCSPCacheInit);
    if (!kSecOCSPCache)
        return;

    _SecOCSPCacheAddResponse(kSecOCSPCache, response, localResponderURI);
}

SecOCSPResponseRef SecOCSPCacheCopyMatching(SecOCSPRequestRef request,
    CFURLRef localResponderURI /* may be NULL */) {
    pthread_once(&kSecOCSPCacheOnce, SecOCSPCacheInit);
    if (!kSecOCSPCache)
        return NULL;

    return _SecOCSPCacheCopyMatching(kSecOCSPCache, request, localResponderURI);
}

/* This should be called on a normal non emergency exit. This function
   effectively does a SecOCSPCacheFlush.
   Currently this is called from our atexit handeler.
   This function expires any records that are stale and commits.

   Idea for future cache management policies:
   Expire old cache entires from database if:
    - The time to do so has arrived based on the nextExpire date in the
      policy table.
    - If the size of the database exceeds the limit set in the maxSize field
      in the policy table, vacuum the db.  If the database is still too
      big, expire records on a LRU basis.
 */
void SecOCSPCacheGC(void) {
    if (kSecOCSPCache)
        _SecOCSPCacheGC(kSecOCSPCache);
}

/* Call this periodically or perhaps when we are exiting due to low memory. */
void SecOCSPCacheFlush(void) {
    if (kSecOCSPCache)
        _SecOCSPCacheFlush(kSecOCSPCache);
}
