//
//  SecOTRSession.c
//  libsecurity_libSecOTR
//
//  Created by Mitch Adler on 2/22/11.
//  Copyright 2011 Apple Inc. All rights reserved.
//

#include "SecOTRSession.h"

#include "SecOTRMath.h"
#include "SecOTRDHKey.h"
#include "SecOTRSessionPriv.h"
#include "SecOTRPackets.h"
#include "SecOTRPacketData.h"

#include <utilities/SecCFWrappers.h>

#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFString.h>

#include <Security/SecBasePriv.h>
#include <Security/SecRandom.h>
#include <Security/SecBase64.h>

#include <AssertMacros.h>

#ifdef USECOMMONCRYPTO
#include <CommonCrypto/CommonHMAC.h>
#endif

#include <corecrypto/cchmac.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccsha1.h>

#include <string.h>
#include <stdlib.h>

#include <syslog.h>

#include "utilities/comparison.h"

CFGiblisFor(SecOTRSession);

static OTRMessageType SecOTRSGetMessageType(CFDataRef message)
{
    OTRMessageType type = kInvalidMessage;

    CFMutableDataRef decodedBytes = CFDataCreateMutable(kCFAllocatorDefault, 0);
    SecOTRGetIncomingBytes(message, decodedBytes);

    const uint8_t *bytes = CFDataGetBytePtr(decodedBytes);
    size_t size = CFDataGetLength(decodedBytes);

    require_noerr(ReadHeader(&bytes, &size, &type), fail);

fail:
    CFReleaseNull(decodedBytes);

    return type;
}

const char *SecOTRPacketTypeString(CFDataRef message)
{
    if (!message) return "NoMessage";
    switch (SecOTRSGetMessageType(message)) {
        case kDHMessage:                return "DHMessage (0x02)";
        case kDataMessage:              return "DataMessage (0x03)";
        case kDHKeyMessage:             return "DHKeyMessage (0x0A)";
        case kRevealSignatureMessage:   return "RevealSignatureMessage (0x11)";
        case kSignatureMessage:         return "SignatureMessage (0x12)";
        case kInvalidMessage:           return "InvalidMessage (0xFF)";
        default:                        return "UnknownMessage";
    }
}

static const char *SecOTRAuthStateString(SecOTRAuthState authState)
{
    switch (authState) {
        case kIdle:                     return "Idle";
        case kAwaitingDHKey:            return "AwaitingDHKey";
        case kAwaitingRevealSignature:  return "AwaitingRevealSignature";
        case kAwaitingSignature:        return "AwaitingSignature";
        case kDone:                     return "Done";
        default:                        return "InvalidState";
    }
}

static CF_RETURNS_RETAINED CFStringRef SecOTRSessionCopyDescription(CFTypeRef cf) {
    SecOTRSessionRef session = (SecOTRSessionRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR("<%s %s%s%s%s %d:%d %s%s>"),
                                    SecOTRAuthStateString(session->_state),
                                    session->_me ? "F" : "-",
                                    session->_them ? "P" : "-",
                                    session->_receivedDHMessage ? "D" : "-",
                                    session->_receivedDHKeyMessage ? "K" : "-",
                                    session->_keyID,
                                    session->_theirKeyID,
                                    session->_theirPreviousKey ? "P" : "-",
                                    session->_theirKey ? "T" : "-");
}

static void SecOTRSessionDestroy(CFTypeRef cf) {
    SecOTRSessionRef session = (SecOTRSessionRef)cf;

    CFReleaseNull(session->_receivedDHMessage);
    CFReleaseNull(session->_receivedDHKeyMessage);

    CFReleaseNull(session->_me);
    CFReleaseNull(session->_myKey);
    CFReleaseNull(session->_myNextKey);

    CFReleaseNull(session->_them);
    CFReleaseNull(session->_theirKey);
    CFReleaseNull(session->_theirPreviousKey);

    CFReleaseNull(session->_macKeysToExpose);

    dispatch_release(session->_queue);
}

