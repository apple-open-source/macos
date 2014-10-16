/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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


// Test syncing between SecItemDataSource and SOSTestDataSource

#include <Regressions/SOSTestDevice.h>
#include <Regressions/SOSTestDataSource.h>
#include "secd_regressions.h"
#include "SecdTestKeychainUtilities.h"

#include <SecureObjectSync/SOSDigestVector.h>
#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSPeer.h>
#include <Security/SecBase64.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <corecrypto/ccsha2.h>
#include <securityd/SecItemServer.h>
#include <securityd/SecItemDataSource.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecIOFormat.h>
#include <utilities/SecFileLocations.h>

#include <AssertMacros.h>
#include <stdint.h>

static int kTestTestCount = 125;

static void nosha1(void) {
    __block int iteration = 0;
    __block CFErrorRef error = NULL;
    SOSTestDeviceListTestSync("nosha1", test_directive, test_reason, 0, true, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest) {
        iteration++;
        // Add 10 items in first 10 sync messages
        if (iteration <= 6) {
            CFStringRef account = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("item%d"), iteration);
            SOSTestDeviceAddGenericItem(source, account, CFSTR("nosha1"));
            CFReleaseSafe(account);
            // Corrupt the 4th item added
            if (iteration == 4) {
                ok(SecDbPerformWrite(source->db, &error, ^(SecDbConnectionRef dbconn) {
                    ok(SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &error, ^(bool *commit) {
                        ok(SecDbExec(dbconn, CFSTR("UPDATE genp SET sha1=X'0000000000000000000000000000000000000000' WHERE rowid=5;"), &error),
                           "Corrupting rowid 5 by zeroing sha1: %@", error);
                        CFReleaseNull(error);
                    }), "SecDbTransaction: %@", error);
                    CFReleaseNull(error);
                }), "SecDbPerformWrite: %@", error);
                CFReleaseNull(error);
                return true;
            }
            return true;
        }


        return false;
    }, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message) {
        return false;
    }, CFSTR("Bad"), CFSTR("Good"), NULL);
}

static void drop_item(void) {
    __block int iteration = 0;
    __block CFErrorRef error = NULL;
    SOSTestDeviceListTestSync("drop_item", test_directive, test_reason, 0, true, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest) {
        iteration++;
        // Add 10 items in first 10 sync messages
        if (iteration <= 6) {
            CFStringRef account = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("item%d"), iteration);
            SOSTestDeviceAddGenericItem(source, account, CFSTR("drop_item"));
            CFReleaseSafe(account);
            // Corrupt the 4th item added
            if (iteration == 4) {
                ok(SecDbPerformWrite(source->db, &error, ^(SecDbConnectionRef dbconn) {
                    ok(SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &error, ^(bool *commit) {
                        ok(SecDbExec(dbconn, CFSTR("DELETE FROM genp WHERE rowid=5;"), &error),
                           "Corrupting rowid 5 by deleting object: %@", error);
                        CFReleaseNull(error);
                    }), "SecDbTransaction: %@", error);
                    CFReleaseNull(error);
                }), "SecDbPerformWrite: %@", error);
                CFReleaseNull(error);
                return true;
            }
            return true;
        }


        return false;
    }, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message) {
        return false;
    }, CFSTR("Abegail"), CFSTR("Billy"), NULL);
}

static void drop_manifest(void) {
    __block int iteration = 0;
    SOSTestDeviceListTestSync("drop_manifest", test_directive, test_reason, 0, true, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest) {
        iteration++;
        // Add 5 items on Alice and 4 on Bob in first 9 sync messages
        if (iteration <= 9) {
            CFStringRef account = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("item%d"), iteration / 2);
            SOSTestDeviceAddGenericItem(source, account, CFSTR("drop_manifest"));
            CFReleaseSafe(account);
            // Corrupt the manifest after 4th item added
            if (iteration == 4) {
                SOSEngineRef engine = SOSDataSourceGetSharedEngine(source->ds, NULL);
                SOSManifestRef mf = SOSEngineCopyManifest(engine, NULL);
                struct SOSDigestVector empty = SOSDigestVectorInit;
                ok(SOSEngineUpdateLocalManifest(engine, kSOSDataSourceSOSTransaction,
                                                (struct SOSDigestVector *)SOSManifestGetDigestVector(mf), &empty, NULL),
                   "droped manifest from %@", source);
                CFReleaseNull(mf);
                return true;
            }
            return true;
        }

        return false;
    }, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message) {
        return false;
    }, CFSTR("Ann"), CFSTR("Ben"), NULL);
}

