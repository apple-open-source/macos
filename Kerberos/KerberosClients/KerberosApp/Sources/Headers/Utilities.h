/*
 * Utilities.h
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

#define DAYS(x)           (x / 86400)
#define HOURS(x)          (x / 3600 % 24)
#define MINUTES(x)        (x / 60 % 60)
#define SECONDS(x)        (x % 60)
#define ROUNDEDMINUTES(x) ((SECONDS (x) > 0) ? (MINUTES (x) + 1) : MINUTES (x))
#define kFiveMinutes      (5*60)

#define kNoAction               0
#define kGetTicketsAction       1
#define kRenewTicketsAction     2
#define kDestroyTicketsAction   3
#define kChangePasswordAction   4
#define kChangeActiveUserAction 5

@interface Utilities : NSObject
{
}

+ (NSString *) stringForCCVersion: (cc_uint32) version;

+ (NSString *) stringForCredentialState: (int) state 
                                 format: (int) format;

+ (NSString *) stringForTimeRemaining: (cc_time_t) timeRemaining 
                                state: (int) state 
                               format: (int) format;

+ (NSDictionary *) attributesForInfoWindowWithTicketState: (int) state;

+ (NSDictionary *) attributesForDockIcon;

+ (NSDictionary *) attributesForMenuItemOfFontSize: (float) fontSize 
                                            italic: (BOOL) isItalic;

+ (NSDictionary *) attributesForTicketColumnCellOfControlSize: (NSControlSize) controlSize 
                                                         bold: (BOOL) isBold 
                                                       italic: (BOOL) isItalic;

+ (NSDictionary *) attributesForLifetimeColumnCellOfControlSize: (NSControlSize) controlSize
                                                           bold: (BOOL) isBold 
                                                          state: (int) state
                                                  timeRemaining: (cc_time_t) timeRemaining;

+ (void) synchronizeCacheMenu: (NSMenu *) menu
                     fontSize: (float) fontSize
       staticPrefixItemsCount: (int) staticPrefixItemsCount
                   headerItem: (BOOL) headerItem
            checkDefaultCache: (BOOL) checkDefaultCache
            defaultCacheIndex: (int *) defaultCacheIndex
                     selector: (SEL) selector
                       sender: (id) sender;

+ (NSString *) stringForErrorCode: (int) error;

+ (void) displayAlertForError: (KLStatus) error 
                       action: (int) action 
                       sender: (id) sender;

@end