static void SecOTRSessionResetInternal(SecOTRSessionRef session)
{
    session->_state = kIdle;
    
    CFReleaseNull(session->_receivedDHMessage);
    CFReleaseNull(session->_receivedDHKeyMessage);

    session->_keyID = 0;
    CFReleaseNull(session->_myKey);
    CFReleaseNull(session->_myNextKey);
    //session->_myNextKey = SecOTRFullDHKCreate(kCFAllocatorDefault);
    session->_theirKeyID = 0;
    CFReleaseNull(session->_theirKey);
    CFReleaseNull(session->_theirPreviousKey);
    CFReleaseNull(session->_macKeysToExpose);
    session->_macKeysToExpose = CFDataCreateMutable(kCFAllocatorDefault, 0);

    bzero(session->_keyCache, sizeof(session->_keyCache));
}

void SecOTRSessionReset(SecOTRSessionRef session)
{
    dispatch_sync_f(session->_queue, session, (dispatch_function_t) SecOTRSessionResetInternal);
}


SecOTRSessionRef SecOTRSessionCreateFromID(CFAllocatorRef allocator,
                                           SecOTRFullIdentityRef myID,
                                           SecOTRPublicIdentityRef theirID)
{
    SecOTRSessionRef newID = CFTypeAllocate(SecOTRSession, struct _SecOTRSession, allocator);

    newID->_queue = dispatch_queue_create("OTRSession", DISPATCH_QUEUE_SERIAL);

    newID->_me = myID;
    newID->_them = theirID;
    newID->_receivedDHMessage = NULL;
    newID->_receivedDHKeyMessage = NULL;
    newID->_myKey = NULL;
    newID->_myNextKey = NULL;
    newID->_theirKey = NULL;
    newID->_theirPreviousKey = NULL;
    newID->_macKeysToExpose = NULL;
    newID->_textOutput = false;

    SecOTRSessionResetInternal(newID);

    CFRetain(newID->_me);
    CFRetain(newID->_them);

    return newID;
}

SecOTRSessionRef SecOTRSessionCreateFromIDAndFlags(CFAllocatorRef allocator,
                                           SecOTRFullIdentityRef myID,
                                           SecOTRPublicIdentityRef theirID,
                                           uint32_t flags)
{
    SecOTRSessionRef newID = SecOTRSessionCreateFromID(allocator, myID, theirID);
    if (flags & kSecOTRSendTextMessages) {
        newID->_textOutput = true;
    }
    return newID;
}

static uint64_t constant_zero = 0;

static void SecOTRSFindKeysForMessage(SecOTRSessionRef session,
                                      SecOTRFullDHKeyRef myKey,
                                      SecOTRPublicDHKeyRef theirKey,
                                      bool sending,
                                      uint8_t** messageKey, uint8_t** macKey, uint64_t **counter)
{
    SecOTRCacheElement* emptyKeys = NULL;
    SecOTRCacheElement* cachedKeys = NULL;
    
    if ((NULL == myKey) || (NULL == theirKey)) {
        if (messageKey)
            *messageKey = NULL;
        if (macKey)
            *macKey = NULL;
        if (counter)
            *counter = &constant_zero;
            
        return;
    }
    
    for(int i = 0; i < kOTRKeyCacheSize; ++i)
    {
        if (0 == constant_memcmp(session->_keyCache[i]._fullKeyHash, SecFDHKGetHash(myKey), CCSHA1_OUTPUT_SIZE)
         && (0 == constant_memcmp(session->_keyCache[i]._publicKeyHash, SecPDHKGetHash(theirKey), CCSHA1_OUTPUT_SIZE))) {
            cachedKeys = &session->_keyCache[i];
            break;
        }

        if (emptyKeys == NULL
         && session->_keyCache[i]._fullKey == NULL) {
            emptyKeys = &session->_keyCache[i];
        }
    }

    if (cachedKeys == NULL) {
        if (emptyKeys == NULL) {
            syslog(LOG_ERR, "SecOTRSession key cache was full. Should never happen, spooky.\n");
            emptyKeys = &session->_keyCache[0];
        }

        // Fill in the entry.
        emptyKeys->_fullKey = myKey;
        memcpy(emptyKeys->_fullKeyHash, SecFDHKGetHash(myKey), CCSHA1_OUTPUT_SIZE);
        emptyKeys->_publicKey = theirKey;
        memcpy(emptyKeys->_publicKeyHash, SecPDHKGetHash(theirKey), CCSHA1_OUTPUT_SIZE);
        
        emptyKeys->_counter = 0;
        emptyKeys->_theirCounter = 0;

        SecOTRDHKGenerateOTRKeys(emptyKeys->_fullKey, emptyKeys->_publicKey,
                              emptyKeys->_sendEncryptionKey, emptyKeys->_sendMacKey,
                              emptyKeys->_receiveEncryptionKey, emptyKeys->_receiveMacKey);

        cachedKeys = emptyKeys;
    }
    
    if (messageKey)
        *messageKey = sending ? cachedKeys->_sendEncryptionKey : cachedKeys->_receiveEncryptionKey;
    if (macKey)
        *macKey = sending ? cachedKeys->_sendMacKey : cachedKeys->_receiveMacKey;
    if (counter)
        *counter = sending ? &cachedKeys->_counter : &cachedKeys->_theirCounter;
}

