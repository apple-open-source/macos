//
//  su-41-secdb-stress.c
//  utilities
//
//  Created by Michael Brouwer on 7/25/13.
//  Copyright (c) 2013 Apple Inc. All rights reserved.
//

#include <utilities/SecCFWrappers.h>
#include <utilities/SecDb.h>
#include <utilities/SecDispatchRelease.h>

#include <CoreFoundation/CoreFoundation.h>

#include "utilities_regressions.h"
#include <time.h>

#define kTestCount 3415

// Queue to protect counters and test_ok invocations
static dispatch_queue_t count_queue;

#define ts_ok(THIS, ...) \
({ \
    bool is_ok = !!(THIS); \
    dispatch_sync(count_queue, ^{ \
        test_ok(is_ok, test_create_description(__VA_ARGS__), test_directive, \
            test_reason, __FILE__, __LINE__, NULL); \
    }); \
    is_ok; \
})

#define ts_ok_status(THIS, ...) \
({ \
    OSStatus _this = (THIS); \
    __block bool is_ok; \
    dispatch_sync(count_queue, ^{ \
        is_ok = test_ok(!_this, test_create_description(__VA_ARGS__), \
            test_directive, test_reason, __FILE__, __LINE__, \
            "#     status: %s(%ld)\n", \
            sec_errstr(_this), _this); \
    }); \
    is_ok; \
})


typedef void (^SecDbBlock)(SecDbConnectionRef dbconn);

#define SecDbExecWithSql(dbconn, sql)  test_SecDbExecWithSql(dbconn, sql, test_directive, test_reason, __FILE__, __LINE__)

static void test_SecDbExecWithSql(SecDbConnectionRef dbconn, CFStringRef sql CF_CONSUMED, const char *directive,
                                        const char *reason, const char *file, unsigned line) {
    CFErrorRef execError = NULL;
    bool is_ok = !!(SecDbExec(dbconn, sql, &execError));
    dispatch_sync(count_queue, ^{
        test_ok(is_ok, test_create_description("exec %@: %@", sql, execError), directive, reason, file, line, NULL);
    });
    CFReleaseNull(execError);
    CFReleaseSafe(sql);
}

#define SecDbDeleteWithInts(dbconn, key, value)  test_SecDbDeleteWithInts(dbconn, key, value, test_directive, test_reason, __FILE__, __LINE__)
static void test_SecDbDeleteWithInts(SecDbConnectionRef dbconn, int key, int value, const char *directive,
                                                 const char *reason, const char *file, unsigned line) {
    test_SecDbExecWithSql(dbconn, CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                           CFSTR("DELETE FROM tablea WHERE key=%d AND value=%d;"), key, value), directive, reason, file, line);
}

static void SecDbDoReadOp(SecDbConnectionRef dbconn, size_t seed) {
    switch (seed % 2) {
        case 0:
        {
            CFErrorRef prepareError = NULL;
            CFStringRef sql = CFSTR("SELECT key,value FROM tablea;");
            ts_ok(SecDbPrepare(dbconn, sql, &prepareError, ^void (sqlite3_stmt *stmt) {
                CFErrorRef stepError = NULL;
                ts_ok(SecDbStep(dbconn, stmt, &stepError, ^(bool *stop) {
                    //const unsigned char *key = sqlite3_column_text(stmt, 1);
                    //pass("got a row key: %s", key);
                    // A row happened, we're done
                    *stop = true;
                }), "SecDbStep: %@", stepError);
                CFReleaseNull(stepError);
            }), "SecDbPrepare: %@", prepareError);
            CFReleaseNull(prepareError);
            break;
        }
        case 1:
        {
            CFErrorRef prepareError = NULL;
            CFStringRef sql = CFSTR("SELECT key,value FROM tablea;");
            ts_ok(SecDbPrepare(dbconn, sql, &prepareError, ^void (sqlite3_stmt *stmt) {
                CFErrorRef stepError = NULL;
                ts_ok(SecDbStep(dbconn, stmt, &stepError, ^(bool *stop) {
                    //const unsigned char *key = sqlite3_column_text(stmt, 1);
                    //pass("got a row key: %s", key);
                }), "SecDbStep: %@", stepError);
                CFReleaseNull(stepError);
                sqlite3_reset(stmt);
                ts_ok(SecDbStep(dbconn, stmt, &stepError, ^(bool *stop) {
                    //const unsigned char *key = sqlite3_column_text(stmt, 1);
                    //pass("got a row key: %s", key);
                    *stop = true;
                }), "SecDbStep: %@", stepError);
                CFReleaseNull(stepError);
            }), "SecDbPrepare: %@", prepareError);
            CFReleaseNull(prepareError);
            break;
        }
    }
}

