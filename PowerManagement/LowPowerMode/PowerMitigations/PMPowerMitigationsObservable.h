//
//  PMPowerMitigationsObservable.h
//  LowPowerMode-Embedded
//
//  Created by Prateek Malhotra on 11/20/24.
//

#ifndef PMPowerMitigationsObservable_h
#define PMPowerMitigationsObservable_h

#import "PMPowerMitigationInfo.h"
#import "PMPowerMitigationSession.h"
#import "PMPowerMitigationsObserver.h"

@protocol PMPowerMitigationsObservable <NSObject>

- (void)addObserver:(id<PMPowerMitigationsObserver>)observer forClientIdentifier:(ClientIdentifier *)clientIdentifier;
- (void)removeObserver:(id<PMPowerMitigationsObserver>)observer forClientIdentifier:(ClientIdentifier *)clientIdentifier;
// `getCurrentMitigationInfoForClientIdentifier` deprecated in favor of `copyCurrentMitigationInfoForClientIdentifier`
- (PMPowerMitigationInfo *)getCurrentMitigationInfoForClientIdentifier:(ClientIdentifier *)clientIdentifier SPI_DEPRECATED_WITH_REPLACEMENT("copyCurrentMitigationInfoForClientIdentifier", ios(19.0, 19.0));
- (PMPowerMitigationInfo *)copyCurrentMitigationInfoForClientIdentifier:(ClientIdentifier *)clientIdentifier;

@end

#endif /* PMPowerMitigationsObservable_h */
