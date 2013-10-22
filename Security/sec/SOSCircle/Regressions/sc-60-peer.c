//
//  sc-60-peer.c
//  sec
//
//  Created by Michael Brouwer on 8/2/12.
//  Copyright 2012 Apple Inc. All rights reserved.
//

#include <SecureObjectSync/SOSPeer.h>

#include "SOSCircle_regressions.h"

#include <corecrypto/ccsha2.h>

#include <Security/SecBase64.h>

#include <utilities/SecCFWrappers.h>

#include <stdint.h>

static int kTestTestCount = 13;

struct SOSTestTransport {
    struct SOSTransport t;
    unsigned t_msg_count;
    uint8_t digest[CCSHA256_OUTPUT_SIZE];
};


static void tests(void)
{
    const unsigned kSOSPeerVersion = 0;

    CFErrorRef error = NULL;
    SOSPeerRef peer;

    /* Create peer test. */
    CFStringRef peer_id = CFSTR("peer 60");
    
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
    
    
    ok(peer = SOSPeerCreateSimple(peer_id, kSOSPeerVersion, &error, sendBlock),
       "create peer: %@", error);
    CFReleaseNull(error);

    /* Send a test message. */
    is((int)msg_count, 0, "no message sent yet");
    size_t msglen = 10;
    uint8_t msg[msglen];
    memcpy(msg, "0123456789", msglen);
    CFDataRef message = CFDataCreate(NULL, msg, msglen);
    ok(SOSPeerSendMessage(peer, message, &error),
       "send message to peer: %@", error);
    CFReleaseNull(error);
    is((int)msg_count, 1, "We sent %d/1 messages", msg_count);
    CFRelease(message);

    /* Check the peer's version. */
    is(SOSPeerGetVersion(peer), kSOSPeerVersion, "version is correct");

    /* Get the peer's manifest. */
    SOSManifestRef manifest = SOSPeerCopyManifest(peer, &error);
    ok(manifest == NULL, "No manifest yet for this peer: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(manifest);

    /* Get the peer's manifest digest. */
    CFDataRef digest = SOSPeerCopyManifestDigest(peer, &error);
    ok(digest == NULL, "No digest yet for this peer's manifest: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(digest);

    ok(manifest = SOSManifestCreateWithBytes(NULL, 0, &error), "Create empty manifest: %@", error);
    CFReleaseNull(error);
    ok(SOSPeerSetManifest(peer, manifest, &error), "Set empty manifest on peer: %@", error);
    CFReleaseNull(error);

    /* Get the peer's empty manifest digest. */
    digest = SOSPeerCopyManifestDigest(peer, &error);
    ok(digest, "Got a digest: %@ this peer's manifest: %@", digest, error);
    CFReleaseNull(error);

    /* Clean up. */
    SOSPeerDispose(peer);

    SOSPeerRef reinflated_peer = NULL;
    ok(reinflated_peer = SOSPeerCreateSimple(peer_id, kSOSPeerVersion, &error, sendBlock),
       "create peer: %@", error);
    CFReleaseNull(error);

    CFDataRef digestAfterReinflate = SOSPeerCopyManifestDigest(reinflated_peer, &error);
    ok(digest != NULL, "Got NULL after reinflate (%@)", error);
    ok(CFEqual(digest, digestAfterReinflate), "Compare digest after reinflate, before: %@ after: %@", digest, digestAfterReinflate);
    CFReleaseNull(error);

    SOSPeerDispose(reinflated_peer);

    CFReleaseNull(digestAfterReinflate);
    CFReleaseNull(digest);
}

int sc_60_peer(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

	return 0;
}
