/* CustomOutlineView.m created by epeyton on Wed 05-Jan-2000 */

#import "CustomOutlineView.h"

@implementation CustomOutlineView

- (NSString *)view:(NSView *)view stringForToolTip:(NSToolTipTag)tag point:(NSPoint)point userData:(void *)data
{

    int column = [self columnAtPoint:point];
    int row = [self rowAtPoint:point];

    NSTableColumn *tableColumn;
    id cell;

    if (column > -1 && row > -1)
        tableColumn = [[self tableColumns] objectAtIndex:column];

    // set the contents of the tooltip from the cell

    cell = [tableColumn dataCellForRow:row];


    NSLog(@"column = %d, row = %d, x = %f, y = %f", column, row);
    return [cell stringValue];
}

@end
