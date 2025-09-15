//
//  PMPowerMitigationSession.h
//  LowPowerMode-Embedded
//
//  Created by Prateek Malhotra on 11/20/24.
//

#ifndef PMPowerMitigationSession_h
#define PMPowerMitigationSession_h

#import "PMPowerMitigationInfo.h"

@interface PMPowerMitigationSession : NSObject <NSSecureCoding>

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@property (copy, readonly, nonatomic) NSString *sessionSource;
@property (readonly, nonatomic) PMMitigationSessionReason engagementReason;
@property (readonly, nonatomic) NSTimeInterval timeDuration;
@property (readonly, nonatomic) PMMitigationLevel systemWideMitigationLevel;


- (instancetype)initWithSource:(NSString *)sessionSource
                        reason:(PMMitigationSessionReason)reason
                         level:(PMMitigationLevel)mitigationLevel
                      duration:(NSTimeInterval)durationInSecs;

- (BOOL)isTimedSession;
@end


#endif /* PMPowerMitigationSession_h */
