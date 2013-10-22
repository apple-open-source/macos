//
//  KDSecCircle.h
//  Security
//
//  Created by J Osborne on 2/20/13.
//
//

#import "SecureObjectSync/SOSCloudCircle.h"
#import <Foundation/Foundation.h>

@interface KDSecCircle : NSObject

@property (readonly) BOOL isInCircle;
@property (readonly) BOOL isOutOfCircle;

@property (readonly) SOSCCStatus rawStatus;

@property (readonly) NSString *status;
@property (readonly) NSError *error;

// Both of these are arrays of KDCircelPeer objects
@property (readonly) NSArray *peers;
@property (readonly) NSArray *applicants;

-(void)addChangeCallback:(dispatch_block_t)callback;
-(id)init;

// these are "try to", and may (most likely will) not complete by the time they return
-(void)enableSync;
-(void)disableSync;
-(void)rejectApplicantId:(NSString*)applicantId;
-(void)acceptApplicantId:(NSString*)applicantId;

@end
