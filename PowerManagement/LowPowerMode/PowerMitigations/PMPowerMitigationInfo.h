//
//  PMPowerMitigationInfo.h
//  LowPowerMode-Embedded
//
//  Created by Prateek Malhotra on 11/20/24.
//

#ifndef PMPowerMitigationInfo_h
#define PMPowerMitigationInfo_h

#import <Foundation/Foundation.h>

typedef NSString ClientIdentifier;

typedef NS_ENUM(NSInteger, PMMitigationLevel) {
    PMMitigationLevelNone = 0,
    PMMitigationLevelLow = 20,
    PMMitigationLevelMedium = 50,
    PMMitigationLevelHigh = 70,
    PMMitigationLevelExtreme = 100,
};

#define PMMitigationLevelLPM PMMitigationLevelHigh

typedef NS_ENUM(NSInteger, PMMitigationSessionReason) {
    PMMitigationSessionReasonUserInitiated = 0,
    PMMitigationSessionReasonSystemInitiated = 1,
};

@interface PMPowerMitigationInfo : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@property (readonly, nonatomic) PMMitigationLevel mitigationLevel;
@property (copy, readonly, nonatomic) ClientIdentifier *clientIdentifier;

- (instancetype)initWithMitigationLevel:(PMMitigationLevel)mitigationLevel clientIdentifier:(ClientIdentifier *)identifier;

@end

#endif /* PMPowerMitigationInfo_h */
