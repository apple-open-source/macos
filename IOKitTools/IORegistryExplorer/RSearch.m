/* RSearch.m created by epeyton on Fri 14-Jan-2000 */

#import "RSearch.h"

#import "RExplorer.h"

@implementation RSearch

- (void)awakeFromNib
{
    [resultsTable setDelegate:self];
    [resultsTable setDataSource:self];
    [searchWindow setDelegate:self];

    [searchWindow setFrameAutosaveName:@"SearchWindow"];
    [searchWindow setFrameUsingName:@"SearchWindow"];

    [[[resultsTable tableColumns] objectAtIndex:0] setMaxWidth:10000];

    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(applicationWillBecomeActiveNotification:)
        name: NSApplicationWillBecomeActiveNotification
        object: NSApp];


    return;
}

- (void)displaySearchWindow:(id)sender
{
    [self importFindText];
    [searchWindow makeKeyAndOrderFront:self];
    [searchWindow display];
    return;
}

- (void)search:(id)sender
{
    
    if (![searchKeysCheckBox intValue] && ![searchValuesCheckBox intValue]) {
        return;
    }

    if (resultsArray) {
        [resultsArray release];
    }

    resultsArray = [[explorer searchResultsForText:[searchTextField stringValue] searchKeys:YES searchValues:NO] copy];

    if (![resultsArray count]) {
        NSBeep();
    }
    [resultsTable reloadData];
    [self exportFindText];

    return;
}

- (void)goTo:(id)sender
{
    NSString *path = [[resultsArray objectAtIndex:[resultsTable selectedRow]] substringFromIndex:5];
    [explorer goToPath:path];
    return;
}

- (void)goToNextResult:(id)sender
{
    int selectedRow = [resultsTable selectedRow];
    if ((selectedRow + 1 ) < [resultsArray count]) {
        [resultsTable selectRow:(selectedRow + 1) byExtendingSelection:NO];
        [self goTo:self];
    } else if ((selectedRow + 1) == [resultsArray count]) {
        [resultsTable selectRow:0 byExtendingSelection:NO];
        [self goTo:self];
    }
    
    return;
}

- (void)goToPreviousResult:(id)sender
{
    int selectedRow = [resultsTable selectedRow];
    if (selectedRow > 0) {
        [resultsTable selectRow:(selectedRow - 1) byExtendingSelection:NO];
        [self goTo:self];
    } else if (selectedRow == 0) {
        [resultsTable selectRow:([resultsArray count] - 1) byExtendingSelection:NO];
        [self goTo:self];
    }

    return;
}

// table delegation
- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
    return [resultsArray count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(int)row
{
    // need to remove the /Root from the displayed values
    return [[resultsArray objectAtIndex:row] substringFromIndex:5];
}


/*
** Import text from the find pasteboard to cell #0 of the specified Form.
** Returns YES if text was imported, NO if not.  Insures that imported
** text is null-terminated.
*/

- (BOOL) importFindText
{
    NSPasteboard 	  *findPB;
    NSArray		  *types;			// array of types strings
    NSEnumerator	  *typesEnumerator;		// an enumerator on that array
    NSString		  *typesString;			//
    NSString		  *data;
    BOOL	  	   didImport = NO;

    if (!(findPB = [NSPasteboard pasteboardWithName:NSFindPboard])) {
        return NO;
    }

        types = [findPB types];
        typesEnumerator = [types objectEnumerator];

    while (typesString = [typesEnumerator nextObject]) {
        if (![typesString isEqualToString:NSStringPboardType]) {
            continue;
        }

        if (data = [findPB stringForType:NSStringPboardType]) {
            if (data && [data length]) {
                [searchTextField setStringValue:data];
                didImport = YES;
            }
            return didImport;
        }
    }
    return NO;
}

- (BOOL) exportFindText
{
    NSPasteboard	*findPB;
    NSString	*tex;

    if (!(tex = [searchTextField stringValue]) || [tex length] == 0) {
                return NO;
    }
    if (findPB = [NSPasteboard pasteboardWithName:NSFindPboard]) {
        [findPB declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:NULL];
        if ([findPB setString:tex forType:NSStringPboardType])
        {
            return YES;
        }
    }
    return NO;
}

- (void) applicationWillBecomeActiveNotification: (NSNotification *)aNotification
/*
    Update the find panel with the pasteboard contents when the app is activated.
*/
{

    [self importFindText];

}




@end
