/*
 * KLSLifetimeFormatter.m
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLoginServer/KLSLifetimeFormatter.m,v 1.3 2003/04/23 21:58:49 lxs Exp $
 *
 * Copyright 2003 Massachusetts Institute of Technology.
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

#import "KLSLifetimeFormatter.h"

@implementation KLSLifetimeFormatter

- (id) initWithMinimum: (int) minimum
               maximum: (int) maximum
             increment: (int) increment
{
    noneString      = NSLocalizedString (@"KLStringNoLifetime", NULL);
    dayString       = NSLocalizedString (@"KLStringDay", NULL);
    daysString      = NSLocalizedString (@"KLStringDays", NULL);
    hourString      = NSLocalizedString (@"KLStringHour", NULL);
    hoursString     = NSLocalizedString (@"KLStringHours", NULL);
    minuteString    = NSLocalizedString (@"KLStringMinute", NULL);
    minutesString   = NSLocalizedString (@"KLStringMinutes", NULL);
    secondString    = NSLocalizedString (@"KLStringSecond", NULL);
    secondsString   = NSLocalizedString (@"KLStringSeconds", NULL);
    separatorString = NSLocalizedString (@"KLStringLifetimeSeparator", NULL);

    minimumValue = minimum;
    maximumValue = maximum;
    incrementValue = increment;
    
    return self;
}

- (KLLifetime) lifetimeForInt: (int) number
{
    return minimumValue + (number * incrementValue);
}

- (NSString *)stringForObjectValue:(id)anObject
{
    KLLifetime lifetime;
    KLTime days, hours, minutes, seconds;
    NSMutableString *tempString = [NSMutableString string]; // temporary mutable string
    
    // check to make sure we have a number (so we can use the intValue method)
    if (![anObject isKindOfClass:[NSNumber class]]) {
        return NULL;
    }
    
    // Break up the lifetime into hours, minutes and seconds
    lifetime = minimumValue + ([anObject intValue] * incrementValue);
    days = lifetime / 86400;
    hours = lifetime / 3600 % 24;
    minutes = lifetime / 60 % 60;
    seconds = lifetime % 60;
    
    // Create the lifetime description.  Do singulars and plurals too.
    if (days == 0 && hours == 0 && minutes == 0 && seconds == 0) {
        [tempString appendString: noneString]; 
    } else {
        if (days > 0) {
            if ([tempString length] > 0) {
                [tempString appendString: separatorString];
            }
            [tempString appendFormat:@"%ld %@", days, (days == 1 ? dayString : daysString)];
        }
        
        if (hours > 0) {
            if ([tempString length] > 0) {
                [tempString appendString: separatorString];
            }
            [tempString appendFormat:@"%ld %@", hours, (hours == 1 ? hourString : hoursString)];
        }
        
        if (minutes > 0) {
            if ([tempString length] > 0) {
                [tempString appendString: separatorString];
            }
            [tempString appendFormat:@"%ld %@", minutes, (minutes == 1 ? minuteString : minutesString)];            
        }

        if (seconds > 0) {
            if ([tempString length] > 0) {
                [tempString appendString: separatorString];
            }
            [tempString appendFormat:@"%ld %@", seconds, (seconds == 1 ? secondString : secondsString)];            
        }
    }

    // Return an NSString (non-mutable) from our mutable temporary
    return [NSString stringWithString: tempString];
}

- (NSAttributedString *)attributedStringForObjectValue:(id)anObject withDefaultAttributes:(NSDictionary *)attributes
{
    // Pass attributes through
    NSAttributedString *outString = [[NSAttributedString alloc] initWithString:[self stringForObjectValue: anObject] 
                                                                attributes:attributes];
	return [outString autorelease];
}

@end
