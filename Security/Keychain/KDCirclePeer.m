//
//  KDCirclePeer.m
//  Security
//
//  Created by J Osborne on 2/25/13.
//
//

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
