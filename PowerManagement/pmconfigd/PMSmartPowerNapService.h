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
//  PMSmartPowerNapService.h
//  PMSmartPowerNapService
//
//  Created by Archana on 9/22/21.
//

#ifndef PMSmartPowerNapService_h
#define PMSmartPowerNapService_h
#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection.h>
#if !XCTEST
#import <BacklightServices/BLSBacklightChangeRequest.h>
#import <BacklightServices/BLSBacklightStateObserving.h>
#import <BacklightServices/BLSBacklightChangeEvent.h>

#import <BacklightServices/BLSBacklight.h>
#endif
#import "_PMSmartPowerNapProtocol.h"


#if !XCTEST
@interface PMSmartPowerNapService : NSXPCListener <NSXPCListenerDelegate, _PMSmartPowerNapProtocol, BLSBacklightStateObserving>
#else
@interface PMSmartPowerNapService : NSXPCListener <NSXPCListenerDelegate, _PMSmartPowerNapProtocol>
#endif

+ (instancetype)sharedInstance;
- (void)updateState:(_PMSmartPowerNapState)state;
- (void)enterSmartPowerNap;
- (void)exitSmartPowerNap;
@end
#endif /* PMSmartPowerNapService_h */
