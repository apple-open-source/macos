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

static int kTestTestCount = 1286;

__unused static bool SOSCircleHandleCircleWithLock(SOSEngineRef engine, CFStringRef myID, CFDataRef message, CFErrorRef *error) {

    CFMutableArrayRef trustedPeers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableArrayRef untrustedPeers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFStringRef peerID = NULL;
    const uint8_t expected[20] =  { 0xea, 0x6c, 0x01, 0x4d,
        0xc7, 0x2d, 0x6f, 0x8c,
        0xcd, 0x1e, 0xd9, 0x2a,
        0xce, 0x1d, 0x41, 0xf0,
        0xd8, 0xde, 0x89, 0x57 };

    const char resultSize = sizeof(expected);

    CFDataRef coder = CFDataCreate(kCFAllocatorDefault, expected, resultSize);
    CFArrayForEachC(SOSEngineGetPeerIDs(engine), peerID){
        CFArrayAppendValue(trustedPeers, peerID);
        SOSEngineSetCoderData(engine, peerID, coder, error);
    };
    CFReleaseNull(coder);

    CFShow(trustedPeers);
    // all trusted
    SOSEngineCircleChanged_locked(engine, myID,trustedPeers, untrustedPeers);

    // make first peer untrusted
    peerID = (CFStringRef)CFArrayGetValueAtIndex(trustedPeers, 0);
    CFArrayAppendValue(untrustedPeers, peerID);
    CFArrayRemoveAllValue(trustedPeers, peerID);
    //we should see peerState cleared out except for the coder!
    SOSEngineCircleChanged_locked(engine, myID, trustedPeers, untrustedPeers);

    CFArrayAppendValue(trustedPeers, peerID);
    CFArrayRemoveAllValue(untrustedPeers, peerID);


    return true;
}

static void testsync3(const char *name,  const char *test_directive, const char *test_reason) {
    __block int iteration=0;
    SOSTestDeviceListTestSync(name, test_directive, test_reason, 0, false, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest) {
        iteration++;
        if (iteration == 12 || iteration == 13) {
            pass("pre-rcv %@", dest);
        }
        if (iteration == 19) {
            pass("pre-send %@", source);
        }
        return false;
    }, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message) {
        if (iteration == 10) {
            pass("pre-add %@", source);
            //SOSTestDeviceAddGenericItem(source, CFSTR("test_account"), CFSTR("test service"));
            SOSTestDeviceAddRemoteGenericItem(source, CFSTR("test_account"), CFSTR("test service"));
            pass("post-add %@", source);
            return true; // db changed
        } else if (iteration == 12 || iteration == 15) {
            pass("post-rcv %@", dest);
        }
        return false;
    }, CFSTR("AAA"), CFSTR("BBB"), CFSTR("CCC"), NULL);
}

static void testsync2(const char *name,  const char *test_directive, const char *test_reason, void (^aliceInit)(SOSDataSourceRef ds), void (^bobInit)(SOSDataSourceRef ds), CFStringRef msg, ...) {
    __block int iteration=0;
    SOSTestDeviceListTestSync(name, test_directive, test_reason, kSOSPeerVersion, false, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest) {
        if (iteration == 96) {
            pass("%@ before message", source);
        }
        return false;
    }, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message) {
        iteration++;
        if (iteration == 60) {
            pass("%@ before addition", source);
            //SOSTestDeviceAddGenericItem(source, CFSTR("test_account"), CFSTR("test service"));
            SOSTestDeviceAddRemoteGenericItem(source, CFSTR("test_account"), CFSTR("test service"));
            pass("%@ after addition", source);
            return true;
        }
        return false;
    }, CFSTR("alice"), CFSTR("bob"), CFSTR("claire"), CFSTR("dave"),CFSTR("edward"), CFSTR("frank"), CFSTR("gary"), NULL);
}

