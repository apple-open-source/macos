//
//  PMPowerMitigationInfo.m
//  LowPowerMode-Embedded
//
//  Created by Prateek Malhotra on 11/20/24.
//

#import "PMPowerMitigationInfo.h"

@implementation PMPowerMitigationInfo

- (instancetype)initWithMitigationLevel:(PMMitigationLevel)mitigationLevel
{
    return [self initWithMitigationLevel:mitigationLevel clientIdentifier:nil];
}

- (instancetype)initWithMitigationLevel:(PMMitigationLevel)mitigationLevel
                       clientIdentifier:(ClientIdentifier *)identifier
{
    self = [super init];
    if (self) {
        _mitigationLevel = mitigationLevel;
        _clientIdentifier = identifier;
    }
    return self;
}


@end
