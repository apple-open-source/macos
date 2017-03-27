/*
 * Copyright (c) 2012-2016 Apple Inc. All Rights Reserved.
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


#include "SecDb.h"
#include "debugging.h"

#include <sqlite3.h>
#include <sqlite3_private.h>
#include <CoreFoundation/CoreFoundation.h>
#include <libgen.h>
#include <sys/csr.h>
#include <sys/stat.h>
#include <AssertMacros.h>
#include "SecCFWrappers.h"
#include "SecCFError.h"
#include "SecIOFormat.h"
#include <stdio.h>
#include "Security/SecBase.h"


//
// Architecturally inverted files
// These are in SecureObjectSync but utilities depends on them
// <rdar://problem/20802079> Fix layer violation (SOSDigestVector, SOSManifest, SecDB.c)
//
#include <Security/SecureObjectSync/SOSDigestVector.h>
#include <Security/SecureObjectSync/SOSManifest.h>

#define HAVE_UNLOCK_NOTIFY  0

struct __OpaqueSecDbStatement {
    CFRuntimeBase _base;

    SecDbConnectionRef dbconn;
    sqlite3_stmt *stmt;
};

struct __OpaqueSecDbConnection {
    CFRuntimeBase _base;

    //CFMutableDictionaryRef statements;

    SecDbRef db;     // NONRETAINED, since db or block retains us
    bool readOnly;
    bool inTransaction;
    SecDbTransactionSource source;
    bool isCorrupted;
    int maybeCorruptedCode;
    bool hasIOFailure;
    CFErrorRef corruptionError;
    sqlite3 *handle;
    // Pending deletions and additions for the current transaction
    // Entires are either:
    // 1) a CFArrayRef of 1 element representing a deletion,
    // 2) a CFArrayRef of 2 elements representing the element 0 having been replaced with element 1
    // 3) a CFTypeRef that is not a CFArrayRef, representing an add of the element in question.
    CFMutableArrayRef changes;
};

struct __OpaqueSecDb {
    CFRuntimeBase _base;

    CFStringRef db_path;
    dispatch_queue_t queue;
    dispatch_queue_t commitQueue;
    CFMutableArrayRef connections;
    dispatch_semaphore_t write_semaphore;
    dispatch_semaphore_t read_semaphore;
    bool didFirstOpen;
    bool (^opened)(SecDbConnectionRef dbconn, bool didCreate, bool *callMeAgainForNextConnection, CFErrorRef *error);
    bool callOpenedHandlerForNextConnection;
    CFMutableArrayRef notifyPhase; /* array of SecDBNotifyBlock */
    mode_t mode; /* database file permissions, default 0600 */
    bool readWrite; /* open database read-write, default true */
    bool allowRepair; /* allow database repair, default true */
    bool useWAL; /* use WAL mode, default true */
};

// MARK: Error domains and error helper functions

CFStringRef kSecDbErrorDomain = CFSTR("com.apple.utilities.sqlite3");

bool SecDbError(int sql_code, CFErrorRef *error, CFStringRef format, ...) {
    if (sql_code == SQLITE_OK) return true;

    if (error) {
        va_list args;
        CFIndex code = sql_code;
        CFErrorRef previousError = *error;

        *error = NULL;
        va_start(args, format);
        SecCFCreateErrorWithFormatAndArguments(code, kSecDbErrorDomain, previousError, error, NULL, format, args);
        va_end(args);
    }
    return false;
}

bool SecDbErrorWithDb(int sql_code, sqlite3 *db, CFErrorRef *error, CFStringRef format, ...) {
    if (sql_code == SQLITE_OK) return true;
    if (error) {
        va_list args;
        va_start(args, format);
        CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, format, args);
        va_end(args);

        int extended_code = sqlite3_extended_errcode(db);
        if (sql_code == extended_code)
            SecDbError(sql_code, error, CFSTR("%@: [%d] %s"), message, sql_code, sqlite3_errmsg(db));
        else
            SecDbError(sql_code, error, CFSTR("%@: [%d->%d] %s"), message, sql_code, extended_code, sqlite3_errmsg(db));
        CFReleaseSafe(message);
    }
    return false;
}

bool SecDbErrorWithStmt(int sql_code, sqlite3_stmt *stmt, CFErrorRef *error, CFStringRef format, ...) {
    if (sql_code == SQLITE_OK) return true;
    if (error) {
        va_list args;
        va_start(args, format);
        CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, format, args);
        va_end(args);

        sqlite3 *db = sqlite3_db_handle(stmt);
        const char *sql = sqlite3_sql(stmt);
        int extended_code = sqlite3_extended_errcode(db);
        if (sql_code == extended_code)
            SecDbError(sql_code, error, CFSTR("%@: [%d] %s sql: %s"), message, sql_code, sqlite3_errmsg(db), sql);
        else
            SecDbError(sql_code, error, CFSTR("%@: [%d->%d] %s sql: %s"), message, sql_code, extended_code, sqlite3_errmsg(db), sql);
        CFReleaseSafe(message);
    }
    return false;
}


// MARK: -
// MARK: Static helper functions

static bool SecDbOpenHandle(SecDbConnectionRef dbconn, bool *created, CFErrorRef *error);
static bool SecDbHandleCorrupt(SecDbConnectionRef dbconn, int rc, CFErrorRef *error);

#pragma mark -
#pragma mark SecDbRef

static CFStringRef
SecDbCopyFormatDescription(CFTypeRef value, CFDictionaryRef formatOptions)
{
    SecDbRef db = (SecDbRef)value;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<SecDb path:%@ connections: %@>"), db->db_path, db->connections);
}


static void
SecDbDestroy(CFTypeRef value)
{
    SecDbRef db = (SecDbRef)value;
    CFReleaseSafe(db->connections);
    CFReleaseSafe(db->db_path);
    dispatch_release(db->queue);
    dispatch_release(db->commitQueue);
    dispatch_release(db->read_semaphore);
    dispatch_release(db->write_semaphore);
    if (db->opened)
        Block_release(db->opened);
    CFReleaseNull(db->notifyPhase);
}

CFGiblisFor(SecDb)

SecDbRef
SecDbCreateWithOptions(CFStringRef dbName, mode_t mode, bool readWrite, bool allowRepair, bool useWAL,
                       bool (^opened)(SecDbConnectionRef dbconn, bool didCreate, bool *callMeAgainForNextConnection, CFErrorRef *error))
{
    SecDbRef db = NULL;

    db = CFTypeAllocate(SecDb, struct __OpaqueSecDb, kCFAllocatorDefault);
    require(db != NULL, done);

    CFStringPerformWithCString(dbName, ^(const char *dbNameStr) {
        db->queue = dispatch_queue_create(dbNameStr, DISPATCH_QUEUE_SERIAL);
    });
    CFStringRef commitQueueStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@-commit"), dbName);
    CFStringPerformWithCString(commitQueueStr, ^(const char *cqNameStr) {
        db->commitQueue = dispatch_queue_create(cqNameStr, DISPATCH_QUEUE_CONCURRENT);
    });
    CFReleaseNull(commitQueueStr);
    db->read_semaphore = dispatch_semaphore_create(kSecDbMaxReaders);
    db->write_semaphore = dispatch_semaphore_create(kSecDbMaxWriters);
    db->connections = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    db->opened = opened ? Block_copy(opened) : NULL;
    if (getenv("__OSINSTALL_ENVIRONMENT") != NULL) {
        // TODO: Move this code out of this layer
        secinfo("#SecDB", "SecDB: running from installer");

        db->db_path = CFSTR("file::memory:?cache=shared");
    } else {
        db->db_path = CFStringCreateCopy(kCFAllocatorDefault, dbName);
    }
    db->mode = mode;
    db->readWrite = readWrite;
    db->allowRepair = allowRepair;
    db->useWAL = useWAL;

done:
    return db;
}

