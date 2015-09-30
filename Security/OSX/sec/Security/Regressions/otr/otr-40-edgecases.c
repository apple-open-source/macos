/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
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

#define renegotiateTests 9

static void renegotiate(SecOTRSessionRef *bobSession, SecOTRSessionRef *aliceSession, bool serializeNegotiating, CFDataRef messageDuringNegotiation){
    
    CFMutableDataRef aliceRestartPacket = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFMutableDataRef bobDHKeyPacket = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    CFDataRef empty = CFDataCreate(kCFAllocatorDefault, NULL, 0);

    CFMutableDataRef hopefullyNothing = CFDataCreateMutable(kCFAllocatorDefault, 0);

    //send a restart packet after session has successfully sent messages
    ok_status(SecOTRSAppendRestartPacket(*aliceSession, aliceRestartPacket), "Alice restart packet");
    
    if(serializeNegotiating)
        serializeAndDeserialize(aliceSession);
    
    SKIP: {
        skip("No messageDuringNegotiation", 1, messageDuringNegotiation);
        is(SecOTRSVerifyAndExposeMessage(*aliceSession, messageDuringNegotiation, hopefullyNothing), errSecOTRNotReady, "Message during negotiation");
    }

    if(serializeNegotiating)
        serializeAndDeserialize(aliceSession);

    ok_status(SecOTRSProcessPacket(*bobSession, aliceRestartPacket, bobDHKeyPacket),
              "Bob DH packet failed");
    
    if (serializeNegotiating)
        serializeAndDeserialize(bobSession);
    
    // Step 3: With one "real" DH key message, and one replayed DH commit message, try to get a "reveal sig" out of one side
    
    CFMutableDataRef aliceRevealSigPacket = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*aliceSession, bobDHKeyPacket, aliceRevealSigPacket),
              "Alice DH Key packet failed");
    
    if (serializeNegotiating)
        serializeAndDeserialize(aliceSession);
    
    CFMutableDataRef bobRevealSigResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*bobSession, aliceRevealSigPacket, bobRevealSigResponse),
              "Bob DH Key packet failed");
    
    if (serializeNegotiating)
        serializeAndDeserialize(bobSession);
    
    // Step 4: Having gotten the reveal signature, now work for the signature
    
    CFMutableDataRef aliceSigPacket = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*aliceSession, bobRevealSigResponse, aliceSigPacket),
              "Bob Reveal sig failed");
    
    if (serializeNegotiating)
        serializeAndDeserialize(aliceSession);
    
    
    ok(SecOTRSGetIsReadyForMessages(*bobSession), "Bob is ready");
    ok(SecOTRSGetIsReadyForMessages(*aliceSession), "Alice is ready");
    
    CFReleaseNull(aliceRestartPacket);
    CFReleaseNull(aliceRevealSigPacket);
    CFReleaseNull(aliceSigPacket);
    
    CFReleaseNull(bobDHKeyPacket);
    CFReleaseNull(bobRevealSigResponse);
    
    CFReleaseNull(hopefullyNothing);
    CFReleaseNull(empty);

}


#define sendMessagesCount(n) ((n) * 14)
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
        
        ok_status(SecOTRSSignAndProtectMessage(*aliceSession, rawBobToAlice, protectedBobToAlice), "encode reply");
        ok_status(SecOTRSVerifyAndExposeMessage(*bobSession, protectedBobToAlice, aliceDecode), "decode reply");
        
        if (serialize) {
            serializeAndDeserialize(bobSession);
            serializeAndDeserialize(aliceSession);
        }
        
        ok(CFDataGetLength(rawBobToAlice) == CFDataGetLength(aliceDecode)
           && 0 == memcmp(CFDataGetBytePtr(rawBobToAlice), CFDataGetBytePtr(aliceDecode), (size_t)CFDataGetLength(rawBobToAlice)), "reply matched");
        
        CFReleaseNull(rawAliceToBob);
        CFReleaseNull(rawBobToAlice);
        CFReleaseNull(protectedBobToAlice);
        CFReleaseNull(protectedAliceToBob);
        CFReleaseNull(aliceDecode);
        
        rawAliceToBob = CFDataCreate(kCFAllocatorDefault, (const uint8_t*)aliceToBob, (CFIndex) strlen(aliceToBob));
        protectedAliceToBob = CFDataCreateMutable(kCFAllocatorDefault, 0);
        bobDecode = CFDataCreateMutable(kCFAllocatorDefault, 0);
        
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
        
        bobToAlice = "i liked your silly message from me to you";
        rawBobToAlice = CFDataCreate(kCFAllocatorDefault, (const uint8_t*)bobToAlice, (CFIndex) strlen(bobToAlice));
        protectedBobToAlice = CFDataCreateMutable(kCFAllocatorDefault, 0);
        aliceDecode = CFDataCreateMutable(kCFAllocatorDefault, 0);
        
        ok_status(SecOTRSSignAndProtectMessage(*aliceSession, rawBobToAlice, protectedBobToAlice), "encode reply");
        ok_status(SecOTRSVerifyAndExposeMessage(*bobSession, protectedBobToAlice, aliceDecode), "decode reply");
        
        if (serialize) {
            serializeAndDeserialize(bobSession);
            serializeAndDeserialize(aliceSession);
        }
        
        ok(CFDataGetLength(rawBobToAlice) == CFDataGetLength(aliceDecode)
           && 0 == memcmp(CFDataGetBytePtr(rawBobToAlice), CFDataGetBytePtr(aliceDecode), (size_t)CFDataGetLength(rawBobToAlice)), "reply matched");
        
        CFReleaseNull(rawAliceToBob);
        CFReleaseNull(rawBobToAlice);
        CFReleaseNull(protectedBobToAlice);
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