static void SecDbDoWriteOp(SecDbConnectionRef dbconn, size_t seed) {
    switch (seed % 6) {
        case 0:
            SecDbExecWithSql(dbconn, CFSTR("INSERT INTO tablea(key,value)VALUES(1,2);"));
            break;
        case 1:
        {
            CFErrorRef txnError = NULL;
            ts_ok(SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &txnError, ^(bool *commit) {
                CFErrorRef execError = NULL;
                ts_ok(SecDbExec(dbconn, CFSTR("INSERT INTO tablea (key,value)VALUES(13,21);"), &execError),
                      "exec: %@", execError);
                CFReleaseNull(execError);
                ts_ok(SecDbExec(dbconn, CFSTR("INSERT INTO tablea (key,value)VALUES(2,5);"), &execError),
                      "exec: %@", execError);
                CFReleaseNull(execError);
            }), "SecDbTransaction: %@", txnError);
            CFReleaseNull(txnError);
            break;
        }
        case 2:
        {
            CFErrorRef prepareError = NULL;
            CFStringRef sql = CFSTR("INSERT INTO tablea(key,value)VALUES(?,?);");
            ts_ok(SecDbPrepare(dbconn, sql, &prepareError, ^void (sqlite3_stmt *stmt) {
                CFErrorRef stepError = NULL;
                ts_ok_status(sqlite3_bind_text(stmt, 1, "key1", 4, NULL), "bind_text[1]");
                ts_ok_status(sqlite3_bind_blob(stmt, 2, "value1", 6, NULL), "bind_blob[2]");
                ts_ok(SecDbStep(dbconn, stmt, &stepError, NULL), "SecDbStep: %@", stepError);
                CFReleaseNull(stepError);
            }), "SecDbPrepare: %@", prepareError);
            CFReleaseNull(prepareError);
            break;
        }
        case 3:
            SecDbDeleteWithInts(dbconn, 1, 2);
            break;
        case 4:
            SecDbDeleteWithInts(dbconn, 13, 21);
            break;
        case 5:
            SecDbDeleteWithInts(dbconn, 2, 5);
            break;
    }
}

static void tests(void)
{
    count_queue = dispatch_queue_create("count_queue", DISPATCH_QUEUE_SERIAL);

    CFTypeID typeID = SecDbGetTypeID();
    CFStringRef tid = CFCopyTypeIDDescription(typeID);
    ts_ok(CFEqual(CFSTR("SecDb"), tid), "TypeIdDescription is SecDb");
    CFReleaseNull(tid);

    typeID = SecDbConnectionGetTypeID();
    tid = CFCopyTypeIDDescription(typeID);
    ts_ok(CFEqual(CFSTR("SecDbConnection"), tid), "TypeIdDescription is SecDbConnection");
    CFReleaseNull(tid);

    const char *home_var = getenv("HOME");
    CFStringRef dbName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s/Library/Keychains/su-41-sqldb-stress.db"), home_var ? home_var : "");
    CFStringPerformWithCString(dbName, ^(const char *path) { unlink(path); });

    SecDbRef db = SecDbCreate(dbName, ^bool (SecDbConnectionRef dbconn, bool did_create, CFErrorRef *firstOpenError) {
        // This test will run when the database is first opened.
        return ts_ok(SecDbExec(dbconn, CFSTR("CREATE TABLE tablea(key TEXT,value BLOB);"), firstOpenError),
           "create table: %@", *firstOpenError);
    });
    ts_ok(db, "SecDbCreate");

    __block CFIndex max_idle = 0;
    __block CFIndex max_readers = 0;
    __block CFIndex max_writers = 0;
    __block CFIndex cur_readers = 0;
    __block CFIndex cur_writers = 0;

    dispatch_group_t group = dispatch_group_create();
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    for (size_t job=0; job < 1000; ++job) {
        dispatch_group_async(group, queue, ^{
            CFIndex cur_idle = SecDbIdleConnectionCount(db);
            dispatch_sync(count_queue, ^{ if (max_idle < cur_idle) max_idle = cur_idle; });
            CFErrorRef performError = NULL;
            if (job % 7 == 0) {
                ts_ok(SecDbPerformWrite(db, &performError, ^void (SecDbConnectionRef dbconn) {
                    dispatch_sync(count_queue, ^{ cur_writers++; if (max_writers < cur_writers) max_writers = cur_writers; });
                    SecDbDoWriteOp(dbconn, job);
                    dispatch_sync(count_queue, ^{ cur_writers--; });
                }), "write %@", performError);
            } else {
                CFErrorRef performError = NULL;
                ts_ok(SecDbPerformRead(db, &performError, ^void (SecDbConnectionRef dbconn) {
                    dispatch_sync(count_queue, ^{ cur_readers++; if (max_readers < cur_readers) max_readers = cur_readers; });
                    SecDbDoReadOp(dbconn, job);
                    dispatch_sync(count_queue, ^{ cur_readers--; });
                }), "read %@", performError);
            }
            CFReleaseNull(performError);
        });
    }
    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
    dispatch_release(group);

    CFErrorRef writeError = NULL;
    ts_ok(SecDbPerformWrite(db, &writeError, ^(SecDbConnectionRef dbconn){
        SecDbExecWithSql(dbconn, CFSTR("DROP TABLE tablea;"));
    }), "SecDbPerformWrite: %@", writeError);
    CFReleaseNull(writeError);

    dispatch_release_null(count_queue);

    is(max_idle, kSecDbMaxIdleHandles, "max idle connection count is %d", kSecDbMaxIdleHandles);
    is(max_writers, kSecDbMaxWriters, "max writers is %d", kSecDbMaxWriters);
    is(max_readers, kSecDbMaxReaders, "max readers is %d", kSecDbMaxReaders);

    CFReleaseNull(db);
}

int su_41_secdb_stress(int argc, char *const *argv)
{
    plan_tests(kTestCount);
    tests();

    return 0;
}
