//
//  _PMPowerModeSession.h
//
//  Created by Prateek Malhotra on 6/24/24.
//  Copyright © 2024 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <LowPowerMode/_PMPowerModeState.h>

#if TARGET_OS_OSX

typedef NS_ENUM(NSInteger, PMPowerModeExpirationEventType) {
    PMPowerModeExpirationEventTypeNone = 0,                                       // Indefinite sessions
    PMPowerModeExpirationEventTypeTime = 1,
    PMPowerModeExpirationEventTypeSufficientCharge = 2,                           // LPM only
    PMPowerModeExpirationEventTypePowerSourceChange = 3                           // Portables only
};

typedef NS_ENUM(NSInteger, PMPowerModeExpirationEventParams) {
    PMPowerModeExpirationEventParamsNone = 0,
    PMPowerModeExpirationEventParamsTime_1hour = 1,
    PMPowerModeExpirationEventParamsTime_UntilTomorrow = 2
};

@interface _PMPowerModeSession : NSObject

/**
 * @abstract            The mode that is active for this session
*/
@property (nonatomic, readonly) PMPowerMode mode;

/**
 * @abstract            The state of the power mode
*/
@property (nonatomic, readonly) PMPowerModeState state;

/**
 * @abstract            Expiration type for the active power mode
 * @discussion          Depending on the expiration type, other properties may be useful to inspect.
 *                      For example, PMPowerModeExpirationReasonTime will have the `expiresAt` set.
*/
@property (nonatomic, readonly) PMPowerModeExpirationEventType expirationEventType;

/**
 * @abstract            Start time for this session
*/
@property (nonatomic, readonly) NSDate *startedAt;

/**
 * @abstract            When the session is expected to expire.
 * @discussion          Will only be set for sessions of expirationReason PMPowerModeExpirationReasonTime
*/
@property (nonatomic, readonly) NSDate *expiresAt;

@end


#endif
