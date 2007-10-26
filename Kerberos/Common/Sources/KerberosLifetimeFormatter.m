/*
 * KerberosLifetimeFormatter.m
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

#import "KerberosLifetimeFormatter.h"

@implementation KerberosLifetimeFormatter

// ---------------------------------------------------------------------------

- (id) initWithDisplaySeconds: (BOOL) seconds
                  shortFormat: (BOOL) shortFormat
{
    // Assume a 1-1 mapping between lifetime and control values
    return [self initWithControlMinimum: 0 
                       controlIncrement: 1 
                         displaySeconds: seconds 
                            shortFormat: shortFormat];
}

// ---------------------------------------------------------------------------

- (id) initWithControlMinimum: (int) minimum
             controlIncrement: (int) increment
{
    return [self initWithControlMinimum: minimum 
                       controlIncrement: increment
                         displaySeconds: YES 
                            shortFormat: NO];    
}

// ---------------------------------------------------------------------------

- (id) initWithControlMinimum: (int) minimum
             controlIncrement: (int) increment
              displaySeconds: (BOOL) seconds
                 shortFormat: (BOOL) shortFormat
{
    if ((self = [super init])) {
        controlMinimum = minimum;      // the lifetime value of the control's zero-point
        controlIncrement = increment;  // the lifetime increment of each control step
        displaySeconds = seconds;
        displayShortFormat = shortFormat;
    }
    return self;
}

// ---------------------------------------------------------------------------

- (time_t) lifetimeForControl: (NSControl *) control
{
    return controlMinimum + ([control intValue] * controlIncrement);
}
 
// ---------------------------------------------------------------------------

- (NSString *) stringForLifetime: (time_t) lifetime
{
    NSMutableString *string = [NSMutableString string];
    NSString *separatorKey = (displayShortFormat) ? @"LifetimeStringSeparatorShortFormat" : 
        @"LifetimeStringSeparatorLongFormat";
    NSString *separator = NSLocalizedStringFromTable (separatorKey, @"KerberosLifetimeFormatter", NULL);
    NSString *key = NULL;
    
    // Break the lifetime up into time units
    time_t days     = (lifetime / 86400);
    time_t hours    = (lifetime / 3600 % 24);
    time_t minutes  = (lifetime / 60 % 60);
    time_t seconds  = (lifetime % 60);
    
    // If we aren't going to display seconds, round up
    if (!displaySeconds) {
        if (seconds >  0) { seconds = 0; minutes++; }
        if (minutes > 59) { minutes = 0; hours++; }
        if (hours   > 23) { hours   = 0; days++; }
    }
    
    if (days > 0) {
        if (displayShortFormat) { 
            key = (days > 1) ? @"LifetimeStringDaysShortFormat" : @"LifetimeStringDayShortFormat"; 
        } else { 
            key = (days > 1) ? @"LifetimeStringDaysLongFormat" : @"LifetimeStringDayLongFormat";
        }
        [string appendFormat: NSLocalizedStringFromTable (key, @"KerberosLifetimeFormatter", NULL), days];
    }
    
    if ((hours > 0) || displayShortFormat) {
        if (displayShortFormat) { 
            key = (hours > 1) ? @"LifetimeStringHoursShortFormat" : @"LifetimeStringHourShortFormat"; 
        } else { 
            key = (hours > 1) ? @"LifetimeStringHoursLongFormat" : @"LifetimeStringHourLongFormat";
        }
        if ([string length] > 0) { [string appendString: separator]; }
        [string appendFormat: NSLocalizedStringFromTable (key, @"KerberosLifetimeFormatter", NULL), hours];
    }
    
    if ((minutes > 0) || displayShortFormat) {
        if (displayShortFormat) { 
            key = (minutes > 1) ? @"LifetimeStringMinutesShortFormat" : @"LifetimeStringMinuteShortFormat"; 
        } else { 
            key = (minutes > 1) ? @"LifetimeStringMinutesLongFormat" : @"LifetimeStringMinuteLongFormat";
        }
        if ([string length] > 0) { [string appendString: separator]; }
        [string appendFormat: NSLocalizedStringFromTable (key, @"KerberosLifetimeFormatter", NULL), minutes];
    }
    
    if (displaySeconds && ((seconds > 0) || displayShortFormat)) {
        if (displayShortFormat) { 
            key = (seconds > 1) ? @"LifetimeStringSecondsShortFormat" : @"LifetimeStringSecondShortFormat"; 
        } else { 
            key = (seconds > 1) ? @"LifetimeStringSecondsLongFormat" : @"LifetimeStringSecondLongFormat";
        }
        if ([string length] > 0) { [string appendString: separator]; }
        [string appendFormat: NSLocalizedStringFromTable (key, @"KerberosLifetimeFormatter", NULL), seconds];
    }

    // Return an NSString (non-mutable) from our mutable temporary
    return [NSString stringWithString: string];
}

// ---------------------------------------------------------------------------

- (NSString *) stringForObjectValue: (id) anObject
{
    return [self stringForLifetime: [self lifetimeForControl: anObject]];
}

// ---------------------------------------------------------------------------

- (NSAttributedString *) attributedStringForObjectValue: (id) anObject withDefaultAttributes: (NSDictionary *) attributes
{
    // Pass attributes through
    return [[[NSAttributedString alloc] initWithString: [self stringForObjectValue: anObject] 
                                            attributes: attributes] autorelease];
}

@end
