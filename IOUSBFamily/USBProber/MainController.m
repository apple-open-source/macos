/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#import "MainController.h"

static const double    REFRESH_TIMER_STEP_SIZE  =  0.5;
static const double    USB_LOGGER_OUTPUT_REFRESH  =  0.1;

BOOL busDevicesChanged=NO;

@implementation mainController

- (void)loadPrefs
{
    NSUserDefaults *prefs = [[NSUserDefaults standardUserDefaults] retain];

    [mainTabView selectTabViewItemAtIndex:[prefs integerForKey:@"Selected Tab"]];
    [self tabView:mainTabView didSelectTabViewItem:[mainTabView selectedTabViewItem]];
    [usbextensionsModulesPopup selectItemAtIndex:[prefs integerForKey:@"Kernel Extensions"]];
    [ioregPlanePopup selectItemAtIndex:[prefs integerForKey:@"IORegistry Plane"]];
    [usbLoggerLoggingLevel selectItemAtIndex:[prefs integerForKey:@"Logging Level"]];
    if ([prefs integerForKey:@"Prober Should AutoRefresh"] == 0) {
        [proberAutoRefreshButton setState:NSOffState];
        proberShouldAutoRefresh = NO;
        [proberRefreshButton setEnabled:YES];
    }
    else {
        [proberAutoRefreshButton setState:NSOnState];
        proberShouldAutoRefresh = YES;
        [proberRefreshButton setEnabled:NO];
    }
    if ([prefs integerForKey:@"IORegistry Should Not AutoRefresh"] == 0) {
        [ioregistryAutoRefreshButton setState:NSOnState];
        ioregShouldAutoRefresh = YES;
        [ioregRefreshButton setEnabled:NO];
    }
    else {
        [ioregistryAutoRefreshButton setState:NSOffState];
        ioregShouldAutoRefresh = NO;
        [ioregRefreshButton setEnabled:YES];
    }
    
    [prefs release];
}

- (void)savePrefs
{
    NSUserDefaults *prefs = [[NSUserDefaults standardUserDefaults] retain];

    [prefs setInteger:[mainTabView indexOfTabViewItem:[mainTabView selectedTabViewItem]] forKey:@"Selected Tab"];
    [prefs setInteger:[usbextensionsModulesPopup indexOfSelectedItem] forKey:@"Kernel Extensions"];
    [prefs setInteger:[ioregPlanePopup indexOfSelectedItem] forKey:@"IORegistry Plane"];
    [prefs setInteger:[usbLoggerLoggingLevel indexOfSelectedItem] forKey:@"Logging Level"];
    [prefs setInteger:![proberAutoRefreshButton state] forKey:@"Prober Should AutoRefresh"];
    [prefs setInteger:![ioregistryAutoRefreshButton state] forKey:@"IORegistry Should Not AutoRefresh"];

    [prefs synchronize];
    [prefs release];
}

- (IBAction)probeButtonPress:(id)sender
{
    [BusProbeClass USBProbe];
    [self reloadOutlineView:proberOutlineView];
}

- (void)startRefreshTimer: (id)sender
{
    [refreshTimer invalidate];
    [refreshTimer release];
    refreshTimer = [[NSTimer scheduledTimerWithTimeInterval: (NSTimeInterval)REFRESH_TIMER_STEP_SIZE
                                                     target:                         self
                                                   selector:                       @selector(doRefreshTimerRepeating:)
                                                   userInfo:                       nil
                                                    repeats:                        YES] retain];
    return;
}

- (void)doRefreshTimerRepeating: (NSTimer *)timer
{
    if ([loadingDataLock tryLock]) {
        if (busDevicesChanged) {
            busDevicesChanged = NO;
            if (proberShouldAutoRefresh)
                [self probeButtonPress:nil];
            if (ioregShouldAutoRefresh)
                [self refreshIOReg:nil];
        }
        [loadingDataLock unlock];
    }
    return;
}

