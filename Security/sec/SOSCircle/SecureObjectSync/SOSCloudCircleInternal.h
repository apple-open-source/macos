//
//  SOSCloudCircleInternal.h
//
//  Created by Mitch Adler on 11/13/12.
//
//

#ifndef _SECURITY_SOSCLOUDCIRCLEINTERNAL_H_
#define _SECURITY_SOSCLOUDCIRCLEINTERNAL_H_

#include <SecureObjectSync/SOSCloudCircle.h>
#include <xpc/xpc.h>
#include <Security/SecKey.h>

CFArrayRef SOSCCCopyConcurringPeerPeerInfo(CFErrorRef* error);

bool SOSCCPurgeUserCredentials(CFErrorRef* error);

CFStringRef SOSCCGetStatusDescription(SOSCCStatus status);
SecKeyRef SOSCCGetUserPrivKey(CFErrorRef *error);
SecKeyRef SOSCCGetUserPubKey(CFErrorRef *error);

/*!
 @function SOSCCProcessSyncWithAllPeers
 @abstract Returns the information (string, hopefully URL) that will lead to an explanation of why you have an incompatible circle.
 @param error What went wrong if we returned NULL.
 */

SyncWithAllPeersReason SOSCCProcessSyncWithAllPeers(CFErrorRef* error);

#endif
