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

#import "USBLoggerFilterWindowController.h"

double timeStampFromLogLine(NSString * line);

@implementation USBLoggerFilterWindowController

- init {
    if (self = [super init]) {
        _currentFilterString = nil;
        _outputLogLines = [[NSMutableArray alloc] init];
    }
    return self;
}

- (void)dealloc {
    [_currentFilterString release];
    [_outputLogLines release];
    [super dealloc];
}

- (void)awakeFromNib {
    [FilterProgressIndicator setUsesThreadedAnimation:YES];
    [FilterOutput setTarget:self];
    [FilterOutput setDoubleAction:@selector(itemDoubleClicked:)];

    [self setupRecentSearchesMenu];
}

- (void)setupRecentSearchesMenu {
    // we can only do this if we're running on 10.3 or later (where FilterTextField is an NSSearchField instance)
    if ([FilterTextField respondsToSelector: @selector(setRecentSearches:)]) {
        NSMenu *cellMenu = [[NSMenu alloc] initWithTitle:@"Search Menu"];
        NSMenuItem *recentsTitleItem, *norecentsTitleItem, *recentsItem, *separatorItem, *clearItem;
        id searchCell = [FilterTextField cell];

        [FilterTextField setRecentsAutosaveName:@"logger_output_search"];
        [searchCell setMaximumRecents:10];

        recentsTitleItem = [[NSMenuItem alloc] initWithTitle:@"Recent Searches" action: nil keyEquivalent:@""];
        [recentsTitleItem setTag:NSSearchFieldRecentsTitleMenuItemTag];
        [cellMenu insertItem:recentsTitleItem atIndex:0];
        [recentsTitleItem release];
        norecentsTitleItem = [[NSMenuItem alloc] initWithTitle:@"No recent searches" action: nil keyEquivalent:@""];
        [norecentsTitleItem setTag:NSSearchFieldNoRecentsMenuItemTag];
        [cellMenu insertItem:norecentsTitleItem atIndex:1];
        [norecentsTitleItem release];
        recentsItem = [[NSMenuItem alloc] initWithTitle:@"Recents" action: nil keyEquivalent:@""];
        [recentsItem setTag:NSSearchFieldRecentsMenuItemTag];
        [cellMenu insertItem:recentsItem atIndex:2];
        [recentsItem release];
        separatorItem = (NSMenuItem *)[NSMenuItem separatorItem];
        [separatorItem setTag:NSSearchFieldRecentsTitleMenuItemTag];
        [cellMenu insertItem:separatorItem atIndex:3];
        clearItem = [[NSMenuItem alloc] initWithTitle:@"Clear" action: nil keyEquivalent:@""];
        [clearItem setTag:NSSearchFieldClearRecentsMenuItemTag];
        [cellMenu insertItem:clearItem atIndex:4];
        [clearItem release];
        [searchCell setSearchMenuTemplate:cellMenu];
        [cellMenu release];
    }
}

