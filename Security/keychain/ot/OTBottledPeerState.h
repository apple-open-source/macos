/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#if OCTAGON

#ifndef OTBottledPeerState_h
#define OTBottledPeerState_h

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/* Bottled Peer States */
@protocol SecOTBottledPeerState <NSObject>
@end
typedef NSString<SecOTBottledPeerState> OTBottledPeerState;

// Octagon Trust currently logged out
extern OTBottledPeerState* const SecOTBottledPeerStateLoggedOut;

// Octagon Trust currently signed in
extern OTBottledPeerState* const SecOTBottledPeerStateSignedIn;

// Octagon Trust: check bottle states
extern OTBottledPeerState* const SecOTBottledPeerStateInventoryCheck;

// Octagon Trust: update bottles for current peerid with current octagon keys
extern OTBottledPeerState* const SecOTBottledPeerStateUpdateBottles;

// Octagon Trust: bottles exist for the current octagon key set and have been writte to cloudkit
extern OTBottledPeerState* const SecOTBottledPeerStateCleanBottles;

// Octagon Trust: bottles for current peerid have been updated and persited locally, but not commited to cloudkit
extern OTBottledPeerState* const SecOTBottledPeerStateDirtyBottles;

NS_ASSUME_NONNULL_END

#endif /* OTBottledPeerState_h */
#endif
