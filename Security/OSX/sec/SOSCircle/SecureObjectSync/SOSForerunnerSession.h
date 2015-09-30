/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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

/*!
 @header SOSForerunnerSession.h
 Describes interfaces for both requesting and approving forerunner sessions. A
 A forerunner session encapsulates the following control flow between two 
 devices, Requestor and Acceptor, when Requestor attempts to join a syncing 
 circle already inhabited by Acceptor.

 0. Requestor creates a requesting session containing PAKE key pair
 1. Requestor creates a packet to request membership in the syncing circle;
	Packet includes session public key
 2. Requestor sends RequestPacket to Acceptor using an interface of its 
	choosing
 3. Acceptor receives RequestPacket.
 4. Acceptor creates an approving session containing PAKE key pair with 
    RequestPacket
 5. Acceptor generates Secret, a six-digit code that never leaves Acceptor
 6. Acceptor generates ChallengePacket, derived from the public key in 
    RequestPacket and Secret
 7. Acceptor sends ChallengePacket to Requestor using an interface of its 
    choosing
 8. Requestor receives ChallengePacket
 9. Requestor asks User to enter Secret
10. Requestor creates ResponsePacket, derived from Secret and the public key 
    contained in ChallengePacket
11. Requestor sends ResponsePacket to Acceptor using an interface of its 
    choosing
12. Acceptor receives ResponsePacket
13. Acceptor validates ResponsePacket
14. Acceptor generates HSA2Code
14b. Acceptor encrypts and attests to the HSA2Code to its session key
15. Acceptor sends encrypted HSA2Code to Requestor using an interface of its
    choosing
16. Requestor receives encrypted HSA2Code
16b. Requestor decrypts and verifies HSA2Code
17. Requestor sends HSA2Code to Apple
18. Apple adds Requestor to trusted device list
19. Requestor generates Identity
20. Requestor applies to syncing circle with Identity
 */

#ifndef _SEC_SOSFORERUNNERSESSION_H_
#define _SEC_SOSFORERUNNERSESSION_H_

#include <sys/cdefs.h>
#include <os/base.h>
#include <os/object.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFError.h>

__BEGIN_DECLS

/*!
 @const SECFR_API_VERSION
 An API version that may be used during the preprocessing phase to determine
 which version of the API is being built against. This may be used to guard
 against breaking due to changes in the API that are not sync'ed against your
 project. For example, if version 20150424 adds a new method, 
 SOSFRSNewMethod(), you may guard your use of that method with
 
 #if SECFR_API_VERSION >= 20150424
 SOSFRSNewMethod();
 #endif // SECFR_API_VERSION >= 20150424
 */
#define SECFR_API_VERSION 20150424

/*!
 @type SOSForerunnerRequestorSessionRef
 An opaque type representing the requesting side of a session being used to
 enter the requestor into a syncing circle. The object has no thread affinity,
 but it is not safe to invoke methods on the same object from multiple threads
 concurrently.
 */
typedef struct __OpaqueSOSForerunnerRequestorSession
		*SOSForerunnerRequestorSessionRef;

/*!
 @function SOSForerunnerRequestorSessionGetTypeID

 @abstract
 Returns the type identifier for the requestor session class.

 @result
 A type identifier.
 */
OS_EXPORT OS_WARN_RESULT
CFTypeID
SOSForerunnerRequestorSessionGetTypeID(void);

/*!
 @function SOSForerunnerRequestorSessionCreate

 @abstract 
 Creates a new requesting session object to negotiate entry into a syncing
 circle.
 
 @param allocator
 The vestigal CoreFoundation allocator. Pass NULL or 
 {@link kCFAllocatorDefault}.
 
 @param username
 The AppleID for the account whose syncing circle is to be joined.
 
 @param dsid
 The DirectoryServices identifier for the AppleID given in {@param username}.

 @result
 A new session object. This object must be released with {@link CFRelease} when
 it is no longer needed.
 */
