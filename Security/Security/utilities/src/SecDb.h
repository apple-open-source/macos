/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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


#ifndef _UTILITIES_SECDB_H_
#define _UTILITIES_SECDB_H_

#include <CoreFoundation/CoreFoundation.h>
#include <sqlite3.h>

__BEGIN_DECLS

// MARK: SecDbRef and SecDbConnectionRef forward declarations
typedef struct __OpaqueSecDb *SecDbRef;
typedef struct __OpaqueSecDbConnection *SecDbConnectionRef;
typedef struct __OpaqueSecDbStatement *SecDbStatementRef;
struct SOSDigestVector;

// MARK: Configuration values, not used by clients directly.
// TODO: Move this section to a private header
enum {
    kSecDbMaxReaders = 4,
    kSecDbMaxWriters = 1,
    kSecDbMaxIdleHandles = 3,
};

// MARK: SecDbTransactionType
enum {
    kSecDbNoneTransactionType = 0,
    kSecDbImmediateTransactionType,
    kSecDbExclusiveTransactionType,
    kSecDbNormalTransactionType,
    kSecDbExclusiveRemoteTransactionType,
};
typedef CFOptionFlags SecDbTransactionType;

enum SecDbTransactionPhase {
    kSecDbTransactionDidRollback = 0,       // A transaction just got rolled back
    kSecDbTransactionWillCommit,        // A transaction is about to commit.
    kSecDbTransactionDidCommit,         // A transnaction sucessfully committed.
};
typedef CFOptionFlags SecDbTransactionPhase;

enum SecDbTransactionSource {
    kSecDbSOSTransaction,        // A remotely initated transaction.
    kSecDbAPITransaction,        // A user initated transaction.
};
typedef CFOptionFlags SecDbTransactionSource;

// MARK: --
// MARK: Error creation helpers.

// SQLITE3 errors are in this domain
extern CFStringRef kSecDbErrorDomain;

bool SecDbError(int sql_code, CFErrorRef *error, CFStringRef format, ...);
bool SecDbErrorWithDb(int sql_code, sqlite3 *db, CFErrorRef *error, CFStringRef format, ...);
bool SecDbErrorWithStmt(int sql_code, sqlite3_stmt *stmt, CFErrorRef *error, CFStringRef format, ...);

// MARK: mark -
// MARK: mark SecDbRef

typedef void (^SecDBNotifyBlock)(SecDbConnectionRef dbconn, SecDbTransactionPhase phase, SecDbTransactionSource source, struct SOSDigestVector *removals, struct SOSDigestVector *additions);

CFTypeID SecDbGetTypeID(void);

SecDbRef SecDbCreate(CFStringRef dbName, bool (^opened)(SecDbConnectionRef dbconn, bool did_create, CFErrorRef *error));

void SecDbSetNotifyPhaseBlock(SecDbRef db, dispatch_queue_t queue, SecDBNotifyBlock notifyPhase);

// Read only connections go to the end of the queue, writeable
// connections go to the start of the queue.  Use SecDbPerformRead() and SecDbPerformWrite() if you
// can to avoid leaks.
SecDbConnectionRef SecDbConnectionAquire(SecDbRef db, bool readOnly, CFErrorRef *error);
void SecDbConnectionRelease(SecDbConnectionRef dbconn);

// Perform a database read operation,
bool SecDbPerformRead(SecDbRef db, CFErrorRef *error, void (^perform)(SecDbConnectionRef dbconn));
bool SecDbPerformWrite(SecDbRef db, CFErrorRef *error, void (^perform)(SecDbConnectionRef dbconn));

// TODO: DEBUG only -> Private header
CFIndex SecDbIdleConnectionCount(SecDbRef db);

CFStringRef SecDbGetPath(SecDbRef db);

// MARK: -
// MARK: SecDbConectionRef

CFTypeID SecDbConnectionGetTypeID(void);

bool SecDbPrepare(SecDbConnectionRef dbconn, CFStringRef sql, CFErrorRef *error, void(^exec)(sqlite3_stmt *stmt));

bool SecDbStep(SecDbConnectionRef dbconn, sqlite3_stmt *stmt, CFErrorRef *error, void (^row)(bool *stop));

bool SecDbExec(SecDbConnectionRef dbconn, CFStringRef sql, CFErrorRef *error);

bool SecDbCheckpoint(SecDbConnectionRef dbconn, CFErrorRef *error);

bool SecDbTransaction(SecDbConnectionRef dbconn, SecDbTransactionType ttype, CFErrorRef *error,
                      void (^transaction)(bool *commit));

sqlite3 *SecDbHandle(SecDbConnectionRef dbconn);

// Do not call this unless you are SecDbItem!
void SecDbRecordChange(SecDbConnectionRef dbconn, CFDataRef deleted, CFDataRef inserted);

// MARK: -
// MARK: Bind helpers

#if 0
bool SecDbBindNull(sqlite3_stmt *stmt, int param, CFErrorRef *error);
#endif
bool SecDbBindBlob(sqlite3_stmt *stmt, int param, const void *zData, size_t n, void(*xDel)(void*), CFErrorRef *error);
bool SecDbBindText(sqlite3_stmt *stmt, int param, const char *zData, size_t n, void(*xDel)(void*), CFErrorRef *error);
bool SecDbBindDouble(sqlite3_stmt *stmt, int param, double value, CFErrorRef *error);
bool SecDbBindInt(sqlite3_stmt *stmt, int param, int value, CFErrorRef *error);
bool SecDbBindInt64(sqlite3_stmt *stmt, int param, sqlite3_int64 value, CFErrorRef *error);
bool SecDbBindObject(sqlite3_stmt *stmt, int param, CFTypeRef value, CFErrorRef *error);

// MARK: -
// MARK: SecDbStatementRef

bool SecDbReset(sqlite3_stmt *stmt, CFErrorRef *error);
bool SecDbClearBindings(sqlite3_stmt *stmt, CFErrorRef *error);
bool SecDbFinalize(sqlite3_stmt *stmt, CFErrorRef *error);
sqlite3_stmt *SecDbPrepareV2(SecDbConnectionRef dbconn, const char *sql, size_t sqlLen, const char **sqlTail, CFErrorRef *error);
sqlite3_stmt *SecDbCopyStmt(SecDbConnectionRef dbconn, CFStringRef sql, CFStringRef *tail, CFErrorRef *error);
bool SecDbReleaseCachedStmt(SecDbConnectionRef dbconn, CFStringRef sql, sqlite3_stmt *stmt, CFErrorRef *error);
bool SecDbWithSQL(SecDbConnectionRef dbconn, CFStringRef sql, CFErrorRef *error, bool(^perform)(sqlite3_stmt *stmt));
bool SecDbForEach(sqlite3_stmt *stmt, CFErrorRef *error, bool(^row)(int row_index));

// Mark the database as corrupted.
void SecDbCorrupt(SecDbConnectionRef dbconn);

__END_DECLS

#endif /* !_UTILITIES_SECDB_H_ */
