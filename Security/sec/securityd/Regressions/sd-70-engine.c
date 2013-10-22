//
//  sd-70-engine.c
//  sec
//
//  Created by Michael Brouwer on 11/9/12.
//  Copyright 2012 Apple Inc. All rights reserved.
//
//

// Test syncing between SecItemDataSource and SOSTestDataSource

#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSPeer.h>

#include "securityd_regressions.h"

#include <corecrypto/ccsha2.h>
#include <Security/SecBase64.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <securityd/SecItemServer.h>

#include <utilities/SecFileLocations.h>

#include <stdint.h>
#include "SOSTestDataSource.h"
#include "SOSTestTransport.h"

#include <AssertMacros.h>

static int kTestTestCount = 74;

// TODO: Make this shared.
static CFStringRef SOSMessageCopyDigestHex(CFDataRef message) {
    uint8_t digest[CCSHA1_OUTPUT_SIZE];
    ccdigest(ccsha1_di(), CFDataGetLength(message), CFDataGetBytePtr(message), digest);
    CFMutableStringRef hex = CFStringCreateMutable(0, 2 * sizeof(digest));
    for (unsigned int ix = 0; ix < sizeof(digest); ++ix) {
        CFStringAppendFormat(hex, 0, CFSTR("%02X"), digest[ix]);
    }
    return hex;
}

static void testsync(const char *name,  const char *test_directive, const char *test_reason, void (^aliceInit)(SOSDataSourceRef ds), void (^bobInit)(SOSDataSourceRef ds), CFStringRef msg, ...) {
    CFErrorRef error = NULL;

    /* Setup Alice and Bob's dataSources. */
    SOSDataSourceFactoryRef aliceDataSourceFactory = SecItemDataSourceFactoryCreateDefault();
    SOSDataSourceRef aliceDataSource = NULL;
    CFArrayRef ds_names = aliceDataSourceFactory->copy_names(aliceDataSourceFactory);
    if (ds_names && CFArrayGetCount(ds_names) > 0) {
        CFStringRef name = CFArrayGetValueAtIndex(ds_names, 0);
        ok (aliceDataSource = aliceDataSourceFactory->create_datasource(aliceDataSourceFactory, name, false, &error), "create datasource \"%@\" [error: %@]", name, error);
        CFReleaseNull(error);
    }
    CFReleaseNull(ds_names);

    SOSDataSourceRef bobDataSource = SOSTestDataSourceCreate();

    /* Setup Alice engine and peer for Alice to talk to Bob */
    SOSEngineRef aliceEngine;
    ok(aliceEngine = SOSEngineCreate(aliceDataSource, &error), "create alice engine: %@", error);
    CFReleaseNull(error);
    CFStringRef bobID = CFStringCreateWithFormat(kCFAllocatorDefault, 0, CFSTR("Bob-%s"), name);

    __block CFDataRef queued_message = NULL;

    SOSPeerSendBlock enqueueMessage =  ^bool (CFDataRef message, CFErrorRef *error) {
        if (queued_message)
            fail("We already had an unproccessed message");

        queued_message = (CFDataRef) CFRetain(message);
        return true;
    };

    CFDataRef (^dequeueMessage)() = ^CFDataRef () {
        CFDataRef result = queued_message;
        queued_message = NULL;

        return result;
    };

    SOSPeerRef bobPeer;
    ok(bobPeer = SOSPeerCreateSimple(bobID, kSOSPeerVersion, &error, enqueueMessage),
       "create peer: %@", error);

    /* Setup Bob engine and peer for Bob to talk to Alice */
    SOSEngineRef bobEngine;
    ok(bobEngine = SOSEngineCreate(bobDataSource, &error), "create bob engine: %@", error);
    CFReleaseNull(error);
    CFStringRef aliceID = CFStringCreateWithFormat(kCFAllocatorDefault, 0, CFSTR("Alice-%s"), name);

    SOSPeerRef alicePeer;
    ok(alicePeer = SOSPeerCreateSimple(aliceID, kSOSPeerVersion, &error, enqueueMessage),
       "create peer: %@", error);
    CFReleaseNull(error);

    /* Now call provided setup blocks to populate the dataSources with
     interesting stuff. */
    aliceInit(aliceDataSource);
    bobInit(bobDataSource);

    /* Start syncing by making alice send the first message. */
    ok(SOSEngineSyncWithPeer(aliceEngine, bobPeer, false, &error), "tell Alice sync with peer Bob");
    CFDataRef message;

	va_list msgs;
	va_start(msgs, msg);

    int msg_index = 0;
    bool alice = false;
    for (;;) {
        message = dequeueMessage();
        msg_index++;
        /* We are expecting a message and msg is it's digest. */
        if (message) {
            CFStringRef messageDesc = SOSMessageCopyDescription(message);
            CFStringRef messageDigestStr = SOSMessageCopyDigestHex(message);
            if (msg) {
                bool handeled = SOSEngineHandleMessage(alice ? aliceEngine : bobEngine, alice ? bobPeer : alicePeer, message, &error);
                if (!CFEqual(messageDigestStr, msg)) {
                    if (handeled) {
                        fail("%s %s received message [%d] digest %@ != %@ %@", name, alice ? "Alice" : "Bob", msg_index, messageDigestStr, msg, messageDesc);
                    } else {
                        fail("%s %s failed to handle message [%d] digest %@ != %@ %@: %@", name, alice ? "Alice" : "Bob", msg_index, messageDigestStr, msg, messageDesc, error);
                        CFReleaseNull(error);
                    }
                } else if (handeled) {
                    pass("%s %s handled message [%d] %@", name, alice ? "Alice" : "Bob", msg_index, messageDesc);
                } else {
                    fail("%s %s failed to handle message [%d] %@: %@", name, alice ? "Alice" : "Bob", msg_index, messageDesc, error);
                    CFReleaseNull(error);
                }
            } else {
                fail("%s %s sent extra message [%d] with digest %@: %@", name, alice ? "Bob" : "Alice", msg_index, messageDigestStr, messageDesc);
            }
            CFRelease(messageDigestStr);
            CFRelease(messageDesc);
            CFRelease(message);
        } else {
            if (msg) {
                fail("%s %s expected message [%d] with digest %@, none received", name, alice ? "Alice" : "Bob", msg_index, msg);
            }
        }

        if (msg) {
            alice = !alice;
            msg = va_arg(msgs, CFStringRef);
        } else
            break;
    }

	va_end(msgs);

    SOSEngineDispose(aliceEngine); // Also disposes aliceDataSource
    SOSPeerDispose(alicePeer);
    CFReleaseSafe(aliceID);

    SOSEngineDispose(bobEngine); // Also disposes bobDataSource
    SOSPeerDispose(bobPeer);
    CFReleaseSafe(bobID);

    aliceDataSourceFactory->release(aliceDataSourceFactory);
}