- (void)startLoggerOutputRefreshTimer: (id)sender
{
    [loggerOutputRefreshTimer invalidate];
    [loggerOutputRefreshTimer release];
    loggerOutputRefreshTimer = [[NSTimer scheduledTimerWithTimeInterval: (NSTimeInterval)USB_LOGGER_OUTPUT_REFRESH
                                                                 target:                         self
                                                               selector:                       @selector(outputNewLoggerText:)
                                                               userInfo:                       nil
                                                                repeats:                        YES] retain];
    return;
}

- (void)outputNewLoggerText: (NSTimer *)timer
{
    if ([usbloggerOutputLock tryLock]) {
        if (bufferedLoggerOuput != nil)
        {
            NSRange endMarker = NSMakeRange([[loggerOutput string] length], 0);
            BOOL usbloggerIsScrolledToEnd;
            int i,j;
    
            if (usbloggerScroller == nil)
                usbloggerScroller = [[loggerOutput enclosingScrollView] verticalScroller];
    
            usbloggerIsScrolledToEnd = (![usbloggerScroller isEnabled] || [usbloggerScroller floatValue] == 1);
    
            [loggerOutput replaceCharactersInRange:endMarker withString:(NSString *)bufferedLoggerOuput];
    
            if (usbloggerIsScrolledToEnd)
            {
                endMarker.location += [(NSString *)bufferedLoggerOuput length];
                [loggerOutput scrollRangeToVisible:endMarker];
            }

            j=[(NSMutableString *)bufferedLoggerOuput retainCount];
            for (i=0; i < j; i++)
                [(NSMutableString *)bufferedLoggerOuput release];
            bufferedLoggerOuput = nil;
        }
        [usbloggerOutputLock unlock];
    }
}

-(void)monitorForDeviceChanges:(id)anObject
{
    NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];
    kern_return_t kr;
    mach_port_t masterPort;
    CFRunLoopSourceRef  runLoopSource;
    IONotificationPortRef gNotifyPort;
    io_iterator_t  gAddedIter,gRemovedIter;
    CFRunLoopRef  gRunLoop;
    int dummyInteger=0;
    
    
    assert( KERN_SUCCESS == (kr = IOMasterPort(MACH_PORT_NULL, &masterPort)));
    
    gNotifyPort = IONotificationPortCreate(masterPort);
    
    runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);
    
    gRunLoop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(gRunLoop, runLoopSource, kCFRunLoopDefaultMode);

    kr = IOServiceAddMatchingNotification(gNotifyPort,
                                          kIOFirstMatchNotification,
                                          IOServiceMatching(kIOUSBDeviceClassName),
                                          dumpIter,
                                          NULL,
                                          &gAddedIter);
    kr = IOServiceAddMatchingNotification(gNotifyPort,
                                          kIOTerminatedNotification,
                                          IOServiceMatching(kIOUSBDeviceClassName),
                                          dumpIter,
                                          NULL,
                                          &gRemovedIter);
    dumpIter(&dummyInteger, gAddedIter);
    dumpIter(&dummyInteger, gRemovedIter);
    mach_port_deallocate(mach_task_self(), masterPort);
    masterPort = 0;
    CFRunLoopRun();
    
    [pool release];
}

void dumpIter( void *refCon, io_iterator_t iter )
{
    io_object_t obj;
    if (refCon == NULL)
        busDevicesChanged = YES;
    while( (obj = IOIteratorNext( iter)))
        IOObjectRelease( obj );
}

