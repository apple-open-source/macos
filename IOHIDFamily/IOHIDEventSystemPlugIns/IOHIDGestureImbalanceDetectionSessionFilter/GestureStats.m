//
//  GestureStats.mm
//
//  Created by Austin Erck on 7/16/24.
//

#import <Foundation/Foundation.h>
#import <HID/HIDEvent.h>
#import <IOKit/hid/IOHIDEventTypes.h>

#import "GestureStats.h"
#import "IOHIDGestureImbalanceDetectionSessionFilter.h"

#define kEventPhaseWithoutMomentumMask 0x8F
#define kMaxTrackedImbalancesPerDevice 8

// MARK: - GesturePhase

@implementation GesturePhase

- (instancetype)initWithType:(IOHIDEventType)type
{
    self = [super init];
    if (self)
    {
        _type = type;
        _options = 0;
        _children = [NSMutableDictionary new];
    }
    return self;
}

- (nonnull NSDictionary*)debug
{
    // Recursively call child `debug` methods
    NSMutableArray<NSDictionary*>* children = [NSMutableArray new];
    [self.children enumerateKeysAndObjectsUsingBlock:^(NSNumber* type, GesturePhase* gesturePhase, BOOL* stop) {
        [children addObject:gesturePhase.debug];
    }];

    return @{
        @"Type" : @(self.type),
        @"Options" : @(self.options),
        @"Children" : children.copy,
    };
}

@end

// MARK: - GestureStats

@interface GestureStats ()

@property(nonatomic) NSDictionary* _Nonnull ignoreList;

@end

@implementation GestureStats

- (instancetype)initWithServiceID:(nonnull NSNumber*)serviceID
{
    self = [super init];
    if (self)
    {
        _serviceID = serviceID;
        _removedAt = nil;
        _gesturePhases = [NSMutableDictionary new];
        _gestureImbalances = [NSMutableArray new];
        _ignoreList = @{
            @(kIOHIDEventTypeScroll) : @{
                @"Children" : @{
                    @(kIOHIDEventTypeScroll) : @{
                        @"Ignore" : @(true)
                    }
                }
            }
        };
    }
    return self;
}

/// Re-implementation of `IOHIDEventGetPhase` with int argument support
/// Additionally removed momentum based phase bits
- (IOHIDEventPhaseBits)getPhaseFromOptions:(IOHIDEventOptionBits)options
{
    // Added `kEventPhaseWithoutMomentumMask` instead of `kIOHIDEventEventPhaseMask` mask ensure momentum bits are removed from normal phase values
    return (options >> kIOHIDEventEventOptionPhaseShift) & kEventPhaseWithoutMomentumMask;
}

/// Re-implementation of `IOHIDEventGetScrollMomentum` with int argument support
- (IOHIDEventScrollMomentumBits)getScrollMomentumFromOptions:(IOHIDEventOptionBits)options
{
    IOHIDEventScrollMomentumBits bits = 0;
    bits |= (options >> kIOHIDEventScrollMomentumShift) & kIOHIDEventScrollMomentumMask;
    bits |= (options >> kIOHIDEventScrollMomentumLowerShift) & kIOHIDEventScrollMomentumLowerMask;
    return bits;
}

/// Converts IOHIDEventType to human readable string for more readable logging
- (nonnull NSString*)eventNameFromType:(uint32_t)eventType
{
    switch (eventType)
    {
        case kIOHIDEventTypeScroll:
            return @"Scroll";
        case kIOHIDEventTypeDockSwipe:
            return @"DockSwipe";
        case kIOHIDEventTypeScale:
            return @"Scale";
        case kIOHIDEventTypeRotation:
            return @"Rotation";
        case kIOHIDEventTypeTranslation:
            return @"Translation";
        case kIOHIDEventTypeFluidTouchGesture:
            return @"FluidTouchGesture";
        case kIOHIDEventTypeForce:
            return @"Force";
        default:
            return [NSString stringWithFormat:@"%d", eventType];
    }
}

