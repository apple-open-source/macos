#import <AppKit/AppKit.h>

@interface MyDocument : NSDocument {
    IBOutlet NSTextField	*totalField;
    IBOutlet NSTableView	*tableview;
    NSMutableArray			*logItems;
	NSTimer					*timer;
	Boolean					state;
}

- (void)startTimer:(id)sender;
- (void)createNewLogItem:(id)sender;
- (void)deleteSelectedLogItem:(id)sender;
- (NSMutableArray *)logItems;
- (void)setLogItems: (NSMutableArray *)value;
- (void) doTimer: (NSTimer *) myTimer;

- (int)numberOfRowsInTableView:(NSTableView *)tv;
- (id)tableView:(NSTableView *)tv objectValueForTableColumn: (NSTableColumn *)tc row: (int)row;

@end
