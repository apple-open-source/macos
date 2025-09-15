//
//  PMPowerMitigationsServiceProtocol.h
//  LowPowerMode-Embedded
//
//  Created by Prateek Malhotra on 2/24/25.
//

#ifndef PMPowerMitigationsServiceProtocol_h
#define PMPowerMitigationsServiceProtocol_h

#import <Foundation/Foundation.h>

@protocol PMPowerMitigationsServiceProtocol

// register client
- (void)registerForPowerMitigations;

@end

#endif /* PMPowerMitigationsServiceProtocol_h */