SecDbRef
SecDbCreate(CFStringRef dbName,
            bool (^opened)(SecDbConnectionRef dbconn, bool didCreate, bool *callMeAgainForNextConnection, CFErrorRef *error))
{
    return SecDbCreateWithOptions(dbName, 0600, true, true, true, opened);
}

CFIndex
SecDbIdleConnectionCount(SecDbRef db) {
    __block CFIndex count = 0;
    dispatch_sync(db->queue, ^{
        count = CFArrayGetCount(db->connections);
    });
    return count;
}

void SecDbAddNotifyPhaseBlock(SecDbRef db, SecDBNotifyBlock notifyPhase)
{
    SecDBNotifyBlock block = Block_copy(notifyPhase); /* Force the block off the stack */
    if (db->notifyPhase == NULL) {
        db->notifyPhase = CFArrayCreateMutableForCFTypes(NULL);
    }
    CFArrayAppendValue(db->notifyPhase, block);
    Block_release(block);
}

static void SecDbNotifyPhase(SecDbConnectionRef dbconn, SecDbTransactionPhase phase) {
    if (CFArrayGetCount(dbconn->changes)) {
        CFArrayRef changes = dbconn->changes;
        dbconn->changes = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        if (dbconn->db->notifyPhase) {
            CFArrayForEach(dbconn->db->notifyPhase, ^(const void *value) {
                SecDBNotifyBlock notifyBlock = (SecDBNotifyBlock)value;
                notifyBlock(dbconn, phase, dbconn->source, changes);
            });
        }
        CFReleaseSafe(changes);
    }
}

static void SecDbOnNotify(SecDbConnectionRef dbconn, void (^perform)()) {
    perform();
}

CFStringRef SecDbGetPath(SecDbRef db) {
    if(!db) {
        return NULL;
    }
    return db->db_path;
}


#pragma mark -
#pragma mark SecDbConnectionRef

static bool SecDbCheckCorrupted(SecDbConnectionRef dbconn)
{
    __block bool isCorrupted = true;
    __block CFErrorRef error = NULL;
    SecDbPrepare(dbconn, CFSTR("PRAGMA integrity_check"), &error, ^(sqlite3_stmt *stmt) {
        SecDbStep(dbconn, stmt, &error, ^(bool *stop) {
            const char * result = (const char*)sqlite3_column_text(stmt, 0);
            if (result && strncasecmp(result, "ok", 3) == 0) {
                isCorrupted = false;
            }
        });
    });
    if (error) {
        secinfo("#SecDB", "#SecDB warning error %{public}@ when running integrity check", error);
        CFRelease(error);
    }
    return isCorrupted;
}

static bool SecDbDidCreateFirstConnection(SecDbConnectionRef dbconn, bool didCreate, CFErrorRef *error)
{
    secinfo("#SecDB", "#SecDB starting maintenance");
    bool ok = true;

    if (!didCreate && !dbconn->isCorrupted) {
        dbconn->isCorrupted = SecDbCheckCorrupted(dbconn);
        if (dbconn->isCorrupted) {
            secinfo("#SecDB", "#SecDB integrity check=fail");
        } else {
            secinfo("#SecDB", "#SecDB starting maintenance");
        }
    }

    if (!dbconn->isCorrupted && dbconn->db->opened) {
        CFErrorRef localError = NULL;

        dbconn->db->callOpenedHandlerForNextConnection = false;
        ok = dbconn->db->opened(dbconn, didCreate, &dbconn->db->callOpenedHandlerForNextConnection, &localError);

        if (!ok)
            secerror("opened block failed: %@", localError);

        if (!dbconn->isCorrupted && error && *error == NULL) {
            *error = localError;
            localError = NULL;
        } else {
            if (localError)
                secerror("opened block failed: error is released and lost");
            CFReleaseNull(localError);
        }
    }

    if (dbconn->isCorrupted) {
        ok = SecDbHandleCorrupt(dbconn, 0, error);
    }

    secinfo("#SecDB", "#SecDB starting maintenance");
    return ok;
}

void SecDbCorrupt(SecDbConnectionRef dbconn, CFErrorRef error)
{
    CFStringRef str = CFStringCreateWithFormat(NULL, NULL, CFSTR("SecDBCorrupt: %@"), error);
    if (str) {
        char buffer[1000] = "?";
        uint32_t errorCode = 0;
        CFStringGetCString(str, buffer, sizeof(buffer), kCFStringEncodingUTF8);
        os_log_fault(secLogObjForScope("SecEmergency"), "%s", buffer);
        if (error)
            errorCode = (uint32_t)CFErrorGetCode(error);
        __security_simulatecrash(str, __sec_exception_code_CorruptDb(errorCode));
        CFRelease(str);
    }
    dbconn->isCorrupted = true;
    CFRetainAssign(dbconn->corruptionError, error);
}


static uint8_t knownDbPathIndex(SecDbConnectionRef dbconn)
{

    if(CFEqual(dbconn->db->db_path, CFSTR("/Library/Keychains/keychain-2.db")))
        return 1;
    if(CFEqual(dbconn->db->db_path, CFSTR("/Library/Keychains/ocspcache.sqlite3")))
        return 2;
    if(CFEqual(dbconn->db->db_path, CFSTR("/Library/Keychains/TrustStore.sqlite3")))
        return 3;
    if(CFEqual(dbconn->db->db_path, CFSTR("/Library/Keychains/caissuercache.sqlite3")))
        return 4;

    /* Unknown DB path */
    return 0;
}


// Return true if there was no error, returns false otherwise and set *error to an appropriate CFErrorRef.
static bool SecDbConnectionCheckCode(SecDbConnectionRef dbconn, int code, CFErrorRef *error, CFStringRef desc, ...) {
    if (code == SQLITE_OK || code == SQLITE_DONE)
        return true;

    if (error) {
        va_list args;
        va_start(args, desc);
        CFStringRef msg = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, desc, args);
        va_end(args);
        SecDbErrorWithDb(code, dbconn->handle, error, msg);
        CFRelease(msg);
    }

    dbconn->hasIOFailure |= (SQLITE_IOERR == code);

    /* If it's already corrupted, don't try to recover */
    if (dbconn->isCorrupted) {
        CFStringRef reason = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                      CFSTR("SQL DB %@ is corrupted already. Not trying to recover, corruption error was: %d (previously %d)"),
                                                      dbconn->db->db_path, code, dbconn->maybeCorruptedCode);
        secerror("%@",reason);
        __security_simulatecrash(reason, __sec_exception_code_TwiceCorruptDb(knownDbPathIndex(dbconn)));
        CFReleaseSafe(reason);
        return false;
    }

    // NOTADB means file is garbage, so it's functionally equivalent to corruption
    dbconn->isCorrupted = (SQLITE_CORRUPT == code) || (SQLITE_NOTADB == code);
    if (dbconn->isCorrupted) {
        /* Run integrity check and only make dbconn->isCorrupted true and
           run the corruption handler if the integrity check conclusively fails. */
        dbconn->maybeCorruptedCode = code;
        dbconn->isCorrupted = SecDbCheckCorrupted(dbconn);
        if (dbconn->isCorrupted) {
            secerror("operation returned code: %d integrity check=fail", code);
            SecDbHandleCorrupt(dbconn, code, error);
        } else {
            secerror("operation returned code: %d: integrity check=pass", code);
        }
    }

    return false;
}

