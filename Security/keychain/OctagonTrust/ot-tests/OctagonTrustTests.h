/*
* Copyright (c) 2020 Apple Inc. All Rights Reserved.
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


#ifndef OctagonTrustTests_h
#define OctagonTrustTests_h

#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingTestsBase.h"
#import "keychain/ot/OTControl.h"
#import "keychain/ot/OTControlProtocol.h"
#import "keychain/ot/OctagonControlServer.h"

NS_ASSUME_NONNULL_BEGIN

@interface ProxyXPCConnection : NSObject <NSXPCListenerDelegate>
@property NSXPCListener *listener;
@property (nonatomic) id obj;
@property (nonatomic) NSXPCInterface *serverInterface;
- (instancetype)initWithInterface:(NSXPCInterface*)interface obj:(id)obj;
- (BOOL)listener:(NSXPCListener*)listener newConnection:(NSXPCConnection*)newConnection;
@end

@interface OctagonTrustTests : CloudKitKeychainSyncingTestsBase
@property (nonatomic) OTControl*  otControl;
@property (nonatomic) ProxyXPCConnection*  otXPCProxy;
@property (nonatomic) id mockClique;

@end


@interface OctagonTrustTests (OctagonTrustTestsErrors)
@end

NS_ASSUME_NONNULL_END
#endif /* OctagonTrustTests_h */
