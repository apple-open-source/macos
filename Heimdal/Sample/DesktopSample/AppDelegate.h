//
//  AppDelegate.h
//  DesktopSample
//
//  Created by Love Hörnquist Åstrand on 2011-11-13.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>

@property (assign) IBOutlet NSWindow *window;
@property (assign) IBOutlet NSTableView *tableview;
@property (retain) IBOutlet NSMutableArray *credentials;
@property (assign) IBOutlet NSArrayController *arrayController;

@end
