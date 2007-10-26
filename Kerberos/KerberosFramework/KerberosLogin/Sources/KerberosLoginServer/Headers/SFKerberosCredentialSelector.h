/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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

#import <Cocoa/Cocoa.h>
#import "KLMachIPC.h"
#import "AuthenticationController.h"
#import "AuthenticationControllerSimple.h"

@class KerberosPrincipal;

#define KerbCredSelLog if(gKerbAuthLogging)NSLog // to enable, in the terminal>defaults write com.apple.kerberosAuthLogging KerberosAuthLogging YES

@interface SFKerberosCredentialSelector : NSObject 
{
    AuthenticationController *_controller;
	BOOL _fromKeychain;
    NSMutableString *_acquiredCacheName;
	KerberosPrincipal *_acquiredPrincipal;
}

-(KLStatus)selectCredentialWithPrincipal:(KerberosPrincipal*)principal 
										serviceName:(NSString*)serviceName 
										applicationTask:(task_t)applicationTask 
										applicationPath:(NSString*)applicationPath 
                                        inLifetime:(KLIPCTime)inLifetime
                                        inRenewableLifetime:(KLIPCTime)inRenewableLifetime
										inFlags:(KLIPCFlags)inFlags
                                        inStartTime:(KLIPCTime)inStartTime
                                        inForwardable:(KLIPCBoolean)inForwardable
										inProxiable:(KLIPCBoolean)inProxiable
                                        inAddressless:(KLIPCBoolean)inAddressless
										isAutoPopup:(boolean_t)isAutoPopup
										inApplicationName:(NSString*)inApplicationName
										inApplicationIcon:(NSImage*)inApplicationIcon;

-(NSString*)acquiredCacheName;

-(void)setAcquiredPrincipal:(KerberosPrincipal*)principal;
-(KerberosPrincipal*)acquiredPrincipal;

-(void)_setPrintNameWithUserName:(NSString *)userName serverName:(NSString*)serverName item:(SecKeychainItemRef)itemRef;

@end