#if HAVE_UNLOCK_NOTIFY

static void SecDbUnlockNotify(void **apArg, int nArg) {
    int i;
    for(i=0; i<nArg; i++) {
        dispatch_semaphore_t dsema = (dispatch_semaphore_t)apArg[i];
        dispatch_semaphore_signal(dsema);
    }
}

static bool SecDbWaitForUnlockNotify(SecDbConnectionRef dbconn, sqlite3_stmt *stmt, CFErrorRef *error) {
    int rc;
    dispatch_semaphore_t dsema = dispatch_semaphore_create(0);
    rc = sqlite3_unlock_notify(dbconn->handle, SecDbUnlockNotify, dsema);
    assert(rc == SQLITE_LOCKED || rc == SQLITE_OK);
    if (rc == SQLITE_OK) {
        dispatch_semaphore_wait(dsema, DISPATCH_TIME_FOREVER);
    }
    dispatch_release(dsema);
    return (rc == SQLITE_OK
            ? true
            : (stmt
               ? SecDbErrorWithStmt(rc, stmt, error, CFSTR("sqlite3_unlock_notify"))
               : SecDbErrorWithDb(rc, dbconn->handle, error, CFSTR("sqlite3_unlock_notify"))));
}

#endif

#define BUSY_TIMEOUT_MS (5 * 60 * 1000)  /* 5 minutes */

static bool SecDbBusyHandler(SecDbConnectionRef dbconn, CFErrorRef *error) {
    return SecDbErrorWithDb(sqlite3_busy_timeout(dbconn->handle, BUSY_TIMEOUT_MS), dbconn->handle, error, CFSTR("busy_handler"));
}

static int sleepBackoff[] = { 10, 20, 50, 100, 250 };
static int sumBackoff[]   = { 10, 30, 80, 180, 430 };
static int NumberOfSleepBackoff = sizeof(sleepBackoff)/sizeof(sleepBackoff[0]);

// Return true causes the operation to be tried again.
static bool SecDbWaitIfNeeded(SecDbConnectionRef dbconn, int s3e, sqlite3_stmt *stmt, CFStringRef desc, int nTries, CFErrorRef *error) {
#if HAVE_UNLOCK_NOTIFY
    if (s3e == SQLITE_LOCKED) { // Optionally check for extended code being SQLITE_LOCKED_SHAREDCACHE
        return SecDbWaitForUnlockNotify(dbconn, stmt, error))
    }
#endif
    if (((0xFF & s3e) == SQLITE_BUSY) || ((0xFF & s3e) == SQLITE_LOCKED)) {
        int totaltimeout, timeout;

        _Static_assert(sizeof(sumBackoff) == sizeof(sleepBackoff), "matching arrays not matching");
        _Static_assert(sizeof(sumBackoff[0]) == sizeof(sleepBackoff[0]), "matching arrays not matching");

        if (nTries < NumberOfSleepBackoff) {
            timeout = sleepBackoff[nTries];
            totaltimeout = sumBackoff[nTries];
        } else {
            timeout = sleepBackoff[NumberOfSleepBackoff - 1];
            totaltimeout = sumBackoff[NumberOfSleepBackoff - 1] + (timeout * (nTries - NumberOfSleepBackoff));
        }
        if (totaltimeout < BUSY_TIMEOUT_MS) {
            secinfo("#SecDB", "sqlite busy/locked: %d ntries: %d totaltimeout: %d", s3e, nTries, totaltimeout);
            sqlite3_sleep(timeout);
            return true;
        } else {
            secinfo("#SecDB", "sqlite busy/locked: too long: %d ms, giving up", totaltimeout);
        }
    }

    return SecDbConnectionCheckCode(dbconn, s3e, error, desc);
}

enum SecDbStepResult {
    kSecDbErrorStep = 0,
    kSecDbRowStep = 1,
    kSecDbDoneStep = 2,
};
typedef enum SecDbStepResult SecDbStepResult;

static SecDbStepResult _SecDbStep(SecDbConnectionRef dbconn, sqlite3_stmt *stmt, CFErrorRef *error) {
    assert(stmt != NULL);
    int s3e;
    int ntries = 0;
    for (;;) {
        s3e = sqlite3_step(stmt);
        if (s3e == SQLITE_ROW) {
            return kSecDbRowStep;
        } else if (s3e == SQLITE_DONE) {
            /*
             ** ^[SQLITE_DONE] means that the statement has finished executing
             ** successfully.  sqlite3_step() should not be called again on this virtual
             ** machine without first calling [] to reset the virtual
             ** machine back to its initial state.
             */
            sqlite3_reset(stmt);
            return kSecDbDoneStep;
        } else if (!SecDbWaitIfNeeded(dbconn, s3e, stmt, CFSTR("step"), ntries, error)) {
            return kSecDbErrorStep;
        }
        ntries++;
    };
}

bool
SecDbExec(SecDbConnectionRef dbconn, CFStringRef sql, CFErrorRef *error)
{
    bool ok = true;
    CFRetain(sql);
    while (sql) {
        CFStringRef tail = NULL;
        if (ok) {
            sqlite3_stmt *stmt = SecDbCopyStmt(dbconn, sql, &tail, error);
            ok = stmt != NULL;
            if (stmt) {
                SecDbStepResult sr;
                while ((sr = _SecDbStep(dbconn, stmt, error)) == kSecDbRowStep);
                if (sr == kSecDbErrorStep)
                    ok = false;
                ok &= SecDbReleaseCachedStmt(dbconn, sql, stmt, error);
            }
        } else {
            // TODO We already have an error here we really just want the left over sql in it's userData
            ok = SecDbError(SQLITE_ERROR, error, CFSTR("Error with unexecuted sql remaining %@"), sql);
        }
        CFRelease(sql);
        sql = tail;
    }
    return ok;
}

static bool SecDbBeginTransaction(SecDbConnectionRef dbconn, SecDbTransactionType type, CFErrorRef *error)
{
    bool ok = true;
    CFStringRef query;
    switch (type) {
        case kSecDbImmediateTransactionType:
            secnoticeq("db", "SecDbBeginTransaction SecDbBeginTransaction %p", dbconn);
            query = CFSTR("BEGIN IMMEDATE");
            break;
        case kSecDbExclusiveRemoteTransactionType:
            secnoticeq("db", "SecDbBeginTransaction kSecDbExclusiveRemoteTransactionType %p", dbconn);
            dbconn->source = kSecDbSOSTransaction;
            // FALL THROUGH
        case kSecDbExclusiveTransactionType:
            if (type==kSecDbExclusiveTransactionType)
                secnoticeq("db", "SecDbBeginTransaction kSecDbExclusiveTransactionType %p", dbconn);
            query = CFSTR("BEGIN EXCLUSIVE");
            break;
        case kSecDbNormalTransactionType:
            secnoticeq("db", "SecDbBeginTransaction kSecDbNormalTransactionType %p", dbconn);
            query = CFSTR("BEGIN");
            break;
        default:
            secnoticeq("db", "SecDbBeginTransaction invalid transaction type %lu", type);
            ok = SecDbError(SQLITE_ERROR, error, CFSTR("invalid transaction type %" PRIu32), type);
            query = NULL;
            break;
    }

    if (query != NULL && sqlite3_get_autocommit(dbconn->handle) != 0) {
        ok = SecDbExec(dbconn, query, error);
    }
    if (ok)
        dbconn->inTransaction = true;

    return ok;
}

