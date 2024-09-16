//
//  msdosProgressHelper.m
//  msdos.appex
//
//  Created by William Stouder-Studenmund on 5/1/24.
//

#import "msdosProgressHelper.h"
#import <FSKit/FSKit.h>

@implementation msdosProgressHelper

-(nullable instancetype)initWithProgress:(NSProgress *)progress
{
    self = [super init];
    if (self) {
        _parentProgress = progress;
        _childProgress = nil;
    }
    return self;
}

-(NSError* _Nullable)startPhase:(NSString *)description
                parentUnitCount:(int64_t)parentUnitCount
                phaseTotalCount:(int64_t)phaseTotalCount
               completedCounter:(const unsigned int *)completedCounter
{
    if (_childProgress != nil) {
        // We are in the middle a phase - expect it to end before starting a new one.
        os_log_fault(fskit_std_log(), "%s missing endPhase call for %@", __FUNCTION__, _parentProgress.localizedDescription);
        return fs_errorForPOSIXError(EINVAL);
    }

    _parentProgress.localizedDescription = description;
    _childProgress = [NSProgress progressWithTotalUnitCount:phaseTotalCount];
    [_parentProgress addChild:_childProgress withPendingUnitCount:parentUnitCount];

    return nil;
}

-(void)endPhase:(NSString *)description
{
    // We expect to be called for cleanup once sometime after startPhase got called.
    // Silently do nothing if we got unexpected end call
    if (_childProgress) {
        _parentProgress.localizedDescription = description;
        _childProgress.completedUnitCount = _childProgress.totalUnitCount;
        _childProgress = nil;
    }
}

@end
