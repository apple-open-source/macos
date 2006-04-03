/*
 * TargetOwnedTimer.m
 *
 * $Header$
 *
 * Copyright 2004 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#import "TargetOwnedTimer.h"

// Kerberos.app has a bunch of objects which are created by the main controller
// (either directly or indirectly) and released when the controller no longer 
// needs them.  Some of these objects want NSTimers -- and they want the NSTimer
// invalidated when they are released by the controller.
//
// Unfortunately, NSTimers retain their targets. As a result, if the objects were
// directly instantiated the timers, they would never get freed when released 
// since the timer retained the object.  Yay for circular dependencies created 
// by refcounted objects!  Woooo!
//
// This isn't inherently wrong.  Owning the object you plan to call back into is 
// safer because you know it will exist when you call into it.  And depending on 
// how your application works, you might actually want your objects to persist 
// until the timer fires and then be released.  
//
// Obviously we could create invalidate methods for every Kerberos object that needs
// to create a timer and then call the timer's invalidate method from there, but 
// that sort of defeats the usefulness of having refcounted objects in the first 
// place.  And it's prone to simple logic errors which cause silent memory leaks
// since a number of the aforementioned objects are in NSArrays and are only
// freed as a side effect of being removed from the array.
//
// Instead, here is a TargetOwnedTimer class which encapsulates an NSTimer so that  
// the real NSTimer can retain this class.  This reduces the above problem to just
// having to remember to invalidate the TargetOwnedTimer before releasing it in the
// real object's dealloc method (which is a lot easier to remember and verify).  
// When the real object dealloc method is called, it can manually deallocate the 
// TargetOwnedTimer using invalidate (removing the refcount) followed by release.
//
// Basically this just makes timer memory management work like it does for 
// NSNotifications.


@implementation TargetOwnedTimer

// ---------------------------------------------------------------------------

+ (TargetOwnedTimer *) scheduledTimerWithTimeInterval: (NSTimeInterval) seconds 
                                               target: (id) target 
                                             selector: (SEL) selector 
                                             userInfo: (id) userInfo 
                                              repeats: (BOOL) repeats
{
    TargetOwnedTimer *toTimer = [[TargetOwnedTimer alloc] initWithTimeInterval: seconds 
                                                                        target: target 
                                                                      selector: selector 
                                                                      userInfo: userInfo 
                                                                       repeats: repeats];
    [[NSRunLoop currentRunLoop] addTimer: [toTimer timer] forMode: NSDefaultRunLoopMode];
    return [toTimer autorelease];
}

// ---------------------------------------------------------------------------

+ (TargetOwnedTimer *) scheduledTimerWithFireDate: (NSDate *) fireDate
                                         interval: (NSTimeInterval) seconds 
                                           target: (id) target 
                                         selector: (SEL) selector 
                                         userInfo: (id) userInfo 
                                          repeats: (BOOL) repeats
{
    TargetOwnedTimer *toTimer = [[TargetOwnedTimer alloc] initWithFireDate: fireDate
                                                                  interval: seconds 
                                                                    target: target 
                                                                  selector: selector 
                                                                  userInfo: userInfo 
                                                                   repeats: repeats];
    [[NSRunLoop currentRunLoop] addTimer: [toTimer timer] forMode: NSDefaultRunLoopMode];
    return [toTimer autorelease];
}

// ---------------------------------------------------------------------------

- (id) initWithTimeInterval: (NSTimeInterval) seconds 
                     target: (id) target 
                   selector: (SEL) selector 
                   userInfo: (id) userInfo 
                    repeats: (BOOL) repeats
{
    if ((self = [super init])) {
        timerTarget = target;
        timerSelector = selector;
        timerUserInfo = userInfo;
        
        dprintf ("TargetOwnedTimer for %lx initializing", (long) timerTarget);        
        
        timer = [[NSTimer timerWithTimeInterval: seconds
                                        target: self
                                      selector: @selector (timer:)
                                      userInfo: NULL
                                       repeats: repeats] retain];
        if (timer == NULL) {
            [self release];
            return NULL;
        }
    }
    return self;
}

// ---------------------------------------------------------------------------

- (id) initWithFireDate: (NSDate *) fireDate
               interval: (NSTimeInterval) seconds
                 target: (id) target 
               selector: (SEL) selector 
               userInfo: (id) userInfo 
                repeats: (BOOL) repeats
{
    if ((self = [super init])) {
        timerTarget = target;
        timerSelector = selector;
        timerUserInfo = userInfo;
        
        dprintf ("TargetOwnedTimer for %lx initializing", (long) timerTarget);        
        
        timer = [[NSTimer alloc] initWithFireDate: fireDate
                                        interval: seconds
                                           target: self
                                         selector: @selector (timer:)
                                         userInfo: NULL
                                          repeats: repeats];
        if (timer == NULL) {
            [self release];
            return NULL;
        }
    }
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    dprintf ("TargetOwnedTimer for %lx deallocating", (long) timerTarget);        
    [timer release];
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (NSTimer *) timer
{
    return timer;
}

// ---------------------------------------------------------------------------

- (void) invalidate
{
    // Call before releasing TargetOwnedTimer so its refcount goes to 0
    [timer invalidate];
}

// ---------------------------------------------------------------------------

- (BOOL) isValid
{
    return [timer isValid];
}

// ---------------------------------------------------------------------------

- (void) fire
{
    [timer fire];
}

// ---------------------------------------------------------------------------

- (NSDate *) fireDate
{
    return [timer fireDate];
}

// ---------------------------------------------------------------------------

- (void) setFireDate: (NSDate *) date
{
    [timer setFireDate: date];
}

// ---------------------------------------------------------------------------

- (NSTimeInterval) timeInterval
{
    return [timer timeInterval];
}


// ---------------------------------------------------------------------------

- (id) userInfo
{
    return timerUserInfo;
}

// ---------------------------------------------------------------------------

- (void) timer: (NSTimer *) timer
{
    [timerTarget performSelector: timerSelector withObject: self];
}

@end