static bool SecDbEndTransaction(SecDbConnectionRef dbconn, bool commit, CFErrorRef *error)
{
    __block bool ok = true;
    __block bool commited = false;

    dispatch_block_t notifyAndExec = ^{
        if (commit) {
            secnoticeq("db", "SecDbEndTransaction kSecDbTransactionWillCommit %p", dbconn);
            SecDbNotifyPhase(dbconn, kSecDbTransactionWillCommit);
            commited = ok = SecDbExec(dbconn, CFSTR("END"), error);
            secnoticeq("db", "SecDbEndTransaction kSecDbTransactionWillCommit %p (after notify)", dbconn);
        } else {
            ok = SecDbExec(dbconn, CFSTR("ROLLBACK"), error);
            commited = false;
        }
        dbconn->inTransaction = false;
        SecDbNotifyPhase(dbconn, commited ? kSecDbTransactionDidCommit : kSecDbTransactionDidRollback);
        secnoticeq("db", "SecDbEndTransaction %s %p", commited ? "kSecDbTransactionDidCommit" : "kSecDbTransactionDidRollback", dbconn);
        dbconn->source = kSecDbAPITransaction;
    };

    SecDbPerformOnCommitQueue(dbconn, true, notifyAndExec);

    return ok;
}

bool SecDbTransaction(SecDbConnectionRef dbconn, SecDbTransactionType type,
                      CFErrorRef *error, void (^transaction)(bool *commit))
{
    bool ok = true;
    bool commit = true;

    if (dbconn->inTransaction) {
        transaction(&commit);
        if (!commit) {
            secinfo("#SecDB", "#SecDB nested transaction asked to not be committed");
        }
    } else {
        ok = SecDbBeginTransaction(dbconn, type, error);
        if (ok) {
            transaction(&commit);
            ok = SecDbEndTransaction(dbconn, commit, error);
        }
    }

    return ok && commit;
}

sqlite3 *SecDbHandle(SecDbConnectionRef dbconn) {
    return dbconn->handle;
}

bool SecDbStep(SecDbConnectionRef dbconn, sqlite3_stmt *stmt, CFErrorRef *error, void (^row)(bool *stop)) {
    for (;;) {
        switch (_SecDbStep(dbconn, stmt, error)) {
            case kSecDbErrorStep:
                secdebug("db", "kSecDbErrorStep %@", error?*error:NULL);
                return false;
            case kSecDbRowStep:
                secdebug("db", "kSecDbRowStep %@", error?*error:NULL);
                if (row) {
                    bool stop = false;
                    row(&stop);
                    if (stop)
                        return true;
                    break;
                }
                SecDbError(SQLITE_ERROR, error, CFSTR("SecDbStep SQLITE_ROW returned without a row handler"));
                return false;
            case kSecDbDoneStep:
                secdebug("db", "kSecDbDoneStep %@", error?*error:NULL);
                return true;
        }
    }
}

bool SecDbCheckpoint(SecDbConnectionRef dbconn, CFErrorRef *error)
{
    return SecDbConnectionCheckCode(dbconn, sqlite3_wal_checkpoint(dbconn->handle, NULL), error, CFSTR("wal_checkpoint"));
}

static bool SecDbFileControl(SecDbConnectionRef dbconn, int op, void *arg, CFErrorRef *error) {
    return SecDbConnectionCheckCode(dbconn, sqlite3_file_control(dbconn->handle, NULL, op, arg), error, CFSTR("file_control"));
}

static sqlite3 *_SecDbOpenV2(const char *path, int flags, CFErrorRef *error) {
#if HAVE_UNLOCK_NOTIFY
    flags |= SQLITE_OPEN_SHAREDCACHE;
#endif
    sqlite3 *handle = NULL;
    int s3e = sqlite3_open_v2(path, &handle, flags, NULL);
    if (s3e) {
        if (handle) {
            SecDbErrorWithDb(s3e, handle, error, CFSTR("open_v2 \"%s\" 0x%X"), path, flags);
            sqlite3_close(handle);
            handle = NULL;
        } else {
            SecDbError(s3e, error, CFSTR("open_v2 \"%s\" 0x%X"), path, flags);
        }
    }
    return handle;
}

static bool SecDbOpenV2(SecDbConnectionRef dbconn, const char *path, int flags, CFErrorRef *error) {
    return (dbconn->handle = _SecDbOpenV2(path, flags, error)) != NULL;
}

static bool SecDbTruncate(SecDbConnectionRef dbconn, CFErrorRef *error)
{
    int flags = SQLITE_TRUNCATE_AUTOVACUUM_FULL;
    if (dbconn->db->useWAL) {
        flags |= SQLITE_TRUNCATE_JOURNALMODE_WAL;
    }
    __block bool ok = SecDbFileControl(dbconn, SQLITE_TRUNCATE_DATABASE, &flags, error);
    if (!ok) {
        sqlite3_close(dbconn->handle);
        dbconn->handle = NULL;
        CFStringPerformWithCString(dbconn->db->db_path, ^(const char *path) {
            if (error)
                CFReleaseNull(*error);
            if (SecCheckErrno(unlink(path), error, CFSTR("unlink %s"), path)) {
                ok = SecDbOpenHandle(dbconn, NULL, error);
            }
        });
        if (!ok) {
            secinfo("#SecDB", "#SecDB Failed to delete db handle: %{public}@", error ? *error : NULL);
            abort();
        }
    }

    return ok;
}

static bool SecDbHandleCorrupt(SecDbConnectionRef dbconn, int rc, CFErrorRef *error)
{
    if (!dbconn->db->allowRepair) {
        SecCFCreateErrorWithFormat(rc, kSecErrnoDomain, NULL, error, NULL,
                                   CFSTR("SecDbHandleCorrupt repair not allowed: [%d] %s"), rc, strerror(rc));
        return false;
    }

    // Backup current db.
    __block bool didRename = false;
    CFStringPerformWithCString(dbconn->db->db_path, ^(const char *db_path) {
        sqlite3 *corrupt_db = NULL;
        char buf[PATH_MAX+1];
        snprintf(buf, sizeof(buf), "%s-corrupt", db_path);
        if (dbconn->handle && (corrupt_db = _SecDbOpenV2(buf, SQLITE_OPEN_READWRITE, error))) {
            int on = 1;
            didRename =  SecDbErrorWithDb(sqlite3_file_control(corrupt_db, NULL, SQLITE_FCNTL_PERSIST_WAL, &on), corrupt_db, error, CFSTR("persist wal"));
            didRename &=  SecDbErrorWithDb(sqlite3_file_control(corrupt_db, NULL, SQLITE_REPLACE_DATABASE, (void *)dbconn->handle), corrupt_db, error, CFSTR("replace database"));
            sqlite3_close(corrupt_db);
        }
        if (!didRename) {
            if (dbconn->handle)
                secerror("Tried to rename corrupt database at path %@, but we failed: %@, trying explicit rename", dbconn->db->db_path, error ? *error : NULL);
            if (error)
                CFReleaseNull(*error);

            didRename = (!dbconn->handle || SecDbError(sqlite3_close(dbconn->handle), error, CFSTR("close"))) &&
                SecCheckErrno(rename(db_path, buf), error, CFSTR("rename %s %s"), db_path, buf) &&
                SecDbOpenHandle(dbconn, NULL, error);
        }
        if (didRename) {
            secerror("Database at path %@ is corrupt. Copied it to %s for further investigation.", dbconn->db->db_path, buf);
        } else {
            seccritical("Tried to copy corrupt database at path %@, but we failed: %@", dbconn->db->db_path, error ? *error : NULL);
        }
    });

    bool ok = (didRename &&
               (dbconn->handle || SecDbOpenHandle(dbconn, NULL, error)) &&
               SecDbTruncate(dbconn, error));

    // Mark the db as not corrupted, even if something failed.
    // Always note we are no longer in the corruption handler
    dbconn->isCorrupted = false;

    // Invoke our callers opened callback, since we just created a new database
    if (ok && dbconn->db->opened) {
        dbconn->db->callOpenedHandlerForNextConnection = false;
        ok = dbconn->db->opened(dbconn, true, &dbconn->db->callOpenedHandlerForNextConnection, error);
    }

    return ok;
}