- (IBAction)reloadOutlineView:(NSOutlineView *)outlineview
{
    if (outlineview == proberOutlineView) {
        Node *node = [BusProbeClass busprobeRootNode];
        int j;
        [proberOutlineView reloadItem:[BusProbeClass busprobeRootNode] reloadChildren:YES];
        [proberOutlineView expandItem:[BusProbeClass busprobeRootNode]];
        for (j=0;j < [node childrenCount]; j++) {
            [proberOutlineView expandItem:[node childAtIndex:j]];
        }
    }
    else if (outlineview == ioregOutlineView) {
        int i, j, k;
        [ioregOutlineView reloadItem:[IORegistryClass ioregRootNode] reloadChildren:YES];
        [ioregOutlineView expandItem:[IORegistryClass ioregRootNode]];
        if ([ioregPlanePopup indexOfSelectedItem] == 0) { // IOUSB
            for (i=0; i < [[IORegistryClass ioregRootNode] childrenCount]; i++)
                [ioregOutlineView expandItem:[[IORegistryClass ioregRootNode] childAtIndex:i] expandChildren:YES];
        }
        else { // IOService
            Node *node = [IORegistryClass ioregRootNode];
            for (j=0;j < [node childrenCount]; j++) {
                [ioregOutlineView expandItem:[node childAtIndex:j]];
                for (k=0; k < [[node childAtIndex:j] childrenCount]; k++)
                    [ioregOutlineView expandItem:[[node childAtIndex:j] childAtIndex:k]];
            }
        }
    }
}

- (IBAction)toggleAutorefresh:(id)sender
{
    switch ([sender tag]) {
        case 1: // this is for the prober panel
            if (proberShouldAutoRefresh) {
                proberShouldAutoRefresh = NO;
                [proberRefreshButton setEnabled:YES];
            }
            else {
                proberShouldAutoRefresh = YES;
                [proberRefreshButton setEnabled:NO];
                [self probeButtonPress:nil];
            }
            break;
        case 2: // this is for the ioreg panel
            if (ioregShouldAutoRefresh) {
                ioregShouldAutoRefresh = NO;
                [ioregRefreshButton setEnabled:YES];
            }
            else {
                ioregShouldAutoRefresh = YES;
                [ioregRefreshButton setEnabled:NO];
                [self refreshIOReg:nil];
            }
            break;
    }
}

// This method is a drop in replacement for the one that is currently in the file MainController.m

- (IBAction)usbextensionsGetVersions:(id)sender
{
    NSString *extensionsString = [[NSString alloc] init];
    NSString *outputString =  [[NSString alloc] init]; // greppedString, after its been cleaned up to be more readable
    NSArray *grepArguments; // the arguments for grep, determined by the "Modules" popup in the UI
    NSArray *awkArguments = [NSArray arrayWithObjects:@"{print $6,$7}",nil]; // These arguments clean up kmodstat nicely
    
    
    if (extensionsNeedsAuth) { // runnings on a system without the "kextstat" tool, so we need auth.
        if ([[authorization sharedInstance] authenticate])
            extensionsString = [systemCommands kmodstatWithAuth:YES];
        else {
            NSRunAlertPanel(NSLocalizedStringFromTable(@"title", @"LaunchTime", @"Text for authorization error"), NSLocalizedStringFromTable(@"authorization error text", @"LaunchTime", @"Text for authorization error"), nil, nil, nil);
            return;
        }
    }
    else {
        extensionsString = [systemCommands kmodstatWithAuth:NO];
    }

    // Figure out what modules to grep for, and set arguments as appropriate, based on the "Modules" selected in the UI
    switch ([usbextensionsModulesPopup indexOfSelectedItem]) {
        case 0: // chose USB
            grepArguments = [NSArray arrayWithObjects:@"USB",nil];
            break;
        default: // else chose All
            grepArguments = [NSArray arrayWithObjects:@"-v",@"(Version)",nil];
            break;
    }
    outputString = [systemCommands awk:[systemCommands grep:extensionsString arguments:grepArguments] arguments:awkArguments];
    [usbextensionsOutput setString:[NSString stringWithFormat:@"Currently loaded kernel modules:\n\n"]];
    [usbextensionsOutput replaceCharactersInRange:NSMakeRange([[usbextensionsOutput string] length], 0) withString:outputString];
}


