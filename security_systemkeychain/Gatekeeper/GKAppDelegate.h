//
//  Copyright (c) 2012 Apple. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "AuthorityTableView.h"

@interface GKAppDelegate : NSObject <NSApplicationDelegate, AuthorityTableDelegate, NSTableViewDataSource>

@property (assign) IBOutlet NSWindow *window;
@property (retain) NSMutableArray *authority;
@property (assign) IBOutlet NSTableView *tableView;
@property (assign) IBOutlet NSArrayController *arrayController;
@property (assign) AuthorizationRef authRef;
@property (retain) NSData *authData;
@property (retain) NSArray *sortDescriptors;

- (BOOL)doTargetQuery:(CFTypeRef)target withAttributes:(NSDictionary *)attributes;
- (IBAction)addApplication:(NSButtonCell *)sender;

@end
