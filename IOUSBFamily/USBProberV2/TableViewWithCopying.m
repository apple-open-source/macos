/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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


#import "TableViewWithCopying.h"


@implementation TableViewWithCopying

- (IBAction)copy:(id)sender {
    NSMutableString *pasteboardString = [[NSMutableString alloc] init];
	NSIndexSet	*indexSet;
    BOOL doneOneRow = NO;
    
	indexSet = [self selectedRowIndexes];
	NSUInteger current = [indexSet firstIndex];

	while (current != NSNotFound) {
        NSMutableString *thisRow = [[NSMutableString alloc] init];
        NSEnumerator *columnEnumerator = [[self tableColumns] objectEnumerator];
        id column;
        BOOL doneOneCol = NO;
        
        if (doneOneRow) {
            [thisRow appendString:@"\n"];
        }
        
        while (column = [columnEnumerator nextObject]) {
            if (doneOneCol) {
                [thisRow appendString:@"      "];
            }
            [thisRow appendFormat:@"%@",[[self dataSource] tableView:self objectValueForTableColumn:column row:current]];
            doneOneCol= YES;
        }
        
        [pasteboardString appendString:thisRow];
        doneOneRow= YES;
         
		 current = [indexSet indexGreaterThanIndex: current];
    }

    [[NSPasteboard generalPasteboard] declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:self];
        
    [[NSPasteboard generalPasteboard] setString:pasteboardString forType:@"NSStringPboardType"];
    
    [pasteboardString release];
}

- (NSString *)stringRepresentation {
    NSMutableString *finalString = [[NSMutableString alloc] init];
    int row;
    BOOL doneOneRow = NO;
    int numberOfRows = [self numberOfRows];
    
    for (row = 0; row < numberOfRows; row++) {
        NSMutableString *thisRow = [[NSMutableString alloc] init];
        NSEnumerator *columnEnumerator = [[self tableColumns] objectEnumerator];
        id column;
        BOOL doneOneCol = NO;
        
        if (doneOneRow) {
            [thisRow appendString:@"\n"];
        }
        
        while (column = [columnEnumerator nextObject]) {
            if (doneOneCol) {
                [thisRow appendString:@"      "];
            }
            [thisRow appendFormat:@"%@",[[self dataSource] tableView:self objectValueForTableColumn:column row:row]];
            doneOneCol= YES;
        }

        [finalString appendString:thisRow];
        doneOneRow= YES;
    }
    
    return [finalString autorelease];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    if ([NSStringFromSelector([menuItem action]) isEqualToString:@"copy:"]) {
        return ([self selectedRow] != -1);
    } else {
        return YES;
    }
}

@end
