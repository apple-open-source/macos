//
//  KDSecCircle.m
//  Security
//
//  Created by J Osborne on 2/20/13.
//
//

#import "KDSecCircle.h"
#import "KDCirclePeer.h"
#include <notify.h>
#include <dispatch/dispatch.h>
#import "SecureObjectSync/SOSCloudCircle.h"
#include "SecureObjectSync/SOSPeerInfo.h"

@interface KDSecCircle ()
@property (retain) NSMutableArray *callbacks;

@property (readwrite) unsigned long long changeCount;

@property (readwrite) SOSCCStatus rawStatus;

@property (readwrite) NSString *status;
@property (readwrite) NSError *error;

@property (readwrite) NSArray *peers;
@property (readwrite) NSArray *applicants;

@property (readwrite) dispatch_queue_t queue_;

@end

@implementation KDSecCircle

-(void)updateCheck
{
	// XXX: assert not on main_queue
	CFErrorRef err = NULL;
	SOSCCStatus newRawStatus = SOSCCThisDeviceIsInCircle(&err);
    
    NSArray *peerInfos = (__bridge NSArray *)(SOSCCCopyApplicantPeerInfo(&err));
    NSMutableArray *newApplicants = [[NSMutableArray alloc] initWithCapacity:peerInfos.count];
	[peerInfos enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        [newApplicants addObject:[[KDCirclePeer alloc] initWithPeerObject:obj]];
    }];
	
	peerInfos = (__bridge NSArray *)(SOSCCCopyPeerPeerInfo(&err));
    NSMutableArray *newPeers = [[NSMutableArray alloc] initWithCapacity:peerInfos.count];
	[peerInfos enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        [newPeers addObject:[[KDCirclePeer alloc] initWithPeerObject:obj]];
    }];
    
    NSLog(@"rawStatus %d, #applicants %lu, #peers %lu, err=%@", newRawStatus, (unsigned long)[newApplicants count], (unsigned long)[newPeers count], err);

	dispatch_async(dispatch_get_main_queue(), ^{
        self.rawStatus = newRawStatus;
        
		switch (newRawStatus) {
			case kSOSCCInCircle:
				self.status = @"In Circle";
				break;
				
			case kSOSCCNotInCircle:
				self.status = @"Not In Circle";
				break;

			case kSOSCCRequestPending:
				self.status = @"Request Pending";
				break;
				
			case kSOSCCCircleAbsent:
				self.status = @"Circle Absent";
				break;
				
			case kSOSCCError:
				self.status = [NSString stringWithFormat:@"Error: %@", err];
				break;
				
			case kSOSCCParamErr:
				self.status = [NSString stringWithFormat:@"ParamError: %@", err];
				break;
				
			default:
				self.status = [NSString stringWithFormat:@"Unknown status code %d", self.rawStatus];
				break;
		}
        
        self.applicants = [newApplicants copy];
        self.peers = [newPeers copy];
        self.error = (__bridge NSError *)(err);

        self.changeCount++;
        for (dispatch_block_t callback in self.callbacks) {
            callback();
        }
	});
}

// XXX It's a botch to use the "name" and not applicant, but
// it is hard to get anythign else to survive a serialastion
// trip thoguth NSUserNotificationCenter.
//
// Er, now that I look more closely maybe SOSPeerInfoGetPeerID...

typedef void (^applicantBlock)(id applicant);

-(void)forApplicantId:(NSString*)applicantId run:(applicantBlock)applicantBlock
{
    dispatch_async(self.queue_, ^{
		for (KDCirclePeer *applicant in self.applicants) {
			if ([applicantId isEqualToString:applicantId]) {
				applicantBlock(applicant.peerObject);
				break;
			}
		}
    });
}

-(void)acceptApplicantId:(NSString*)applicantId
{
    [self forApplicantId:applicantId run:^void(id applicant) {
        CFErrorRef err;
        bool ok = SOSCCAcceptApplicants((__bridge CFArrayRef)(@[applicant]), &err);
        NSAssert(ok, @"Error %@ while accepting %@ (%@)", err, applicantId, applicant);
    }];
}

-(void)rejectApplicantId:(NSString*)applicantId
{
    [self forApplicantId:applicantId run:^void(id applicant) {
        CFErrorRef err;
        bool ok = SOSCCRejectApplicants((__bridge CFArrayRef)(@[applicant]), &err);
        NSAssert(ok, @"Error %@ while rejecting %@ (%@)", err, applicantId, applicant);
    }];
}

-(id)init
{
	self = [super init];
	int token;
    
    self->_queue_ = dispatch_queue_create([[NSString stringWithFormat:@"KDSecCircle@%p", self] UTF8String], NULL);
    self->_callbacks = [NSMutableArray new];
    // Replace "com.apple.security.secureobjectsync.circlechanged" with kSOSCCCircleChangedNotification once it is exported
	notify_register_dispatch("com.apple.security.secureobjectsync.circlechanged", &token, self.queue_, ^(int token){
		[self updateCheck];
	});
    
    return self;
}

-(void)addChangeCallback:(dispatch_block_t)callback
{
	[self.callbacks addObject:callback];
    if (self.changeCount) {
        dispatch_async(dispatch_get_main_queue(), callback);
    } else if (self.callbacks.count == 1) {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            [self updateCheck];
        });
    }
}

-(BOOL)isInCircle
{
    return (self.rawStatus == kSOSCCInCircle) ? YES : NO;
}

-(BOOL)isOutOfCircle
{
    return (self.rawStatus == kSOSCCNotInCircle || self.rawStatus == kSOSCCCircleAbsent);
}

-(void)enableSync
{
    CFErrorRef err;
	if (self.rawStatus == kSOSCCCircleAbsent) {
		SOSCCResetToOffering(&err);
	} else {
		SOSCCRequestToJoinCircle(&err);
	}
}

-(void)disableSync
{
    CFErrorRef err;
    SOSCCRemoveThisDeviceFromCircle(&err);
}

@end
