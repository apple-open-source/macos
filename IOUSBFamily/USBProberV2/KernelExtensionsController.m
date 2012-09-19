/*
 * Copyright © 1998-2010 Apple Inc.  All rights reserved.
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


#import "KernelExtensionsController.h"

@implementation KernelExtensionsController

static NSInteger sortKextArray(NSDictionary * dict1, NSDictionary * dict2, void *context) {
    int order = [(NSNumber *)[(NSArray *)context objectAtIndex:0] intValue];
    int col = [(NSNumber *)[(NSArray *)context objectAtIndex:1] intValue];
    
    if (order == 0) {
        switch (col) {
            case 0:
                return [(NSString *)[dict1 objectForKey:@"Name"] caseInsensitiveCompare:[dict2 objectForKey:@"Name"]];
                break;
            case 1:
                return [(NSString *)[dict1 objectForKey:@"Version"] caseInsensitiveCompare:[dict2 objectForKey:@"Version"]];
                break;
            case 2:
                return [[NSNumber numberWithInt:[(NSString *)[dict1 objectForKey:@"Size"] intValue]] compare:[NSNumber numberWithInt:[[dict2 objectForKey:@"Size"] intValue]]];
                break;
            case 3:
                return [[NSNumber numberWithInt:[(NSString *)[dict1 objectForKey:@"Wired"] intValue]] compare:[NSNumber numberWithInt:[[dict2 objectForKey:@"Wired"] intValue]]];
                break;
            case 4:
                return [(NSString *)[dict1 objectForKey:@"Address"] caseInsensitiveCompare:[dict2 objectForKey:@"Address"]];
                break;
        }
    } else {
        switch (col) {
            case 0:
                return [(NSString *)[dict2 objectForKey:@"Name"] caseInsensitiveCompare:[dict1 objectForKey:@"Name"]];
                break;
            case 1:
                return [(NSString *)[dict2 objectForKey:@"Version"] caseInsensitiveCompare:[dict1 objectForKey:@"Version"]];
                break;
            case 2:
                return [[NSNumber numberWithInt:[[dict2 objectForKey:@"Size"] intValue]] compare:[NSNumber numberWithInt:[(NSString *)[dict1 objectForKey:@"Size"] intValue]]];
                break;
            case 3:
                return [[NSNumber numberWithInt:[[dict2 objectForKey:@"Wired"] intValue]] compare:[NSNumber numberWithInt:[(NSString *)[dict1 objectForKey:@"Wired"] intValue]]];
                break;
            case 4:
                return [(NSString *)[dict2 objectForKey:@"Address"] caseInsensitiveCompare:[dict1 objectForKey:@"Address"]];
                break;
        }
    }

    return NSOrderedSame;
}

- init {
    if (self = [super init]) {
        _loadedExtensions = [[NSMutableArray array] retain];
    }
    return self;
}

- (void)dealloc {
    [_loadedExtensions release];
	[super dealloc];
}

- (void)awakeFromNib {
	[KextOutputTable setFont:[NSFont fontWithName:@"Monaco" size:10]];
	[KextOutputTable setAllowsColumnResizing:YES];
    [self Refresh:self];
}

- (IBAction)Refresh:(id)sender {
    if ([[KextTypePopUpButton selectedItem] tag] == 0) {
		[ _loadedExtensions setArray:[KextInfoGatherer loadedExtensionsContainingString:@"USB"]];
        if (_loadedExtensions == nil) {
            // an error occured when trying to load the kext list
            _loadedExtensions = [[NSMutableArray array] retain];
        }
    }
    else if ([[KextTypePopUpButton selectedItem] tag] == 1) {
        [ _loadedExtensions  setArray:[KextInfoGatherer loadedExtensions]];
        if (_loadedExtensions == nil) {
            // an error occured when trying to load the kext list
            _loadedExtensions = [[NSMutableArray array] retain];
        }
    }
    
    if (previousSortedColumn != nil) {
        [KextOutputTable setIndicatorImage:nil inTableColumn:previousSortedColumn];
        [previousSortedColumn release];
        previousSortedColumn = nil;
    }
    
    [KextOutputTable reloadData];
}

- (IBAction)SaveOutput:(id)sender {
    NSSavePanel *sp = [NSSavePanel savePanel];
    [sp setAllowedFileTypes:[NSArray arrayWithObjects:@"txt", @"plist", nil]];
    [sp setDirectoryURL:[NSURL URLWithString:NSHomeDirectory()]];
    [sp setNameFieldStringValue:@"Kernel Extensions"];
    [sp setExtensionHidden:NO];
    
    NSRect exFrame;
    exFrame.origin.x=0;
    exFrame.origin.y=0;
    exFrame.size.width=200;
    exFrame.size.height=100;
    ExtensionSelector *newExSel = [[ExtensionSelector alloc] initWithFrame:exFrame];
    NSMutableDictionary *allowedTypesDictionary = [NSMutableDictionary dictionary];
    [allowedTypesDictionary setValue:@"txt" forKey:@"Text"];
    [allowedTypesDictionary setValue:@"plist" forKey:@"XML"];
    [newExSel populatePopuButtonWithArray:allowedTypesDictionary];
    [newExSel setCurrentSelection:@"Text"];
    [sp setAccessoryView:newExSel];
    newExSel.theSavePanel = sp;
    
    [sp beginSheetModalForWindow:[NSApp mainWindow] completionHandler:^(NSInteger returnCode)
	{
        if (returnCode==NSOKButton)
        {
            NSString *selectedFileExtension = [[sp nameFieldStringValue] pathExtension];
            if (NSOrderedSame != [selectedFileExtension caseInsensitiveCompare:@"plist"])
            {
				NSString *finalString = [(TableViewWithCopying *)KextOutputTable stringRepresentation];
				
                if (![finalString writeToURL:[sp URL] atomically:YES encoding:NSUTF8StringEncoding error:NULL])
                {
                    NSBeep();
                }
            }
            else
            {
                
                if (![_loadedExtensions writeToURL:[sp URL] atomically:YES])
                {
					NSBeep();
				}
			}
        }
    }
	];
	
	[newExSel release];
}


- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView {
    return [_loadedExtensions count];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex {
    return [[_loadedExtensions objectAtIndex:rowIndex] objectForKey:[aTableColumn identifier]];
}

- (void)tableView:(NSTableView *)aTableView didClickTableColumn:(NSTableColumn *)aTableColumn {
    NSString *colName = [aTableColumn identifier];
    NSImage *currentIndicatorImage = [aTableView indicatorImageInTableColumn:aTableColumn];
    NSNumber *contextColNumber;
    NSNumber *contextSortOrder;
    

    if (previousSortedColumn != nil) {
        [aTableView setIndicatorImage:nil inTableColumn:previousSortedColumn];
        [previousSortedColumn release];
        previousSortedColumn = nil;
    }
    previousSortedColumn = [aTableColumn retain];
    
    if (currentIndicatorImage == nil || currentIndicatorImage == [NSImage imageNamed:@"NSDescendingSortIndicator"]) {
        contextSortOrder = [NSNumber numberWithInt:0];
        [aTableView setIndicatorImage:[NSImage imageNamed:@"NSAscendingSortIndicator"] inTableColumn:aTableColumn];
    } else {
        contextSortOrder = [NSNumber numberWithInt:1];
        [aTableView setIndicatorImage:[NSImage imageNamed:@"NSDescendingSortIndicator"] inTableColumn:aTableColumn];
    }

    if ([colName isEqualToString:@"Name"]) {
        contextColNumber = [NSNumber numberWithInt:0];
    } else if ([colName isEqualToString:@"Version"]) {
        contextColNumber = [NSNumber numberWithInt:1];
    } else if ([colName isEqualToString:@"Size"]) {
        contextColNumber = [NSNumber numberWithInt:2];
    } else if ([colName isEqualToString:@"Wired"]) {
        contextColNumber = [NSNumber numberWithInt:3];
    } else if ([colName isEqualToString:@"Address"]) {
        contextColNumber = [NSNumber numberWithInt:4];
    } else {
        contextColNumber = [NSNumber numberWithInt:0];
    }
    
    [_loadedExtensions sortUsingFunction:sortKextArray context:[NSArray arrayWithObjects:contextSortOrder,contextColNumber,nil]];
    
    [aTableView reloadData];
}

@end
