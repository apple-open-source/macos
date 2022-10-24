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
//  _PMSmartPowerNap.h
//  _PMSmartPowerNap
//
//  Created by Archana on 9/14/21.
//

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#import <LowPowerMode/_PMSmartPowerNapProtocol.h>
#import <LowPowerMode/_PMSmartPowerNapCallbackProtocol.h>

#define kIOPMSmartPowerNapNotifyName "com.apple.powerd.smartpowernap"
#define kIOPMSmartPowerNapExit  0
#define kIOPMSmartPowerNapTransient 1
#define kIOPMSmartPowerNapEntry 2
#define kIOPMSmartPowerNapInterruptionNotifyName "com.apple.powerd.smartpowernap.interruption"

extern NSString *const kPMSmartPowerNapServiceName;

@interface _PMSmartPowerNap : NSObject <_PMSmartPowerNapProtocol, _PMSmartPowerNapCallbackProtocol>
@property (nonatomic, retain) NSXPCConnection *connection;
@property (nonatomic, retain) NSString *identifier;
@property (nonatomic, copy) _PMSmartPowerNapCallback callback;
@property (nonatomic, retain) dispatch_queue_t callback_queue;
@property (nonatomic) _PMSmartPowerNapState current_state;
@property BOOL connection_interrupted;
/*
 Register to receive updates when smart power nap state changes
 */
- (void)registerWithCallback:(dispatch_queue_t)queue callback:(_PMSmartPowerNapCallback)callback;

/*
 Unregister
 */
- (void)unregister;

/*
 Get current state of smart power nap. States are defined in _PMSmartPowerNapProtocol.h. This
 state is cached in the client
 */
- (_PMSmartPowerNapState)state;

/*
 Get current state of smart power nap from powerd. This is a blocking synchronous call
 */
- (_PMSmartPowerNapState)syncState;

/*
 Re-register after powerd exits
 */
- (void)reRegister;
@end