SecOTRSessionRef SecOTRSessionCreateFromData(CFAllocatorRef allocator, CFDataRef data)
{
    if (data == NULL)
        return NULL;

    SecOTRSessionRef result = NULL;
    SecOTRSessionRef session = CFTypeAllocate(SecOTRSession, struct _SecOTRSession, allocator);

    const uint8_t *bytes = CFDataGetBytePtr(data);
    size_t size = (size_t)CFDataGetLength(data);

    session->_queue = dispatch_queue_create("OTRSession", DISPATCH_QUEUE_SERIAL);

    session->_me = NULL;
    session->_them = NULL;
    session->_myKey = NULL;
    session->_myNextKey = NULL;
    session->_theirKey = NULL;
    session->_theirPreviousKey = NULL;
    session->_receivedDHMessage = NULL;
    session->_receivedDHKeyMessage = NULL;
    bzero(session->_keyCache, sizeof(session->_keyCache));

    uint8_t version;
    require_noerr(ReadByte(&bytes, &size, &version), fail);
    require(version <= 3, fail);

    require_noerr(ReadLong(&bytes, &size, &session->_state), fail);
    session->_me = SecOTRFullIdentityCreateFromBytes(kCFAllocatorDefault, &bytes, &size, NULL);
    require(session->_me != NULL, fail);
    session->_them = SecOTRPublicIdentityCreateFromBytes(kCFAllocatorDefault, &bytes, &size, NULL);
    require(session->_them != NULL, fail);
    
    require(size > sizeof(session->_r), fail);
    memcpy(session->_r, bytes, sizeof(session->_r));
    bytes += sizeof(session->_r);
    size -= sizeof(session->_r);

    {
        uint8_t hasMessage = false;
        ReadByte(&bytes, &size, &hasMessage);
        if (hasMessage) {
            session->_receivedDHMessage = CFDataCreateMutableFromOTRDATA(kCFAllocatorDefault, &bytes, &size);
        }
    }

    if (version >= 2) {
        uint8_t hasMessage = false;
        ReadByte(&bytes, &size, &hasMessage);
        if (hasMessage) {
            session->_receivedDHKeyMessage = CFDataCreateMutableFromOTRDATA(kCFAllocatorDefault, &bytes, &size);
        }
    }

    if (version < 3) {
        uint8_t ready;
        require_noerr(ReadByte(&bytes, &size, &ready), fail);
        if (ready && session->_state == kIdle)
            session->_state = kDone;
    }

    require_noerr(ReadLong(&bytes, &size, &session->_keyID), fail);
    if (session->_keyID > 0) {
        session->_myKey = SecOTRFullDHKCreateFromBytes(kCFAllocatorDefault, &bytes, &size);
        require(session->_myKey != NULL, fail);
        session->_myNextKey = SecOTRFullDHKCreateFromBytes(kCFAllocatorDefault, &bytes, &size);
        require(session->_myNextKey != NULL, fail);
    }
    
    require_noerr(ReadLong(&bytes, &size, &session->_theirKeyID), fail);
    if (session->_theirKeyID > 0) {
        if (session->_theirKeyID > 1) {
            session->_theirPreviousKey = SecOTRPublicDHKCreateFromSerialization(kCFAllocatorDefault, &bytes, &size);
            require(session->_theirPreviousKey != NULL, fail);
        }
        session->_theirKey = SecOTRPublicDHKCreateFromSerialization(kCFAllocatorDefault, &bytes, &size);
        require(session->_theirKey != NULL, fail);
    }

    uint64_t *counter;
    SecOTRSFindKeysForMessage(session, session->_myKey, session->_theirKey, false, NULL, NULL, &counter);
    require_noerr(ReadLongLong(&bytes, &size, counter), fail);
    SecOTRSFindKeysForMessage(session, session->_myKey, session->_theirKey, true, NULL, NULL, &counter);
    require_noerr(ReadLongLong(&bytes, &size, counter), fail);
    SecOTRSFindKeysForMessage(session, session->_myKey, session->_theirPreviousKey, false, NULL, NULL, &counter);
    require_noerr(ReadLongLong(&bytes, &size, counter), fail);
    SecOTRSFindKeysForMessage(session, session->_myKey, session->_theirPreviousKey, true, NULL, NULL, &counter);
    require_noerr(ReadLongLong(&bytes, &size, counter), fail);
    SecOTRSFindKeysForMessage(session, session->_myNextKey, session->_theirKey, false, NULL, NULL, &counter);
    require_noerr(ReadLongLong(&bytes, &size, counter), fail);
    SecOTRSFindKeysForMessage(session, session->_myNextKey, session->_theirKey, true, NULL, NULL, &counter);
    require_noerr(ReadLongLong(&bytes, &size, counter), fail);
    SecOTRSFindKeysForMessage(session, session->_myNextKey, session->_theirPreviousKey, false, NULL, NULL, &counter);
    require_noerr(ReadLongLong(&bytes, &size, counter), fail);
    SecOTRSFindKeysForMessage(session, session->_myNextKey, session->_theirPreviousKey, true, NULL, NULL, &counter);
    require_noerr(ReadLongLong(&bytes, &size, counter), fail);
    
    session->_macKeysToExpose = CFDataCreateMutableFromOTRDATA(kCFAllocatorDefault, &bytes, &size);
    require(session->_macKeysToExpose != NULL, fail);
    
    uint8_t textMode;
    require_noerr(ReadByte(&bytes, &size, &textMode), fail);
    session->_textOutput = (textMode != 0);

    result = session;
    session = NULL;

fail:
    CFReleaseNull(session);
    return result;
}


