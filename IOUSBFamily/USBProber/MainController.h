/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#import <Cocoa/Cocoa.h>
#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/IOMessage.h>
#import <IOKit/IOCFPlugIn.h>
#import <IOKit/usb/IOUSBLib.h>
#import <stdio.h>
#import "Authorization.h"
#import "SystemCommands.h"
#import "Node.h"
#import "IORegistryClass.h"
#import "BusProbeClass.h"
#import "USBLoggerClass.h"

@interface mainController : NSObject
{
    IBOutlet id mainTabView;
    IBOutlet id mainWindow;
    IBOutlet id proberOutlineView;
    IBOutlet id ioregistryAutoRefreshButton;
    IBOutlet id proberAutoRefreshButton;
    IBOutlet id usbextensionsOutput;
    IBOutlet id usbextensionsModulesPopup;
    IBOutlet id proberRefreshButton;
    IBOutlet id ioregOutlineView;
    IBOutlet id ioregRefreshButton;
    IBOutlet id ioregPlanePopup;
    IBOutlet id loggerStartButton;
    IBOutlet id loggerOutput;
    IBOutlet id usbLoggerLoggingLevel;
    IBOutlet id filteredLoggerOutput;
    IBOutlet id usbloggerFilterFreshButton;

    NSTimer *refreshTimer;
    NSTimer *loggerOutputRefreshTimer;
    NSLock *loadingDataLock;
    NSLock *usbloggerOutputLock;
    NSPrintInfo *printInfo;
    NSScroller *usbloggerScroller;
    volatile NSMutableString *bufferedLoggerOuput;
    
    BOOL proberShouldAutoRefresh;
    BOOL ioregShouldAutoRefresh;
    BOOL extensionsNeedsAuth;
}

// These Methods load and save application preferences
// Currently, these consist of the last selected tab, autorefresh settings,
// and popup menu selections
- (void)loadPrefs;
- (void)savePrefs;

// Called when Refresh is pressed in the Bus Probe pane. Also called programmatically to cleanly reload the Bus Probe pane
- (IBAction)probeButtonPress:(id)sender;

// Used for UI refreshing
- (void)startRefreshTimer: (id)sender; // for bus probe and IORegistry UI
- (void)doRefreshTimerRepeating: (NSTimer *)timer;
- (void)startLoggerOutputRefreshTimer: (id)sender; //  for usblogger UI
- (void)outputNewLoggerText: (NSTimer *)time;

// This thread gets spun off, and checks for USB devices coming and going
-(void)monitorForDeviceChanges:(id)anObject;
void dumpIter( void *refCon, io_iterator_t iter );

// Reloads the specified outline view, as well as expands some Nodes
- (void)reloadOutlineView:(NSOutlineView *)outlineview;

// When a Refresh Automatically button is pressed in the UI, this method toggles the flags
- (IBAction)toggleAutorefresh:(id)sender;

// Refreshes the output in the Kernel Extensions pane
- (IBAction)usbextensionsGetVersions:(id)sender;

// Called when Refresh is pressed in the IORegistry pane. Also called programmatically to refresh the pane
- (IBAction)refreshIOReg:(id)sender;

// Saves the output of whatever window or pane is currently selected. Called by all the Save buttons in UI and the Save menu
- (IBAction)saveOutput:(id)sender;

// Puts whatever lines are selected in an outlineview onto the Pasteboard. Formatting (indents, depth) is also copied
- (void)copyOutlineViewToPasteboard:(NSOutlineView *)outlineview;

// Refreshes the contents of whichever pane is visible
- (IBAction)refreshItem:(id)sender;

// Selects a pane of the main tab view, based on the 'tag' of the sender
- (IBAction)selectMainTabViewItemAtIndex:(id)sender;

// Expands and collapses item in an outline view when they are double clicked
- (void)userDoubleClickedRow:(id)sender;

// delegate methods
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification;
- (void)applicationWillTerminate:(NSNotification *)aNotification;
- (void)windowWillClose:(NSNotification *)aNotification;
- (BOOL)validateMenuItem:(NSMenuItem *)anItem;
- (void)tabView:(NSTabView *)tabView didSelectTabViewItem:(NSTabViewItem *)tabViewItem;
- (void)copy:(id)sender;
- (void)clear:(id)sender;
- (void)print:(id)sender;

// returns the printInfo object for the app.
- (NSPrintInfo *)printInfo;

// returns a string contains all the visible text in an outline view.
- (NSString *)exposedItemsInOutlineView:(NSOutlineView *)ov;

// Handles any generic notifications we send out
- (void)handleIncomingGenericNotification:(NSNotification *)notification;

// usbLogger sends us notifications with output text as the object. This method handles the notification, and adds the text to bufferedLoggerOuput
- (void)handleIncomingUSBLoggerData:(NSNotification *)notification;

@end