static void testsync(const char *name,  const char *test_directive, const char *test_reason, void (^aliceInit)(SOSDataSourceRef ds), void (^bobInit)(SOSDataSourceRef ds), ...) {
    __block int msg_index = 0;
    __block int last_msg_index = 0;
    va_list args;
    va_start(args, bobInit);
    CFArrayRef messages = CFArrayCreateForVC(kCFAllocatorDefault, &kCFTypeArrayCallBacks, args);
    SOSTestDeviceListTestSync(name, test_directive, test_reason, 0, false,
                      ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest) {
                          if (msg_index == 0) {
                              aliceInit(source->ds);
                              bobInit(dest->ds);
                              return true;
                          }
                          return false;
                      }, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message) {
                          CFStringRef hexMsg = msg_index < CFArrayGetCount(messages) ? (CFStringRef)CFArrayGetValueAtIndex(messages, msg_index) : 0;
                          /* We are expecting a message and msg is it's digest. */
                          if (message) {
                              msg_index++;
                              CFStringRef messageDigestStr = SOSMessageCopyDigestHex(message);
                              if (hexMsg) {
                                  if (CFEqual(messageDigestStr, hexMsg)) {
                                      pass("%s %@ handled message [%d] %@", name, SOSEngineGetMyID(dest->ds->engine), msg_index, message);
                                  } else {
                                      fail("%s %@ received message [%d] digest %@ != %@ %@", name, SOSEngineGetMyID(dest->ds->engine), msg_index, messageDigestStr, hexMsg, message);
                                  }
                                  last_msg_index = msg_index;
                              } else {
                                  fail("%s %@ sent extra message [%d] with digest %@: %@", name, SOSEngineGetMyID(source->ds->engine), msg_index, messageDigestStr, message);
                              }
                              CFReleaseSafe(messageDigestStr);
                              //SOSCircleHandleCircleWithLock(source->ds->engine, SOSEngineGetMyID(source->ds->engine), CFDataCreate(kCFAllocatorDefault, 0, 0), NULL);

                          }
                          return false;
                      }, CFSTR("alice"), CFSTR("bob"), NULL);

    if (msg_index < CFArrayGetCount(messages)) {
        fail("%s nothing sent expecting message [%d] digest %@", name, msg_index, CFArrayGetValueAtIndex(messages, msg_index));
    } else if (last_msg_index < msg_index) {
        fail("%s exchanged %d messages not in expected list", name, msg_index - last_msg_index);
    }


}

#if 0
// Test syncing an empty circle with 1 to 10 devices and both version 0 and version 2 protocols
static void testsyncempty(void) {
    CFMutableArrayRef deviceIDs = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    for (int deviceIX=0; deviceIX < 10; ++deviceIX) {
        CFStringRef deviceID = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%c"), 'A' + deviceIX);
        CFArrayAppendValue(deviceIDs, deviceID);
        CFReleaseSafe(deviceID);
        if (deviceIX > 0) {
            for (CFIndex version = 0; version < 3; version += 2) {
                CFMutableDictionaryRef testDevices = SOSTestDeviceListCreate(false, version, deviceIDs);
                SOSTestDeviceListSync("syncempty", test_directive, test_reason, testDevices, NULL, NULL);
                SOSTestDeviceListInSync("syncempty", test_directive, test_reason, testDevices);
                CFReleaseSafe(testDevices);
            }
        }
    }
    CFReleaseSafe(deviceIDs);
}
#endif

static CFIndex syncmany_add(int iteration) {
    if (iteration % 7 < 3 && iteration < 10)
        return iteration % 17 + 200;
    return 0;
}

static void testsyncmany(const char *name, const char *test_directive, const char *test_reason, int devFirst, int devCount, int version, CFIndex (*should_add)(int iteration)) {
    CFMutableArrayRef deviceIDs = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    for (int deviceIX=0; deviceIX < devCount; ++deviceIX) {
        CFStringRef deviceID = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%c"), 'A' + deviceIX);
        CFArrayAppendValue(deviceIDs, deviceID);
        CFReleaseSafe(deviceID);
        if (deviceIX >= devFirst) {
            CFMutableDictionaryRef testDevices = SOSTestDeviceListCreate(false, version, deviceIDs);
            __block int iteration = 0;
            SOSTestDeviceListSync(name, test_directive, test_reason, testDevices, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest) {
                bool didAdd = false;
                iteration++;
                // Add 10 items in first 10 sync messages
                CFIndex toAdd = should_add(iteration);
                if (toAdd) {
                    CFStringRef account = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("item%d"), iteration);
                    didAdd = SOSTestDeviceAddGenericItems(source, toAdd, account, CFSTR("testsyncmany"));
                    CFReleaseSafe(account);
                }
                if (iteration == 279 || iteration == 459)
                    pass("pre-send[%d] %@", iteration, source);

                return didAdd;
            }, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message) {
                if (iteration == 262)
                    pass("post-rcv[%d] %@", iteration, dest);

                if (iteration == 272 || iteration == 279)
                    pass("post-send[%d] %@", iteration, source);

                return false;
            });
            SOSTestDeviceListInSync(name, test_directive, test_reason, testDevices);
            CFReleaseSafe(testDevices);
        }
    }
    CFReleaseSafe(deviceIDs);
}