- (IBAction)FilterOutput:(id)sender
{
    NSScroller *scroller = [[FilterOutput enclosingScrollView] verticalScroller];
    BOOL isScrolledToEnd = (![scroller isEnabled] || [scroller floatValue] == 1);
    NSEnumerator *entryEnumerator = [[LoggerController logEntries] objectEnumerator];
    LoggerEntry *thisEntry;
    
    [_currentFilterString release];
    if (![[FilterTextField stringValue] isEqualToString:@""]) {
        _currentFilterString = [[FilterTextField stringValue] retain];
    } else {
        _currentFilterString = nil;
    }

    [_outputLogLines removeAllObjects];
    
    [FilterProgressIndicator startAnimation:self];
    while (thisEntry = [entryEnumerator nextObject]) {
        if ([thisEntry level] <= 0)
            continue;
        if (_currentFilterString != nil && [[thisEntry text] rangeOfString:_currentFilterString options:NSCaseInsensitiveSearch].location != NSNotFound) {
            [_outputLogLines addObject:thisEntry];
        }
    }
    [FilterProgressIndicator stopAnimation:self];
    
    [FilterOutput reloadData];
    
    if (isScrolledToEnd) {
        [FilterOutput scrollRowToVisible:[_outputLogLines count]-1];
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView {
    return [_outputLogLines count];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex {
    return [[_outputLogLines objectAtIndex:rowIndex] text];
}

- (void)itemDoubleClicked:(NSTableView *)sender {
    NSString *lineText = [[sender dataSource] tableView:sender objectValueForTableColumn:[[sender tableColumns] objectAtIndex:0] row:[sender selectedRow]];
    NSString *clickedLogString = [lineText stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]; // strip the trailing newlines and spaces
    double clickedTimeStamp = timeStampFromLogLine( clickedLogString );

    if (clickedTimeStamp == -1) {
        return;
    }
    
    NSArray *displayedLines = [LoggerController displayedLogLines];
    int numberOfLines = [displayedLines count];
    int first = 0, middle = 0, last = numberOfLines-1;
    double midTimeStamp = 0;
    
    while (first <= last) {
        middle = (first + last) / 2;
        midTimeStamp = timeStampFromLogLine([displayedLines objectAtIndex:middle]);
        if (midTimeStamp == -1) {
            middle++;
            continue;
        }
        if (clickedTimeStamp > midTimeStamp) 
            first = middle + 1;
        else if (clickedTimeStamp < midTimeStamp) 
            last = middle - 1;
        else
            break;
    }

    // [displayedLines objectAtIndex:middle] has a time stamp identical or closest to the timestamp
    // of the clicked line. Now let's try to find the exact line, if we can

    if (midTimeStamp != clickedTimeStamp) {
        // timestamps dont match, so the clicked line was likely filtered out (not visible). Just scroll to the
        // line we found ("middle"), which had the closest timestamp we cound find
        [LoggerController scrollToVisibleLine:[displayedLines objectAtIndex:middle]];
    } else {
        // walk up and down looking for the exact matching line
        BOOL found = NO;
        int lineNo;
        NSString *thisLine = NULL;
        NSCharacterSet *whitespaceAndNewlines  = [NSCharacterSet whitespaceAndNewlineCharacterSet];

        // walk down, starting with (the line we found)
        lineNo = middle;
        while (lineNo >= 0) {
            thisLine = [[displayedLines objectAtIndex:lineNo] stringByTrimmingCharactersInSet:whitespaceAndNewlines];
            if ( timeStampFromLogLine(thisLine) != midTimeStamp ) {
                break;
            } else if ([thisLine isEqualToString:clickedLogString]) {
                found = YES;
                break;
            }
            lineNo--;
        }

        // if still no exact match found, walk up, starting with (the line we found + 1)
        if (!found) {
            lineNo = middle+1;
            while (lineNo <= numberOfLines-1) {
                thisLine = [[displayedLines objectAtIndex:lineNo] stringByTrimmingCharactersInSet:whitespaceAndNewlines];
                if ( timeStampFromLogLine(thisLine) != midTimeStamp ) {
                    break;
                } else if ([thisLine isEqualToString:clickedLogString]) {
                    found = YES;
                    break;
                }
                lineNo++;
            }
        }
        
        if (found) {
            [LoggerController scrollToVisibleLine:thisLine];
        }
        else {
            // we did not find the exact line, so just scroll to the line we found originally
            // (which has the same time stamp as what we're looking for)
            [LoggerController scrollToVisibleLine:[displayedLines objectAtIndex:middle]];
        }
    }
}

double timeStampFromLogLine(NSString * line) {
    double timeStamp = -1;
    int level = -1;

#if 1
    struct tm timePtr;
    time_t  theTime;
    char month[4];
    int day, hour, min, sec, micro;
    
    sscanf((char *)[line cStringUsingEncoding:NSUTF8StringEncoding],"%s %d %d:%d:%d.%d  [%d]", month, &day, &hour, &min, &sec, &micro, &level);
    
    timePtr.tm_sec = sec;
    timePtr.tm_min = min;
    timePtr.tm_hour = hour;
    timePtr.tm_mday = day;
    timePtr.tm_mon = 8;         // Hard coding the month and the years since 1900, as it really does not matter in this case
    timePtr.tm_year = 111;
    
    theTime = mktime(&timePtr);
    
    timeStamp = theTime + micro/1000.0;
#else
    sscanf((char *)[line cStringUsingEncoding:NSUTF8StringEncoding],"\t%f [%d]", &timeStamp, &level);
#endif
    
    return timeStamp;
}

@end
