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


#import "BusProbeController.h"

@implementation BusProbeController

- init {
    if (self = [super init]) {
        _devicesArray = [[NSMutableArray alloc] init];
        _busProber = [[BusProber alloc] initWithListener:self devicesArray:_devicesArray];
    }
    return self;
}

- (void)dealloc {
    [_devicesArray release];
    [_devicesArray release];
    [_busProber release];
    [super dealloc];
}

- (void)awakeFromNib {
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"BusProbeDontAutoRefresh"] == YES) {
        [RefreshCheckBox setState:NSOffState];
//enable/disable button        [RefreshButton setEnabled:YES];
    } else {
        [RefreshCheckBox setState:NSOnState];
//enable/disable button        [RefreshButton setEnabled:NO];
    }
    
    [BusProbeOutputOV setTarget:BusProbeOutputOV];
    [BusProbeOutputOV setDoubleAction:@selector(itemDoubleClicked)];
    
    [self expandOutlineViewItems];
}

- (IBAction)Refresh:(id)sender
{
    [_busProber refreshData:YES];
}

- (IBAction)SaveOutput:(id)sender
{
    NSSavePanel *sp = [NSSavePanel savePanel];
    int result;
    
    [sp setRequiredFileType:@"txt"];
    result = [sp runModalForDirectory:NSHomeDirectory() file:@"USB Bus Probe"];
    if (result == NSOKButton) {
        NSMutableString *finalString = [[NSMutableString alloc] init];
        NSEnumerator *devicesEnumerator = [_devicesArray objectEnumerator];
        BusProbeDevice *thisDevice;
        
        while (thisDevice = [devicesEnumerator nextObject]) {
            [finalString appendFormat:@"%@",thisDevice];
        }
        
        if (![finalString writeToFile:[sp filename] atomically:YES encoding:NSUTF8StringEncoding error:NULL])
            NSBeep();
        
        [finalString release];
    }
}

- (IBAction)ToggleAutoRefresh:(id)sender
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    if ([sender state] == NSOffState) {
        [defaults setBool:YES forKey:@"BusProbeDontAutoRefresh"];
//enable/disable button        [RefreshButton setEnabled:YES];
    } else {
        [defaults setBool:NO forKey:@"BusProbeDontAutoRefresh"];
//enable/disable button        [RefreshButton setEnabled:NO];
        [self Refresh:self];
    }
}

- (void)busProberInformationDidChange:(BusProber *)aProber {
    [BusProbeOutputOV reloadData];
    [self expandOutlineViewItems];
}

- (void)expandOutlineViewItems {
    NSEnumerator *enumerator;
    id thisNode;

    enumerator = [_devicesArray objectEnumerator];
    while (thisNode = [enumerator nextObject]) {
        [BusProbeOutputOV expandItem:thisNode];
    }
}

- (id)outlineView:(NSOutlineView *)ov child:(int)index ofItem:(id)item
{
    if (item == nil) {
        return [_devicesArray objectAtIndex:index];
    } else if ([item class] == [BusProbeDevice class]) {
        return [[(BusProbeDevice *)item rootNode] childAtIndex:index];
    }
    return [(OutlineViewNode *)item childAtIndex:index];
}

- (BOOL)outlineView:(NSOutlineView *)ov isItemExpandable:(id)item
{
    if ([item class] == [BusProbeDevice class]) {
        return [[(BusProbeDevice *)item rootNode] isExpandable];
    }
    return [item isExpandable];
}

- (int)outlineView:(NSOutlineView *)ov numberOfChildrenOfItem:(id)item
{
    if (item == nil) {
        return [_devicesArray count];
    } else if ([item class] == [BusProbeDevice class]) {
        return [[(BusProbeDevice *)item rootNode] childrenCount];
    }
    return [item childrenCount];
}

- (id)outlineView:(NSOutlineView *)ov objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{   
    if ([[tableColumn identifier] intValue] == 0) {
        if ([item class] == [BusProbeDevice class]) {
            return [[(BusProbeDevice *)item rootNode] name];
        }
        return [(OutlineViewNode *)item name];
    } else {
        if ([item class] == [BusProbeDevice class]) {
            return [[(BusProbeDevice *)item rootNode] value];
        }
        return [item value];
    }
}

@end