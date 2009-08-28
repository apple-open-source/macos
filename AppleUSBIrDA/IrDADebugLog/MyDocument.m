#import "MyDocument.h"
#import "LogItem.h"
#import "GetEntry.h"

@implementation MyDocument

- (void)updateTotal {
    [totalField setObjectValue: [NSNumber numberWithInt:[[self logItems] count]]];
}

- (void)updateViews {
    [self updateTotal];
    [tableview reloadData];
}

- (void)createNewLogItem:(id)sender {
    int row;

	[GetEntry getEntry: [self logItems]];
	
	//Select and show  thelast item added!
	row = [[self logItems] count] - 1;
    [self updateViews];
    [tableview selectRow: row byExtendingSelection: NO];
    [tableview scrollRowToVisible: row];
    [self updateChangeCount: NSChangeDone];
}

- (void)deleteSelectedLogItem:(id)sender {
    int row = [tableview selectedRow];
    if (row != -1) {
        [[self logItems] removeObjectAtIndex: row];
        [tableview noteNumberOfRowsChanged];
        if (--row >= 0) {
            [tableview selectRow: row byExtendingSelection: NO];
            [tableview scrollRowToVisible: row];
        }
        [self updateTotal];
        [self updateChangeCount: NSChangeDone];
    }
}

- (int)numberOfRowsInTableView:(NSTableView *)tv {
    [self updateTotal];
    return [[self logItems] count];
}

- (id)tableView:(NSTableView *)tv
    objectValueForTableColumn: (NSTableColumn *)tc
    row: (int)row
{
    /*
    The table column identifier (NB not title, think of localisation)
    should be the name of the instance variable it will dispplay.
    We can therefore retrieve the variable using key-value coding
    rather than having to have switch statements for each value.
    */
	
	/* Compute and Format the delta in Seconds */
	if ([[tc identifier] isEqualTo: @"delta"] ){
		SInt32	delta;
		
		if (row != 0){
			UInt32	current = [[[[self logItems] objectAtIndex: row] valueForKey: @"time"]unsignedIntValue];
			UInt32	previous = [[[[self logItems] objectAtIndex: row - 1] valueForKey: @"time"]unsignedIntValue];
			
			delta	= current - previous;
		}
		else{
			delta = 0;
		}
		if (delta > 999999 || delta < 0)
			delta = 999999;
		return [NSNumber numberWithLong:delta];
	}
	/* Format the time in Seconds */
	if ([[tc identifier] isEqualTo: @"time"] ){
		char	newTime[255];
		UInt32	raw				= [[[[self logItems] objectAtIndex: row] valueForKey: [tc identifier]]unsignedIntValue];
		UInt32	secs			= raw / 1000000;
		UInt32	usecs			= raw % 1000000;
		
		sprintf( newTime, "%4ld.%06ld", secs, usecs);
		return [NSString stringWithCString:newTime];
	}
	/* Format the numbers in Hex */
	if ([[tc identifier] isEqualTo: @"second"] | [[tc identifier] isEqualTo: @"first"]){
		char hexValue[255];
		
		sprintf(hexValue, "0x%04x", [[[[self logItems] objectAtIndex: row] valueForKey: [tc identifier]]unsignedIntValue]);
		return [NSString stringWithCString:hexValue];
	}
	/* Do the default for the type */
    return [[[self logItems] objectAtIndex: row] valueForKey: [tc identifier]];
}

- (void)tableView:(NSTableView *)tv
    setObjectValue:(id)object
    forTableColumn:(NSTableColumn *)tc
    row:(int)row
{
}

- (NSString *)windowNibName {
    // Override returning the nib file name of the document
    // If you need to use a subclass of NSWindowController or if your document supports multiple NSWindowControllers, you should remove this method and override -makeWindowControllers instead.
    return @"MyDocument";
}

- (void)windowControllerDidLoadNib:(NSWindowController *) aController{
    [super windowControllerDidLoadNib:aController];
    // Add any code here that need to be executed once the windowController has loaded the document's window.
}

- (NSData *)dataRepresentationOfType:(NSString *)aType {
    // We need an archived representation of the model.
    // The model is an array of logItems.  An array knows how to archive
    // itself; we should have written encoding method for LogItem.
    return [NSArchiver archivedDataWithRootObject: [self logItems]];
}

- (BOOL)loadDataRepresentation:(NSData *)data ofType:(NSString *)aType {
    // We should be passed an archived representation of the
    // array of logItems.
    [self setLogItems: [NSUnarchiver unarchiveObjectWithData: data]];
    return YES;
}

- (void)dealloc {
    [self setLogItems:nil];
    [super dealloc];
}

// accessor methods
- (NSArray *)logItems {
    if (logItems == nil) {
        [self setLogItems: [NSMutableArray array]];
    }
    return logItems;
}
- (void)setLogItems:(NSMutableArray *)value {
    [value retain];
    [logItems release];
    logItems = value;
}

- (void) doTimer: (NSTimer *) myTimer{
//		NSLog(@"doTimer");
		[self createNewLogItem: myTimer];
}

- (void)startTimer:(id)sender{
	if (state){
		NSLog(@"Stop timer now!");
		[timer invalidate];
		// Display and keep track of state in button
		[sender setTitle: @"Capture"];
		state = false;
	}
	else{
		NSLog(@"Start timer now!");
		timer = [NSTimer scheduledTimerWithTimeInterval: .5 target: self selector: @selector(doTimer:) userInfo: nil repeats: YES];

		// Display and keep track of state in button
		[sender setTitle: @"Stop"];
		state = true;
	}
}

@end