static bool SecDbLoggingEnabled(CFStringRef type)
{
    CFTypeRef profile = NULL;
    bool enabled = false;

    if (csr_check(CSR_ALLOW_APPLE_INTERNAL) != 0)
        return false;

    profile = (CFNumberRef)CFPreferencesCopyValue(CFSTR("SQLProfile"), CFSTR("com.apple.security"), kCFPreferencesAnyUser, kCFPreferencesAnyHost);

    if (profile == NULL)
        return false;

    if (CFGetTypeID(profile) == CFBooleanGetTypeID()) {
        enabled = CFBooleanGetValue((CFBooleanRef)profile);
    } else if (CFGetTypeID(profile) == CFNumberGetTypeID()) {
        int32_t num = 0;
        CFNumberGetValue(profile, kCFNumberSInt32Type, &num);
        enabled = !!num;
    }

    CFReleaseSafe(profile);

    return enabled;
}

static unsigned
SecDbProfileMask(void)
{
    static dispatch_once_t onceToken;
    static unsigned profile_mask = 0;

    // sudo defaults write /Library/Preferences/com.apple.security SQLProfile -bool true
    dispatch_once(&onceToken, ^{
        if (SecDbLoggingEnabled(CFSTR("SQLProfile")))
            profile_mask = SQLITE_TRACE_PROFILE;
#if DEBUG
        profile_mask |= SQLITE_TRACE_STMT;
#else
        if (SecDbLoggingEnabled(CFSTR("SQLTrace")))
            profile_mask = SQLITE_TRACE_STMT;
#endif
        if (SecDbLoggingEnabled(CFSTR("SQLRow")))
            profile_mask = SQLITE_TRACE_ROW;
        secinfo("#SecDB", "sqlDb: sql trace mask: 0x%08x", profile_mask);
    });
    return profile_mask;
}

static int
SecDbTraceV2(unsigned mask, void *ctx, void *p, void *x) {
    SecDbConnectionRef dbconn __unused = ctx;
    const char *trace = "unknown";

    if (mask == SQLITE_TRACE_PROFILE)
        trace = "profile";
    else if (mask == SQLITE_TRACE_STMT)
        trace = "stmt";
    else if (mask == SQLITE_TRACE_ROW)
        trace = "row";

    secinfo("#SecDB", "#SecDB %{public}s", trace);
    return 0;
}


static bool SecDbOpenHandle(SecDbConnectionRef dbconn, bool *created, CFErrorRef *error)
{
    __block bool ok = true;

    CFStringPerformWithCString(dbconn->db->db_path, ^(const char *db_path) {
        int flags = (dbconn->db->readWrite) ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY;
        ok = created && SecDbOpenV2(dbconn, db_path, flags, NULL);
        if (!ok) {
            ok = true;
            if (created) {
                char *tmp = dirname((char *)db_path);
                if (tmp) {
                    mode_t omode = dbconn->db->mode;
                    if (omode & S_IRUSR) { omode |= S_IXUSR; } // owner can read
                    if (omode & S_IRGRP) { omode |= S_IXGRP; } // group can read
                    if (omode & S_IROTH) { omode |= S_IXOTH; } // other can read
                    int errnum = mkpath_np(tmp, omode);
                    if (errnum != 0 && errnum != EEXIST) {
                        SecCFCreateErrorWithFormat(errnum, kSecErrnoDomain, NULL, error, NULL,
                                                   CFSTR("mkpath_np %s: [%d] %s"), tmp, errnum, strerror(errnum));
                        ok = false;
                    }
                }
            }
            // if the enclosing directory is ok, try to create the database.
            // this forces us to open it read-write, so we'll need to be the owner here.
            ok = ok && SecDbOpenV2(dbconn, db_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, error);
            if (ok) {
                chmod(db_path, dbconn->db->mode); // default: 0600 (S_IRUSR | S_IWUSR)
                if (created)
                    *created = true;
            }
        }

        if (ok) {
            unsigned mask = SecDbProfileMask();
            if (mask) {
                (void)sqlite3_trace_v2(dbconn->handle,
                                       mask,
                                       SecDbTraceV2,
                                       dbconn);
            }
        }

        ok = ok && SecDbBusyHandler(dbconn, error);
    });

    return ok;
}

static SecDbConnectionRef
SecDbConnectionCreate(SecDbRef db, bool readOnly, CFErrorRef *error)
{
    SecDbConnectionRef dbconn = NULL;

    dbconn = CFTypeAllocate(SecDbConnection, struct __OpaqueSecDbConnection, kCFAllocatorDefault);
    require(dbconn != NULL, done);

    dbconn->db = db;
    dbconn->readOnly = readOnly;
    dbconn->inTransaction = false;
    dbconn->source = kSecDbInvalidTransaction;
    dbconn->isCorrupted = false;
    dbconn->maybeCorruptedCode = 0;
    dbconn->hasIOFailure = false;
    dbconn->corruptionError = NULL;
    dbconn->handle = NULL;
    dbconn->changes = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

done:
    return dbconn;
}

static bool SecDbConnectionIsReadOnly(SecDbConnectionRef dbconn) {
    return dbconn->readOnly;
}

static void SecDbConectionSetReadOnly(SecDbConnectionRef dbconn, bool readOnly) {
    dbconn->readOnly = readOnly;
}

/* Read only connections go to the end of the queue, writeable connections
 go to the start of the queue. */