#define kNegotiateTestCount (14 + sendMessagesCount(1) * 4 \
+ renegotiateTests)

static void negotiate(SecOTRSessionRef* aliceSession, SecOTRSessionRef* bobSession, SecOTRSessionRef *brandNewBobSession, bool serializeNegotiating, bool serializeMessaging, bool textMode, bool compact)
{
    const int kEmptyMessageSize = textMode ? 6 : 0;
    
    // Step 1: Create a start packet for each side of the transaction
    CFMutableDataRef bobStartPacket = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSAppendStartPacket(*bobSession, bobStartPacket), "Bob start packet");
    
    if (serializeNegotiating)
        serializeAndDeserialize(bobSession);
    
    CFMutableDataRef aliceStartPacket = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSAppendStartPacket(*aliceSession, aliceStartPacket), "Alice start packet");
    
    if (serializeNegotiating)
        serializeAndDeserialize(aliceSession);
    
    // Step 2: Exchange the start packets, forcing the DH commit messages to collide
    CFMutableDataRef aliceDHKeyResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*aliceSession, bobStartPacket, aliceDHKeyResponse),
              "Bob DH packet failed");
    
    if (serializeNegotiating)
        serializeAndDeserialize(aliceSession);
    
    CFReleaseNull(bobStartPacket);
    
    CFMutableDataRef bobDHKeyResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*bobSession, aliceStartPacket, bobDHKeyResponse),
              "Alice DH packet failed");
    
    if (serializeNegotiating)
        serializeAndDeserialize(bobSession);
    
    CFReleaseNull(aliceStartPacket);
    
    // Step 3: With one "real" DH key message, and one replayed DH commit message, try to get a "reveal sig" out of one side
    
    CFMutableDataRef bobRevealSigResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*bobSession, aliceDHKeyResponse, bobRevealSigResponse),
              "Alice DH Key packet failed");
    
    if (serializeNegotiating)
        serializeAndDeserialize(bobSession);
    
    CFReleaseNull(aliceDHKeyResponse);
    
    CFMutableDataRef aliceRevealSigResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*aliceSession, bobDHKeyResponse, aliceRevealSigResponse),
              "Bob DH Key packet failed");
    
    if (serializeNegotiating)
        serializeAndDeserialize(aliceSession);
    
    CFReleaseNull(bobDHKeyResponse);
    
    // Step 4: Having gotten the reveal signature, now work for the signature
    
    CFMutableDataRef aliceSigResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*aliceSession, bobRevealSigResponse, aliceSigResponse),
              "Bob Reveal sig failed");
    
    if (serializeNegotiating)
        serializeAndDeserialize(aliceSession);
    
    CFReleaseNull(bobRevealSigResponse);
    
    CFMutableDataRef bobSigResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*bobSession, aliceRevealSigResponse, bobSigResponse),
              "Alice Reveal sig failed");
    
    if (serializeNegotiating)
        serializeAndDeserialize(bobSession);
    
    CFReleaseNull(aliceRevealSigResponse);
    
    // Step 5: All the messages have been sent, now deal with any replays from the collision handling
    CFMutableDataRef bobFinalResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*bobSession, aliceSigResponse, bobFinalResponse),
              "Alice Final Sig failed");
    
    if (serializeNegotiating)
        serializeAndDeserialize(bobSession);
    
    CFMutableDataRef aliceFinalResponse = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSProcessPacket(*aliceSession, bobSigResponse, aliceFinalResponse),
              "Bob Final Sig failed");
    
    is(kEmptyMessageSize, CFDataGetLength(aliceFinalResponse), "Alice had nothing left to say");
    CFReleaseNull(aliceFinalResponse);
    CFReleaseNull(bobSigResponse);
    
    if (serializeNegotiating)
        serializeAndDeserialize(aliceSession);
    
    is(kEmptyMessageSize, CFDataGetLength(bobFinalResponse), "Bob had nothing left to say");
    ok(SecOTRSGetIsReadyForMessages(*bobSession), "Bob is ready");
    ok(SecOTRSGetIsReadyForMessages(*aliceSession), "Alice is ready");
    
    CFReleaseNull(aliceSigResponse);
    CFReleaseNull(bobFinalResponse);
    
    sendMessages(1, bobSession, aliceSession, serializeMessaging);
    sendMessages(1, aliceSession, bobSession, serializeMessaging);
    sendMessages(1, bobSession, aliceSession, serializeMessaging);
    sendMessages(1, aliceSession, bobSession, serializeMessaging);
    
    const char *bobToAlice = "i liked your silly message from me to you";
    CFDataRef rawBobToAlice = CFDataCreate(kCFAllocatorDefault, (const uint8_t*)bobToAlice, (CFIndex) strlen(bobToAlice));
    CFMutableDataRef protectedBobToAlice = CFDataCreateMutable(kCFAllocatorDefault, 0);

    ok_status(SecOTRSSignAndProtectMessage(*bobSession, rawBobToAlice, protectedBobToAlice), "protected bob to alice message");

    CFReleaseNull(rawBobToAlice);

    renegotiate(brandNewBobSession, aliceSession, serializeNegotiating, protectedBobToAlice);

    CFReleaseNull(protectedBobToAlice);

}