/// Converts IOHIDEventPhaseBits/IOHIDEventScrollMomentumBits to human readable string for more readable logging
- (nonnull NSString*)phaseStringFromValue:(IOHIDEventOptionBits)options
                            isScrollEvent:(bool)isScrollEvent
{
    IOHIDEventPhaseBits phase = [self getPhaseFromOptions:options];
    NSString* phaseString = nil;
    switch (phase)
    {
        case kIOHIDEventPhaseUndefined:
            phaseString = @"PhaseUndefined";
            break;
        case kIOHIDEventPhaseEnded:
            phaseString = @"PhaseEnded";
            break;
        case kIOHIDEventPhaseCancelled:
            phaseString = @"PhaseCancelled";
            break;
        case kIOHIDEventPhaseMayBegin:
            phaseString = @"PhaseMayBegan";
            break;
        case kIOHIDEventPhaseBegan:
            phaseString = @"PhaseBegan";
            break;
        case kIOHIDEventPhaseChanged:
            phaseString = @"PhaseChanged";
            break;
        default:
            phaseString = [NSString stringWithFormat:@"%d", phase];
    }

    IOHIDEventScrollMomentumBits scrollMomentum = [self getScrollMomentumFromOptions:options];
    NSString* momentumString = nil;
    switch (scrollMomentum)
    {
        case kIOHIDEventScrollMomentumUndefined:
            momentumString = @"MomentumUndefined";
            break;
        case kIOHIDEventScrollMomentumContinue:
            momentumString = @"MomentumContinue";
            break;
        case kIOHIDEventScrollMomentumStart:
            momentumString = @"MomentumStart";
            break;
        case kIOHIDEventScrollMomentumEnd:
            momentumString = @"MomentumEnd";
            break;
        case kIOHIDEventScrollMomentumWillBegin:
            momentumString = @"MomentumWillBegin";
            break;
        case kIOHIDEventScrollMomentumInterrupted:
            momentumString = @"MomentumInterrupted";
            break;
        default:
            momentumString = [NSString stringWithFormat:@"%d", scrollMomentum];
    }

    return isScrollEvent ? [NSString stringWithFormat:@"(%@,%@)", phaseString, momentumString] : phaseString;
}

/// Given any HID event, determines which types we care about
/// - Parameter event: HID event that we check before processing
- (void)handleHIDEvent:(nonnull HIDEvent*)event
{
    [self updateGesturePhases:self.gesturePhases withEvent:event depth:0 ignoreList:self.ignoreList];
}

- (void)updateGesturePhases:(nonnull NSMutableDictionary<NSNumber*, GesturePhase*>*)gesturePhases withEvent:(nonnull HIDEvent*)event depth:(NSUInteger)depth ignoreList:(NSDictionary* _Nullable)ignoreList
{
    // Fetch existing `GesturePhase` or create a new one
    GesturePhase* gesturePhase = gesturePhases[@(event.type)];
    if (!gesturePhase)
    {
        gesturePhase = [[GesturePhase alloc] initWithType:event.type];
        gesturePhases[@(event.type)] = gesturePhase;
    }

    // Grab the ignore dictionary for the event type at the current depth. Check if imbalances should be ignored
    NSDictionary* ignoreListEvent = ignoreList[@(event.type)];
    NSNumber* ignoreImbalance = ignoreListEvent[@"Ignore"];

    // Check for a gesture imbalance
    bool balancedEvent = [self checkForImbalanceInEvent:event previousOptions:gesturePhase.options ignoreImbalance:ignoreImbalance.boolValue];
    bool isScrollEvent = event.type == kIOHIDEventTypeScroll;
    os_log_debug(logHandle(), "%@%@: %@ -> %@ (imbalance=%d ignored=%d)", [@"" stringByPaddingToLength:depth * 3 withString:@"   " startingAtIndex:0], [self eventNameFromType:event.type], [self phaseStringFromValue:gesturePhase.options isScrollEvent:isScrollEvent], [self phaseStringFromValue:event.options isScrollEvent:isScrollEvent], !balancedEvent, ignoreImbalance.boolValue);

    // Update current options
    gesturePhase.options = event.options;

    // Recursively check all child events
    for (HIDEvent* childEvent in event.children)
    {
        [self updateGesturePhases:gesturePhase.children withEvent:childEvent depth:depth + 1 ignoreList:ignoreListEvent[@"Children"]];
    }
}