SecDbConnectionRef SecDbConnectionAcquire(SecDbRef db, bool readOnly, CFErrorRef *error) {
    CFRetain(db);
    secinfo("dbconn", "acquire %s connection", readOnly ? "ro" : "rw");
    dispatch_semaphore_wait(readOnly ? db->read_semaphore : db->write_semaphore, DISPATCH_TIME_FOREVER);
    __block SecDbConnectionRef dbconn = NULL;
    __block bool ok = true;
    __block bool ranOpenedHandler = false;
    dispatch_sync(db->queue, ^{
        if (!db->didFirstOpen) {
            bool didCreate = false;
            ok = dbconn = SecDbConnectionCreate(db, false, error);
            CFErrorRef localError = NULL;
            if (ok && !SecDbOpenHandle(dbconn, &didCreate, &localError)) {
                secerror("Unable to create database: %@", localError);
                if (localError && CFEqual(CFErrorGetDomain(localError), kSecDbErrorDomain)) {
                    int code = (int)CFErrorGetCode(localError);
                    dbconn->isCorrupted = (SQLITE_CORRUPT == code) || (SQLITE_NOTADB == code);
                }
                // If the open failure isn't due to corruption, propagate the error.
                ok = dbconn->isCorrupted;
                if (!ok && error && *error == NULL) {
                    *error = localError;
                    localError = NULL;
                }
            }
            CFReleaseNull(localError);

            if (ok) {
                db->didFirstOpen = ok = SecDbDidCreateFirstConnection(dbconn, didCreate, error);
                ranOpenedHandler = true;
            }
            if (!ok)
                CFReleaseNull(dbconn);
        } else {
            /* Try to get one from the cache */
            CFIndex count = CFArrayGetCount(db->connections);
            while (count && !dbconn) {
                CFIndex ix = readOnly ? count - 1 : 0;
                dbconn = (SecDbConnectionRef)CFArrayGetValueAtIndex(db->connections, ix);
                if (dbconn)
                    CFRetain(dbconn);
                else
                    secerror("got NULL dbconn at index: %" PRIdCFIndex " skipping", ix);
                CFArrayRemoveValueAtIndex(db->connections, ix);
            }
        }
    });

    if (dbconn) {
        /* Make sure the connection we found has the right access */
        if (SecDbConnectionIsReadOnly(dbconn) != readOnly) {
            SecDbConectionSetReadOnly(dbconn, readOnly);
        }
    } else if (ok) {
        /* Nothing found in cache, create a new connection */
        bool created = false;
        dbconn = SecDbConnectionCreate(db, readOnly, error);
        if (dbconn && !SecDbOpenHandle(dbconn, &created, error)) {
            CFReleaseNull(dbconn);
        }
    }

    if (dbconn && !ranOpenedHandler && dbconn->db->opened) {
        dispatch_sync(db->queue, ^{
            if (dbconn->db->callOpenedHandlerForNextConnection) {
                dbconn->db->callOpenedHandlerForNextConnection = false;
                if (!dbconn->db->opened(dbconn, false, &dbconn->db->callOpenedHandlerForNextConnection, error)) {
                    if (!dbconn->isCorrupted || !SecDbHandleCorrupt(dbconn, 0, error)) {
                        CFReleaseNull(dbconn);
                    }
                }
            }
        });
    }

    if (!dbconn) {
        // If acquire fails we need to signal the semaphore again.
        dispatch_semaphore_signal(readOnly ? db->read_semaphore : db->write_semaphore);
        CFRelease(db);
    }

    return dbconn;
}

void SecDbConnectionRelease(SecDbConnectionRef dbconn) {
    if (!dbconn) {
        secerror("called with NULL dbconn");
        return;
    }
    SecDbRef db = dbconn->db;
    secinfo("dbconn", "release %@", dbconn);
    dispatch_sync(db->queue, ^{
        bool readOnly = SecDbConnectionIsReadOnly(dbconn);
        if (dbconn->hasIOFailure) {
            // Something wrong on the file layer (e.g. revoked file descriptor for networked home)
            // so we don't trust our existing connections anymore.
            CFArrayRemoveAllValues(db->connections);
        } else {
            CFIndex count = CFArrayGetCount(db->connections);
            // Add back possible writable dbconn to the pool.
            CFArrayInsertValueAtIndex(db->connections, readOnly ? count : 0, dbconn);
            // Remove the last (probably read-only) dbconn from the pool.
            if (count >= kSecDbMaxIdleHandles) {
                CFArrayRemoveValueAtIndex(db->connections, count);
            }
        }
        // Signal after we have put the connection back in the pool of connections
        dispatch_semaphore_signal(readOnly ? db->read_semaphore : db->write_semaphore);
        CFRelease(dbconn);
        CFRelease(db);
    });
}

bool SecDbPerformRead(SecDbRef db, CFErrorRef *error, void (^perform)(SecDbConnectionRef dbconn)) {
    SecDbConnectionRef dbconn = SecDbConnectionAcquire(db, true, error);
    bool success = false;
    if (dbconn) {
        perform(dbconn);
        success = true;
        SecDbConnectionRelease(dbconn);
    }
    return success;
}

bool SecDbPerformWrite(SecDbRef db, CFErrorRef *error, void (^perform)(SecDbConnectionRef dbconn)) {
    if(!db) {
        SecError(errSecNotAvailable, error, CFSTR("failed to get a db handle"));
        return false;
    }
    SecDbConnectionRef dbconn = SecDbConnectionAcquire(db, false, error);
    bool success = false;
    if (dbconn) {
        perform(dbconn);
        success = true;
        SecDbConnectionRelease(dbconn);
    }
    return success;
}

static CFStringRef
SecDbConnectionCopyFormatDescription(CFTypeRef value, CFDictionaryRef formatOptions)
{
    SecDbConnectionRef dbconn = (SecDbConnectionRef)value;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<SecDbConnection %s %s>"),
                                    dbconn->readOnly ? "ro" : "rw", dbconn->handle ? "open" : "closed");
}

static void
SecDbConnectionDestroy(CFTypeRef value)
{
    SecDbConnectionRef dbconn = (SecDbConnectionRef)value;
    if (dbconn->handle) {
        sqlite3_close(dbconn->handle);
    }
    dbconn->db = NULL;
    CFReleaseNull(dbconn->changes);
    CFReleaseNull(dbconn->corruptionError);

}

void SecDbPerformOnCommitQueue(SecDbConnectionRef dbconn, bool barrier, dispatch_block_t perform) {
    if (barrier) {
        dispatch_barrier_sync(dbconn->db->commitQueue, ^{
            perform();
        });
    } else {
        dispatch_sync(dbconn->db->commitQueue, ^{
            perform();
        });
    }
}

// MARK: -
// MARK: Bind helpers

#if 0
bool SecDbBindNull(sqlite3_stmt *stmt, int param, CFErrorRef *error) {
    bool ok = SecDbErrorWithStmt(sqlite3_bind_null(stmt, param),
                                 stmt, error, CFSTR("bind_null[%d]"), param);
    secinfo("bind", "bind_null[%d]: %@", param, error ? *error : NULL);
    return ok;
}
#endif

bool SecDbBindBlob(sqlite3_stmt *stmt, int param, const void *zData, size_t n, void(*xDel)(void*), CFErrorRef *error) {
    if (n > INT_MAX) {
        return SecDbErrorWithStmt(SQLITE_TOOBIG, stmt, error,
                                  CFSTR("bind_blob[%d]: blob bigger than INT_MAX"), param);
    }
    bool ok = SecDbErrorWithStmt(sqlite3_bind_blob(stmt, param, zData, (int)n, xDel),
                                 stmt, error, CFSTR("bind_blob[%d]"), param);
    secinfo("bind", "bind_blob[%d]: %.*s: %@", param, (int)n, zData, error ? *error : NULL);
    return ok;
}

