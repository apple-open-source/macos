/*
 * Copyright (c) 2012 Apple Computer, Inc. All Rights Reserved.
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

//
//  SOSCloudCircle.h
//

#ifndef _SECURITY_SOSCLOUDCIRCLE_H_
#define _SECURITY_SOSCLOUDCIRCLE_H_

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFError.h>

// #include <SecureObjectSync/SOSPeer.h>

__BEGIN_DECLS


//
// CFError info for propogated errors
//

extern CFStringRef kSOSErrorDomain;

enum {
    kSOSErrorPrivateKeyAbsent = 1,
    kSOSErrorPublicKeyAbsent = 2,

    kSOSErrorWrongPassword = 3,

    kSOSErrorNotReady = 4, // System not yet ready (before first unlock)

    kSOSErrorIncompatibleCircle = 5, // We saw an incompatible circle out there.
};

//
// Types
//

enum {
    kSOSCCInCircle          = 0,
    kSOSCCNotInCircle       = 1,
    kSOSCCRequestPending    = 2,
    kSOSCCCircleAbsent      = 3,
    kSOSCCError             = -1,

// Never being used, were a bad idea, have clients leaving here deprecated.
    kSOSCCParamErr  __attribute__((deprecated)) = -2,
    kSOSCCMemoryErr __attribute__((deprecated)) = -3
};

typedef int SOSCCStatus;

extern const char * kSOSCCCircleChangedNotification;

/*!
 @function SOSCCSetUserCredentials
 @abstract Uses the user authentication credential (password) to create an internal EC Key Pair for authenticating Circle changes.
 @param user_label This string can be used for a label to tag the resulting credential data for persistent storage.
 @param user_password The user's password that's used as input to generate EC keys for Circle authenticating operations.
 @param error What went wrong if we returned false.
 @discussion This call needs to be made whenever a call that updates a Cloud Circle returns an error of kSOSErrorPrivateKeyAbsent (credential timeout) or kSOSErrorPublicKeyAbsent (programmer error).

     Any caller to SetUserCredential is asserting that they know the credential is correct.

     If you are uncertain (unable to verify) use TryUserCredentials, but if you can know it's better
     to call Set so we can recover from password change.
 */

bool SOSCCSetUserCredentials(CFStringRef user_label, CFDataRef user_password, CFErrorRef* error);


/*!
 @function SOSCCTryUserCredentials
 @abstract Uses the user authentication credential (password) to create an internal EC Key Pair for authenticating Circle changes.
 @param user_label This string can be used for a label to tag the resulting credential data for persistent storage.
 @param user_password The user's password that's used as input to generate EC keys for Circle authenticating operations.
 @param error What went wrong if we returned false.
 @discussion When one of the user credential requiring calls below (almost all) need a credential it will fail with kSOSErrorPrivateKeyAbsent. If you don't have an outside way to confirm correctness of the password we will attempt to use the passed in value and if it doesn't match the public information we currently have we'll fail.
 */

bool SOSCCTryUserCredentials(CFStringRef user_label, CFDataRef user_password, CFErrorRef* error);


/*!
 @function SOSCCRegisterUserCredentials
 @abstract Deprecated name for SOSCCSetUserCredentials.
 */
bool SOSCCRegisterUserCredentials(CFStringRef user_label, CFDataRef user_password, CFErrorRef *error);

/*!
 @function SOSCCCanAuthenticate
 @abstract Determines whether we currently have valid credentials to authenticate a circle operation.
 @param error What went wrong if we returned false.
 */

bool SOSCCCanAuthenticate(CFErrorRef *error);

/*!
 @function SOSCCThisDeviceIsInCircle
 @abstract Finds and returns if this devices status in the user's circle. 
 @param error What went wrong if we returned kSOSCCError.
 @result kSOSCCInCircle if we're in the circle.
 @discussion If we have an error figuring out if we're in the circle we return false and the error.
 */
SOSCCStatus SOSCCThisDeviceIsInCircle(CFErrorRef* error);

/*!
 @function SOSCCRequestToJoinCircle
 @abstract Requests that this device join the circle.
 @param error What went wrong if we tried to join.
 @result true if we pushed the request out successfully. False if there was an error.
 @discussion Requests to join the user's circle or all the pending circles (other than his) if there are multiple pending circles.
 */
bool SOSCCRequestToJoinCircle(CFErrorRef* error);

