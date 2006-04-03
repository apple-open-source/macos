/*
 * Utilities.m
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

#import "Utilities.h"
#import "CacheCollection.h"
#import "Credential.h"
#import "LifetimeFormatter.h"

#define kItalicObliqueness       0.3

@implementation Utilities

// ---------------------------------------------------------------------------

+ (NSString *) stringForCCVersion: (cc_uint32) version
{
    NSString *key = @"KAppStringCredentialsVersionNone";
    
    if (version == cc_credentials_v5) {
        key = @"KAppStringCredentialsVersionV5";
    } else if (version == cc_credentials_v4) {
        key = @"KAppStringCredentialsVersionV4";        
    } else if (version == cc_credentials_v4_v5) {
        key = @"KAppStringCredentialsVersionV4V5";        
    }
    
    return NSLocalizedString (key, NULL);
}

// ---------------------------------------------------------------------------

+ (NSString *) stringForCredentialState: (int) state format: (int) format
{
    NSMutableString *string = [NSMutableString string];
    
    if (state == CredentialValid) {
        [string setString: NSLocalizedString (@"KAppStringValid", NULL)];
    } else {
        NSString *key = (state & CredentialExpired) ? @"KAppStringExpired" : @"KAppStringNotValid";
        [string setString: NSLocalizedString (key, NULL)];
        
        if (format == kLongFormat) {
            NSString *longStateString = NULL;
            
            if (state & CredentialBeforeStartTime) {
                longStateString = NSLocalizedString (@"KAppStringBeforeStartTime", NULL);
            } else if (state & CredentialNeedsValidation) {
                longStateString = NSLocalizedString (@"KAppStringNeedsValidation", NULL);            
            } else if (state & CredentialBadAddress) {
                longStateString = NSLocalizedString (@"KAppStringBadAddress", NULL);                            
            }
            
            if (longStateString != NULL) {
                NSString *longStateFormat = NSLocalizedString (@"KAppStringLongStateFormat", NULL);
                [string appendFormat: longStateFormat, longStateString];
            }
        }
    }
    
    return [NSString stringWithString: string];
}

// ---------------------------------------------------------------------------

+ (NSString *) stringForTimeRemaining: (cc_time_t) timeRemaining state: (int) state format: (int) format
{
    if (state == CredentialValid) {
        LifetimeFormatter *lifetimeFormatter = [[LifetimeFormatter alloc] initWithDisplaySeconds: NO
                                                                                     shortFormat: (format == kShortFormat)];
        NSString *string = [lifetimeFormatter stringForLifetime: timeRemaining];
        [lifetimeFormatter release];

        return string;
    } else {
        return [Utilities stringForCredentialState: state format: format];
    }
}

// ---------------------------------------------------------------------------

+ (NSDictionary *) attributesForInfoWindowWithTicketState: (int) state
{
    if (state == CredentialValid) {
        return [NSDictionary dictionary];
    } else {
        return [NSDictionary dictionaryWithObjectsAndKeys: 
            [NSColor redColor], NSForegroundColorAttributeName, NULL];
    }
}

// ---------------------------------------------------------------------------

+ (NSDictionary *) attributesForDockIcon
{
    NSMutableParagraphStyle *alignment = [[[NSMutableParagraphStyle alloc] init] autorelease];
    [alignment setParagraphStyle: [NSParagraphStyle defaultParagraphStyle]];
    [alignment setAlignment: NSLeftTextAlignment];
    
    return [NSDictionary dictionaryWithObjectsAndKeys: 
        [NSFont boldSystemFontOfSize: 22], NSFontAttributeName, 
        alignment, NSParagraphStyleAttributeName, NULL];
}

// ---------------------------------------------------------------------------

+ (NSDictionary *) attributesForMenuItemOfFontSize: (float) fontSize italic: (BOOL) isItalic
{
    NSFont *font = [NSFont menuFontOfSize: fontSize];
    NSNumber *obliqueness = [NSNumber numberWithFloat: isItalic ? kItalicObliqueness : 0.0];
    
    return [NSDictionary dictionaryWithObjectsAndKeys: 
        font, NSFontAttributeName, 
        obliqueness, NSObliquenessAttributeName, NULL];
}

// ---------------------------------------------------------------------------

+ (NSDictionary *) attributesForTicketColumnCellOfControlSize: (NSControlSize) controlSize 
                                                         bold: (BOOL) isBold 
                                                       italic: (BOOL) isItalic
{
    float fontSize = (controlSize == NSRegularControlSize) ? 12 : 11;
    NSFont *font = isBold ? [NSFont boldSystemFontOfSize: fontSize] : [NSFont systemFontOfSize: fontSize];

    NSMutableParagraphStyle *alignment = [[[NSMutableParagraphStyle alloc] init] autorelease];
    [alignment setParagraphStyle: [NSParagraphStyle defaultParagraphStyle]];
    [alignment setAlignment: NSLeftTextAlignment];
    
    NSNumber *obliqueness = [NSNumber numberWithFloat: isItalic ? kItalicObliqueness : 0.0];
    
    return [NSDictionary dictionaryWithObjectsAndKeys: 
        font, NSFontAttributeName, 
        alignment, NSParagraphStyleAttributeName,
        obliqueness, NSObliquenessAttributeName, NULL];
}

// ---------------------------------------------------------------------------

+ (NSDictionary *) attributesForLifetimeColumnCellOfControlSize: (NSControlSize) controlSize
                                                           bold: (BOOL) isBold 
                                                          state: (int) state
                                                  timeRemaining: (cc_time_t) timeRemaining
{
    NSColor *color = ((state == CredentialValid) && 
                      (timeRemaining > kFiveMinutes)) ? [NSColor blackColor] : [NSColor redColor];
    
    NSNumber *obliqueness = [NSNumber numberWithFloat: (state == CredentialValid) ? 0.0 : kItalicObliqueness];

    float fontSize = (controlSize == NSRegularControlSize) ? 12 : 11;
    NSFont *font = isBold ? [NSFont boldSystemFontOfSize: fontSize] : [NSFont systemFontOfSize: fontSize];
    
    NSMutableParagraphStyle *alignment = [[[NSMutableParagraphStyle alloc] init] autorelease];
    [alignment setParagraphStyle: [NSParagraphStyle defaultParagraphStyle]];
    [alignment setAlignment: NSRightTextAlignment];
    
    return [NSDictionary dictionaryWithObjectsAndKeys: 
        color, NSForegroundColorAttributeName,
        obliqueness, NSObliquenessAttributeName,
        font, NSFontAttributeName, 
        alignment, NSParagraphStyleAttributeName, NULL];
}

// ---------------------------------------------------------------------------

+ (void) synchronizeCacheMenu: (NSMenu *) menu
                     fontSize: (float) fontSize
       staticPrefixItemsCount: (int) staticPrefixItemsCount
                   headerItem: (BOOL) headerItem
            checkDefaultCache: (BOOL) checkDefaultCache
            defaultCacheIndex: (int *) defaultCacheIndex
                     selector: (SEL) selector
                       sender: (id) sender
{
    int cacheCount = [[CacheCollection sharedCacheCollection] numberOfCaches];
    int headerItemCount = (cacheCount <= 0 || headerItem) ? 1 : 0;
    int dynamicItemCount = (cacheCount > 0) ? cacheCount : 0;
    int totalItemCount = staticPrefixItemsCount + headerItemCount + dynamicItemCount;
    int headerItemIndex = staticPrefixItemsCount;
    int firstDynamicItemIndex = headerItemIndex + headerItemCount;
       
    // Remove excess dynamic items
    while ([menu numberOfItems] > totalItemCount) {
        [menu removeItemAtIndex: headerItemIndex]; 
    }

    // Add back the correct number of items
    while ([menu numberOfItems] < totalItemCount) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle: @"" 
                                                      action: selector
                                               keyEquivalent: @""];
        [item setTarget: sender];
        [menu insertItem: item atIndex: headerItemIndex]; 
        [item release];  // menu will retain
    }

    if (headerItemCount > 0) {
        NSString *key = (cacheCount > 0) ? @"KAppStringAvailableTickets" : @"KAppStringNoTicketsAvailable" ;
        NSDictionary *attributes = [Utilities attributesForMenuItemOfFontSize: fontSize italic: NO];
        NSAttributedString *title = [[[NSAttributedString alloc] initWithString: NSLocalizedString (key, NULL)
                                                                     attributes: attributes] autorelease];
        
        NSMenuItem *item = [menu itemAtIndex: headerItemIndex];
        [item setAttributedTitle: title];
        [item setEnabled: NO];
        [item setState: NSOffState];
    }

    if (cacheCount > 0) {
        int i;

        for (i = firstDynamicItemIndex; i < (firstDynamicItemIndex + dynamicItemCount); i++) {
            NSMenuItem *item = [menu itemAtIndex: i];            
            Cache *cache = [[CacheCollection sharedCacheCollection] cacheAtIndex: (i - firstDynamicItemIndex)];
            if (cache != NULL) {
                BOOL isDefaultCache = [cache isDefault];
                int itemState = (checkDefaultCache && isDefaultCache) ? NSOnState : NSOffState;
                NSAttributedString *title = [cache stringValueForMenuWithFontSize: fontSize];

                if (isDefaultCache && (defaultCacheIndex != NULL)) {
                    *defaultCacheIndex = i;  // remember for caller
                }
                    
                [item setAttributedTitle: title];
                [item setState: itemState];
                [item setEnabled: YES];
            } else {
                [item setTitle: @"error"];  // This should never happen
                [item setEnabled: NO];
            }
        }
    }
}

// ---------------------------------------------------------------------------

+ (NSString *) stringForErrorCode: (KLStatus) error
{
    NSString *string = NULL;
    char *errorText;
    
    if (KLGetErrorString (error, &errorText) == klNoErr) { 
        string = [NSString stringWithUTF8String: errorText]; 
        KLDisposeString (errorText);
    } else {
        string = [NSString stringWithFormat: NSLocalizedString (@"KAppStringUnknownErrorFormat", NULL), error];
    }
    
    return string;
}

// ---------------------------------------------------------------------------

+ (void) displayAlertForError: (KLStatus) error 
                       action: (int) action 
                       sender: (id) sender
{
    NSWindow *parentWindow = NULL;
    if ([sender isMemberOfClass: [NSWindowController class]]) { parentWindow = [sender window]; }
    if ([sender isMemberOfClass: [NSWindow class]])           { parentWindow = sender; }
    
    NSString *key = @"KAppStringGenericError";
    
    switch (action) {
        case kGetTicketsAction:
            key = @"KAppStringGetTicketsError";
            break;
        case kRenewTicketsAction:
            key = @"KAppStringRenewTicketsError";
            break;
        case kDestroyTicketsAction:
            key = @"KAppStringDestroyTicketsError";
            break;
        case kChangePasswordAction:
            key = @"KAppStringChangePasswordError";
            break;
        case kChangeActiveUserAction:
            key = @"KAppStringChangeActiveUserError";
            break;
    }
        
    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    [alert addButtonWithTitle: NSLocalizedString (@"KAppStringOK", NULL)];
    [alert setMessageText: NSLocalizedString (key, NULL)];
    [alert setInformativeText: [Utilities stringForErrorCode: error]];
    [alert setAlertStyle: NSWarningAlertStyle];
    
    
    if (parentWindow != NULL) {
        [alert beginSheetModalForWindow: parentWindow 
                          modalDelegate: sender 
                         didEndSelector: @selector(errorSheetDidEnd:returnCode:contextInfo:) 
                            contextInfo: NULL];
    } else {
        [alert runModal];
    }
}

@end
