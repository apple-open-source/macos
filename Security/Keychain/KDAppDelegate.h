//
//  KDAppDelegate.h
//  Keychain
//
//  Created by J Osborne on 2/13/13.
//
//

#import <Cocoa/Cocoa.h>
#import "KDSecItems.h"
#import "KDSecCircle.h"

@interface KDAppDelegate : NSObject <NSApplicationDelegate>

@property (assign) IBOutlet NSWindow *window;
@property (assign) IBOutlet NSTableView *itemTable;
@property (assign) IBOutlet NSTextFieldCell *itemTableTitle;
@property (retain) id<NSTableViewDataSource> itemDataSource;

@property (assign) IBOutlet NSButton *enableKeychainSyncing;
@property (assign) IBOutlet NSTextFieldCell *circleStatusCell;
@property (assign) IBOutlet NSTextFieldCell *peerCountCell;
@property (assign) IBOutlet NSTextView *peerTextList;
@property (assign) IBOutlet NSTextFieldCell *applicantCountCell;
@property (assign) IBOutlet NSTextView *applicantTextList;
@property (assign) IBOutlet NSProgressIndicator *syncSpinner;

@property (retain) KDSecCircle *circle;

@property (retain) NSMutableArray *stuffNotToLeak;

-(IBAction)enableKeychainSyncingClicked:(id)sender;
@end
