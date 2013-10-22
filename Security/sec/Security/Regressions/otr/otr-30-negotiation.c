//
//  otr-30-negotiation.c
//  regressions
//
//  Created by Mitch Adler on 6/7/11.
//  Copyright 2011 Apple Inc. All rights reserved.
//

#include "Security_regressions.h"

#include <CoreFoundation/CFData.h>
#include <Security/SecOTRSession.h>
#include <Security/SecInternal.h>
#include <Security/SecBasePriv.h>

static void SecMPLogError(CFErrorRef error) {
    if (error == NULL) {
        return;
    }
    CFDictionaryRef tempDictionary = CFErrorCopyUserInfo(error);
    CFIndex errorCode = CFErrorGetCode(error);
    CFStringRef errorDomain = CFErrorGetDomain(error);
    CFStringRef errorString = CFDictionaryGetValue(tempDictionary, kCFErrorDescriptionKey);
    CFErrorRef previousError = (CFErrorRef)CFDictionaryGetValue(tempDictionary, kCFErrorUnderlyingErrorKey);
    if (previousError != NULL) {
        SecMPLogError(previousError);
    }
    char errorDomainStr[1024];
    char errorStringStr[1024];
    
    CFStringGetCString(errorDomain, errorDomainStr, 1024, kCFStringEncodingUTF8);
    CFStringGetCString(errorString, errorStringStr, 1024, kCFStringEncodingUTF8);
    printf("MessageProtection: %s (%ld) -- %s\n", errorDomainStr, errorCode, errorStringStr);
    CFReleaseSafe(tempDictionary);
}

static void serializeAndDeserialize(SecOTRSessionRef* thisOne)
{
    CFMutableDataRef serialized = CFDataCreateMutable(kCFAllocatorDefault, 0);

    SecOTRSAppendSerialization(*thisOne, serialized);
    CFReleaseNull(*thisOne);
    *thisOne = SecOTRSessionCreateFromData(kCFAllocatorDefault, serialized);

    CFReleaseSafe(serialized);
}



#define sendMessagesCount(n) ((n) * 8)
static void sendMessages(int howMany, SecOTRSessionRef *bobSession, SecOTRSessionRef *aliceSession, bool serialize)
{
    for(int count = howMany; count > 0; --count) {
        const char* aliceToBob = "aliceToBob";
        CFDataRef rawAliceToBob = CFDataCreate(kCFAllocatorDefault, (const uint8_t*)aliceToBob, (CFIndex) strlen(aliceToBob));
        CFMutableDataRef protectedAliceToBob = CFDataCreateMutable(kCFAllocatorDefault, 0);
        CFMutableDataRef bobDecode = CFDataCreateMutable(kCFAllocatorDefault, 0);

        ok_status(SecOTRSSignAndProtectMessage(*aliceSession, rawAliceToBob, protectedAliceToBob), "encode message");
        ok_status(SecOTRSVerifyAndExposeMessage(*bobSession, protectedAliceToBob, bobDecode), "Decode message");

        if (serialize) {
            serializeAndDeserialize(bobSession);
            serializeAndDeserialize(aliceSession);
        }

        ok(CFDataGetLength(rawAliceToBob) == CFDataGetLength(bobDecode)
           && 0 == memcmp(CFDataGetBytePtr(rawAliceToBob), CFDataGetBytePtr(bobDecode), (size_t)CFDataGetLength(rawAliceToBob)), "Didn't match!");

        CFReleaseNull(rawAliceToBob);
        CFReleaseNull(protectedAliceToBob);
        CFReleaseNull(bobDecode);

        const char* bobToAlice = "i liked your silly message from me to you";
        CFDataRef rawBobToAlice = CFDataCreate(kCFAllocatorDefault, (const uint8_t*)bobToAlice, (CFIndex) strlen(bobToAlice));
        CFMutableDataRef protectedBobToAlice = CFDataCreateMutable(kCFAllocatorDefault, 0);
        CFMutableDataRef aliceDecode = CFDataCreateMutable(kCFAllocatorDefault, 0);

        ok_status(SecOTRSSignAndProtectMessage(*bobSession, rawBobToAlice, protectedBobToAlice), "encode reply");
        ok_status(SecOTRSVerifyAndExposeMessage(*aliceSession, protectedBobToAlice, aliceDecode), "decode reply");

        if (serialize) {
            serializeAndDeserialize(bobSession);
            serializeAndDeserialize(aliceSession);
        }

        ok(CFDataGetLength(rawBobToAlice) == CFDataGetLength(aliceDecode)
           && 0 == memcmp(CFDataGetBytePtr(rawBobToAlice), CFDataGetBytePtr(aliceDecode), (size_t)CFDataGetLength(rawBobToAlice)), "reply matched");

        CFReleaseNull(rawAliceToBob);
        CFReleaseNull(protectedAliceToBob);
        CFReleaseNull(aliceDecode);

        CFStringRef stateString = CFCopyDescription(*bobSession);
        ok(stateString, "getting state from bob");
        CFReleaseNull(stateString);

        stateString = CFCopyDescription(*aliceSession);
        ok(stateString, "getting state from alice");
        CFReleaseNull(stateString);
    }
}

