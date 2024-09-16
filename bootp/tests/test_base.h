/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#ifndef test_base_h
#define test_base_h

#import "test_utils.h"

@interface IPConfigurationFrameworkTestBase : NSObject

@property (nonatomic, strong) NSString * description;
@property (nonatomic, strong) __attribute__((NSObject)) SCDynamicStoreRef store;
@property (nonatomic, strong) dispatch_queue_t storeQueue;
@property (atomic, strong) dispatch_semaphore_t storeSem;
@property (nonatomic, strong) __attribute__((NSObject)) IOEthernetControllerRef interfaceController;
@property (nonatomic, strong) dispatch_queue_t interfaceQueue;
@property (atomic, strong) dispatch_semaphore_t interfaceSem;
@property (nonatomic, strong) NSString * ifname;
@property (nonatomic, weak) NSString * serviceKey;
@property (atomic, strong) dispatch_semaphore_t serviceSem;
@property (nonatomic, weak) NSString * serviceKey2;
@property (atomic, strong) dispatch_semaphore_t serviceSem2;
@property (nonatomic) BOOL alternativeValidation;
@property (nonatomic, weak) NSString * pdServiceKey;
@property (nonatomic, strong) NSString * dhcpServerIfname;
@property (nonatomic, strong) NSString * dhcpClientIfname;

+ (instancetype)sharedInstance;
- (instancetype)init;
- (void)dealloc;
- (BOOL)dynamicStoreInitialize;
- (void)dynamicStoreDestroy;
- (BOOL)ioUserEthernetInterfaceCreate;
- (void)ioUserEthernetInterfaceDestroy;
- (void)setService:(IPConfigurationServiceRef)service;
- (void)setService2:(IPConfigurationServiceRef)service;

@end

#endif /* test_base_h */
