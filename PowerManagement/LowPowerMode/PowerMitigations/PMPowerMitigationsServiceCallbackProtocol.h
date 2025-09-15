//
//  PMPowerMitigationsServiceCallbackProtocol.h
//  LowPowerMode-Embedded
//
//  Created by Prateek Malhotra on 2/24/25.
//

#ifndef PMPowerMitigationsServiceCallbackProtocol_h
#define PMPowerMitigationsServiceCallbackProtocol_h

#import <Foundation/Foundation.h>
#import "PMPowerMitigationSession.h"

@protocol PMPowerMitigationsServiceCallbackProtocol

- (void)didUpdateToMitigation:(PMPowerMitigationSession *)newMitigation
               fromMitigation:(PMPowerMitigationSession *)oldMitigation;

@end

#endif /* PMPowerMitigationsServiceCallbackProtocol_h */
