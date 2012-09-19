/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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


#import "MainApplicationController.h"

@implementation MainApplicationController

- (IBAction)ChooseTab:(id)sender {
    [MainTabView selectTabViewItemAtIndex:[sender tag]];
}

- (IBAction)ClearOutput:(id)sender {
    switch ([MainTabView indexOfTabViewItem:[MainTabView selectedTabViewItem]]) {
        case 3:
            [LoggerController ClearOutput:sender];
            break;
        default:
            // do nothing
            break;
    }
}

- (IBAction)MarkOutput:(id)sender {
    switch ([MainTabView indexOfTabViewItem:[MainTabView selectedTabViewItem]]) {
        case 3:
            [LoggerController MarkOutput:sender];
            break;
        default:
            // do nothing
            break;
    }
}

- (IBAction)Refresh:(id)sender {
    switch ([MainTabView indexOfTabViewItem:[MainTabView selectedTabViewItem]]) {
        case 0:
            [BPController Refresh:sender];
            break;
        case 1:
            [KEController Refresh:sender];
            break;
        case 2:
            [IORegController Refresh:sender];
            break;
        case 4:
            [PSController Refresh:sender];
            break;
        case 3:
        default:
            // do nothing
            break;
    }
}

- (IBAction)SaveOutput:(id)sender {
    switch ([MainTabView indexOfTabViewItem:[MainTabView selectedTabViewItem]]) {
        case 0:
            [BPController SaveOutput:sender];
            break;
        case 1:
            [KEController SaveOutput:sender];
            break;
        case 2:
            [IORegController SaveOutput:sender];
            break;
        case 3:
            [LoggerController SaveOutput:sender];
            break;
        case 4:
            [PSController SaveOutput:sender];
            break;
        default:
            // do nothing
            break;
    }
}

- (IBAction)ToggleIORegDetaiLDrawer:(id)sender {
    [IORegDetailedOutputDrawer toggle:self];
}

-(IBAction)changeFileType:(id)sender
{
    // NSLog(@"iChnaged");
}

- (void)tabView:(NSTabView *)tabView didSelectTabViewItem:(NSTabViewItem *)tabViewItem {
    if (tabView == MainTabView) {
        int index = [tabView indexOfTabViewItem:[tabView selectedTabViewItem]];
        switch (index) {
            case 0:
                [[BusProbeOutput window] makeFirstResponder:BusProbeOutput];
                break;
            case 1:
                [[KernelExtensionsOutput window] makeFirstResponder:KernelExtensionsOutput];
                break;
            case 2:
                [[IORegistryOutput window] makeFirstResponder:IORegistryOutput];
                break;
            case 3:
                [[USBLoggerOutput window] makeFirstResponder:USBLoggerOutput];
                break;
            case 4:
                [[PortStatusOutput window] makeFirstResponder:PortStatusOutput];
                break;
            default:
                // do nothing
                break;
        }
        
        if ( ([IORegDetailedOutputDrawer state] == NSDrawerOpeningState || [IORegDetailedOutputDrawer state] == NSDrawerOpenState) && index != 2 ) {
            [IORegDetailedOutputDrawer close];
        }
    }
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    id tabIndex = [[NSUserDefaults standardUserDefaults] objectForKey:@"SelectedMainTabViewItem"];
    
    if (tabIndex != nil) {
        int index = [tabIndex intValue];
        if (index >= 0 && index <= 4) {
            [MainTabView selectTabViewItemAtIndex:index];
        }
    }
}

- (void)windowWillClose:(NSNotification *)aNotification {
    [NSApp terminate:self];
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
	NSInteger index = [MainTabView indexOfTabViewItem:[MainTabView selectedTabViewItem]];
    [[NSUserDefaults standardUserDefaults] setObject:[NSNumber numberWithInt:index] forKey:@"SelectedMainTabViewItem"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    if ([NSStringFromSelector([menuItem action]) isEqualToString:@"Refresh:"]) {
        if ([MainTabView indexOfTabViewItem:[MainTabView selectedTabViewItem]] == 3) {
            // USB Logger does not have a 'refresh' option
            return NO;
        } else return YES;
    } else if ([NSStringFromSelector([menuItem action]) isEqualToString:@"ClearOutput:"]) {
        if ([MainTabView indexOfTabViewItem:[MainTabView selectedTabViewItem]] != 3) {
            // Only USB Logger has the 'clear output' option
            return NO;
        } else return YES;
    } else if ([NSStringFromSelector([menuItem action]) isEqualToString:@"MarkOutput:"]) {
        if ([MainTabView indexOfTabViewItem:[MainTabView selectedTabViewItem]] != 3) {
            // Only USB Logger has the 'mark output' option
            return NO;
        } else return YES;
    } else if ([NSStringFromSelector([menuItem action]) isEqualToString:@"ToggleIORegDetaiLDrawer:"]) {
        if ([MainTabView indexOfTabViewItem:[MainTabView selectedTabViewItem]] != 2) {
            // Only enable when IORegistry is shown
            return NO;
        } else return YES;
    } else return YES;
}

@end
