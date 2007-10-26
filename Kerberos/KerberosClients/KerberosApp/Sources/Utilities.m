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
#import "KerberosCacheCollection.h"
#import "KerberosCache.h"

#define kItalicObliqueness 0.3

@implementation Utilities

// ---------------------------------------------------------------------------

+ (NSAttributedString *) attributedStringForControlType: (UtilitiesControlType) type
                                                 string: (NSString *) string
                                              alignment: (UtilitiesStringAlignment) alignment
                                                   bold: (BOOL) isBold
                                                 italic: (BOOL) isItalic
                                                    red: (BOOL) isRed
{
    NSMutableDictionary *attributes = [NSMutableDictionary dictionary];

    if (alignment != kUtilitiesNoStringAlignment) {
        NSMutableParagraphStyle *style = [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
        [style setAlignment: (alignment == kUtilitiesLeftStringAlignment ?
                              NSLeftTextAlignment : NSRightTextAlignment)];
        [style setLineBreakMode: (alignment == kUtilitiesLeftStringAlignment ?
                                  NSLineBreakByTruncatingTail : NSLineBreakByTruncatingHead)];
        [attributes setObject: style forKey: NSParagraphStyleAttributeName];
        [style release];
    }
    
    NSFont *font = NULL;
    if (type == kUtilitiesTableCellControlType) {
        font = [NSFont systemFontOfSize: 12];
    } else if (type == kUtilitiesSmallTableCellControlType) {
        font = [NSFont systemFontOfSize: 11];
    } else if (type == kUtilitiesMenuItemControlType) {
        font = [NSFont menuFontOfSize: 0];
    } else if (type == kUtilitiesPopupMenuItemControlType) {
        font = [NSFont menuFontOfSize: 11];
    }
    if (font) {
        if (isBold) {
            font = [[NSFontManager sharedFontManager] convertFont: font
                                                      toHaveTrait: NSBoldFontMask];
        }
        [attributes setObject: font forKey: NSFontAttributeName];
    }
    
    
    if (isItalic) {
        // Most of the system fonts don't have an italic version
        [attributes setObject: [NSNumber numberWithFloat: kItalicObliqueness]
                       forKey: NSObliquenessAttributeName];
    }
        
    if (isRed) {
        [attributes setObject: [NSColor redColor]
                       forKey: NSForegroundColorAttributeName];
    }
    
    return [[[NSAttributedString alloc] initWithString: string
                                           attributes: attributes] autorelease];
}

// ---------------------------------------------------------------------------

+ (void) synchronizeCacheMenu: (NSMenu *) menu
                        popup: (BOOL) isPopupMenu
       staticPrefixItemsCount: (int) staticPrefixItemsCount
                   headerItem: (BOOL) headerItem
            checkDefaultCache: (BOOL) checkDefaultCache
            defaultCacheIndex: (int *) defaultCacheIndex
                     selector: (SEL) selector
                       sender: (id) sender
{
    int cacheCount = [[KerberosCacheCollection sharedCacheCollection] numberOfCaches];
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
        NSMenuItem *item = [menu itemAtIndex: headerItemIndex];
        [item setAttributedTitle: [Utilities attributedStringForControlType: (isPopupMenu ?
                                                                              kUtilitiesPopupMenuItemControlType :
                                                                              kUtilitiesMenuItemControlType)
                                                                     string: NSLocalizedString (key, NULL)
                                                                  alignment: kUtilitiesNoStringAlignment
                                                                       bold: NO
                                                                     italic: NO
                                                                        red: NO]];
        [item setEnabled: NO];
        [item setState: NSOffState];
    }

    if (cacheCount > 0) {
        int i;

        for (i = firstDynamicItemIndex; i < (firstDynamicItemIndex + dynamicItemCount); i++) {
            NSMenuItem *item = [menu itemAtIndex: i];            
            KerberosCache *cache = [[KerberosCacheCollection sharedCacheCollection] cacheAtIndex: (i - firstDynamicItemIndex)];
            if (cache) {
                BOOL isDefaultCache = [cache isDefault];
                int itemState = (checkDefaultCache && isDefaultCache) ? NSOnState : NSOffState;
                if (isDefaultCache && (defaultCacheIndex != NULL)) {
                    *defaultCacheIndex = i;  // remember for caller
                }
                    
                [item setAttributedTitle: [Utilities attributedStringForControlType: (isPopupMenu ?
                                                                                      kUtilitiesPopupMenuItemControlType :
                                                                                      kUtilitiesMenuItemControlType)
                                                                             string: [cache principalString]
                                                                          alignment: kUtilitiesNoStringAlignment
                                                                               bold: NO
                                                                             italic: ([cache state] != CredentialValid)
                                                                                red: NO]];
                [item setState: itemState];
                [item setEnabled: YES];
            } else {
                [item setTitle: @"error"];  // This should never happen
                [item setEnabled: NO];
            }
        }
    }
}

@end
