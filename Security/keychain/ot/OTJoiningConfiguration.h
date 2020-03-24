/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#ifndef SECURITY_OT_OTJOININGCONFIGURATION_H
#define SECURITY_OT_OTJOININGCONFIGURATION_H 1

#if __OBJC2__

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface OTJoiningConfiguration : NSObject <NSSecureCoding>

@property (nonatomic, strong) NSString* protocolType;
@property (nonatomic, strong) NSString* uniqueDeviceID;
@property (nonatomic, strong) NSString* uniqueClientID;
@property (nonatomic, strong) NSString* containerName;
@property (nonatomic, strong) NSString* contextID;
@property (nonatomic, strong) NSString* pairingUUID;
@property (nonatomic) uint64_t epoch;
@property (nonatomic) BOOL isInitiator;

// Set this to non-zero if you want to configure your timeouts
@property int64_t timeout;

- (instancetype)initWithProtocolType:(NSString*)protocolType
                      uniqueDeviceID:(NSString*)uniqueDeviceID
                      uniqueClientID:(NSString*)uniqueClientID
                         pairingUUID:(NSString* _Nullable)pairingUUID
                       containerName:(NSString* _Nullable)containerName
                           contextID:(NSString*)contextID
                               epoch:(uint64_t)epoch
                         isInitiator:(BOOL)isInitiator;
-(instancetype)init NS_UNAVAILABLE;

@end
NS_ASSUME_NONNULL_END

#endif /* __OBJC2__ */
#endif /* SECURITY_OT_OTJOININGCONFIGURATION_H */
