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

/*

- (void)mouseMoved:(NSEvent *)theEvent
{
    // get the point and find the cell,
    NSPoint pt = [theEvent locationInWindow];
    int column = [self columnAtPoint:pt];
    int row = [self rowAtPoint:pt];

    NSTableColumn *tableColumn;
    id cell;

    //NSLog(@"column = %d, row = %d, x = %f, y = %f", column, row, pt.x, pt.y);

    return;

    if (column > -1 && row > -1)
        tableColumn = [[self tableColumns] objectAtIndex:column];
    
    // set the contents of the tooltip from the cell

    cell = [tableColumn dataCellForRow:row];

    NSLog(cell);

    [self setToolTip:[cell stringValue]];

    return;
}

- (void)mouseEntered:(NSEvent *)theEvent
{
    NSLog(@"Entered");
    [[self window] setAcceptsMouseMovedEvents:YES];
}

- (void)mouseExited:(NSEvent *)theEvent
{
    NSLog(@"Exited");
    [[self window] setAcceptsMouseMovedEvents:NO];

} */

@end