- (IBAction)refreshIOReg:(id)sender
{
    if (sender == ioregPlanePopup) {
        if (ioregShouldAutoRefresh) {
            [[IORegistryClass ioregRootNode] init];
            switch ([ioregPlanePopup indexOfSelectedItem]) {
                case 0:
                    [[IORegistryClass ioregRootNode] setItemValue: @"IOUSB Plane"];
                    break;
                default:
                    [[IORegistryClass ioregRootNode] setItemValue: @"IOService Plane"];
                    break;
            }
            [IORegistryClass doIOReg:[ioregPlanePopup indexOfSelectedItem]];
            [self reloadOutlineView:ioregOutlineView];
        }
        else
            return;
    }
    else {
        [[IORegistryClass ioregRootNode] init];
        switch ([ioregPlanePopup indexOfSelectedItem]) {
            case 0:
                [[IORegistryClass ioregRootNode] setItemValue: @"IOUSB Plane"];
                break;
            default:
                [[IORegistryClass ioregRootNode] setItemValue: @"IOService Plane"];
                break;
        }
        [IORegistryClass doIOReg:[ioregPlanePopup indexOfSelectedItem]];
        [self reloadOutlineView:ioregOutlineView];
    }
}

- (IBAction)saveOutput:(id)sender
{
    NSSavePanel *sp;
    int result;

    if ([[filteredLoggerOutput window] isKeyWindow]) {
        sp = [NSSavePanel savePanel];
        [sp setRequiredFileType:@"txt"];
        result = [sp runModalForDirectory:NSHomeDirectory() file:@"Filtered USB Log"];
        if (result == NSOKButton) {
            if (![[filteredLoggerOutput string] writeToFile:[sp filename] atomically:YES])
                NSBeep();
        }
        return;
    }

    switch ([mainTabView indexOfTabViewItem:[mainTabView selectedTabViewItem]]) {
        case 0: // bus probe
            sp = [NSSavePanel savePanel];
            [sp setRequiredFileType:@"txt"];
            result = [sp runModalForDirectory:NSHomeDirectory() file:@"USB Bus Probe"];
            if (result == NSOKButton) {
                if (![[[BusProbeClass busprobeRootNode] stringRepresentationWithInitialIndent:0 recurse:YES] writeToFile:[sp filename] atomically:YES])
                    NSBeep();
            }
                break;
        case 1: // kernel extensions
            sp = [NSSavePanel savePanel];
            [sp setRequiredFileType:@"txt"];
            result = [sp runModalForDirectory:NSHomeDirectory() file:@"USB Extension Versions"];
            if (result == NSOKButton) {
                if (![[usbextensionsOutput string] writeToFile:[sp filename] atomically:YES])
                    NSBeep();
            }
                break;
        case 2: // IORegistry
            sp = [NSSavePanel savePanel];
            [sp setRequiredFileType:@"txt"];
            result = [sp runModalForDirectory:NSHomeDirectory() file:@"IORegistry"];
            if (result == NSOKButton) {
                if (![[[IORegistryClass ioregRootNode] stringRepresentationWithInitialIndent:0 recurse:YES] writeToFile:[sp filename] atomically:YES])
                    NSBeep();
            }
                break;
        case 3: // usb logger
            [usbloggerOutputLock lock];
            sp = [NSSavePanel savePanel];
            [sp setRequiredFileType:@"txt"];
            result = [sp runModalForDirectory:NSHomeDirectory() file:@"USB Log"];
            if (result == NSOKButton) {
                if (![[loggerOutput string] writeToFile:[sp filename] atomically:YES])
                    NSBeep();
            }
            [usbloggerOutputLock unlock];
                break;
    }
}