OS_EXPORT OS_MALLOC OS_OBJECT_RETURNS_RETAINED OS_WARN_RESULT
SOSForerunnerRequestorSessionRef
SOSForerunnerRequestorSessionCreate(CFAllocatorRef allocator,
		CFStringRef username, uint64_t dsid);

/*!
 @function SOSFRSCopyRequestPacket

 @abstract 
 Returns a request packet suitable for requesting to join a syncing circle.
 
 @param session
 The session from which to copy the request packet.
 
 @param error
 Upon unsuccessful return, an error object describing the failure condition. May
 be NULL.

 @result
 A new data object representing the request packet.
 */
OS_EXPORT OS_OBJECT_RETURNS_RETAINED OS_WARN_RESULT OS_NONNULL1
CFDataRef
SOSFRSCopyRequestPacket(SOSForerunnerRequestorSessionRef session,
		CFErrorRef *error);

/*!
 @function SOSFRSCopyResponsePacket

 @abstract
 Returns a response packet suitable for responding to a challenge to join a
 syncing circle.

 @param session
 The session from which to copy the response packet.
 
 @param challenge
 The challenge packet received from the approving device.
 
 @param secret
 The six-digit secret generated by the approving device and entered by the user 
 on the requesting device.
 
 @param peerInfo
 A dictionary containing information about the peer, such as GPS location, 
 device type, etc. Pass NULL for now. This contents of this dictionary will be
 defined at a future date.
 
 @param error
 Upon unsuccessful return, an error object describing the failure condition. May
 be NULL.

 @result
 A new data object representing the response packet.
 */
OS_EXPORT OS_OBJECT_RETURNS_RETAINED OS_WARN_RESULT OS_NONNULL1 OS_NONNULL2
OS_NONNULL3
CFDataRef
SOSFRSCopyResponsePacket(SOSForerunnerRequestorSessionRef session,
		CFDataRef challenge, CFStringRef secret, CFDictionaryRef peerInfo,
		CFErrorRef *error);

/*!
 @function SOSFRSCopyHSA2CodeFromPacket

 @abstract
 Returns the HSA2 join code from the encrypted packet sent by the approving 
 device.

 @param session
 The session from which to copy the HSA2 join code.

 @param hsa2packet
 The encrypted packet containing the HSA2 join code sent by the approving 
 device.

 @result
 A new data object representing the HSA2 join code.
 */
OS_EXPORT OS_OBJECT_RETURNS_RETAINED OS_WARN_RESULT OS_NONNULL1 OS_NONNULL2
CFDataRef
SOSFRSCopyHSA2CodeFromPacket(SOSForerunnerRequestorSessionRef session,
		CFDataRef hsa2packet, CFErrorRef *error);

/*!
 @function SOSFRSCopyDecryptedData

 @abstract
 Decrypts data received through the secured communication channel negotiated by
 the session.

 @param session
 The session that the encrypted data is associated with.
 
 @param encrypted
 The encrypted data received from the approving device.

 @result
 A new data object representing the decrypted data received from the approving
 device.
 */
OS_EXPORT OS_OBJECT_RETURNS_RETAINED OS_WARN_RESULT OS_NONNULL1 OS_NONNULL2
CFDataRef
SOSFRSCopyDecryptedData(SOSForerunnerRequestorSessionRef session,
		CFDataRef encrypted);

/*!
 @type SOSForerunnerAcceptorSessionRef
 An opaque type representing the accepting side of a session being used to
 enter a new requesting device into the syncing circle of which the acceptor is
 a member. The object has no thread affinity, but it is not safe to invoke 
 methods on the same object from multiple threads concurrently.
 */
typedef struct __OpaqueSOSForerunnerAcceptorSession
		*SOSForerunnerAcceptorSessionRef;

/*!
 @function SOSForerunnerAcceptorSessionGetTypeID

 @abstract
 Returns the type identifier for the acceptor session class.

 @result
 A type identifier.
 */
OS_EXPORT OS_WARN_RESULT
CFTypeID
SOSForerunnerAcceptorSessionGetTypeID(void);

