//
//  SecOTRSession.h
//  libsecurity_libSecOTR
//
//  Created by Mitch Adler on 2/22/11.
//  Copyright 2011 Apple Inc. All rights reserved.
//

#ifndef _SECOTRSESSION_H_
#define _SECOTRSESSION_H_

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFData.h>

#include <Security/SecOTR.h>

__BEGIN_DECLS

// MARK: MessageTypes

enum SecOTRSMessageKind {
    kOTRNegotiationPacket,
    kOTRDataPacket,
    kOTRUnknownPacket
};

// MARK: OTR Session

enum SecOTRCreateFlags {
    kSecOTRSendTextMessages = 1, // OTR messages will be encoded as Base-64 with header/footer per the standard, not just given back in binary
};

/*!
 @typedef
 @abstract   OTRSessions encapsulate a commuincaiton between to parties using the
             otr protocol.
 @discussion Sessions start with IDs. One end sends a start packet (created with AppendStartPacket).
             Both sides process packets they exchange on the negotiation channel.
 */
typedef struct _SecOTRSession* SecOTRSessionRef;

SecOTRSessionRef SecOTRSessionCreateFromID(CFAllocatorRef allocator,
                                           SecOTRFullIdentityRef myID,
                                           SecOTRPublicIdentityRef theirID);

SecOTRSessionRef SecOTRSessionCreateFromIDAndFlags(CFAllocatorRef allocator,
                                           SecOTRFullIdentityRef myID,
                                           SecOTRPublicIdentityRef theirID,
                                           uint32_t flags);

SecOTRSessionRef SecOTRSessionCreateFromData(CFAllocatorRef allocator, CFDataRef data);

    void SecOTRSessionReset(SecOTRSessionRef session);
OSStatus SecOTRSAppendSerialization(SecOTRSessionRef publicID, CFMutableDataRef serializeInto);

OSStatus SecOTRSAppendStartPacket(SecOTRSessionRef session, CFMutableDataRef appendInitiatePacket);

OSStatus SecOTRSAppendRestartPacket(SecOTRSessionRef session, CFMutableDataRef appendPacket);

OSStatus SecOTRSProcessPacket(SecOTRSessionRef session,
                              CFDataRef incomingPacket,
                              CFMutableDataRef negotiationResponse);
    
OSStatus SecOTRSEndSession(SecOTRSessionRef session,
                           CFMutableDataRef messageToSend);


bool SecOTRSGetIsReadyForMessages(SecOTRSessionRef session);
bool SecOTRSGetIsIdle(SecOTRSessionRef session);

enum SecOTRSMessageKind SecOTRSGetMessageKind(SecOTRSessionRef session, CFDataRef incomingPacket);

/*!
 @function
 @abstract   Precalculates keys for current key sets to save time when sending or receiving.
 @param      session                OTRSession receiving message
 */
void SecOTRSPrecalculateKeys(SecOTRSessionRef session);
    
/*!
 @function
 @abstract   Encrypts and Signs a message with OTR credentials.
 @param      session                OTRSession receiving message
 @param      incomingMessage        Cleartext message to protect
 @param      protectedMessage       Data to append the encoded protected message to
 @result     OSStatus               errSecAuthFailed -> bad signature, no data appended.
 */

OSStatus SecOTRSSignAndProtectMessage(SecOTRSessionRef session,
                                      CFDataRef sourceMessage,
                                      CFMutableDataRef protectedMessage);

/*!
 @function
 @abstract   Verifies and exposes a message sent via OTR
 @param      session                OTRSession receiving message
 @param      incomingMessage        Encoded message
 @param      exposedMessageContents Data to append the exposed message to
 @result     OSStatus               errSecAuthFailed -> bad signature, no data appended.
 */

OSStatus SecOTRSVerifyAndExposeMessage(SecOTRSessionRef session,
                                       CFDataRef incomingMessage,
                                       CFMutableDataRef exposedMessageContents);



const char *SecOTRPacketTypeString(CFDataRef message);

__END_DECLS

#endif