OSStatus SecOTRSAppendSerialization(SecOTRSessionRef session, CFMutableDataRef serializeInto)
{
    __block OSStatus result  = errSecParam;

    require(session, abort);
    require(serializeInto, abort);

    CFIndex start = CFDataGetLength(serializeInto);

    dispatch_sync(session->_queue, ^{
        const uint8_t version = 3;

        CFDataAppendBytes(serializeInto, &version, sizeof(version));

        AppendLong(serializeInto, session->_state);

        result = (SecOTRFIAppendSerialization(session->_me, serializeInto, NULL)) ? errSecSuccess : errSecParam;
    
        if (result == errSecSuccess) {
            result = (SecOTRPIAppendSerialization(session->_them, serializeInto, NULL)) ? errSecSuccess : errSecParam;
        }
    
        if (result == errSecSuccess) {
            CFDataAppendBytes(serializeInto, session->_r, sizeof(session->_r));

            if (session->_receivedDHMessage == NULL) {
                AppendByte(serializeInto, 0);
            } else {
                AppendByte(serializeInto, 1);
                AppendCFDataAsDATA(serializeInto, session->_receivedDHMessage);
            }

            if (session->_receivedDHKeyMessage == NULL) {
                AppendByte(serializeInto, 0);
            } else {
                AppendByte(serializeInto, 1);
                AppendCFDataAsDATA(serializeInto, session->_receivedDHKeyMessage);
            }

            AppendLong(serializeInto, session->_keyID);
            if (session->_keyID > 0) {
                SecFDHKAppendSerialization(session->_myKey, serializeInto);
                SecFDHKAppendSerialization(session->_myNextKey, serializeInto);
            }

            AppendLong(serializeInto, session->_theirKeyID);
            if (session->_theirKeyID > 0) {
                if (session->_theirKeyID > 1) {
                    SecPDHKAppendSerialization(session->_theirPreviousKey, serializeInto);
                }
                SecPDHKAppendSerialization(session->_theirKey, serializeInto);
            }
            
            uint64_t *counter;
            SecOTRSFindKeysForMessage(session, session->_myKey, session->_theirKey, false, NULL, NULL, &counter);
            AppendLongLong(serializeInto, *counter);
            SecOTRSFindKeysForMessage(session, session->_myKey, session->_theirKey, true, NULL, NULL, &counter);
            AppendLongLong(serializeInto, *counter);
            SecOTRSFindKeysForMessage(session, session->_myKey, session->_theirPreviousKey, false, NULL, NULL, &counter);
            AppendLongLong(serializeInto, *counter);
            SecOTRSFindKeysForMessage(session, session->_myKey, session->_theirPreviousKey, true, NULL, NULL, &counter);
            AppendLongLong(serializeInto, *counter);
            SecOTRSFindKeysForMessage(session, session->_myNextKey, session->_theirKey, false, NULL, NULL, &counter);
            AppendLongLong(serializeInto, *counter);
            SecOTRSFindKeysForMessage(session, session->_myNextKey, session->_theirKey, true, NULL, NULL, &counter);
            AppendLongLong(serializeInto, *counter);
            SecOTRSFindKeysForMessage(session, session->_myNextKey, session->_theirPreviousKey, false, NULL, NULL, &counter);
            AppendLongLong(serializeInto, *counter);
            SecOTRSFindKeysForMessage(session, session->_myNextKey, session->_theirPreviousKey, true, NULL, NULL, &counter);
            AppendLongLong(serializeInto, *counter);

            AppendCFDataAsDATA(serializeInto, session->_macKeysToExpose);
            
            AppendByte(serializeInto, session->_textOutput ? 1 : 0);
        }
    });

    if (result != errSecSuccess)
        CFDataSetLength(serializeInto, start);

abort:
    return result;
}


