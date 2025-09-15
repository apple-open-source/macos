//
//  PMPowerMitigationSession.m
//  LowPowerMode-Embedded
//
//  Created by Prateek Malhotra on 11/20/24.
//

#import <Foundation/Foundation.h>
#import "PMPowerMitigationSession.h"

@implementation PMPowerMitigationSession

- (instancetype)initWithSource:(NSString *)sessionSource
                        reason:(PMMitigationSessionReason)reason
                         level:(PMMitigationLevel)mitigationLevel {
    return [self initWithSource:sessionSource reason:reason level:mitigationLevel duration:0];
}

- (instancetype)initWithSource:(NSString *)sessionSource
                        reason:(PMMitigationSessionReason)reason
                         level:(PMMitigationLevel)mitigationLevel
                      duration:(NSTimeInterval)durationInSecs {
    
    self = [super init];
    if (self) {
        _sessionSource = sessionSource;
        _engagementReason = reason;
        _systemWideMitigationLevel = mitigationLevel;
        _timeDuration = durationInSecs;
    }
    return self;
}

- (BOOL)isTimedSession {
    return (_timeDuration > 0);
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (void)encodeWithCoder:(nonnull NSCoder *)coder
{
    [coder encodeObject:_sessionSource forKey:@"sessionSource"];
    [coder encodeInteger:_engagementReason forKey:@"engagementReason"];
    [coder encodeInteger:_systemWideMitigationLevel forKey:@"systemWideMitigationLevel"];
    [coder encodeDouble:_timeDuration forKey:@"timeDuration"];
}

- (nullable instancetype)initWithCoder:(nonnull NSCoder *)coder
{
    self = [super init];
    if (!self) {
        return nil;
    }
    _sessionSource = [coder decodeObjectOfClass:[NSString class] forKey:@"sessionSource"];
    _engagementReason = [coder decodeIntegerForKey:@"engagementReason"];
    _systemWideMitigationLevel = [coder decodeIntegerForKey:@"systemWideMitigationLevel"];
    _timeDuration = [coder decodeDoubleForKey:@"timeDuration"];
    return self;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<PMPowerMitigationSession: Level %ld, sessionSource %@>", (long)_systemWideMitigationLevel, _sessionSource];
}

@end