static void testsync2p(void) {
    __block int iteration = 0;
    SOSTestDeviceListTestSync("testsync2p", test_directive, test_reason, 0, false, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest) {
        iteration++;
        // Add 10 items in first 10 sync messages
        if (iteration <= 10) {
            CFStringRef account = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("item%d"), iteration);
            SOSTestDeviceAddGenericItem(source, account, CFSTR("testsync2p"));
            CFReleaseSafe(account);
            return true;
        }
        return false;
    }, ^bool(SOSTestDeviceRef source, SOSTestDeviceRef dest, SOSMessageRef message) {
        return false;
    }, CFSTR("Atestsync2p"), CFSTR("Btestsync2p"), NULL);
}

static void synctests(void) {
#if 0
    // TODO: Adding items gives us non predictable creation and mod dates so
    // the message hashes can't be precomputed.
    CFDictionaryRef item = CFDictionaryCreateForCFTypes
    (0,
     kSecClass, kSecClassGenericPassword,
     kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked,
     kSecAttrSynchronizable, kCFBooleanTrue,
     kSecAttrService, CFSTR("service"),
     kSecAttrAccount, CFSTR("account"),
     NULL);
    SecItemAdd(item, NULL);
    CFReleaseSafe(item);
#endif

SKIP:
    {

#ifdef NO_SERVER
        // Careful with this in !NO_SERVER, it'll destroy debug keychains.
        WithPathInKeychainDirectory(CFSTR("keychain-2-debug.db"), ^(const char *keychain_path) {
            unlink(keychain_path);
        });

        // Don't ever do this in !NO_SERVER, it'll destroy real keychains.
        WithPathInKeychainDirectory(CFSTR("keychain-2.db"), ^(const char *keychain_path) {
            unlink(keychain_path);
        });

        void kc_dbhandle_reset(void);
        kc_dbhandle_reset();
#else
        skip("Keychain not reset", kTestTestCount, false);
#endif

        testsync3("secd_70_engine3", test_directive, test_reason);

        // Sync between 2 empty dataSources
        testsync("secd_70_engine", test_directive, test_reason,
                 ^ (SOSDataSourceRef dataSource) {},
                 ^ (SOSDataSourceRef dataSource) {},
                 CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
                 CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
                 CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
                 CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
                 NULL);

        // Sync a dataSource with one object to an empty dataSource
        testsync("secd_70_engine-alice1", test_directive, test_reason,
                 ^ (SOSDataSourceRef dataSource) {
                     __block CFErrorRef error = NULL;
                     SOSObjectRef object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
                     // TODO: Needs to be a SecDBItemRef for the SecItemDataSource...
                     ok(SOSDataSourceWith(dataSource, &error, ^(SOSTransactionRef txn, bool *commit) {
                         ok(SOSDataSourceMergeObject(dataSource, txn, object, NULL, &error), "dataSource %@ added object %@", SOSEngineGetMyID(dataSource->engine), error);
                     }), "ds transaction failed %@", error);
                     CFReleaseSafe(object);
                     CFReleaseNull(error);
                 },
                 ^ (SOSDataSourceRef dataSource) {},
                 CFSTR("DDDB2DCEB7B36F0757F400251ECD11E377A0DCE8"),
                 CFSTR("B2777CC898AE381B3F375B27E4FD9757F6CE9948"),
                 CFSTR("CB67BF9ECF00DC7664834DE7A2D7CC1523D25341"),
                 CFSTR("ADAA3ACE75ED516CB91893413EE9CC9ED04CA47B"),

                 //CFSTR("ADAA3ACE75ED516CB91893413EE9CC9ED04CA47B"),
                 //CFSTR("147B6C509908CC4A9FC4263973A842104A64CE01"),
                 //CFSTR("019B494F3C06B48BB02C280AF1E19AD861A7003C"),
                 //CFSTR("ADAA3ACE75ED516CB91893413EE9CC9ED04CA47B"),
                 NULL);

        // Sync a dataSource with one object to another dataSource with the same object
        testsync("secd_70_engine-alice1bob1", test_directive, test_reason,
                 ^ (SOSDataSourceRef dataSource) {
#if 0
                     CFErrorRef error = NULL;
                     // TODO: Needs to be a SecDBItemRef for the SecItemDataSource...
                     CFDictionaryRef object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
                     ok(SOSDataSourceMergeObject(dataSource, object, NULL, &error), "dataSource %@ added object %@", SOSEngineGetMyID(dataSource->engine), error);
                     CFReleaseSafe(object);
                     CFReleaseNull(error);
#endif
                 },
                 ^ (SOSDataSourceRef dataSource) {
                     __block CFErrorRef error = NULL;
                     SOSObjectRef object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
                     ok(SOSDataSourceWith(dataSource, &error, ^(SOSTransactionRef txn, bool *commit) {
                         ok(SOSDataSourceMergeObject(dataSource, txn, object, NULL, &error), "dataSource %@ added object %@", SOSEngineGetMyID(dataSource->engine), error);
                     }), "ds transaction failed %@", error);
                     CFReleaseSafe(object);
                     CFReleaseNull(error);
                 },
                 CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
                 CFSTR("CB67BF9ECF00DC7664834DE7A2D7CC1523D25341"),
                 CFSTR("ADAA3ACE75ED516CB91893413EE9CC9ED04CA47B"),

                 //CFSTR("ADAA3ACE75ED516CB91893413EE9CC9ED04CA47B"),
                 //CFSTR("ADAA3ACE75ED516CB91893413EE9CC9ED04CA47B"),
                 //CFSTR("ADAA3ACE75ED516CB91893413EE9CC9ED04CA47B"),
                 NULL);

        // Sync a dataSource with one object to another dataSource with the same object
        testsync("secd_70_engine-alice1bob2", test_directive, test_reason,
                 ^ (SOSDataSourceRef dataSource) {
#if 0
                     CFErrorRef error = NULL;
                     // TODO: Needs to be a SecDBItemRef for the SecItemDataSource...
                     SOSObjectRef object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
                     ok(SOSDataSourceMergeObject(dataSource, object, NULL, &error), "dataSource %@ added object %@", SOSEngineGetMyID(dataSource->engine), error);
                     CFReleaseSafe(object);
                     CFReleaseNull(error);
#endif
                 },
                 ^ (SOSDataSourceRef dataSource) {
                     __block CFErrorRef error = NULL;
                     __block SOSObjectRef object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
                     ok(SOSDataSourceWith(dataSource, &error, ^(SOSTransactionRef txn, bool *commit) {
                         ok(SOSDataSourceMergeObject(dataSource, txn, object, NULL, &error), "dataSource %@ added object %@", SOSEngineGetMyID(dataSource->engine), error);
                         CFReleaseSafe(object);
                         CFReleaseNull(error);
                         object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("account1"), CFSTR("service1"));
                         ok(SOSDataSourceMergeObject(dataSource, txn, object, NULL, &error), "dataSource %@ added object %@", SOSEngineGetMyID(dataSource->engine), error);
                         CFReleaseSafe(object);
                     }), "ds transaction failed %@", error);
                     CFReleaseNull(error);
                 },
                 CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
                 CFSTR("270EB3953B2E1E295F668CFC27CBB7137991A4BE"),
                 CFSTR("D1B3944E3084425F41B2C2EA0BE82170E10AA37D"),

                 //CFSTR("ADAA3ACE75ED516CB91893413EE9CC9ED04CA47B"),
                 //CFSTR("D4049A1063CFBF7CAF8424E13DE3CE926FF5856C"),
                 //CFSTR("9624EA855BBED6B668868BB723443E804D04F6A1"),
                 //CFSTR("063E097CCD4FEB7F3610ED12B3DA828467314846"),
                 //CFSTR("D1B3944E3084425F41B2C2EA0BE82170E10AA37D"),
                 NULL);

        // Sync a dataSource with a tombstone object to another dataSource with the same object
    TODO: {
        todo("<rdar://problem/14049022> Test case in sd-70-engine fails due to need for RowID");
        testsync("secd_70_engine-update", test_directive, test_reason,
                 ^ (SOSDataSourceRef dataSource) {
                     __block CFErrorRef error = NULL;
                     const char *password = "password1";
                     CFDataRef data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)password, strlen(password));
                     // TODO: Needs to be a SecDBItemRef for the SecItemDataSource...
                     SOSObjectRef object_to_find = SOSDataSourceCreateGenericItemWithData(dataSource, CFSTR("test_account"), CFSTR("test service"), true, NULL);
                     SOSObjectRef object = SOSDataSourceCopyObject(dataSource, object_to_find, &error);
                     SOSObjectRef old_object = NULL;
                 SKIP: {
                     skip("no object", 1, ok(object, "Finding object %@, error: %@", object_to_find, error));
                     CFReleaseNull(data);
                     // TODO: Needs to be a SecDBItemRef for the SecItemDataSource...
                     old_object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
                     ok(SOSDataSourceWith(dataSource, &error, ^(SOSTransactionRef txn, bool *commit) {
                         ok(SOSDataSourceMergeObject(dataSource, txn, object, NULL, &error), "dataSource %@ update object %@", SOSEngineGetMyID(dataSource->engine), error);
                     }), "ds transaction failed %@", error);
                 }
                     CFReleaseSafe(data);
                     CFReleaseSafe(old_object);
                     CFReleaseSafe(object);
                     CFReleaseNull(error);
                 },
                 ^ (SOSDataSourceRef dataSource) {
                     __block CFErrorRef error = NULL;
                     __block SOSObjectRef object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
                     ok(SOSDataSourceWith(dataSource, &error, ^(SOSTransactionRef txn, bool *commit) {
                         ok(SOSDataSourceMergeObject(dataSource, txn, object, NULL, &error), "dataSource %@ added object %@", SOSEngineGetMyID(dataSource->engine), error);
                         CFReleaseSafe(object);
                         CFReleaseNull(error);
                         object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("account1"), CFSTR("service1"));
                         ok(SOSDataSourceMergeObject(dataSource, txn, object, NULL, &error), "dataSource %@ added object %@", SOSEngineGetMyID(dataSource->engine), error);
                         CFReleaseSafe(object);
                     }), "ds transaction failed %@", error);
                     CFReleaseNull(error);
                 },
                 CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
                 CFSTR("270EB3953B2E1E295F668CFC27CBB7137991A4BE"),
                 CFSTR("D1B3944E3084425F41B2C2EA0BE82170E10AA37D"),

                 //CFSTR("5D07A221A152D6D6C5F1919189F259A7278A08C5"),
                 //CFSTR("D4049A1063CFBF7CAF8424E13DE3CE926FF5856C"),
                 //CFSTR("137FD34E9BF11B4BA0620E8EBFAB8576BCCCF294"),
                 //CFSTR("5D07A221A152D6D6C5F1919189F259A7278A08C5"),
                 NULL);
    }

        // Sync a dataSource with one object to another dataSource with the same object
        testsync("secd_70_engine-foreign-add", test_directive, test_reason,
                 ^ (SOSDataSourceRef dataSource) {
                 },
                 ^ (SOSDataSourceRef dataSource) {
                     __block CFErrorRef error = NULL;
                     const char *password = "password1";
                     CFDataRef data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)password, strlen(password));
                     __block SOSObjectRef object = SOSDataSourceCreateGenericItemWithData(dataSource, CFSTR("test_account"), CFSTR("test service"), false, data);
                     CFReleaseSafe(data);
                     ok(SOSDataSourceWith(dataSource, &error, ^(SOSTransactionRef txn, bool *commit) {
                         ok(SOSDataSourceMergeObject(dataSource, txn, object, NULL, &error), "dataSource %@ added object %@", SOSEngineGetMyID(dataSource->engine), error);
                         CFReleaseSafe(object);
                         CFReleaseNull(error);
                         object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("account1"), CFSTR("service1"));
                         ok(SOSDataSourceMergeObject(dataSource, txn, object, NULL, &error), "dataSource %@ added object %@", SOSEngineGetMyID(dataSource->engine), error);
                         CFReleaseSafe(object);
                     }), "ds transaction failed %@", error);
                     CFReleaseNull(error);
                 },
                 CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
                 CFSTR("769F63675CEE9CB968BFD9CA48DB9079BFCAFB6C"),
                 CFSTR("818C24B9BC495940836B9C8F76517C838CEFFA98"),
                 
                 //CFSTR("D1B3944E3084425F41B2C2EA0BE82170E10AA37D"),
                 //CFSTR("607EEF976943FD781CFD2B3850E6DC7979AA61EF"),
                 //CFSTR("28434CD1B90CC205460557CAC03D7F12067F2329"),
                 //CFSTR("D1B3944E3084425F41B2C2EA0BE82170E10AA37D"),
                 NULL);
    }
    
    // Sync between 2 empty dataSources
    testsync2("secd_70_engine2", test_directive, test_reason,
              ^ (SOSDataSourceRef dataSource) {},
              ^ (SOSDataSourceRef dataSource) {},
              CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
              CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
              CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
              CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
              CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
              NULL);

    //testsyncempty();
TODO: {
    todo("testsync fails with 10 peers and staggered adds");
    testsyncmany("syncmany", test_directive, test_reason, 9, 10, 0, syncmany_add);
}
    testsyncmany("v2syncmany", test_directive, test_reason, 9, 10, 2, syncmany_add);
    testsync2p();
}

int secd_70_engine(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    /* custom keychain dir */
    secd_test_setup_temp_keychain("secd_70_engine", NULL);

    synctests();
    
    return 0;
}