bool SecOTRSGetIsReadyForMessages(SecOTRSessionRef session)
{
    __block bool result;

    dispatch_sync(session->_queue, ^{ result = session->_state == kDone; });

    return result;
}

bool SecOTRSGetIsIdle(SecOTRSessionRef session)
{
    __block bool result;
    
    dispatch_sync(session->_queue, ^{ result = session->_state == kIdle; });
    
    return result;
}

static void SecOTRSExpireCachedKeysForFullKey(SecOTRSessionRef session, SecOTRFullDHKeyRef myKey)
{
    for(int i = 0; i < kOTRKeyCacheSize; ++i)
    {
        if (0 == constant_memcmp(session->_keyCache[i]._fullKeyHash, SecFDHKGetHash(myKey), CCSHA1_OUTPUT_SIZE)) {
            CFDataAppendBytes(session->_macKeysToExpose, session->_keyCache[i]._receiveMacKey, sizeof(session->_keyCache[i]._receiveMacKey));

            bzero(&session->_keyCache[i], sizeof(session->_keyCache[i]));
        }
    }
}

static void SecOTRSExpireCachedKeysForPublicKey(SecOTRSessionRef session, SecOTRPublicDHKeyRef theirKey)
{
    for(int i = 0; i < kOTRKeyCacheSize; ++i)
    {
        if (0 == constant_memcmp(session->_keyCache[i]._publicKeyHash, SecPDHKGetHash(theirKey), CCSHA1_OUTPUT_SIZE)) {
            CFDataAppendBytes(session->_macKeysToExpose, session->_keyCache[i]._receiveMacKey, sizeof(session->_keyCache[i]._receiveMacKey));

            bzero(&session->_keyCache[i], sizeof(session->_keyCache[i]));
        }
    }
}

