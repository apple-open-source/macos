//
//  sc-70-engine.c
//  sec
//
//  Created by Michael Brouwer on 8/2/12.
//  Copyright 2012 Apple Inc. All rights reserved.
//
//

#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSPeer.h>

#include "SOSCircle_regressions.h"

#include <corecrypto/ccsha2.h>
#include <Security/SecBase64.h>

#include <utilities/SecCFWrappers.h>

#include <stdint.h>
#include "SOSTestDataSource.h"
#include "SOSTestTransport.h"

static int kTestTestCount = 61;

static void tests(void)
{
    CFErrorRef error = NULL;

    /* Transport. */
    __block unsigned msg_count = 0;
    uint8_t msg_digest_buffer[CCSHA256_OUTPUT_SIZE];
    uint8_t *msg_digest = msg_digest_buffer;
    
    SOSPeerSendBlock sendBlock = ^bool (CFDataRef message, CFErrorRef *error) {
        size_t msglen = CFDataGetLength(message);
        const uint8_t *msg = CFDataGetBytePtr(message);
        const struct ccdigest_info *sha256 = ccsha256_di();
        if (msg_count++ == 0) {
            /* message n=0 */
            ccdigest(sha256, msglen, msg, msg_digest);
        } else {
            /* message n=n+1 */
            ccdigest_di_decl(sha256, sha256_ctx);
            ccdigest_init(sha256, sha256_ctx);
            ccdigest_update(sha256, sha256_ctx, sizeof(msg_digest_buffer), msg_digest);
            ccdigest_update(sha256, sha256_ctx, msglen, msg);
            ccdigest_final(sha256, sha256_ctx, msg_digest);
        }
        size_t encmaxlen = SecBase64Encode(msg, msglen, NULL, 0);
        CFMutableDataRef encoded = CFDataCreateMutable(NULL, encmaxlen);
        CFDataSetLength(encoded, encmaxlen);
        SecBase64Result rc;
        char *enc = (char *)CFDataGetMutableBytePtr(encoded);
#ifndef NDEBUG
        size_t enclen =
#endif
            SecBase64Encode2(msg, msglen,
                             enc,
                             encmaxlen, kSecB64_F_LINE_LEN_USE_PARAM,
                             64, &rc);
        assert(enclen < INT32_MAX);
//      printf("=== BEGIN SOSMESSAGE ===\n%.*s\n=== END SOSMESSAGE ===\n", (int)enclen, enc);
        CFRelease(encoded);
        return true;
    };

    /* Create peer test. */
    CFStringRef peer_id = CFSTR("peer 70");
    SOSPeerRef peer;
    ok(peer = SOSPeerCreateSimple(peer_id, kSOSPeerVersion, &error, sendBlock),
       "create peer: %@", error);
    CFReleaseNull(error);

    /* DataSource */
    SOSDataSourceRef ds = SOSTestDataSourceCreate();

    /* Create engine test. */
    SOSEngineRef engine;
    ok(engine = SOSEngineCreate(ds, &error), "create engine: %@", error);
    CFReleaseNull(error);

    /* Make sure the engine did not yet send a any messages to the peer. */
    unsigned expected_msg_count = 0;
    is(msg_count, expected_msg_count, "Engine sent %d/%d messages",
       msg_count, expected_msg_count);

    /* Test passing peer messages to the engine. */
    CFDataRef message;

    /* Hand an empty message to the engine for handeling. */
    message = CFDataCreate(NULL, NULL, 0);
    ok(false == SOSEngineHandleMessage(engine, peer, message, &error),
       "handle empty message: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(message);
    /* Make sure the engine did not yet send a response to the peer. */
    is(msg_count, expected_msg_count, "Engine sent %d/%d messages",
       msg_count, expected_msg_count);


    /* Create manifest digest message. */
    SKIP: {
        skip("Create manifest digest message failed", 2,
             ok(message = SOSEngineCreateManifestDigestMessage(engine, peer,
                                                               &error),
                "create manifest digest message: %@", error));
        CFReleaseNull(error);
        /* Pass manifest digest message to the engine for handeling. */
        skip("", 1,
             ok(SOSEngineHandleMessage(engine, peer, message, &error),
                "handle manifest digest message: %@", error));
        /* Make sure the engine sent a response to the peer. */
        expected_msg_count++;
        is(msg_count, expected_msg_count, "Engine sent %d/%d messages",
           msg_count, expected_msg_count);
    }
    CFReleaseNull(message);
    CFReleaseNull(error);

    /* Create manifest message. */
    SKIP: {
        skip("Create manifest message failed", 2,
             ok(message = SOSEngineCreateManifestMessage(engine, peer, &error),
             "create manifest message: %@", error));
        CFReleaseNull(error);
        /* Pass manifest message to the engine for handeling. */
        skip("", 1,
             ok(SOSEngineHandleMessage(engine, peer, message, &error),
                "handle manifest message: %@", error));
        /* Make sure the engine sent a response to the peer. */
        expected_msg_count++;
        is(msg_count, expected_msg_count, "Engine sent %d/%d messages",
           msg_count, expected_msg_count);
    }
    CFReleaseNull(message);
    CFReleaseNull(error);

    /* Create manifest and objects message. */
    SKIP: {
        skip("Create manifest and objects message failed", 2,
             ok(message = SOSEngineCreateManifestAndObjectsMessage(engine, peer,
                                                                   &error),
                "create manifest and objects message: %@", error));
        CFReleaseNull(error);
        /* Pass manifest and objects message to the engine for handeling. */
        skip("", 1,
             ok(SOSEngineHandleMessage(engine, peer, message, &error),
                "handle manifest and objects message: %@", error));
        /* Make sure the engine sent a response to the peer. */
        expected_msg_count++;
        is(msg_count, expected_msg_count, "Engine sent %d/%d messages",
           msg_count, expected_msg_count);
    }
    CFReleaseNull(message);
    CFReleaseNull(error);

    /* Clean up. */
    SOSPeerDispose(peer);
    SOSEngineDispose(engine);
}

static CFStringRef SOSMessageCopyDigestHex(CFDataRef message) {
    uint8_t digest[CCSHA1_OUTPUT_SIZE];
    ccdigest(ccsha1_di(), CFDataGetLength(message), CFDataGetBytePtr(message), digest);
    CFMutableStringRef hex = CFStringCreateMutable(0, 2 * sizeof(digest));
    for (unsigned int ix = 0; ix < sizeof(digest); ++ix) {
        CFStringAppendFormat(hex, 0, CFSTR("%02X"), digest[ix]);
    }
    return hex;
}

static void testsync(const char *name, void (^aliceInit)(SOSDataSourceRef ds), void (^bobInit)(SOSDataSourceRef ds), CFStringRef msg, ...) {
    CFErrorRef error = NULL;

    /* Setup Alice engine, dataSource and peer for Alice to talk to Bob */
    SOSDataSourceRef aliceDataSource = SOSTestDataSourceCreate();
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

    /* Setup Bob engine, dataSource and peer for Bob to talk to Alice */
    SOSDataSourceRef bobDataSource = SOSTestDataSourceCreate();
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
            } else {
                /* Compare alice and bobs dataSources databases. */
                ok(CFEqual(SOSTestDataSourceGetDatabase(aliceDataSource), SOSTestDataSourceGetDatabase(bobDataSource)), "%s Alice and Bob are in sync", name);
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
}

static void synctests(void) {
    // Sync between 2 empty dataSources
    testsync("empty",
             ^ (SOSDataSourceRef dataSource) {},
             ^ (SOSDataSourceRef dataSource) {},
             CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
             CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
             CFSTR("2AF312E092D67308A0083DFFBF2B6B754B967864"),
             NULL);

    // Sync a dataSource with one object to an empty dataSource
    testsync("alice1",
             ^ (SOSDataSourceRef dataSource) {
                 CFErrorRef error = NULL;
                 SOSObjectRef object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
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
    testsync("alice1bob1",
             ^ (SOSDataSourceRef dataSource) {
                 CFErrorRef error = NULL;
                 SOSObjectRef object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
                 ok(dataSource->add(dataSource, object, &error), "dataSource added object %@", error);
                 CFReleaseSafe(object);
                 CFReleaseNull(error);
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
    testsync("alice1bob2",
             ^ (SOSDataSourceRef dataSource) {
                 CFErrorRef error = NULL;
                 SOSObjectRef object = SOSDataSourceCreateGenericItem(dataSource, CFSTR("test_account"), CFSTR("test service"));
                 ok(dataSource->add(dataSource, object, &error), "dataSource added object %@", error);
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
             CFSTR("ADAA3ACE75ED516CB91893413EE9CC9ED04CA47B"),
             CFSTR("D4049A1063CFBF7CAF8424E13DE3CE926FF5856C"),
             CFSTR("9624EA855BBED6B668868BB723443E804D04F6A1"),
             CFSTR("063E097CCD4FEB7F3610ED12B3DA828467314846"),
             CFSTR("D1B3944E3084425F41B2C2EA0BE82170E10AA37D"),
             NULL);

}

int sc_70_engine(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();
    synctests();

	return 0;
}
