//
//  DispatchStubs.m
//  IOKitPMUnitTests
//
//  Created by Faramola Isiaka on 7/21/21.
//

#import <Foundation/Foundation.h>
#include "DispatchStubs.h"

dispatch_time_t currentTime = 0;

@interface FakeTimer:NSObject {
   //Instance variables
    dispatch_source_t _timerPointer;
    dispatch_time_t _startTime;
    dispatch_time_t _interval;
    dispatch_time_t _leeway;
    dispatch_block_t _eventHandler;
    dispatch_block_t _cancelHandler;
    dispatch_time_t _lastTriggerTime;
}
@end

@implementation FakeTimer

/**
 * @brief Initializer of a FakeTimer object
 * @result a new FakeTimer Object
 */
- (instancetype)init
{
  self = [super init];
  if (self) {
      _timerPointer = NULL;
      _startTime = 0;
      _interval = 0;
      _leeway = 0;
      _eventHandler = nil;
      _cancelHandler = nil;
      _lastTriggerTime = 0;
  }
  return self;
}

/**
 * @brief Gets the pointer to real dispatch timer associated with this FakeTimer.
 * @result pointer to the real dispatch timer
 */
- (dispatch_source_t) getTimerPointer
{
    return _timerPointer;
}

/**
 * @brief Gets the time at which this timer was last triggered
 * @result An abstract representation of time associated with the last time this timer was triggered
 */
- (dispatch_time_t) getLastTriggerTime
{
    return _lastTriggerTime;
}

/**
 * @brief Updates the pointer to real dispatch timer associated with this FakeTimer.
 * @param timerPointer pointer to the real dispatch timer
 */
- (void) updateTimerPointer:(dispatch_source_t) timerPointer
{
    _timerPointer = timerPointer;
}

/**
 * @brief Updates the settings associated with this FakeTimer object.
 * @param startTime The start time of the timer
 * @param interval the nanosecond interval at which the timer triggers
 * @param leeway the amount of time, in nanoseconds, that the system can defer the timer.
 */
- (void) updateTimerSettings:(dispatch_time_t) startTime interval:(dispatch_time_t) interval leeway:(dispatch_time_t) leeway
{
    _startTime = startTime;
    _interval = interval;
    _leeway = leeway;
    _lastTriggerTime = startTime;
}

/**
 * @brief Updates the time at which this timer was last triggered
 * @param lastTriggerTime the new last trigger time
 */
- (void) updateLastTriggerTime:(dispatch_time_t) lastTriggerTime
{
    _lastTriggerTime = lastTriggerTime;
}

/**
 * @brief Updates the event handler associated with a FakeTimer object
 * @param eventHandler the new event handler
 */
- (void) updateEventHandler:(dispatch_block_t) eventHandler
{
    _eventHandler = eventHandler;
}

/**
 * @brief Updates the cancel handler associated with a FakeTimer object
 * @param cancelHandler the new cancel handler
 */
- (void) updateCancelHandler:(dispatch_block_t) cancelHandler
{
    _cancelHandler = cancelHandler;
}

/**
 * @brief Calls the event handler associated with a FakeTimer object
 */
- (void) callEventHandler
{
    _eventHandler();
}

/**
 * @brief Calls the cancel handler associated with a FakeTimer object
 */
- (void) callCancelHandler
{
    _cancelHandler();
}

/**
 * @brief Checks if a FakeTimer object is eligible for triggering.
 *       We say it is eligible if the duration between the last trigger time and the current time
 *       is greater than or equal to the FakeTimer's interval.
 */
- (void) checkForTrigger
{
    dispatch_time_t timeElapsed = currentTime - _lastTriggerTime;
    if(timeElapsed >= _interval)
    {
        uint64_t epochs = timeElapsed / _interval;
        for(uint64_t i = 0; i < epochs; i++)
        {
            [self callEventHandler];
        }
        [self updateLastTriggerTime:currentTime];
    }
}
@end

NSMutableDictionary<NSValue*, FakeTimer* > *fakeTimers;

/**
 * @brief Returns the current time within the DispatchStubs.
 * @result an abstract representation of the current time
 */
dispatch_time_t getCurrentTime(void)
{
    return currentTime;
}

/**
 * @brief Advances the current time within the DispatchStubs by the specified delta.
 * @param delta the amount of time to increase the current time by.
 */
void advanceTime(dispatch_time_t delta)
{
    currentTime += delta;
    
    for(id key in fakeTimers)
    {
        [[fakeTimers objectForKey:key] checkForTrigger];
    }
}

/**
 * @brief Clears all FakeTimer objects
 */
void clearTimers(void)
{
    fakeTimers = nil;
}

/**
 * @brief Resets the  the current time within the DispatchStubs and clears all FakeTimer objects
 */
void clearTime(void)
{
    currentTime = 0;
    clearTimers();
}

/**
 * @brief This is a stub for creating a dispatch_time_t relative to the default clock or modifies an existing dispatch_time_t.
 * @param when the dispatch_function_t value to use as the basis for a new value.  Pass DISPATCH_TIME_NOW to create a new time value relative to now.
 * @param delta the number of nanoseconds to add to the time in the when parameter.
 * @result A new dispatch_time_t.
 */