- (void)copyOutlineViewToPasteboard:(NSOutlineView *)outlineview
{
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSEnumerator *enumerator;
    NSNumber *index;
    NSMutableString *stringToCopy = [[NSMutableString alloc] init];

    [pasteboard declareTypes: [NSArray arrayWithObject: NSStringPboardType] owner: NULL];
    enumerator = [outlineview selectedRowEnumerator];
    while ( (index = [enumerator nextObject]) ) {
        int i;
        for (i=0;i<[outlineview levelForRow:[index intValue]];i++)
            [stringToCopy appendString:@"    "];
        if ([[outlineview itemAtRow:[index intValue]] itemName] != NULL)
            [stringToCopy appendString:[NSString stringWithFormat:@"%@   ",[[outlineview itemAtRow:[index intValue]] itemName]]];
        if ([[outlineview itemAtRow:[index intValue]] itemValue] != NULL)
            [stringToCopy appendString:[NSString stringWithFormat:@"%@",[[outlineview itemAtRow:[index intValue]] itemValue]]];
        [stringToCopy appendString:[NSString stringWithFormat:@"\n"]];
    }
    [pasteboard setString:stringToCopy forType:NSStringPboardType];
    [stringToCopy release];
}

- (IBAction)refreshItem:(id)sender
{
    if ([[filteredLoggerOutput window] isKeyWindow]) {
        [usbloggerFilterFreshButton performClick:usbloggerFilterFreshButton];
    }
    else {
        switch ([mainTabView indexOfTabViewItem:[mainTabView selectedTabViewItem]]) {
            case 0:
                [self probeButtonPress:nil];
                break;
            case 1:
                [self usbextensionsGetVersions:nil];
                break;
            case 2:
                [self refreshIOReg:nil];
                break;
        }
    }
}

- (IBAction)selectMainTabViewItemAtIndex:(id)sender
{
    if ([[filteredLoggerOutput window] isKeyWindow]) {
        [[mainTabView window] makeKeyAndOrderFront:nil];
    }
    [mainTabView selectTabViewItemAtIndex:[sender tag]];
}

- (void)userDoubleClickedRow:(id)sender
{
    int selectedRow = [sender selectedRow];
    
    if (![sender isExpandable:[sender itemAtRow:selectedRow]])
        return;

    if (([[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask)) {
        if ([sender isItemExpanded:[sender itemAtRow:selectedRow]])
            [sender collapseItem:[sender itemAtRow:selectedRow]];
        [sender expandItem:[sender itemAtRow:selectedRow] expandChildren:YES];
    }
    else {
        if ([sender isItemExpanded:[sender itemAtRow:selectedRow]])
            [sender collapseItem:[sender itemAtRow:selectedRow]];
        else
            [sender expandItem:[sender itemAtRow:selectedRow]];
    }
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [self loadPrefs];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(handleIncomingGenericNotification:) name:@"com.apple.USBProber.general" object:@"DataNeedsReload"];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(handleIncomingUSBLoggerData:) name:@"com.apple.USBProber.usblogger" object:nil];
    if (![[NSFileManager defaultManager]fileExistsAtPath:@"/System/Library/Extensions/KLog.kext"]) {
        [loggerStartButton setEnabled:NO];
        [loggerOutput setString:NSLocalizedStringFromTable(@"klog.kext missing error text", @"LaunchTime", @"Text for missing KLog.kext")];
    }
    if ([[NSFileManager defaultManager]fileExistsAtPath:@"/usr/sbin/kextstat"]) {
        extensionsNeedsAuth = NO;
        [self usbextensionsGetVersions:nil];
    }
    else {
        extensionsNeedsAuth = YES;
        [usbextensionsOutput setString:NSLocalizedStringFromTable(@"extensions message text", @"LaunchTime", @"Text for extensions pane at boot")];
    }
    loadingDataLock = [[NSLock alloc] init];
    usbloggerOutputLock = [[NSLock alloc] init];
    [loadingDataLock lock];
    [proberOutlineView setDoubleAction:@selector(userDoubleClickedRow:)];
    [ioregOutlineView setDoubleAction:@selector(userDoubleClickedRow:)];
    [self probeButtonPress:nil];
    [self refreshIOReg:nil];
    [loadingDataLock unlock];
    [self startRefreshTimer:self];
    [self startLoggerOutputRefreshTimer:self];
    [NSThread detachNewThreadSelector: @selector(monitorForDeviceChanges:) toTarget:self withObject:nil];
}

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    [usbLoggerClass CleanUp];
    [self savePrefs];
}

