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

    return;
}

- (void)displaySearchWindow:(id)sender
{
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



@end
