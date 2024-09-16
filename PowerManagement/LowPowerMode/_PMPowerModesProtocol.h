//
//  _PMPowerModesProtocol.h
//
//  Created by Prateek Malhotra on 6/24/24.
//  Copyright © 2024 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <LowPowerMode/_PMPowerModeSession.h>

#if TARGET_OS_OSX

/**
 * @abstract                Handler for mode state change notifications
 * @param session                        Contains information about the new session -- mode, state and expiration reason.
 * @param error                             If the request could not honored by the Modes Manager, this will contain information about the failure reason.
 * */
typedef void (^PMPowerModeUpdateHandler)(_PMPowerModeSession *session, NSError *error);

@protocol _PMPowerModesProtocol

- (BOOL)supportsPowerMode:(PMPowerMode)mode;

/**
 * @abstract            Register to be notified of state changes to any Power Modes
 * @param handler                Will be invoked whenever there is a state change for a power mode
 * @return              Reflects whether the registration with the Modes Manager was successful
 * */
- (IOReturn)registerForUpdatesWithIdentifier:(NSString *)clientIdentifier
                                withCallback:(PMPowerModeUpdateHandler)handler;

/**
 * @abstract            Register to be notified of state changes for a specific power mode.
 * @param handler                Will be invoked whenever there is a state change for the specified power mode
 * @return              Reflects whether the registration with the Modes Manager was successful
 * */
- (IOReturn)registerForUpdatesOfPowerMode:(PMPowerMode)mode
                           withIdentifier:(NSString *)clientIdentifier
                             withCallback:(PMPowerModeUpdateHandler)handler;

/**
 * @abstract            Request a PowerMode to be set or unset. Setting a Power Mode implies unsetting any other active Power Mode.
 * @discussion          Submits an asynchronous request to the Modes Manager to set the Power Mode to the desired state.
 *                      The Modes Manager ensures only one Power Mode is active at a time. Setting a non-OFF PowerState for a Power Mode will result in
 *                      any other active Power Mode being turned off.
 * @param handler                Will be invoked once the request is processed by the Modes Manager to indicate the new state of the power mode.
 * @return              Reflects whether the request is well-formed and the arguments are valid. `kIOReturnSuccess` indicates that the request was
 *                      submitted to the Modes Manager. Depending on system conditions and mode availability, the Power Mode may still not be set to the
 *                      requested set. If the request is not honored, `PMPowerModeUpdateHandler` will be invoked with an error to indicate the failure to
 *                      process the request.
*/
- (IOReturn)setStateTo:(PMPowerModeState)newState
          forPowerMode:(PMPowerMode)mode
            fromSource:(NSString *)source
   withExpirationEvent:(PMPowerModeExpirationEventType)expirationEvent
             andParams:(PMPowerModeExpirationEventParams)expirationParams
          withCallback:(PMPowerModeUpdateHandler)handler;

@end

#endif