static void SecOTRSPrecalculateForPair(SecOTRSessionRef session,
                                       SecOTRFullDHKeyRef myKey,
                                       SecOTRPublicDHKeyRef theirKey)
{
    if (myKey == NULL || theirKey == NULL)
        return;

    SecOTRSFindKeysForMessage(session, myKey, theirKey, true, NULL, NULL, NULL);
    SecOTRSFindKeysForMessage(session, myKey, theirKey, false, NULL, NULL, NULL);
}

static void SecOTRSPrecalculateKeysInternal(SecOTRSessionRef session)
{
    SecOTRSPrecalculateForPair(session, session->_myKey, session->_theirKey);
    SecOTRSPrecalculateForPair(session, session->_myNextKey, session->_theirKey);
    SecOTRSPrecalculateForPair(session, session->_myKey, session->_theirPreviousKey);
    SecOTRSPrecalculateForPair(session, session->_myNextKey, session->_theirPreviousKey);
}

void SecOTRSPrecalculateKeys(SecOTRSessionRef session)
{
    dispatch_sync_f(session->_queue, session, (dispatch_function_t) SecOTRSPrecalculateKeysInternal);
}

enum SecOTRSMessageKind SecOTRSGetMessageKind(SecOTRSessionRef session, CFDataRef message)
{
    enum SecOTRSMessageKind kind = kOTRUnknownPacket;

    CFMutableDataRef decodedBytes = CFDataCreateMutable(kCFAllocatorDefault, 0);
    SecOTRGetIncomingBytes(message, decodedBytes);
    
    const uint8_t *bytes = CFDataGetBytePtr(decodedBytes);
    size_t size = CFDataGetLength(decodedBytes);

    OTRMessageType type;
    require_noerr(ReadHeader(&bytes, &size, &type), fail);
    
    kind = (type == kDataMessage) ? kOTRDataPacket : kOTRNegotiationPacket;

fail:
    CFReleaseNull(decodedBytes);
    
    return kind;
}