- (bool)checkForImbalanceInEvent:(nonnull HIDEvent*)event previousOptions:(IOHIDEventOptionBits)previousOptions ignoreImbalance:(BOOL)ignoreImbalance
{
    bool isScrollEvent = event.type == kIOHIDEventTypeScroll;
    IOHIDEventPhaseBits previousPhase = [self getPhaseFromOptions:previousOptions];
    IOHIDEventPhaseBits phase = [self getPhaseFromOptions:event.options];

    // Checks that the standard HID event phases follow the expected sequence
    bool validNextPhase = false;
    switch (previousPhase)
    {
        case kIOHIDEventPhaseUndefined:
        case kIOHIDEventPhaseEnded:
        case kIOHIDEventPhaseCancelled:
            // Not all(read: most) gestures do not use MayBegin. So need to check for MayBegin and Began
            validNextPhase = phase == kIOHIDEventPhaseUndefined || phase == kIOHIDEventPhaseMayBegin || phase == kIOHIDEventPhaseBegan;
            break;
        case kIOHIDEventPhaseMayBegin:
            // MayBegin is a one-shot are is not balanced
            validNextPhase = phase == kIOHIDEventPhaseBegan || phase == kIOHIDEventPhaseCancelled || phase == kIOHIDEventPhaseUndefined;
            break;
        case kIOHIDEventPhaseBegan:
        case kIOHIDEventPhaseChanged:
            validNextPhase = phase == kIOHIDEventPhaseChanged || phase == kIOHIDEventPhaseEnded || phase == kIOHIDEventPhaseCancelled;
            break;
        default:
            os_log_error(logHandle(), "Gesture with unexpected previous phase %d", previousPhase);
            break;
    }

    // Scroll events transition from using the standard HID event phases to momentum phases.
    // To avoid overwriting a false validNextPhase, we should only check momentum phases when standard phases are successful
    if (validNextPhase && isScrollEvent)
    {
        IOHIDEventScrollMomentumBits previousScrollMomentum = [self getScrollMomentumFromOptions:previousOptions];
        IOHIDEventScrollMomentumBits scrollMomentum = [self getScrollMomentumFromOptions:event.options];

        switch (previousScrollMomentum)
        {
            case kIOHIDEventScrollMomentumUndefined:
                // Undefined and momentum ending conditions should always result in undefined and eventually MayBegin
                validNextPhase = scrollMomentum == kIOHIDEventScrollMomentumUndefined
                    || scrollMomentum == kIOHIDEventScrollMomentumWillBegin;
            case kIOHIDEventScrollMomentumEnd:
            case kIOHIDEventScrollMomentumInterrupted:
            case (kIOHIDEventScrollMomentumEnd | kIOHIDEventScrollMomentumInterrupted):
                // Undefined and momentum ending conditions should always result in undefined and eventually MayBegin
                validNextPhase = scrollMomentum == kIOHIDEventScrollMomentumUndefined
                    || scrollMomentum == kIOHIDEventScrollMomentumWillBegin;
                break;
            case kIOHIDEventScrollMomentumWillBegin:
                // WillBegin/MayBegin are one-shot and are NOT balanced.
                validNextPhase = scrollMomentum == kIOHIDEventScrollMomentumUndefined
                    || scrollMomentum == kIOHIDEventScrollMomentumStart;
                break;
            case kIOHIDEventScrollMomentumStart:
            case kIOHIDEventScrollMomentumContinue:
                validNextPhase = scrollMomentum == kIOHIDEventScrollMomentumContinue
                    || scrollMomentum == kIOHIDEventScrollMomentumEnd
                    || scrollMomentum == (kIOHIDEventScrollMomentumEnd | kIOHIDEventScrollMomentumInterrupted);
                break;
            default:
                os_log_error(logHandle(), "Scroll with unexpected momentum phase %d", previousScrollMomentum);
        }
    }

    // Handle unbalanced gesture logging!
    if (!validNextPhase && !ignoreImbalance)
    {
        // Make room for the latest imbalance
        if (self.gestureImbalances.count >= kMaxTrackedImbalancesPerDevice)
        {
            [self.gestureImbalances removeObjectAtIndex:0];
        }

        [self.gestureImbalances addObject:@{
            @"Date" : NSDate.now,
            @"EventType" : @(event.type),
            @"PreviousOptions" : @(previousOptions),
            @"CurrentOptions" : @(event.options),
        }];

        os_log_error(logHandle(), "Unbalanced %@ gesture detected for service 0x%08X. Phased changed from %@ to %@", [self eventNameFromType:event.type], self.serviceID.unsignedIntValue, [self phaseStringFromValue:previousOptions isScrollEvent:isScrollEvent], [self phaseStringFromValue:event.options isScrollEvent:isScrollEvent]);
    }

    return validNextPhase;
}

- (nonnull NSDictionary*)debug
{
    // Convert gesturePhase to use NSString keys to avoid crashing WindowServer
    // HID event system does not play nice with non-NSString keys :(
    NSMutableArray<NSDictionary*>* gesturePhases = [NSMutableArray new];
    [self.gesturePhases enumerateKeysAndObjectsUsingBlock:^(NSNumber* key, GesturePhase* gesturePhase, BOOL* stop) {
        [gesturePhases addObject:gesturePhase.debug];
    }];

    return @{
        @"ServiceID" : self.serviceID,
        @"RemovedAt" : self.removedAt ?: @"Never",
        @"GesturePhases" : gesturePhases,
        @"GestureImbalances" : self.gestureImbalances
    };
}

@end
