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
//  _PMCoreSmartPowerNap.h
//  LowPowerMode-Embedded
//
//  Created by Prateek Malhotra on 12/7/22.
//

#ifndef _PMCoreSmartPowerNap_h
#define _PMCoreSmartPowerNap_h

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#import <LowPowerMode/_PMCoreSmartPowerNapProtocol.h>
#import <LowPowerMode/_PMCoreSmartPowerNapCallbackProtocol.h>

#define kIOPMCoreSmartPowerNapNotifyName "com.apple.powerd.coresmartpowernap"
#define kIOPMCoreSmartPowerNapExit  0
#define kIOPMCoreSmartPowerNapTransient 1
#define kIOPMCoreSmartPowerNapEntry 2

extern NSString *const kPMCoreSmartPowerNapServiceName;

@interface _PMCoreSmartPowerNap : NSObject <_PMCoreSmartPowerNapProtocol, _PMCoreSmartPowerNapCallbackProtocol>
@property (nonatomic, retain) NSXPCConnection *connection;
@property (nonatomic, retain) NSString *identifier;
@property (nonatomic, copy) _PMCoreSmartPowerNapCallback callback;
@property (nonatomic, retain) dispatch_queue_t callback_queue;
@property (nonatomic) _PMCoreSmartPowerNapState current_state;
@property BOOL connection_interrupted;
/*
 Register to receive updates when core smart power nap state changes
 */
- (void)registerWithCallback:(dispatch_queue_t)queue callback:(_PMCoreSmartPowerNapCallback)callback;

/*
 Unregister
 */
- (void)unregister;

/*
 Get current state of core smart power nap. States are defined in _PMCoreSmartPowerNapProtocol.h. This
 state is cached in the client
 */
- (_PMCoreSmartPowerNapState)state;

/*
 Get current state of core smart power nap from powerd. This is a blocking synchronous call
 */
- (_PMCoreSmartPowerNapState)syncState;

/*
 Re-register after powerd exits
 */
- (void)reRegister;
@end


#endif /* _PMCoreSmartPowerNap_h */