dispatch_time_t dispatch_time(dispatch_time_t when, int64_t delta)
{
    if(when == DISPATCH_TIME_FOREVER)
    {
        return DISPATCH_TIME_FOREVER;
    }
    if(when == DISPATCH_TIME_NOW)
    {
        return currentTime + delta;
    }
    return when + delta;
}

/**
 * @brief This is a stub for creating a dispatch_time_t using an absolute time according to the wall clock.
 * @param when A struct timespec to add time to. If NULL is passed, then this function uses the current time within DispatchStubs
 * @param delta nanoseconds to add
 * @result A new dispatch_time_t.
 */
dispatch_time_t dispatch_walltime(const struct timespec *_Nullable when, int64_t delta)
{
    if(when)
    {
        return when->tv_nsec + (when->tv_sec * NSEC_PER_SEC) + delta;
    }
    return currentTime + delta;
}

/**
 * @brief This stub creates a new dispatch source and an associated FakeTimer object. This stub only handles DISPATCH_SOURCE_TYPE_TIMER and returns NULL for all other types.
 * @param type the type of the dispatch source.
 * @param handle the underlying system handle to monitor
 * @param mask a mask of flags specifying which events are desired.
 * @param queue the dispatch queue to which the event handler block is submitted.
 * @result A new dispatch_time_t.
 */
dispatch_source_t dispatch_source_create(dispatch_source_type_t type, __unused uintptr_t handle, __unused uintptr_t mask, __unused dispatch_queue_t _Nullable queue)
{
    if(type == DISPATCH_SOURCE_TYPE_TIMER)
    {
        if(!fakeTimers)
        {
            fakeTimers = [[NSMutableDictionary alloc] init];
        }
        dispatch_source_t ds = [NSObject<OS_dispatch_source> new];
        FakeTimer* fakeTimer = [[FakeTimer alloc] init];
        [fakeTimer updateTimerPointer:ds];
        [fakeTimers setObject:fakeTimer forKey:[NSValue valueWithNonretainedObject:ds]];
        
        return ds;
    }
    // we only care about timers right now
    return NULL;
}

/**
 * @brief This stub sets a start time, interval, and leeway value for a timer source and its associated FakeTimer object.
 * @param source the dispatch source.
 * @param start the start time of the timer.
 * @param interval the nanosecond interval for the timer.
 * @param leeway the amount of time, in nanoseconds, that the system can defer the timer.
 */
void dispatch_source_set_timer(dispatch_source_t source, dispatch_time_t start, uint64_t interval, uint64_t leeway)
{
    NSValue* key = [NSValue valueWithNonretainedObject:source];
    FakeTimer *fakeTimer = [fakeTimers objectForKey:key];
    if(fakeTimer)
    {
        [fakeTimer updateTimerSettings:start interval:interval leeway:leeway];
        return;
    }
}

/**
 * @brief This stub sets the event handler block for the given dispatch source and its associated FakeTimer object.
 * @param source the dispatch source to modify.
 * @param handler the event handler block
 */
void dispatch_source_set_event_handler(dispatch_source_t source, dispatch_block_t _Nullable handler)
{
    NSValue* key = [NSValue valueWithNonretainedObject:source];
    FakeTimer *fakeTimer = [fakeTimers objectForKey:key];
    if(fakeTimer)
    {
        [fakeTimer updateEventHandler:handler];
    }
}

/**
 * @brief This stub sets the cancellation handler block for the given dispatch source and its associated FakeTimer object.
 * @param source the dispatch source to modify.
 * @param handler the cancellation handler block
 */
void dispatch_source_set_cancel_handler(dispatch_source_t source, dispatch_block_t handler)
{
    NSValue* key = [NSValue valueWithNonretainedObject:source];
    FakeTimer *fakeTimer = [fakeTimers objectForKey:key];
    if(fakeTimer)
    {
        [fakeTimer updateCancelHandler:handler];
    }
}

/**
 * @brief This is a stub for canceling the dispatch source and its associated FakeTimer object, preventing any further invocation of its event handler block.
 * @param source the dispatch source to be canceled
 */
void dispatch_source_cancel(dispatch_source_t source)
{
    if(source)
    {
        NSValue* key = [NSValue valueWithNonretainedObject:source];
        FakeTimer *fakeTimer = [fakeTimers objectForKey:key];
        if(fakeTimer)
        {
            [fakeTimer updateEventHandler:nil];
            [fakeTimer callCancelHandler];
        }
    }
}

/**
 * @brief This is a stub for resuming the invocation of block objects on a dispatch object.
 * @param object the object to be resumed.
 */
void dispatch_resume(dispatch_object_t __unused object)
{
    return;
}

/**
 * @brief This is a stub for suspending the invocation of block objects on a dispatch object.
 * @param object the object to be suspended.
 */
void dispatch_suspend(dispatch_object_t __unused object)
{
    return;
}

/**
 * @brief Teardown Function for DispatchStubs
 */
void DispatchStubsTeardown(void)
{
    clearTimers();
}