/*!
 @function SOSForerunnerAcceptorSessionCreate

 @abstract
 Creates a new accepting session object to negotiate entry of a requesting 
 device into a syncing circle.

 @param allocator
 The vestigal CoreFoundation allocator. Pass NULL or
 {@link kCFAllocatorDefault}.
 
 @param username
 The AppleID for the account whose syncing circle is to be joined.
 
 @param dsid
 The DirectoryServices identifier for the AppleID given in {@param username}.
 
 @param circleSecret
 The six-digit secret generated to join the syncing circle.

 @result
 A new session object. This object must be released with {@link CFRelease} when
 it is no longer needed.
 */
OS_EXPORT OS_MALLOC OS_OBJECT_RETURNS_RETAINED OS_WARN_RESULT OS_NONNULL2
OS_NONNULL4
SOSForerunnerAcceptorSessionRef
SOSForerunnerAcceptorSessionCreate(CFAllocatorRef allocator,
		CFStringRef username, uint64_t dsid, CFStringRef circleSecret);

/*!
 @function SOSFASCopyChallengePacket

 @abstract
 Returns a challenge packet that a requesting device must satisfy to join the
 syncing circle of which the accepting device is a member.

 @param session
 The session from which to copy the challenge packet.
 
 @param requestorPacket
 The initial requestor packet received from the device requesting to join the
 circle.
 
 @param error
 Upon unsuccessful return, an error object describing the failure condition. May
 be NULL.

 @result
 A new data object representing the challenge packet.
 */
OS_EXPORT OS_OBJECT_RETURNS_RETAINED OS_WARN_RESULT OS_NONNULL1 OS_NONNULL2
CFDataRef
SOSFASCopyChallengePacket(SOSForerunnerAcceptorSessionRef session,
		CFDataRef requestorPacket, CFErrorRef *error);

/*!
 @function SOSFASCopyHSA2Packet

 @abstract
 Processes the packet sent in response to the challenge packet by the requesting
 device and, if the challenge is satisfied, arms auto-acceptance into the HSA2
 trusted device list and returns a packet containing the HSA2 join code to be
 sent to the requestor.

 @param session
 The session associated with the challenge that the response was sent to 
 satisfy.
 
 @param responsePacket
 The packet sent by the requestor in response to the challenge.
 
 @param hsa2Code
 The code for the requestor to use to join the HSA2 trusted device list.
 
 @param error
 Upon unsuccessful return, an error object describing the failure condition.
 Unlike the other interfaces in this API suite, this parameter cannot be NULL,
 as different error codes indicate different caller responsibilities.

 If the underlying error is EAGAIN, the caller may attempt to re-negotiate with
 the requesting device. If too many attempts are made to re-negotiate, EBADMSG
 will be returned. At this point, the caller may not attempt to create another
 HSA2 packet; the connection should be terminated and the session torn down.
 
 @result
 An encrypted packet containing the HSA2 join code. NULL in the event of 
 failure.
 */
OS_EXPORT OS_MALLOC OS_OBJECT_RETURNS_RETAINED OS_WARN_RESULT OS_NONNULL1
OS_NONNULL2 OS_NONNULL3 OS_NONNULL4
CFDataRef
SOSFASCopyHSA2Packet(SOSForerunnerAcceptorSessionRef session,
		CFDataRef responsePacket, CFDataRef hsa2Code, CFErrorRef *error);

/*!
 @function SOSFASCopyEncryptedData

 @abstract
 Encrypts data for transport over the negotiated session.

 @param session
 The session object for which to encrypt the given data.

 @param data
 The data to encrypt.

 @result
 The encrypted representation of {@param data}.
 */
OS_EXPORT OS_MALLOC OS_OBJECT_RETURNS_RETAINED OS_WARN_RESULT OS_NONNULL1
OS_NONNULL2
CFDataRef
SOSFASCopyEncryptedData(SOSForerunnerAcceptorSessionRef session,
		CFDataRef data);

__END_DECLS

#endif /* _SEC_SOSFORERUNNERSESSION_H_ */