- (void)windowWillClose:(NSNotification *)aNotification
{
    [NSApp terminate:nil];
}

- (BOOL)validateMenuItem:(NSMenuItem *)anItem
{
    if ([[anItem title] isEqualToString:@"Copy"]) {
        switch ([mainTabView indexOfTabViewItem:[mainTabView selectedTabViewItem]]) {
            case 0:
                [mainWindow makeFirstResponder:proberOutlineView];
                if ([proberOutlineView selectedRow] == -1)
                    return NO;
                    break;
            case 1:
                [mainWindow makeFirstResponder:usbextensionsOutput];
                if ([usbextensionsOutput selectedRange].length == 0)
                    return NO;
                    break;
            case 2:
                [mainWindow makeFirstResponder:ioregOutlineView];
                if ([ioregOutlineView selectedRow] == -1)
                    return NO;
                    break;
            case 3:
                [mainWindow makeFirstResponder:loggerOutput];
                if ([loggerOutput selectedRange].length == 0)
                    return NO;
                    break;
                break;
            default:
                return NO; // nothing else in this window is copyable, so NO
                break;
        }
    }
    else if ([[anItem title] isEqualToString:@"Clear"]) {
        if ([[filteredLoggerOutput window] isKeyWindow]) {
            // its ok to clear the filtered logger output.
        }
        else {
            switch ([mainTabView indexOfTabViewItem:[mainTabView selectedTabViewItem]]) {
                case 0:
                    // we won't let the user clear the outlineview
                    return NO;
                    break;
                case 1:
                    [mainWindow makeFirstResponder:usbextensionsOutput];
                    break;
                case 2:
                     // we won't let the user clear the outlineview
                    return NO;
                    break;
                case 3:
                    [mainWindow makeFirstResponder:loggerOutput];
                    break;
                default:
                    return NO; // nothing else in this window is clearable, so NO
                    break;
            }
        }
    }
    else if ([[anItem title] isEqualToString:@"Refresh"]) {
        if ([[filteredLoggerOutput window] isKeyWindow]) {
            // its always ok to refresh the filtered output
        }
        else {
            switch ([mainTabView indexOfTabViewItem:[mainTabView selectedTabViewItem]]) {
                case 0:
                    if (proberShouldAutoRefresh) // no manual refresh allowed during auto-fresh
                        return NO;
                    break;
                case 1:
                    // its always ok to refresh the kernel extensions pane
                    break;
                case 2:
                    if (ioregShouldAutoRefresh) // no manual refresh allowed during auto-fresh
                        return NO;
                    break;
                case 3:
                    return NO; // the usb logger pane does not 'refresh' really. it start/stops
                    break;
                default:
                    return NO; // nothing else in this window is refreshable, so NO
                    break;
            }
        }
    }
    
    return YES;
}

- (void)tabView:(NSTabView *)tabView didSelectTabViewItem:(NSTabViewItem *)tabViewItem
{
    if (tabView==mainTabView) {
        switch ([mainTabView indexOfTabViewItem:[mainTabView selectedTabViewItem]]) {
            case 0:
                [mainWindow makeFirstResponder:proberOutlineView];	break;
            case 1:
                [mainWindow makeFirstResponder:usbextensionsOutput];	break;
            case 2:
                [mainWindow makeFirstResponder:ioregOutlineView];	break;
            case 3:
                [mainWindow makeFirstResponder:loggerOutput];		break;
        }
    }
}

- (void)copy:(id)sender // copying for TableViews
{
    switch ([mainTabView indexOfTabViewItem:[mainTabView selectedTabViewItem]]) {
        case 0:
            [self copyOutlineViewToPasteboard:proberOutlineView];
            break;
        case 2:
            [self copyOutlineViewToPasteboard:ioregOutlineView];
            break;
    }
    return;
}

- (void)clear:(id)sender
{
    if ([[filteredLoggerOutput window] isKeyWindow]) {
        [filteredLoggerOutput setString:@""];
    }
    else {
        switch ([mainTabView indexOfTabViewItem:[mainTabView selectedTabViewItem]]) {
            case 1:
                [usbextensionsOutput setString:@""];
                break;
            case 3:
                [loggerOutput setString:@""];
                break;
        }
    }
}

