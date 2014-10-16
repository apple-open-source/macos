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


#include <SecureObjectSync/SOSPeer.h>

#include "SOSCircle_regressions.h"

#include <corecrypto/ccsha2.h>

#include <Security/SecBase64.h>

#include <utilities/SecCFWrappers.h>

#include <stdint.h>

static int kTestTestCount = 13;

static void tests(void)
{
#if 0
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
    
    
    ok(peer = SOSPeerCreateSimple(peer_id, kSOSPeerVersion, &error),
       "create peer: %@", error);
    CFReleaseNull(error);

    /* Send a test message. */
    is((int)msg_count, 0, "no message sent yet");
    size_t msglen = 10;
    uint8_t msg[msglen];
    memcpy(msg, "0123456789", msglen);
    CFDataRef message = CFDataCreate(NULL, msg, msglen);
    ok(SOSPeerSendMessage(peer, message, &error, sendBlock),
       "send message to peer: %@", error);
    CFReleaseNull(error);
    is((int)msg_count, 1, "We sent %d/1 messages", msg_count);
    CFRelease(message);

    /* Check the peer's version. */
    is(SOSPeerGetVersion(peer), kSOSPeerVersion, "version is correct");

    //SOSPeerManifestType mfType = kSOSPeerProposedManifest;
    SOSPeerManifestType mfType = kSOSPeerConfirmedManifest;
    /* Get the peer's manifest. */
    SOSManifestRef manifest = SOSPeerGetManifest(peer, mfType, &error);
    ok(manifest == NULL, "No manifest yet for this peer: %@", error);
    CFReleaseNull(error);

    /* Get the peer's manifest digest. */
    CFDataRef digest = SOSManifestGetDigest(manifest, &error);
    ok(digest == NULL, "No digest yet for this peer's manifest: %@", error);
    CFReleaseNull(error);

    ok(manifest = SOSManifestCreateWithBytes(NULL, 0, &error), "Create empty manifest: %@", error);
    CFReleaseNull(error);
    ok(SOSPeerSetManifest(peer, mfType, manifest, &error), "Set empty manifest on peer: %@", error);
    CFReleaseNull(error);

    /* Get the peer's empty manifest digest. */
    digest = SOSManifestGetDigest(manifest, &error);
    ok(digest, "Got a digest: %@ this peer's manifest: %@", digest, error);
    CFReleaseNull(error);

    /* Clean up. */
    CFReleaseSafe(peer);

    SOSPeerRef reinflated_peer = NULL;
    ok(reinflated_peer = SOSPeerCreateSimple(peer_id, kSOSPeerVersion, &error),
       "create peer: %@", error);
    CFReleaseNull(error);

    manifest = SOSPeerGetManifest(reinflated_peer, mfType, &error);
    ok(manifest != NULL, "Got NULL manifest after reinflate (%@)", error);
    CFDataRef digestAfterReinflate = SOSManifestGetDigest(manifest, &error);
    ok(digestAfterReinflate != NULL, "Got NULL digest after reinflate (%@)", error);
    ok(digestAfterReinflate && CFEqual(digest, digestAfterReinflate), "Compare digest after reinflate, before: %@ after: %@", digest, digestAfterReinflate);
    CFReleaseNull(error);

    CFReleaseSafe(reinflated_peer);
#endif
}

int sc_60_peer(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

	return 0;
}