bool SecDbBindText(sqlite3_stmt *stmt, int param, const char *zData, size_t n, void(*xDel)(void*), CFErrorRef *error) {
    if (n > INT_MAX) {
        return SecDbErrorWithStmt(SQLITE_TOOBIG, stmt, error,
                                  CFSTR("bind_text[%d]: text bigger than INT_MAX"), param);
    }
    bool ok = SecDbErrorWithStmt(sqlite3_bind_text(stmt, param, zData, (int)n, xDel), stmt, error,
                                 CFSTR("bind_text[%d]"), param);
    secinfo("bind", "bind_text[%d]: \"%s\": %@", param, zData, error ? *error : NULL);
    return ok;
}

bool SecDbBindDouble(sqlite3_stmt *stmt, int param, double value, CFErrorRef *error) {
    bool ok = SecDbErrorWithStmt(sqlite3_bind_double(stmt, param, value), stmt, error,
                                 CFSTR("bind_double[%d]"), param);
    secinfo("bind", "bind_double[%d]: %f: %@", param, value, error ? *error : NULL);
    return ok;
}

bool SecDbBindInt(sqlite3_stmt *stmt, int param, int value, CFErrorRef *error) {
    bool ok = SecDbErrorWithStmt(sqlite3_bind_int(stmt, param, value), stmt, error,
                                 CFSTR("bind_int[%d]"), param);
    secinfo("bind", "bind_int[%d]: %d: %@", param, value, error ? *error : NULL);
    return ok;
}

bool SecDbBindInt64(sqlite3_stmt *stmt, int param, sqlite3_int64 value, CFErrorRef *error) {
    bool ok = SecDbErrorWithStmt(sqlite3_bind_int64(stmt, param, value), stmt, error,
                                 CFSTR("bind_int64[%d]"), param);
    secinfo("bind", "bind_int64[%d]: %lld: %@", param, value, error ? *error : NULL);
    return ok;
}


/* AUDIT[securityd](done):
 value (ok) is a caller provided, non NULL CFTypeRef.
 */
bool SecDbBindObject(sqlite3_stmt *stmt, int param, CFTypeRef value, CFErrorRef *error) {
    CFTypeID valueId;
    __block bool result = false;

	/* TODO: Can we use SQLITE_STATIC below everwhere we currently use
     SQLITE_TRANSIENT since we finalize the statement before the value
     goes out of scope? */
    if (!value || (valueId = CFGetTypeID(value)) == CFNullGetTypeID()) {
        /* Skip bindings for NULL values.  sqlite3 will interpret unbound
         params as NULL which is exactly what we want. */
#if 1
        result = true;
#else
        result = SecDbBindNull(stmt, param, error);
#endif
    } else if (valueId == CFStringGetTypeID()) {
        CFStringPerformWithCStringAndLength(value, ^(const char *cstr, size_t clen) {
            result = SecDbBindText(stmt, param, cstr, clen, SQLITE_TRANSIENT, error);
        });
    } else if (valueId == CFDataGetTypeID()) {
        CFIndex len = CFDataGetLength(value);
        if (len) {
            result = SecDbBindBlob(stmt, param, CFDataGetBytePtr(value),
                                   len, SQLITE_TRANSIENT, error);
        } else {
            result = SecDbBindText(stmt, param, "", 0, SQLITE_TRANSIENT, error);
        }
    } else if (valueId == CFDateGetTypeID()) {
        CFAbsoluteTime abs_time = CFDateGetAbsoluteTime(value);
        result = SecDbBindDouble(stmt, param, abs_time, error);
    } else if (valueId == CFBooleanGetTypeID()) {
        int bval = CFBooleanGetValue(value);
        result = SecDbBindInt(stmt, param, bval, error);
    } else if (valueId == CFNumberGetTypeID()) {
        Boolean convertOk;
        if (CFNumberIsFloatType(value)) {
            double nval;
            convertOk = CFNumberGetValue(value, kCFNumberDoubleType, &nval);
            result = SecDbBindDouble(stmt, param, nval, error);
        } else {
            SInt32 nval;
            convertOk = CFNumberGetValue(value, kCFNumberSInt32Type, &nval);
            if (convertOk) {
                result = SecDbBindInt(stmt, param, nval, error);
            } else {
                sqlite_int64 nval64;
                convertOk = CFNumberGetValue(value, kCFNumberSInt64Type, &nval64);
                if (convertOk)
                    result = SecDbBindInt64(stmt, param, nval64, error);
            }
        }
        if (!convertOk) {
            result = SecDbError(SQLITE_INTERNAL, error, CFSTR("bind CFNumberGetValue failed for %@"), value);
        }
    } else {
        if (error) {
            CFStringRef valueDesc = CFCopyTypeIDDescription(valueId);
            SecDbError(SQLITE_MISMATCH, error, CFSTR("bind unsupported type %@"), valueDesc);
            CFReleaseSafe(valueDesc);
        }
    }

	return result;
}

// MARK: -
// MARK: SecDbStatementRef

bool SecDbReset(sqlite3_stmt *stmt, CFErrorRef *error) {
    return SecDbErrorWithStmt(sqlite3_reset(stmt), stmt, error, CFSTR("reset"));
}

bool SecDbClearBindings(sqlite3_stmt *stmt, CFErrorRef *error) {
    return SecDbErrorWithStmt(sqlite3_clear_bindings(stmt), stmt, error, CFSTR("clear bindings"));
}

bool SecDbFinalize(sqlite3_stmt *stmt, CFErrorRef *error) {
    sqlite3 *handle = sqlite3_db_handle(stmt);
    int s3e = sqlite3_finalize(stmt);
    return s3e == SQLITE_OK ? true : SecDbErrorWithDb(s3e, handle, error, CFSTR("finalize: %p"), stmt);
}

sqlite3_stmt *SecDbPrepareV2(SecDbConnectionRef dbconn, const char *sql, size_t sqlLen, const char **sqlTail, CFErrorRef *error) {
    sqlite3 *db = SecDbHandle(dbconn);
    if (sqlLen > INT_MAX) {
        SecDbErrorWithDb(SQLITE_TOOBIG, db, error, CFSTR("prepare_v2: sql bigger than INT_MAX"));
        return NULL;
    }
    int ntries = 0;
    for (;;) {
        sqlite3_stmt *stmt = NULL;
        int s3e = sqlite3_prepare_v2(db, sql, (int)sqlLen, &stmt, sqlTail);
        if (s3e == SQLITE_OK)
            return stmt;
        else if (!SecDbWaitIfNeeded(dbconn, s3e, NULL, CFSTR("preparev2"), ntries, error))
            return NULL;
        ntries++;
    }
}

static sqlite3_stmt *SecDbCopyStatementWithTailRange(SecDbConnectionRef dbconn, CFStringRef sql, CFRange *sqlTail, CFErrorRef *error) {
    __block sqlite3_stmt *stmt = NULL;
    if (sql) CFStringPerformWithCStringAndLength(sql, ^(const char *sqlStr, size_t sqlLen) {
        const char *tail = NULL;
        stmt = SecDbPrepareV2(dbconn, sqlStr, sqlLen, &tail, error);
        if (sqlTail && sqlStr < tail && tail < sqlStr + sqlLen) {
            sqlTail->location = tail - sqlStr;
            sqlTail->length = sqlLen - sqlTail->location;
        }
    });

    return stmt;
}