static SOSObjectRef SOSDataSourceCopyObject(SOSDataSourceRef ds, SOSObjectRef match, CFErrorRef *error)
{
    __block SOSObjectRef result = NULL;

    CFDataRef digest = ds->copyDigest(match, error);
    SOSManifestRef manifest = NULL;

    require(digest, exit);
   
    manifest = SOSManifestCreateWithData(digest, error);

    ds->foreach_object(ds, manifest, error, ^ bool (SOSObjectRef object, CFErrorRef *error) {
        if (result == NULL) {
            result = object;
            CFRetainSafe(result);
        }
        
        return true;
    });
        
exit:
    CFReleaseNull(manifest);
    CFReleaseNull(digest);
    return result;
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

    // Sync between 2 empty dataSources
    testsync("sd_70_engine", test_directive, test_reason,
             ^ (SOSDataSourceRef dataSource) {},
             ^ (SOSDataSourceRef dataSource) {},
             CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
             CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
             CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
             NULL);

    // Sync a dataSource with one object to an empty dataSource
    testsync("sd_70_engine-alice1", test_directive, test_reason,
             ^ (SOSDataSourceRef dataSource) {
                 CFErrorRef error = NULL;
                 SOSObjectRef object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
                 // TODO: Needs to be a SecDBItemRef for the SecItemDataSource...
                 ok(dataSource->add(dataSource, object, &error), "dataSource added object %@", error);
                 CFReleaseSafe(object);
                 CFReleaseNull(error);
             },
             ^ (SOSDataSourceRef dataSource) {},
             CFSTR("ADAA3ACE75ED516CB91893413EE9CC9ED04CA47B"),
             CFSTR("147B6C509908CC4A9FC4263973A842104A64CE01"),
             CFSTR("019B494F3C06B48BB02C280AF1E19AD861A7003C"),
             CFSTR("ADAA3ACE75ED516CB91893413EE9CC9ED04CA47B"),
             NULL);

    // Sync a dataSource with one object to another dataSource with the same object
    testsync("sd_70_engine-alice1bob1", test_directive, test_reason,
             ^ (SOSDataSourceRef dataSource) {
#if 0
                 CFErrorRef error = NULL;
                 // TODO: Needs to be a SecDBItemRef for the SecItemDataSource...
                 CFDictionaryRef object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
                 ok(dataSource->add(dataSource, object, &error), "dataSource added object %@", error);
                 CFReleaseSafe(object);
                 CFReleaseNull(error);
#endif
             },
             ^ (SOSDataSourceRef dataSource) {
                 CFErrorRef error = NULL;
                 SOSObjectRef object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
                 ok(dataSource->add(dataSource, object, &error), "dataSource added object %@", error);
                 CFReleaseSafe(object);
                 CFReleaseNull(error);
             },
             CFSTR("ADAA3ACE75ED516CB91893413EE9CC9ED04CA47B"),
             CFSTR("ADAA3ACE75ED516CB91893413EE9CC9ED04CA47B"),
             CFSTR("ADAA3ACE75ED516CB91893413EE9CC9ED04CA47B"),
             NULL);

    // Sync a dataSource with one object to another dataSource with the same object
    testsync("sd_70_engine-alice1bob2", test_directive, test_reason,
             ^ (SOSDataSourceRef dataSource) {
#if 0
                 CFErrorRef error = NULL;
                 // TODO: Needs to be a SecDBItemRef for the SecItemDataSource...
                 SOSObjectRef object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
                 ok(dataSource->add(dataSource, object, &error), "dataSource added object %@", error);
                 CFReleaseSafe(object);
                 CFReleaseNull(error);
#endif
             },
             ^ (SOSDataSourceRef dataSource) {
                 CFErrorRef error = NULL;
                 SOSObjectRef object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
                 ok(dataSource->add(dataSource, object, &error), "dataSource added object %@", error);
                 CFReleaseSafe(object);
                 object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("account1"), CFSTR("service1"));
                 ok(dataSource->add(dataSource, object, &error), "dataSource added object %@", error);
                 CFReleaseSafe(object);
                 CFReleaseNull(error);
             },
             CFSTR("ADAA3ACE75ED516CB91893413EE9CC9ED04CA47B"),
             CFSTR("D4049A1063CFBF7CAF8424E13DE3CE926FF5856C"),
             CFSTR("9624EA855BBED6B668868BB723443E804D04F6A1"),
             CFSTR("063E097CCD4FEB7F3610ED12B3DA828467314846"),
             CFSTR("D1B3944E3084425F41B2C2EA0BE82170E10AA37D"),
             NULL);

    // Sync a dataSource with a tombstone object to another dataSource with the same object
    TODO: {
    todo("<rdar://problem/14049022> Test case in sd-70-engine fails due to need for RowID");
    testsync("sd_70_engine-update", test_directive, test_reason,
             ^ (SOSDataSourceRef dataSource) {
                 CFErrorRef error = NULL;
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
                 ok(dataSource->add(dataSource, object, &error), "dataSource update object %@", error);
             }
                 CFReleaseSafe(data);
                 CFReleaseSafe(old_object);
                 CFReleaseSafe(object);
                 CFReleaseNull(error);
             },
             ^ (SOSDataSourceRef dataSource) {
                 CFErrorRef error = NULL;
                 SOSObjectRef object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
                 ok(dataSource->add(dataSource, object, &error), "dataSource added object %@", error);
                 CFReleaseSafe(object);
                 object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("account1"), CFSTR("service1"));
                 ok(dataSource->add(dataSource, object, &error), "dataSource added object %@", error);
                 CFReleaseSafe(object);
                 CFReleaseNull(error);
             },
             CFSTR("5D07A221A152D6D6C5F1919189F259A7278A08C5"),
             CFSTR("D4049A1063CFBF7CAF8424E13DE3CE926FF5856C"),
             CFSTR("137FD34E9BF11B4BA0620E8EBFAB8576BCCCF294"),
             CFSTR("5D07A221A152D6D6C5F1919189F259A7278A08C5"),
             NULL);
    }

    // Sync a dataSource with one object to another dataSource with the same object
    testsync("sd_70_engine-foreign-add", test_directive, test_reason,
             ^ (SOSDataSourceRef dataSource) {
             },
             ^ (SOSDataSourceRef dataSource) {
                 CFErrorRef error = NULL;
                 const char *password = "password1";
                 CFDataRef data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)password, strlen(password));
                 SOSObjectRef object = SOSDataSourceCreateGenericItemWithData(dataSource, CFSTR("test_account"), CFSTR("test service"), false, data);
                 CFReleaseSafe(data);
                 ok(dataSource->add(dataSource, object, &error), "dataSource added object %@", error);
                 CFReleaseSafe(object);
                 CFReleaseNull(error);
                 object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("account1"), CFSTR("service1"));
                 ok(dataSource->add(dataSource, object, &error), "dataSource added object %@", error);
                 CFReleaseSafe(object);
                 CFReleaseNull(error);
             },
             CFSTR("D1B3944E3084425F41B2C2EA0BE82170E10AA37D"),
             CFSTR("607EEF976943FD781CFD2B3850E6DC7979AA61EF"),
             CFSTR("28434CD1B90CC205460557CAC03D7F12067F2329"),
             CFSTR("D1B3944E3084425F41B2C2EA0BE82170E10AA37D"),
             NULL);
    }
}

int sd_70_engine(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    synctests();

	return 0;
}
