//
//  XServer.h
//
//  Created by Andreas Monitzer on January 6, 2001.
//
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz_1.3/XServer.h,v 1.2 2002/06/19 18:12:02 torrey Exp $ */

#import <Cocoa/Cocoa.h>

#include <drivers/event_status_driver.h>	// for NXEvent

@interface XServer : NSObject {
    // server state
    NSRecursiveLock *serverLock;
    NSPort *signalPort;
    BOOL serverVisible;
    BOOL rootlessMenuBarVisible;
    BOOL appQuitting;
    UInt32 mouseState;
    Class windowClass;

    // server event queue
    BOOL sendServerEvents;
    int eventWriteFD;

    // Aqua interface
    IBOutlet NSWindow *modeWindow;
    IBOutlet NSButton *startupModeButton;
    IBOutlet NSButton *startFullScreenButton;
    IBOutlet NSButton *startRootlessButton;
    IBOutlet NSWindow *helpWindow;
    IBOutlet NSButton *startupHelpButton;
    IBOutlet NSPanel *switchWindow;
}

- (id)init;

- (BOOL)translateEvent:(NSEvent *)anEvent;
- (BOOL)getNXMouse:(NXEvent*)ev;

+ (void)append:(NSString*)value toEnv:(NSString*)name;

- (void)startX;
- (BOOL)startXClients;
- (void)run;
- (void)toggle;
- (void)show;
- (void)hide;
- (void)killServer;
- (void)readPasteboard;
- (void)writePasteboard;
- (void)sendNXEvent:(NXEvent*)ev;
- (void)sendShowHide:(BOOL)show;

// Aqua interface actions
- (IBAction)startFullScreen:(id)sender;
- (IBAction)startRootless:(id)sender;
- (IBAction)closeHelpAndShow:(id)sender;
- (IBAction)showAction:(id)sender;

// NSApplication delegate
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender;
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification;
- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApplication hasVisibleWindows:(BOOL)flag;
- (void)applicationWillResignActive:(NSNotification *)aNotification;
- (void)applicationWillBecomeActive:(NSNotification *)aNotification;

// NSPort delegate
- (void)handlePortMessage:(NSPortMessage *)portMessage;

@end