OSStatus SecOTRSSignAndProtectMessage(SecOTRSessionRef session,
                                      CFDataRef sourceMessage,
                                      CFMutableDataRef protectedMessage)
{
    __block OSStatus result = errSecParam;

    require(session, abort);
    require(sourceMessage, abort);
    require(protectedMessage, abort);

    dispatch_sync(session->_queue, ^{
        if (session->_myKey == NULL ||
            session->_theirKey == NULL) {
            return;
        }
        
        CFMutableDataRef destinationMessage;
        if (session->_textOutput) {
            destinationMessage = CFDataCreateMutable(kCFAllocatorDefault, 0);
        } else {
            destinationMessage = protectedMessage;
        }

        uint8_t *messageKey;
        uint8_t *macKey;
        uint64_t *counter;

        CFIndex start = CFDataGetLength(destinationMessage);

        SecOTRSFindKeysForMessage(session, session->_myKey, session->_theirKey,
                                  true,
                                  &messageKey, &macKey, &counter);

        AppendHeader(destinationMessage, kDataMessage);
        AppendByte(destinationMessage, 0); // Flags, all zero

        AppendLong(destinationMessage, session->_keyID);
        AppendLong(destinationMessage, session->_theirKeyID);
        SecFDHKAppendPublicSerialization(session->_myNextKey, destinationMessage);
        AppendLongLong(destinationMessage, ++*counter);

        CFIndex sourceSize = CFDataGetLength(sourceMessage);
        assert(((unsigned long)sourceSize)<=UINT32_MAX); /* this is correct as long as CFIndex is a signed long */
        AppendLong(destinationMessage, (uint32_t)sourceSize);
        uint8_t* encryptedDataPointer = CFDataIncreaseLengthAndGetMutableBytes(destinationMessage, sourceSize);
        AES_CTR_HighHalf_Transform(kOTRMessageKeyBytes, messageKey,
                                   *counter,
                                   (size_t)sourceSize, CFDataGetBytePtr(sourceMessage),
                                   encryptedDataPointer);

        CFIndex macedContentsSize = CFDataGetLength(destinationMessage) - start;
        CFIndex macSize = CCSHA1_OUTPUT_SIZE;
        uint8_t* macDataPointer = CFDataIncreaseLengthAndGetMutableBytes(destinationMessage, macSize);

#ifdef USECOMMONCRYPTO
        CCHmac(kCCHmacAlgSHA1,
               macKey, kOTRMessageMacKeyBytes,
               CFDataGetBytePtr(destinationMessage) + start, (size_t)macedContentsSize,
               macDataPointer);
#else
        cchmac(ccsha1_di(),
               kOTRMessageMacKeyBytes, macKey,
               macedContentsSize, CFDataGetBytePtr(destinationMessage) + start,
               macDataPointer);
#endif

        CFDataAppend(destinationMessage, session->_macKeysToExpose);

        CFDataSetLength(session->_macKeysToExpose, 0);
        
        if (session->_textOutput) {
            SecOTRPrepareOutgoingBytes(destinationMessage, protectedMessage);
            CFReleaseSafe(destinationMessage);
        }

        result = errSecSuccess;
    });

abort:
    return result;
}

