//
//  GestureStats.h
//
//  Created by Austin Erck on 7/16/24.
//

#pragma once

@interface GesturePhase : NSObject

@property(nonatomic) IOHIDEventType type;
@property(nonatomic) IOHIDEventOptionBits options;
/// Dictionary storing gesture phase info of a child event
/// Structured the same as the gesturePhases dictionary in GestureStats
@property(nonatomic, nonnull) NSMutableDictionary<NSNumber*, GesturePhase*>* children;
@property(readonly, nonnull) NSDictionary* debug;

- (nullable instancetype)init NS_UNAVAILABLE;
- (nullable instancetype)initWithType:(IOHIDEventType)type;

@end

@interface GestureStats : NSObject

@property(nonatomic, nonnull) NSNumber* serviceID;
@property(nonatomic, nullable) NSDate* removedAt;
/// Dictionary storing gesture phase info of all non-ignored events seen by the filter
/// - Key: Event type of event seen by filter
/// - Value: Associated gesture phase info, including event type and options, with the same info stored for event children
@property(nonatomic, nonnull) NSMutableDictionary<NSNumber*, GesturePhase*>* gesturePhases;
@property(nonatomic, nonnull) NSMutableArray<NSDictionary*>* gestureImbalances;
@property(readonly, nonnull) NSDictionary* debug;

- (nullable instancetype)init NS_UNAVAILABLE;
- (nullable instancetype)initWithServiceID:(nonnull NSNumber*)serviceID;
- (void)handleHIDEvent:(nonnull HIDEvent*)event;

@end
