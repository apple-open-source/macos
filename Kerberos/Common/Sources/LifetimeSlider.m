/*
 * LifetimeSlider.m
 *
 * $Header: /cvs/kfm/Common/Sources/LifetimeSlider.m,v 1.2 2004/05/14 01:02:25 lxs Exp $
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

#import "LifetimeSlider.h"
#import "LifetimeFormatter.h"

void SetupLifetimeSlider (NSSlider *slider, 
                          NSTextField *textField, 
                          time_t minimum, 
                          time_t maximum, 
                          time_t value)
{
    time_t min = minimum;
    time_t max = maximum;
    time_t increment = 0;
    
    if (max < min) {
        // swap values
        time_t temp = max;
        max = min;
        min = temp;
    }
    
    int	range = max - min;
    
    if (range < 5*60)              { increment = 1;       // 1 second if under 5 minutes
    } else if (range < 30*60)      { increment = 5;       // 5 seconds if under 30 minutes
    } else if (range < 60*60)      { increment = 15;      // 15 seconds if under 1 hour
    } else if (range < 2*60*60)    { increment = 30;      // 30 seconds if under 2 hours
    } else if (range < 5*60*60)    { increment = 60;      // 1 minute if under 5 hours
    } else if (range < 50*60*60)   { increment = 5*60;    // 5 minutes if under 50 hours
    } else if (range < 200*60*60)  { increment = 15*60;   // 15 minutes if under 200 hours
    } else if (range < 500*60*60)  { increment = 30*60;   // 30 minutes if under 500 hours
    } else                         { increment = 60*60; } // 1 hour otherwise
    
    int roundedMinimum = (min / increment) * increment;
    if (roundedMinimum > min) { roundedMinimum -= increment; }
    if (roundedMinimum <= 0)  { roundedMinimum += increment; }  // ensure it is positive
    
    int roundedMaximum = (max / increment) * increment;
    if (roundedMaximum < max) { roundedMaximum += increment; }
    
    int roundedValue = (value / increment) * increment;
    if (roundedValue < roundedMinimum) { roundedValue = roundedMinimum; }
    if (roundedValue > roundedMaximum) { roundedValue = roundedMaximum; }
    
    if (roundedMinimum == roundedMaximum) {
        [textField setTextColor: [NSColor grayColor]];
        [slider setEnabled: FALSE];
    } else {
        [textField setTextColor: [NSColor blackColor]];
        [slider setEnabled: TRUE];
    }
    
    // Attach the formatter to the slider
    NSDateFormatter *lifetimeFormatter = [[LifetimeFormatter alloc] initWithControlMinimum: roundedMinimum
                                                                          controlIncrement: increment];
    
    [textField setFormatter: lifetimeFormatter];
    [lifetimeFormatter release];  // the textField will retain it
    
    [slider setMinValue: 0];
    [slider setMaxValue: (roundedMaximum - roundedMinimum) / increment];
    [slider setIntValue: (roundedValue - roundedMinimum) / increment];
    [textField takeObjectValueFrom: slider];    
}