#define kNegotiateTestCount (14 + sendMessagesCount(5) \
                               + 2 + sendMessagesCount(1) \
                               + 1 + sendMessagesCount(1))
static void negotiate(SecOTRSessionRef* aliceSession, SecOTRSessionRef* bobSession, bool serialize)
{
    // Step 1: Create a start packet for each side of the transaction
    CFMutableDataRef bobStartPacket = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSAppendStartPacket(*bobSession, bobStartPacket), "Bob start packet");
    
    if (serialize)
        serializeAndDeserialize(bobSession);
    
    CFMutableDataRef aliceStartPacket = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSAppendStartPacket(*aliceSession, aliceStartPacket), "Alice start packet");
    
    if (serialize)
        serializeAndDeserialize(aliceSession);
    
    // Step 2: Exchange the start packets, forcing the DH commit messages to collide
    CFMutableDataRef aliceDHKeyResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*aliceSession, bobStartPacket, aliceDHKeyResponse),
              "Bob DH packet failed");
    
    if (serialize)
        serializeAndDeserialize(aliceSession);
    
    CFReleaseNull(bobStartPacket);
    
    CFMutableDataRef bobDHKeyResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*bobSession, aliceStartPacket, bobDHKeyResponse),
              "Alice DH packet failed");
    
    if (serialize)
        serializeAndDeserialize(bobSession);
    
    CFReleaseNull(aliceStartPacket);
    
    // Step 3: With one "real" DH key message, and one replayed DH commit message, try to get a "reveal sig" out of one side
    
    CFMutableDataRef bobRevealSigResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*bobSession, aliceDHKeyResponse, bobRevealSigResponse),
              "Alice DH Key packet failed");
    
    if (serialize)
        serializeAndDeserialize(bobSession);
    
    CFReleaseNull(aliceDHKeyResponse);
    
    CFMutableDataRef aliceRevealSigResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*aliceSession, bobDHKeyResponse, aliceRevealSigResponse),
              "Bob DH Key packet failed");
    
    if (serialize)
        serializeAndDeserialize(aliceSession);
    
    CFReleaseNull(bobDHKeyResponse);
    
    // Step 4: Having gotten the reveal signature, now work for the signature
    
    CFMutableDataRef aliceSigResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*aliceSession, bobRevealSigResponse, aliceSigResponse),
              "Bob Reveal sig failed");
    
    if (serialize)
        serializeAndDeserialize(aliceSession);
    
    CFReleaseNull(bobRevealSigResponse);
    
    CFMutableDataRef bobSigResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*bobSession, aliceRevealSigResponse, bobSigResponse),
              "Alice Reveal sig failed");
    
    if (serialize)
        serializeAndDeserialize(bobSession);
    
    CFReleaseNull(aliceRevealSigResponse);
    
    // Step 5: All the messages have been sent, now deal with any replays from the collision handling
    CFMutableDataRef bobFinalResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*bobSession, aliceSigResponse, bobFinalResponse),
              "Alice Final Sig failed");
    
    if (serialize)
        serializeAndDeserialize(bobSession);
    
    CFMutableDataRef aliceFinalResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*aliceSession, bobSigResponse, aliceFinalResponse),
              "Bob Final Sig failed");

    if (serialize)
        serializeAndDeserialize(aliceSession);
    
    is(6, CFDataGetLength(bobFinalResponse), "Alice had nothing left to say");
    is(6, CFDataGetLength(bobFinalResponse), "Bob had nothing left to say");
    ok(SecOTRSGetIsReadyForMessages(*bobSession), "Bob is ready");
    ok(SecOTRSGetIsReadyForMessages(*aliceSession), "Alice is ready");

    CFReleaseNull(aliceSigResponse);
    CFReleaseNull(bobFinalResponse);

    sendMessages(5, bobSession, aliceSession, serialize);

    const char* aliceToBob = "deferredMessage";
    CFDataRef rawAliceToBob = CFDataCreate(kCFAllocatorDefault, (const uint8_t*)aliceToBob, (CFIndex) strlen(aliceToBob));
    CFMutableDataRef protectedAliceToBob = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFMutableDataRef bobDecode = CFDataCreateMutable(kCFAllocatorDefault, 0);

    ok_status(SecOTRSSignAndProtectMessage(*aliceSession, rawAliceToBob, protectedAliceToBob), "encode message");

    sendMessages(1, bobSession, aliceSession, serialize);

    ok(errSecOTRTooOld ==SecOTRSVerifyAndExposeMessage(*bobSession, protectedAliceToBob, bobDecode), "Decode old message");

    sendMessages(1, bobSession, aliceSession, serialize);

    ok(errSecOTRTooOld == SecOTRSVerifyAndExposeMessage(*bobSession, protectedAliceToBob, bobDecode), "Fail to decode excessively old message");

    CFReleaseNull(rawAliceToBob);
    CFReleaseNull(protectedAliceToBob);
    CFReleaseNull(bobDecode);
}