/*!
 @function SOSCCRequestToJoinCircleAfterRestore
 @abstract Requests that this device join the circle and do the magic just after restore approval.
 @param error What went wrong if we tried to join.
 @result true if we joined or pushed a request out. False if we failed to try.
 @discussion Uses the cloud identity to get in the circle if it can. If it cannot it falls back on simple application.
 */
bool SOSCCRequestToJoinCircleAfterRestore(CFErrorRef* error);

/*!
 @function SOSCCResetToOffering
 @abstract Resets the cloud to offer this device's circle. 
 @param error What went wrong if we tried to post our circle.
 @result true if we posted the circle successfully. False if there was an error.
 */
bool SOSCCResetToOffering(CFErrorRef* error);

/*!
 @function SOSCCResetToEmpty
 @abstract Resets the cloud to a completely empty circle.
 @param error What went wrong if we tried to post our circle.
 @result true if we posted the circle successfully. False if there was an error.
 */
bool SOSCCResetToEmpty(CFErrorRef* error);

/*!
 @function SOSCCRemoveThisDeviceFromCircle
 @abstract Removes the current device from the circle. 
 @param error What went wrong trying to remove ourselves.
 @result true if we posted the removal. False if there was an error.
 @discussion This removes us from the circle.
 */
bool SOSCCRemoveThisDeviceFromCircle(CFErrorRef* error);

/*!
 @function SOSCCBailFromCircle_BestEffort
 @abstract Attempts to publish a retirement ticket for the current device.
 @param error What went wrong trying to remove ourselves.
 @result true if we posted the ticket. False if there was an error.
 @discussion This attempts to post a retirement ticket that should
 result in other devices removing this device from the circle.  It does so
 with a 5 second timeout.  The only use for this call is when doing a device
 erase.
 */
bool SOSCCBailFromCircle_BestEffort(uint64_t limit_in_seconds, CFErrorRef* error);

/*!
 @function SOSCCCopyApplicantPeerInfo
 @abstract Get the list of peers wishing admittance.
 @param error What went wrong.
 @result Array of PeerInfos for applying peers.
 */
CFArrayRef SOSCCCopyApplicantPeerInfo(CFErrorRef* error);

/*!
 @function SOSCCAcceptApplicants
 @abstract Accepts the applicants into the circle (requires that we recently had the user enter the credentials).
 @param applicants List of applicants to accept.
 @param error What went wrong if we tried to post our circle.
 @result true if we accepted the applicants. False if there was an error.
 */
bool SOSCCAcceptApplicants(CFArrayRef applicants, CFErrorRef* error);

/*!
 @function SOSCCRejectApplicants
 @abstract Rejects the applications for admission (requires that we recently had the user enter the credentials).
 @param applicants List of applicants to reject.
 @param error What went wrong if we tried to post our circle.
 @result true if we rejected the applicants. False if there was an error.
 */
bool SOSCCRejectApplicants(CFArrayRef applicants, CFErrorRef *error);

/*!
 @function SOSCCCopyPeerPeerInfo
 @abstract Returns peers in the circle (we may not be in it). 
 @param error What went wrong trying look at the circle.
 @result Returns a list of peers in the circle currently syncing.
 @discussion We get the list of all peers syncing in the circle.
 */
CFArrayRef SOSCCCopyPeerPeerInfo(CFErrorRef* error);

/*!
 @function SOSCCGetLastDepartureReason
 @abstract Returns the information (string, hopefully URL) that will lead to an explanation of why you have an incompatible circle.
 @param error What went wrong if we returned NULL.
 */
enum DepartureReason {
    kSOSDepartureReasonError = 0,
    kSOSNeverLeftCircle,       // We haven't ever left a circle
    kSOSWithdrewMembership,    // SOSCCRemoveThisDeviceFromCircle
    kSOSMembershipRevoked,     // Via reset or remote removal.
    kSOSLeftUntrustedCircle,   // We saw a circle we could no longer trust
    kSOSNeverAppliedToCircle,  // We've never applied to a circle
};

enum DepartureReason SOSCCGetLastDepartureReason(CFErrorRef *error);

/*!
 @function SOSCCGetIncompatibilityInfo
 @abstract Returns the information (string, hopefully URL) that will lead to an explinatoin of why you have an incompatible circle.
 @param error What went wrong if we returned NULL.
 */
CFStringRef SOSCCCopyIncompatibilityInfo(CFErrorRef *error);

typedef enum SyncWithAllPeersReason {
    kSyncWithAllPeersOtherFail = 0,
    kSyncWithAllPeersSuccess,
    kSyncWithAllPeersLocked,
} SyncWithAllPeersReason;

__END_DECLS

#endif