OSStatus SecOTRSVerifyAndExposeMessage(SecOTRSessionRef session,
                                       CFDataRef incomingMessage,
                                       CFMutableDataRef exposedMessageContents)
{
    __block SecOTRPublicDHKeyRef newKey = NULL;
    __block OSStatus result = errSecParam;


    require(session, abort);
    require(incomingMessage, abort);
    require(exposedMessageContents, abort);

    dispatch_sync(session->_queue, ^{
        const uint8_t* bytes;
        size_t  size;
        CFMutableDataRef decodedBytes = CFDataCreateMutable(kCFAllocatorDefault, 0);
        SecOTRGetIncomingBytes(incomingMessage, decodedBytes);
        
        bytes = CFDataGetBytePtr(decodedBytes);
        size = CFDataGetLength(decodedBytes);

        const uint8_t* macDataStart = bytes;

        uint32_t theirID;
        uint32_t myID;

        if ((result = ReadAndVerifyHeader(&bytes, &size, kDataMessage))){
            CFReleaseSafe(decodedBytes);
            return;
        }

        if (size <= 0) { result = errSecDecode;  CFReleaseSafe(decodedBytes); return; }

        if ((result = ReadAndVerifyByte(&bytes, &size, 0))) {  CFReleaseSafe(decodedBytes); return;} // No flags

        if ((result = ReadLong(&bytes, &size, &theirID))){  CFReleaseSafe(decodedBytes); return; }

        if (theirID != session->_theirKeyID &&
            (session->_theirPreviousKey == NULL || theirID != (session->_theirKeyID - 1)))
        {
            result = ((theirID + 1) < session->_theirKeyID) ? errSecOTRTooOld : errSecOTRIDTooNew;
            CFReleaseSafe(decodedBytes);
            return;
        };

        if ((result = ReadLong(&bytes, &size, &myID))){  CFReleaseSafe(decodedBytes); return; }
        if (myID != session->_keyID && myID != (session->_keyID + 1))
        {
            result = (myID < session->_keyID) ? errSecOTRTooOld : errSecOTRIDTooNew;
            CFReleaseSafe(decodedBytes);
            return;
        };


        // Choose appripriate keys for message:
        {
            uint8_t *messageKey;
            uint8_t *macKey;
            uint64_t *theirCounter;

            SecOTRFullDHKeyRef myKeyForMessage = (myID == session->_keyID) ? session->_myKey : session->_myNextKey;
            SecOTRPublicDHKeyRef theirKeyForMessage = (theirID == session->_theirKeyID) ? session->_theirKey : session->_theirPreviousKey;

            SecOTRSFindKeysForMessage(session, myKeyForMessage, theirKeyForMessage, false,
                                      &messageKey, &macKey, &theirCounter);

            size_t nextKeyMPISize;
            const uint8_t* nextKeyMPIBytes;
            if ((result = SizeAndSkipMPI(&bytes, &size, &nextKeyMPIBytes, &nextKeyMPISize))){  CFReleaseSafe(decodedBytes); return;}

            uint64_t counter;
            if ((result = ReadLongLong(&bytes, &size, &counter))) {  CFReleaseSafe(decodedBytes); return; }

            if (counter <= *theirCounter) { result = errSecOTRTooOld;  CFReleaseSafe(decodedBytes); return; };

            size_t messageSize;
            const uint8_t* messageStart;
            if ((result = SizeAndSkipDATA(&bytes, &size, &messageStart, &messageSize))) {  CFReleaseSafe(decodedBytes); return; }

            size_t macDataSize = (bytes - macDataStart) ? (size_t)(bytes - macDataStart) : 0;
            uint8_t mac[CCSHA1_OUTPUT_SIZE];
            if (sizeof(mac) > size) { result = errSecDecode;  CFReleaseSafe(decodedBytes); return; }

#ifdef USECOMMONCRYPTO
            CCHmac(kCCHmacAlgSHA1,
                   macKey, kOTRMessageMacKeyBytes,
                   macDataStart, macDataSize,
                   mac);
#else
            cchmac(ccsha1_di(),
                   kOTRMessageMacKeyBytes, macKey,
                   macDataSize, macDataStart,
                   mac);
#endif

            if (0 != constant_memcmp(mac, bytes, sizeof(mac))) { result = errSecAuthFailed;  CFReleaseSafe(decodedBytes); return; }
            //if (messageSize > 65535) { result = errSecDataTooLarge;  CFReleaseSafe(decodedBytes); return; }
            uint8_t* dataSpace = CFDataIncreaseLengthAndGetMutableBytes(exposedMessageContents, (CFIndex)messageSize);


            AES_CTR_HighHalf_Transform(kOTRMessageKeyBytes, messageKey,
                                       counter,
                                       messageSize, messageStart,
                                       dataSpace);

            // Everything is good, accept the meta data.
            *theirCounter = counter;

            newKey = SecOTRPublicDHKCreateFromBytes(kCFAllocatorDefault, &nextKeyMPIBytes, &nextKeyMPISize);
        }

        SecOTRSPrecalculateKeysInternal(session);

        bool acceptTheirNewKey = newKey != NULL && theirID == session->_theirKeyID;

        if (acceptTheirNewKey) {
            if (session->_theirPreviousKey) {
                SecOTRSExpireCachedKeysForPublicKey(session, session->_theirPreviousKey);
            }

            CFReleaseNull(session->_theirPreviousKey);
            session->_theirPreviousKey = session->_theirKey;
            session->_theirKey = newKey;

            session->_theirKeyID += 1;

            newKey = NULL;
        }

        if (myID == (session->_keyID + 1)) {
            SecOTRSExpireCachedKeysForFullKey(session, session->_myKey);

            // Swap the keys so we know the current key.
            {
                SecOTRFullDHKeyRef oldKey = session->_myKey;
                session->_myKey = session->_myNextKey;
                session->_myNextKey = oldKey;
            }

            // Derive a new next key by regenerating over the old key.
            SecFDHKNewKey(session->_myNextKey);

            session->_keyID = myID;
        }
        CFReleaseSafe(decodedBytes);
    });

abort:
    CFReleaseNull(newKey);
    return result;
}


OSStatus SecOTRSEndSession(SecOTRSessionRef session,
                           CFMutableDataRef messageToSend)
{
    return errSecUnimplemented;
}