sqlite3_stmt *SecDbCopyStmt(SecDbConnectionRef dbconn, CFStringRef sql, CFStringRef *tail, CFErrorRef *error) {
    // TODO: Add caching and cache lookup of statements
    CFRange sqlTail = {};
    sqlite3_stmt *stmt = SecDbCopyStatementWithTailRange(dbconn, sql, &sqlTail, error);
    if (sqlTail.length > 0) {
        CFStringRef excess = CFStringCreateWithSubstring(CFGetAllocator(sql), sql, sqlTail);
        if (tail) {
            *tail = excess;
        } else {
            SecDbError(SQLITE_INTERNAL, error,
                       CFSTR("prepare_v2: %@ unused sql: %@"),
                       sql, excess);
            CFReleaseSafe(excess);
            SecDbFinalize(stmt, error);
            stmt = NULL;
        }
    }
    return stmt;
}

/*
 TODO: Could do a hack here with a custom kCFAllocatorNULL allocator for a second CFRuntimeBase inside a SecDbStatement,
 TODO: Better yet make a full blow SecDbStatement instance whenever SecDbCopyStmt is called.  Then, when the statement is released, in the Dispose method, we Reset and ClearBindings the sqlite3_stmt * and hand it back to the SecDb with the original CFStringRef for the sql (or hash thereof) as an argument. */
bool SecDbReleaseCachedStmt(SecDbConnectionRef dbconn, CFStringRef sql, sqlite3_stmt *stmt, CFErrorRef *error) {
    if (stmt) {
        return SecDbFinalize(stmt, error);
    }
    return true;
}

bool SecDbPrepare(SecDbConnectionRef dbconn, CFStringRef sql, CFErrorRef *error, void(^exec)(sqlite3_stmt *stmt)) {
    assert(sql != NULL);
    sqlite3_stmt *stmt = SecDbCopyStmt(dbconn, sql, NULL, error);
    if (!stmt)
        return false;

    exec(stmt);
    return SecDbReleaseCachedStmt(dbconn, sql, stmt, error);
}

bool SecDbWithSQL(SecDbConnectionRef dbconn, CFStringRef sql, CFErrorRef *error, bool(^perform)(sqlite3_stmt *stmt)) {
    bool ok = true;
    CFRetain(sql);
    while (sql) {
        CFStringRef tail = NULL;
        if (ok) {
            sqlite3_stmt *stmt = SecDbCopyStmt(dbconn, sql, &tail, error);
            ok = stmt != NULL;
            if (stmt) {
                if (perform) {
                    ok = perform(stmt);
                } else {
                    // TODO: Use a different error scope here.
                    ok = SecError(-50 /* errSecParam */, error, CFSTR("SecDbWithSQL perform block missing"));
                }
                ok &= SecDbReleaseCachedStmt(dbconn, sql, stmt, error);
            }
        } else {
            // TODO We already have an error here we really just want the left over sql in it's userData
            ok = SecDbError(SQLITE_ERROR, error, CFSTR("Error with unexecuted sql remaining %@"), sql);
        }
        CFRelease(sql);
        sql = tail;
    }
    return ok;
}

#if 1
/* SecDbForEach returns true if all SQLITE_ROW returns of sqlite3_step() return true from the row block.
 If the row block returns false and doesn't set an error (to indicate it has reached a limit),
 this entire function returns false. In that case no error will be set. */
bool SecDbForEach(SecDbConnectionRef dbconn, sqlite3_stmt *stmt, CFErrorRef *error, bool(^row)(int row_index)) {
    bool result = false;
    for (int row_ix = 0;;++row_ix) {
        int s3e = sqlite3_step(stmt);
        if (s3e == SQLITE_ROW) {
            if (row) {
                if (!row(row_ix)) {
                    break;
                }
            } else {
                // If we have no row block then getting SQLITE_ROW is an error
                SecDbError(s3e, error,
                           CFSTR("step[%d]: %s returned SQLITE_ROW with NULL row block"),
                           row_ix, sqlite3_sql(stmt));
            }
        } else {
            if (s3e == SQLITE_DONE) {
                result = true;
            } else {
                dbconn->hasIOFailure |= (s3e == SQLITE_IOERR);
                SecDbErrorWithStmt(s3e, stmt, error, CFSTR("step[%d]"), row_ix);
            }
            break;
        }
    }
    return result;
}
#else
bool SecDbForEach(sqlite3_stmt *stmt, CFErrorRef *error, bool(^row)(int row_index)) {
    int row_ix = 0;
    for (;;) {
        switch (_SecDbStep(dbconn, stmt, error)) {
            case kSecDbErrorStep:
                return false;
            case kSecDbRowStep:
                if (row) {
                    if (row(row_ix++))
                        break;
                } else {
                    SecDbError(SQLITE_ERROR, error, CFSTR("SecDbStep SQLITE_ROW returned without a row handler"));
                }
                return false;
            case kSecDbDoneStep:
                return true;
        }
    }
}
#endif

void SecDbRecordChange(SecDbConnectionRef dbconn, CFTypeRef deleted, CFTypeRef inserted) {
    if (!dbconn->db->notifyPhase) return;
    CFTypeRef entry = SecDbEventCreateWithComponents(deleted, inserted);
    if (entry) {
        CFArrayAppendValue(dbconn->changes, entry);
        CFRelease(entry);

        if (!dbconn->inTransaction) {
            secerror("db %@ changed outside txn", dbconn);
            // Only notify of DidCommit, since WillCommit code assumes
            // we are in a txn.
            SecDbOnNotify(dbconn, ^{
                SecDbNotifyPhase(dbconn, kSecDbTransactionDidCommit);
            });
        }
    }
}


CFGiblisFor(SecDbConnection)

//
// SecDbEvent Creation and consumption
//

static SecDbEventRef SecDbEventCreateInsert(CFTypeRef inserted) {
    return CFRetainSafe(inserted);
}

static SecDbEventRef SecDbEventCreateDelete(CFTypeRef deleted) {
    return CFArrayCreate(kCFAllocatorDefault, &deleted, 1, &kCFTypeArrayCallBacks);
}

static SecDbEventRef SecDbEventCreateUpdate(CFTypeRef deleted, CFTypeRef inserted) {
    const void *values[2] = { deleted, inserted };
    return CFArrayCreate(kCFAllocatorDefault, values, 2, &kCFTypeArrayCallBacks);
}

SecDbEventRef SecDbEventCreateWithComponents(CFTypeRef deleted, CFTypeRef inserted) {
    if (deleted && inserted)
        return SecDbEventCreateUpdate(deleted, inserted);
    else if (deleted)
        return SecDbEventCreateDelete(deleted);
    else if (inserted)
        return SecDbEventCreateInsert(inserted);
    else
        return NULL;
}

bool SecDbEventGetComponents(SecDbEventRef event, CFTypeRef *deleted, CFTypeRef *inserted, CFErrorRef *error) {
    if (isArray(event)) {
        CFArrayRef array = event;
        switch (CFArrayGetCount(array)) {
            case 2:
                *deleted = CFArrayGetValueAtIndex(array, 0);
                *inserted = CFArrayGetValueAtIndex(array, 1);
                break;
            case 1:
                *deleted = CFArrayGetValueAtIndex(array, 0);
                *inserted = NULL;
                break;
            default:
                SecError(errSecParam, error, NULL, CFSTR("invalid entry in changes array: %@"), array);
                break;
        }
    } else {
        *deleted = NULL;
        *inserted = event;
    }
    return true;
}