- (void)print:(id)sender
{
    NSSize size = [[self printInfo] paperSize];
    NSTextView *textView;
    textView = [[NSTextView alloc] initWithFrame:NSMakeRect(0,0,size.width - [[self printInfo] leftMargin] - [[self printInfo] rightMargin],size.height - [[self printInfo] topMargin] - [[self printInfo] bottomMargin])];
    [textView setFont:[NSFont fontWithName:[[usbextensionsOutput font] fontName] size:9]];
    [textView setString:@""];

    if ([[filteredLoggerOutput window] isKeyWindow]) {
        [textView setString:[filteredLoggerOutput string]];
    }
    else {
        switch ([mainTabView indexOfTabViewItem:[mainTabView selectedTabViewItem]]) {
            case 0:
                [textView setString:[self exposedItemsInOutlineView:proberOutlineView]];
                break;
            case 1:
                [textView setString:[usbextensionsOutput string]];
                break;
            case 2:
                [textView setString:[self exposedItemsInOutlineView:ioregOutlineView]];
                break;
            case 3:
                [textView setString:[loggerOutput string]];
                break;
        }
    }
    
    [[NSPrintOperation printOperationWithView:textView printInfo:[self printInfo]] runOperation];
    [textView release];
    return;
}

- (NSPrintInfo *)printInfo {
        printInfo = [[NSPrintInfo sharedPrintInfo] copyWithZone:[self zone]];
        [printInfo setHorizontallyCentered:NO];
        [printInfo setVerticallyCentered:NO];
        [printInfo setLeftMargin:72.0];
        [printInfo setRightMargin:72.0];
        [printInfo setTopMargin:72.0];
        [printInfo setBottomMargin:72.0];
    return printInfo;
}

- (NSString *)exposedItemsInOutlineView:(NSOutlineView *)ov
{
    NSMutableString *finalString = [[NSMutableString alloc] init];
    NSRange range;
    int i, numberOfRows = [ov numberOfRows];
    for (i=0; i<numberOfRows; i++) {
        int j;
        for (j=0;j<[ov levelForRow:i];j++)
            [finalString appendString:@"    "];
        if ([[ov itemAtRow:i] itemName] == NULL)
            [finalString appendString:[NSString stringWithFormat:@"%@\n",[[ov itemAtRow:i] itemValue]]];
        else
            [finalString appendString:[NSString stringWithFormat:@"%@   %@\n",[[ov itemAtRow:i] itemName],[[ov itemAtRow:i] itemValue]]];
    }

    // lets get rid of extraneous periods so printouts dont look bad
    range = [finalString rangeOfString:@"....................................." options:NSCaseInsensitiveSearch];
    while (range.location != NSNotFound) {
        [finalString replaceCharactersInRange:range withString:@"............"];
        range = [finalString rangeOfString:@"....................................." options:NSCaseInsensitiveSearch];
    }
        
    return finalString;
}

- (void)handleIncomingGenericNotification:(NSNotification *)notification
{
    if ([[notification object] isEqualToString:@"DataNeedsReload"])
    {
        // a Node was asked to return an out of range object,
        // so it notified us that we better refresh the data
        // setting busDevicesChanged to YES ensures that a refresh will occur
        [loadingDataLock lock];
        busDevicesChanged = YES;
        [loadingDataLock unlock];
    }
}

- (void)handleIncomingUSBLoggerData:(NSNotification *)notification
{
    int i,j=[[notification object] retainCount];;
    [usbloggerOutputLock lock];
    if (bufferedLoggerOuput == nil)
        bufferedLoggerOuput = [[NSMutableString alloc] init];
    [(NSMutableString *)bufferedLoggerOuput appendString:[notification object]];
    
    for (i=1; i < j; i++)
                [[notification object] release];
    
    [usbloggerOutputLock unlock];
}

@end
