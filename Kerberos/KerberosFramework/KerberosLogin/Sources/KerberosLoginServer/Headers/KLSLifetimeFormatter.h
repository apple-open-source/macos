/*
 * KLSLifetimeFormatter.h
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLoginServer/Headers/KLSLifetimeFormatter.h,v 1.3 2003/04/23 21:58:47 lxs Exp $
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

@interface KLSLifetimeFormatter : NSFormatter
{
    NSString *noneString;
    NSString *dayString;
    NSString *daysString;
    NSString *hourString;
    NSString *hoursString;
    NSString *minuteString;
    NSString *minutesString;
    NSString *secondString;
    NSString *secondsString;
    NSString *separatorString;

    int minimumValue;
    int maximumValue;
    int incrementValue;
}

- (id) initWithMinimum: (int) minimum
               maximum: (int) maximum
             increment: (int) increment;

- (KLLifetime) lifetimeForInt: (int) number;
- (NSString *)stringForObjectValue: (id) anObject;
- (NSAttributedString *)attributedStringForObjectValue: (id) anObject withDefaultAttributes: (NSDictionary *) attributes;

@end
