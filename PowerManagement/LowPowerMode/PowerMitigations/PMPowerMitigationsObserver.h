//
//  PMPowerMitigationsObserver.h
//  LowPowerMode-Embedded
//
//  Created by Prateek Malhotra on 11/20/24.
//

#ifndef PMPowerMitigationsObserver_h
#define PMPowerMitigationsObserver_h

#import "PMPowerMitigationInfo.h"
#import "PMPowerMitigationSession.h"

@protocol PMPowerMitigationsObserver <NSObject>

- (void)didChangeToMitigations:(PMPowerMitigationInfo *)newMitigationInfo withSessionInfo:(PMPowerMitigationSession *)sessionInfo;

@end

#endif /* PMPowerMitigationsObserver_h */