#define kTestTestCount (7 + kNegotiateTestCount)

static void tests()
{
    CFErrorRef testError = NULL;
    SecOTRFullIdentityRef aliceID = SecOTRFullIdentityCreate(kCFAllocatorDefault, &testError);
    SecMPLogError(testError);
    CFReleaseNull(testError);
    testError = NULL;
    SecOTRFullIdentityRef bobID = SecOTRFullIdentityCreate(kCFAllocatorDefault, &testError);
    SecMPLogError(testError);
    CFReleaseNull(testError);
    testError = NULL;

    ok(aliceID, "create alice ID");
    ok(bobID, "create bob ID");

    SecOTRPublicIdentityRef alicePublicID = SecOTRPublicIdentityCopyFromPrivate(kCFAllocatorDefault, aliceID, &testError);
    SecMPLogError(testError);
    CFReleaseNull(testError);
    SecOTRPublicIdentityRef bobPublicID = SecOTRPublicIdentityCopyFromPrivate(kCFAllocatorDefault, bobID, &testError);
    SecMPLogError(testError);
    CFReleaseNull(testError);

    ok(alicePublicID, "extract alice public");
    ok(bobPublicID, "extract bob public");

    SecOTRSessionRef aliceSession = SecOTRSessionCreateFromIDAndFlags(kCFAllocatorDefault, aliceID, bobPublicID, kSecOTRSendTextMessages);
    SecOTRSessionRef bobSession = SecOTRSessionCreateFromIDAndFlags(kCFAllocatorDefault, bobID, alicePublicID, kSecOTRSendTextMessages);

    ok(aliceSession, "create alice session");
    ok(bobSession, "create bob session");

    // Release the IDs, sessions shouldn't need us to retain them for them.
    CFReleaseNull(aliceID);
    CFReleaseNull(bobID);

    CFReleaseNull(alicePublicID);
    CFReleaseNull(bobPublicID);

    negotiate(&aliceSession, &bobSession, true);

    /* cleanup keychain */
    ok(SecOTRFIPurgeAllFromKeychain(&testError),"cleanup keychain");
    SecMPLogError(testError);
    CFReleaseNull(testError);

    CFReleaseNull(aliceSession);
    CFReleaseNull(bobSession);
}

int otr_30_negotiation(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

	tests();

	return 0;
}