static void add_sha1(void) {
    TODO: {
        //todo("this never stops syncing");
        __block int iteration = 0;
        __block CFErrorRef error = NULL;
        SOSTestDeviceListTestSync("add_sha1", test_directive, test_reason, 0, true, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest) {
            iteration++;
            // Add 9 items in first 9 sync messages
            if (iteration <= 9) {
                CFStringRef account = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("item%d"), iteration);
                SOSTestDeviceAddGenericItem(source, account, CFSTR("add_sha1"));
                CFReleaseSafe(account);
                // Corrupt the manifest after 4th item added
                if (iteration == 4) {
                    ok(SecDbPerformWrite(source->db, &error, ^(SecDbConnectionRef dbconn) {
                        ok(SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, &error, ^(bool *commit) {
                            ok(SecDbExec(dbconn, CFSTR("UPDATE genp SET sha1=X'0000000000000000000000000000000000000000' WHERE rowid=5;"), &error),
                               "Corrupting rowid 5 by zeroing sha1: %@", error);
                            CFReleaseNull(error);
                        }), "SecDbTransaction: %@", error);
                        CFReleaseNull(error);
                    }), "SecDbPerformWrite: %@", error);
                    CFReleaseNull(error);

                    SOSEngineRef engine = SOSDataSourceGetSharedEngine(source->ds, NULL);
                    struct SOSDigestVector del = SOSDigestVectorInit;
                    struct SOSDigestVector add = SOSDigestVectorInit;
                    uint8_t zeroDigest[20] = {};
                    SOSDigestVectorAppend(&add, zeroDigest);
                    ok(SOSEngineUpdateLocalManifest(engine, kSOSDataSourceSOSTransaction,
                                                    &del, &add, NULL), "corrupting manifest");
                    return true;
                }
                return true;
            }

            return false;
        }, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message) {
            return false;
        }, CFSTR("Andy"), CFSTR("Bill"), NULL);
    }
}

static void change_sha1(void) {
TODO: {
    //todo("this never stops syncing");
    __block int iteration = 0;
    __block CFErrorRef error = NULL;
    SOSTestDeviceListTestSync("change_sha1", test_directive, test_reason, 0, true, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest) {
        iteration++;
        // Add 9 items in first 9 sync messages
        if (iteration <= 9) {
            CFStringRef account = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("item%d"), iteration);
            CFStringRef server = CFSTR("change_sha1");
            // Corrupt the manifest after 4th item added
            if (!SOSDataSourceWithAPI(source->ds, true, &error, ^(SOSTransactionRef txn, bool *commit) {
                SOSObjectRef object = SOSDataSourceCreateGenericItem(source->ds, account, server);
                ok(SOSDataSourceMergeObject(source->ds, txn, object, NULL, &error), "%@ added API object %@", SOSTestDeviceGetID(source), error ? (CFTypeRef)error : (CFTypeRef)CFSTR("ok"));
                if (iteration == 3) {
                    sqlite_int64 rowid = SecDbItemGetRowId((SecDbItemRef)object, NULL);
                    CFStringRef sql = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("UPDATE genp SET sha1=X'0000000000000000000000000000000000000000' WHERE rowid=%lld;"), rowid);
                    ok(SecDbExec((SecDbConnectionRef)txn, sql, &error),
                       "Corrupting rowid %lld by zeroing sha1: %@", rowid, error);
                    CFReleaseNull(sql);
                    SOSEngineRef engine = SOSDataSourceGetSharedEngine(source->ds, NULL);
                    struct SOSDigestVector del = SOSDigestVectorInit;
                    struct SOSDigestVector add = SOSDigestVectorInit;
                    uint8_t zeroDigest[20] = {};
                    SOSDigestVectorAppend(&add, zeroDigest);
                    CFDataRef digest = SOSObjectCopyDigest(source->ds, object, NULL);
                    const uint8_t *d = CFDataGetBytePtr(digest);
                    SOSDigestVectorAppend(&del, d);
                    ok(SOSEngineUpdateLocalManifest(engine, kSOSDataSourceSOSTransaction,
                                                    &del, &add, NULL), "corrupting manifest %lld %02X%02x%02x%02x",
                       rowid, d[0], d[1], d[2], d[3]);
                    CFReleaseSafe(digest);
                }
                CFReleaseSafe(object);
                CFReleaseNull(error);
            }))
                fail("ds transaction %@", error);
            CFReleaseNull(error);
            CFReleaseNull(account);
            return true;
        }
        return false;
    }, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message) {
        if (iteration >= 3)
            pass("%@", source);
        return false;
    }, CFSTR("Alice"), CFSTR("Bob"), NULL);
}
}

int secd_70_engine_corrupt(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    /* custom keychain dir */
    secd_test_setup_temp_keychain("secd_70_engine_corrupt", NULL);

    nosha1();
    drop_item();
    drop_manifest();
    add_sha1();
    change_sha1();

    return 0;
}
