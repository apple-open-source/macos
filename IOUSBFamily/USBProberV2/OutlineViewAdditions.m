/*
 * Copyright © 1998-2012 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import "OutlineViewAdditions.h"


@implementation NSOutlineView(OutlineViewAdditions)

- (void)itemDoubleClicked {
    int selectedRow = [self selectedRow];
    
    if (![self isExpandable:[self itemAtRow:selectedRow]]) {
        return;
    } else if ([[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask) {
        if ([self isItemExpanded:[self itemAtRow:selectedRow]])
            [self collapseItem:[self itemAtRow:selectedRow]];
        [self expandItem:[self itemAtRow:selectedRow] expandChildren:YES];
    } else {
        if ([self isItemExpanded:[self itemAtRow:selectedRow]])
            [self collapseItem:[self itemAtRow:selectedRow]];
        else
            [self expandItem:[self itemAtRow:selectedRow]];
    }
}

- (void)copy:(id)sender {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSMutableString *pasteboardString = [[NSMutableString alloc] init];
	NSIndexSet	*indexSet;
    id  anItem;
    
	indexSet = [self selectedRowIndexes];
	NSUInteger current = [indexSet firstIndex];
	
    [pasteboard declareTypes: [NSArray arrayWithObject: NSStringPboardType] owner: NULL];
	while (current != NSNotFound) {
		int i;
        int levelForRow = [self levelForRow:current];
        for (i=0;i<levelForRow;i++)
            [pasteboardString appendString:@"    "];
        
        anItem = [self itemAtRow:current];
        if ([anItem class] == [BusProbeDevice class]) {
            anItem = [anItem rootNode];
        }
        [pasteboardString appendString:[NSString stringWithFormat:@"%@   %@\n",[anItem name],[anItem value]]];

         current = [indexSet indexGreaterThanIndex: current];
    }
    
    [pasteboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:self];
    
    [pasteboard setString:pasteboardString forType:@"NSStringPboardType"];
    
    [pasteboardString release];
}

@end
