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

#import <Foundation/NSXPCConnection_Private.h>

#import "IPConfigurationLog.h"
#import "IPConfigurationPrivate.h"
#import "IPHPvDInfoRequest.h"

static NSString * const kIPHPvDInfoRequestClientEntitlement = @"com.apple.private.IPConfigurationHelper.PvD";

@interface IPConfigurationHelperDelegate : NSObject<NSXPCListenerDelegate>
@end

@implementation IPConfigurationHelperDelegate

- (instancetype)init
{
	self = [super init];
	if (self) {
		_IPConfigurationInitLog(kIPConfigurationLogCategoryHelper);
	}
	return self;
}

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection {
	id clientEntitlement = nil;
	BOOL acceptNewConnection = NO;

	clientEntitlement = [newConnection valueForEntitlement:kIPHPvDInfoRequestClientEntitlement];
	if (clientEntitlement == nil || ![clientEntitlement isKindOfClass:[NSNumber class]]) {
		goto done;
	}
	acceptNewConnection = ((NSNumber *)clientEntitlement).boolValue;
	if (!acceptNewConnection) {
		IPConfigLog(LOG_NOTICE, "rejecting new connection due to missing entitlement");
		goto done;
	}
	newConnection.exportedObject = [IPHPvDInfoRequestServer new];
	newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:
					   @protocol(IPHPvDInfoRequestProtocol)];
	[newConnection resume];

done:
	return acceptNewConnection;
}

@end

int main(int argc, const char * argv[])
{
	IPConfigurationHelperDelegate *delegate = nil;
	NSXPCListener *listener = nil;

	/*
	 * An app-scoped service inherits the app's user, which is root for configd.
	 * This hops out of root (0) and instead runs this XPCService as user nobody (-2).
	 */
	if (geteuid() == 0) {
		if (seteuid(-2) != 0) {
			IPConfigLog(LOG_ERR, "couldn't deescalate user before launching");
			goto done;
		}
	}
	/* XPC server start */
	delegate = [IPConfigurationHelperDelegate new];
	listener = [NSXPCListener serviceListener];
	listener.delegate = delegate;
	[listener activate];

done:
	return 0;
}
