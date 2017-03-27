//
//  Applicant.m
//  Security
//
//  Created by J Osborne on 3/7/13.
//  Copyright (c) 2013 Apple Inc. All Rights Reserved.
//

#import "Applicant.h"
#include <utilities/SecCFRelease.h>

@implementation Applicant

-(id)initWithPeerInfo:(SOSPeerInfoRef)peerInfo
{
    self = [super init];
    if (!self) {
        return self;
    }
    
    self.rawPeerInfo = CFRetainSafe(peerInfo);
    self.applicantUIState = ApplicantWaiting;
    
    return self;
}

-(NSString*)idString
{
    return (__bridge NSString *)(SOSPeerInfoGetPeerID(self.rawPeerInfo));
}

-(NSString *)name
{
    return (__bridge NSString *)(SOSPeerInfoGetPeerName(self.rawPeerInfo));
}

-(void)dealloc
{
	if (self.rawPeerInfo) {
		CFRelease(self.rawPeerInfo);
	}
}

-(NSString *)description
{
	return [NSString stringWithFormat:@"%@=%@", self.rawPeerInfo, self.applicantUIStateName];
}

-(NSString *)deviceType
{
    return (__bridge NSString *)(SOSPeerInfoGetPeerDeviceType(self.rawPeerInfo));
}

-(NSString *)applicantUIStateName
{
	switch (self.applicantUIState) {
		case ApplicantWaiting:
			return @"Waiting";

		case ApplicantOnScreen:
			return @"OnScreen";

		case ApplicantRejected:
			return @"Rejected";

		case ApplicantAccepted:
			return @"Accepted";

		default:
			return [NSString stringWithFormat:@"UnknownState#%d", self.applicantUIState];
	}
}

@end
