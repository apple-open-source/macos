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


#include <utilities/SecCFRelease.h>
#include <utilities/SecDb.h>

#include <CoreFoundation/CoreFoundation.h>

#include "utilities_regressions.h"
#include <time.h>

#define kTestCount 31

static int count_func(SecDbRef db, const char *name, CFIndex *max_conn_count, bool (*perform)(SecDbRef db, CFErrorRef *error, void (^perform)(SecDbConnectionRef dbconn))) {
    __block int count = 0;
    __block int max_count = 0;
    *max_conn_count = 0;
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_group_t group = dispatch_group_create();

    for (int i = 0; i < 100; ++i) {
        dispatch_group_async(group, queue, ^{
            CFIndex conn_count = SecDbIdleConnectionCount(db);
            if (conn_count > *max_conn_count) {
                *max_conn_count = conn_count;
            }

            CFErrorRef error = NULL;
            if (!perform(db, &error, ^void (SecDbConnectionRef dbconn) {
                count++;
                if (count > max_count) {
                    max_count = count;
                }
                struct timespec ts = { .tv_nsec = 200000 };
                nanosleep(&ts, NULL);
                count--;
            })) {
                fail("perform %s %@", name, error);
                CFReleaseNull(error);
            }
        });
    }
    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
    dispatch_release(group);
    return max_count;
}

static void count_connections(SecDbRef db) {
    __block CFIndex max_conn_count = 0;
    dispatch_group_t group = dispatch_group_create();
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_group_async(group, queue, ^{
        cmp_ok(count_func(db, "writers", &max_conn_count, SecDbPerformWrite), <=, kSecDbMaxWriters, "max writers is %d", kSecDbMaxWriters);
    TODO: {
        todo("can't guarantee all threads used");
        is(count_func(db, "writers", &max_conn_count, SecDbPerformWrite), kSecDbMaxWriters, "max writers is %d", kSecDbMaxWriters);
        }
    });
    dispatch_group_async(group, queue, ^{
        cmp_ok(count_func(db, "readers",  &max_conn_count, SecDbPerformRead), <=, kSecDbMaxReaders, "max readers is %d", kSecDbMaxReaders);
    TODO: {
        todo("can't guarantee all threads used");
        is(count_func(db, "readers",  &max_conn_count, SecDbPerformRead), kSecDbMaxReaders, "max readers is %d", kSecDbMaxReaders);
        }
    });
    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
    dispatch_release(group);
    cmp_ok(max_conn_count, <=, kSecDbMaxIdleHandles, "max idle connection count is %d", kSecDbMaxIdleHandles);
    TODO: {
        todo("can't guarantee all threads idle");
        is(max_conn_count, kSecDbMaxIdleHandles, "max idle connection count is %d", kSecDbMaxIdleHandles);
    }

}

static void tests(void)
{
    CFTypeID typeID = SecDbGetTypeID();
    CFStringRef tid = CFCopyTypeIDDescription(typeID);
    ok(CFEqual(CFSTR("SecDb"), tid), "tid matches");
    CFReleaseNull(tid);

    typeID = SecDbConnectionGetTypeID();
    tid = CFCopyTypeIDDescription(typeID);
    ok(CFEqual(CFSTR("SecDbConnection"), tid), "tid matches");
    CFReleaseNull(tid);

    const char *home_var = getenv("HOME");
    CFStringRef dbName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s/Library/Keychains/su-40-sqldb.db"), home_var ? home_var : "");

    SecDbRef db = SecDbCreate(dbName, NULL);
    CFReleaseNull(dbName);
    ok(db, "SecDbCreate");

    __block CFErrorRef error = NULL;
    ok(SecDbPerformWrite(db, &error, ^void (SecDbConnectionRef dbconn) {
        ok(SecDbExec(dbconn, CFSTR("CREATE TABLE tablea(key TEXT,value BLOB);"), &error),
           "exec: %@", error);
        ok(SecDbExec(dbconn, CFSTR("INSERT INTO tablea(key,value)VALUES(1,2);"), &error),
           "exec: %@", error);

        CFStringRef sql = CFSTR("INSERT INTO tablea(key,value)VALUES(?,?);");
        ok(SecDbPrepare(dbconn, sql, &error, ^void (sqlite3_stmt *stmt) {
            ok_status(sqlite3_bind_text(stmt, 1, "key1", 4, NULL), "bind_text[1]");
            ok_status(sqlite3_bind_blob(stmt, 2, "value1", 6, NULL), "bind_blob[2]");
            ok(SecDbStep(dbconn, stmt, &error, NULL), "SecDbStep: %@", error);
            CFReleaseNull(error);
        }), "SecDbPrepare: %@", error);
        CFReleaseNull(error);

        sql = CFSTR("SELECT key,value FROM tablea;");
        ok(SecDbPrepare(dbconn, sql, &error, ^void (sqlite3_stmt *stmt) {
            ok(SecDbStep(dbconn, stmt, &error, ^(bool *stop) {
                const unsigned char *key = sqlite3_column_text(stmt, 1);
                pass("got a row key: %s", key);
                // A row happened, we're done
                *stop = true;
            }), "SecDbStep: %@", error);
            CFReleaseNull(error);
        }), "SecDbPrepare: %@", error);
        CFReleaseNull(error);

        ok(SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &error, ^(bool *commit) {
            ok(SecDbExec(dbconn, CFSTR("INSERT INTO tablea (key,value)VALUES(13,21);"), &error),
               "exec: %@", error);
            ok(SecDbExec(dbconn, CFSTR("INSERT INTO tablea (key,value)VALUES(2,5);"), &error),
               "exec: %@", error);
        }), "SecDbTransaction: %@", error);

        ok(SecDbPrepare(dbconn, sql, &error, ^void (sqlite3_stmt *stmt) {
            ok(SecDbStep(dbconn, stmt, &error, ^(bool *stop) {
                const unsigned char *key = sqlite3_column_text(stmt, 1);
                pass("got a row key: %s", key);
            }), "SecDbStep: %@", error);
            CFReleaseNull(error);
            sqlite3_reset(stmt);
            ok(SecDbStep(dbconn, stmt, &error, ^(bool *stop) {
                const unsigned char *key = sqlite3_column_text(stmt, 1);
                pass("got a row key: %s", key);
                *stop = true;
            }), "SecDbStep: %@", error);
            CFReleaseNull(error);

        }), "SecDbPrepare: %@", error);

        ok(SecDbExec(dbconn, CFSTR("DROP TABLE tablea;"), &error),
           "exec: %@", error);
    }), "SecDbPerformWrite: %@", error);
    CFReleaseNull(error);

    count_connections(db);

    CFReleaseNull(db);
}

int su_40_secdb(int argc, char *const *argv)
{
    plan_tests(kTestCount);
    tests();
    
    return 0;
}

#if 0
// The following still need tests.
ok(SecDbTransaction(dbconn, kSecDbNoneTransactionType, &error, ^bool {}), "");
ok(SecDbTransaction(dbconn, kSecDbImmediateTransactionType, &error, ^bool {}), "");
ok(SecDbTransaction(dbconn, kSecDbNormalTransactionType, &error, ^bool {}), "");
ok(SecDbPerformRead(SecDbRef db, CFErrorRef *error, void ^(SecDbConnectionRef dbconn){}), "");
SecDbCheckpoint(SecDbConnectionRef dbconn);
#endif
