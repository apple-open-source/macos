/*
* Copyright (c) 2022 Apple Computer, Inc. All rights reserved.
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
//  _PMCoreSmartPowerNapProtocol.h
//  LowPowerMode-Embedded
//
//  Created by Prateek Malhotra on 12/7/22.
//

#ifndef _PMCoreSmartPowerNapProtocol_h
#define _PMCoreSmartPowerNapProtocol_h

// states
typedef NS_ENUM(uint8_t, _PMCoreSmartPowerNapState) {
    _PMCoreSmartPowerNapStateOff = 0,
    _PMCoreSmartPowerNapStateOn,
};

// callback provided by clients for state udpates
typedef void(^_PMCoreSmartPowerNapCallback)(_PMCoreSmartPowerNapState state);

@protocol _PMCoreSmartPowerNapProtocol

// register clients with powerd
- (void)registerWithIdentifier: (NSString *)identifier;

// unregister
- (void)unregisterWithIdentifier: (NSString *)identifier;

// only to be used by testing tools. Entitlement will be enforced
- (void)setState:(_PMCoreSmartPowerNapState)state;

- (void)setCSPNQueryDelta:(uint32_t)seconds;

- (void)setCSPNRequeryDelta:(uint32_t)seconds;

- (void)setCSPNIgnoreRemoteClient:(uint32_t)state;

- (void)setCSPNMotionAlarmThreshold:(uint32_t)seconds;

- (void)setCSPNMotionAlarmStartThreshold:(uint32_t)seconds;

/*
 Get current state of core smart power nap from powerd.
 */
- (void)syncStateWithHandler:(_PMCoreSmartPowerNapCallback)handler;
@end



#endif /* _PMCoreSmartPowerNapProtocol_h */