#define kEdgeCaseTestCount (10 + kNegotiateTestCount * 2)

static void tryEdgeCases(uint32_t flags)
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
    
    SecOTRSessionRef aliceSession = SecOTRSessionCreateFromIDAndFlags(kCFAllocatorDefault, aliceID, bobPublicID, flags);
    SecOTRSessionRef bobSession = SecOTRSessionCreateFromIDAndFlags(kCFAllocatorDefault, bobID, alicePublicID, flags);
    SecOTRSessionRef brandNewBobSession = SecOTRSessionCreateFromIDAndFlags(kCFAllocatorDefault, bobID, alicePublicID, flags);
    SecOTRSessionRef brandNewBobSession2 = SecOTRSessionCreateFromIDAndFlags(kCFAllocatorDefault, bobID, alicePublicID, flags);
    SecOTRSessionRef brandNewBobSession3 = SecOTRSessionCreateFromIDAndFlags(kCFAllocatorDefault, bobID, alicePublicID, flags);
    
    ok(aliceSession, "create alice session");
    ok(bobSession, "create bob session");
    ok(brandNewBobSession, "create new bob session");
    
    
    // Release the IDs, sessions shouldn't need us to retain them for them.
    CFReleaseNull(aliceID);
    CFReleaseNull(bobID);
    
    CFReleaseNull(alicePublicID);
    CFReleaseNull(bobPublicID);
    
    negotiate(&aliceSession, &bobSession, &brandNewBobSession, true, true, false, flags & kSecOTRUseAppleCustomMessageFormat);
    
    negotiate(&aliceSession, &bobSession, &brandNewBobSession2, true, false, false, flags & kSecOTRUseAppleCustomMessageFormat);
    
    
    const char * testMessageString = "i liked your silly message from me to you";
    CFDataRef testMessage = CFDataCreate(kCFAllocatorDefault, (const uint8_t*)testMessageString, (CFIndex) strlen(testMessageString));
    CFMutableDataRef protectedTestMessage = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok_status(SecOTRSSignAndProtectMessage(aliceSession, testMessage, protectedTestMessage), "Make message");
    
    CFMutableDataRef hopefullyNothing = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    is(SecOTRSVerifyAndExposeMessage(brandNewBobSession3, protectedTestMessage, hopefullyNothing), errSecOTRNotReady, "Message Before Negotiating");
    
    /* cleanup keychain */
    ok(SecOTRFIPurgeAllFromKeychain(&testError),"cleanup keychain");
    SecMPLogError(testError);
    CFReleaseNull(testError);
    
    CFReleaseNull(aliceSession);
    CFReleaseNull(bobSession);
    
    CFReleaseNull(brandNewBobSession);
    CFReleaseNull(brandNewBobSession2);
    CFReleaseNull(brandNewBobSession3);
    CFReleaseNull(testMessage);
    CFReleaseNull(protectedTestMessage);
    CFReleaseNull(hopefullyNothing);
}


#define kTestTestCount (2 * kEdgeCaseTestCount)

static void tests()
{
    tryEdgeCases(0);
    tryEdgeCases(kSecOTRUseAppleCustomMessageFormat);
}

int otr_40_edgecases(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    tests();
    
    return 0;
}
