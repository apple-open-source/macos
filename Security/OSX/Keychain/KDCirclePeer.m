/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#import "KDCirclePeer.h"
#include "SecureObjectSync/SOSCloudCircle.h"
#include "SecureObjectSync/SOSPeerInfo.h"

@interface KDCirclePeer ()

@property (readwrite) NSString *name;
@property (readwrite) NSString *idString;
@property (readwrite) id peerObject;

@end

@implementation KDCirclePeer

-(id)initWithPeerObject:(id)peerObject
{
	self = [super init];
	if (!self) {
		return self;
	}
	
	self.peerObject = peerObject;
	self.name = (__bridge NSString *)(SOSPeerInfoGetPeerName((__bridge SOSPeerInfoRef)peerObject));
	self.idString = (__bridge NSString *)(SOSPeerInfoGetPeerID((__bridge SOSPeerInfoRef)peerObject));
	
	return self;
}

-(NSString*)description
{
    return [NSString stringWithFormat:@"[peer n='%@' id='%@' o=%@]", self.name, self.idString, self.peerObject];
}

@end
