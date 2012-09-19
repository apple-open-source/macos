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


#import "IORegistryController.h"

@implementation IORegistryController

- init {
    if (self = [super init]) {
        int plane;
        
        _rootNode = [[IORegOutlineViewNode alloc] initWithName:@"Root" value:@"Root"];
        _detailRootNode = [[IORegDetailOutlineViewNode alloc] init];
        
        if ([[[NSUserDefaults standardUserDefaults] objectForKey:@"IORegistryPlane"] intValue] == 1) {
            plane = kIOService_Plane;
        } else {
            plane = kIOUSB_Plane;
        }
        _infoGatherer = [[IORegInfoGatherer alloc] initWithListener:self rootNode:_rootNode plane:plane];
        
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(detailDrawerDidOpen:) name:NSDrawerDidOpenNotification object:IORegDetailedOutputDrawer];
    }
    return self;
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSDrawerDidOpenNotification object:IORegDetailedOutputDrawer];
    
    [_rootNode release];
    [_detailRootNode release];
    [_infoGatherer release];
    [super dealloc];
}

- (void)awakeFromNib {
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"IORegistryDontAutoRefresh"] == YES) {
        [RefreshCheckBox setState:NSOffState];
//enable/disable button        [RefreshButton setEnabled:YES];
    } else {
        [RefreshCheckBox setState:NSOnState];
//enable/disable button        [RefreshButton setEnabled:NO];
    }
    
    if ([[[NSUserDefaults standardUserDefaults] objectForKey:@"IORegistryPlane"] intValue] == 1) {
        [PlanePopUpButton selectItemAtIndex:1];
    } else {
        [PlanePopUpButton selectItemAtIndex:0];
    }
    
    [IORegOutputOV setTarget:self];
    [IORegOutputOV setAction:@selector(ioregItemSingleClicked:)];
    [IORegOutputOV setDoubleAction:@selector(ioregItemDoubleClicked:)];
    
    [IORegDetailedOutput setTarget:IORegDetailedOutput];
    [IORegDetailedOutput setDoubleAction:@selector(itemDoubleClicked)];
    
    [self expandOutlineViewItems];
}

- (IBAction)Refresh:(id)sender {
    [_detailRootNode removeAllChildren];
    [IORegDetailedOutput reloadData];
    
    [_infoGatherer refreshData:YES];
}

- (IBAction)SaveOutput:(id)sender {
    NSSavePanel *sp = [NSSavePanel savePanel];
    [sp setAllowedFileTypes:[NSArray arrayWithObjects:@"txt", @"plist", nil]];
    [sp setDirectoryURL:[NSURL URLWithString:NSHomeDirectory()]];
    [sp setNameFieldStringValue:@"IORegistry"];
    [sp setExtensionHidden:NO];
    
    NSRect exFrame;
    exFrame.origin.x=0;
    exFrame.origin.y=0;
    exFrame.size.width=200;
    exFrame.size.height=100;
    ExtensionSelector *newExSel = [[ExtensionSelector alloc] initWithFrame:exFrame];
    NSMutableDictionary *allowedTypesDictionary = [NSMutableDictionary dictionary];
    [allowedTypesDictionary setValue:@"txt" forKey:@"Text"];
    [allowedTypesDictionary setValue:@"plist" forKey:@"XML"];
    [newExSel populatePopuButtonWithArray:allowedTypesDictionary];
    [newExSel setCurrentSelection:@"Text"];
    [sp setAccessoryView:newExSel];
    newExSel.theSavePanel = sp;
    
    [sp beginSheetModalForWindow:[NSApp mainWindow] completionHandler:^(NSInteger returnCode){
		
        if (returnCode==NSOKButton)
        {
            NSString *selectedFileExtension = [[sp nameFieldStringValue] pathExtension];
            if (NSOrderedSame != [selectedFileExtension caseInsensitiveCompare:@"plist"])
            {
				NSString *finalString = [_rootNode stringRepresentationOfValues:0];
				
                if (![finalString writeToURL:[sp URL] atomically:YES encoding:NSUTF8StringEncoding error:NULL])
                {
                    NSBeep();
                }
            }
            else
            {
                
                if (![[_rootNode dictionaryVersionOfMe] writeToURL:[sp URL] atomically:YES])
                {
					NSBeep();
				}
			}
        }
    }];
	[newExSel release];
}

- (IBAction)ToggleAutoRefresh:(id)sender {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    if ([sender state] == NSOffState) {
        [defaults setBool:YES forKey:@"IORegistryDontAutoRefresh"];
//enable/disable button        [RefreshButton setEnabled:YES];
    } else {
        [defaults setBool:NO forKey:@"IORegistryDontAutoRefresh"];
//enable/disable button        [RefreshButton setEnabled:NO];
        [self Refresh:self];
    }
}

- (IBAction)TogglePlane:(id)sender {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    if ([sender indexOfSelectedItem] == 0) {
        [defaults setObject:[NSNumber numberWithInt:0] forKey:@"IORegistryPlane"];
        [_infoGatherer setPlane:0];
    } else if ([sender indexOfSelectedItem] == 1) {
        [defaults setObject:[NSNumber numberWithInt:1] forKey:@"IORegistryPlane"];
        [_infoGatherer setPlane:1];
    }
    
    [self Refresh:self];
}

- (void)ioRegInfoGathererInformationDidChange:(IORegInfoGatherer *)aGatherer {
    [IORegOutputOV reloadData];
    [self expandOutlineViewItems];
}

- (void)expandOutlineViewItems {
    NSEnumerator *enumerator;
    id thisNode;
    
    enumerator = [[_rootNode children] objectEnumerator];
    while (thisNode = [enumerator nextObject]) {
        if ([PlanePopUpButton indexOfSelectedItem] == 0) {
            [IORegOutputOV expandItem:thisNode expandChildren:YES];
        } else {
            NSEnumerator *childEnumerator = [[thisNode children] objectEnumerator];
            id thisChild;
            
            [IORegOutputOV expandItem:thisNode];
            
            while(thisChild = [childEnumerator nextObject]) {
                [IORegOutputOV expandItem:thisChild];
            }
        }
    }
}

- (id)outlineView:(NSOutlineView *)ov child:(NSInteger)index ofItem:(id)item
{
    if (ov == IORegOutputOV) {
        if (item == nil) {
            return [_rootNode childAtIndex:index];
        }
        return [(OutlineViewNode *)item childAtIndex:index];
    } else if (IORegDetailedOutput) {
        if (item == nil) {
            return [_detailRootNode childAtIndex:index];
        }
        return [(OutlineViewNode *)item childAtIndex:index];
    }
    
    return nil;
}

- (BOOL)outlineView:(NSOutlineView *)ov isItemExpandable:(id)item
{
    return [item isExpandable];
}

- (NSInteger)outlineView:(NSOutlineView *)ov numberOfChildrenOfItem:(id)item
{
    if (ov == IORegOutputOV) {
        if (item == nil) {
            return [_rootNode childrenCount];
        }
        return [item childrenCount];
    } else if (IORegDetailedOutput) {
        if (item == nil) {
            return [_detailRootNode childrenCount];
        }
        return [item childrenCount];
    }
    
    return 0;
}

- (id)outlineView:(NSOutlineView *)ov objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{   
    if (ov == IORegOutputOV) {
        return [item value];
    } else if (IORegDetailedOutput) {
        if ([[tableColumn identifier] intValue] == 0) {
            return [(OutlineViewNode *)item name];
        } else if ([[tableColumn identifier] intValue] == 1) {
            return [(IORegDetailOutlineViewNode *)item type];
        } else if ([[tableColumn identifier] intValue] == 2) {
            return [item value];
        }
    }
    return nil;
    
}

- (void)outlineViewSelectionDidChange:(NSNotification *)aNotification {
    if ([aNotification object] == IORegOutputOV) {
        [_detailRootNode removeAllChildren];
        [IORegDetailedOutput reloadData];
    }
}

- (void)ioregItemSingleClicked:(id)sender {
        if ([IORegDetailedOutputDrawer state] == NSDrawerOpenState) {
            IORegOutlineViewNode *item = [sender itemAtRow:[sender selectedRow]];
            if ([item representedDevice] != IO_OBJECT_NULL) {
                NSMutableDictionary *propertiesDict;

                if (IORegistryEntryCreateCFProperties( [item representedDevice], (CFMutableDictionaryRef *)&propertiesDict, kCFAllocatorDefault, 0 ) == 0) {
                    [_detailRootNode removeAllChildren];
                    [self populateNode:_detailRootNode withContentsOfDictionary:propertiesDict];
                    [IORegDetailedOutput reloadData];
                    CFRelease(propertiesDict);
                }
            }
        }
}

- (void)ioregItemDoubleClicked:(id)sender {
    [sender itemDoubleClicked];
}

- (void)populateNode:(IORegDetailOutlineViewNode *)node withKey:(NSString *)key value:(id)value {
    IORegDetailOutlineViewNode *   aNewNode;
    
    if (CFGetTypeID(value) == CFStringGetTypeID()) {
        aNewNode = [[IORegDetailOutlineViewNode alloc] initWithName:(NSString *)key value:(NSString *)value];
        [aNewNode setType:@"String"];
        [node addChild:aNewNode];
        [aNewNode release];
    } else if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        switch (CFNumberGetType((CFNumberRef)value)) {
            case kCFNumberSInt32Type:
                aNewNode = [[IORegDetailOutlineViewNode alloc] initWithName:(NSString *)key value:[NSString stringWithFormat:@"%@ (0x%x)",[value description], [value longValue]]];
                break;
            case kCFNumberSInt64Type:
                aNewNode = [[IORegDetailOutlineViewNode alloc] initWithName:(NSString *)key value:[NSString stringWithFormat:@"%@ (0x%x%x)",[value description], [value longLongValue],[value longValue]]];
                break;
            default:
                 aNewNode = [[IORegDetailOutlineViewNode alloc] initWithName:(NSString *)key value:[NSString stringWithFormat:@"%@ (0x%x)",[value description], [value intValue]]];
        }
        [aNewNode setType:@"Number"];
        [node addChild:aNewNode];
        [aNewNode release];
    } else if (CFGetTypeID(value) == CFDataGetTypeID()) {
        aNewNode = [[IORegDetailOutlineViewNode alloc] initWithName:(NSString *)key value:[value description]];
        [node addChild:aNewNode];
        [aNewNode release];
    } else if (CFGetTypeID(value) == CFDictionaryGetTypeID()) {
        aNewNode = [[IORegDetailOutlineViewNode alloc] initWithName:(NSString *)key value:@""];
        [aNewNode setType:@"Dictionary"];
        [self populateNode:aNewNode withContentsOfDictionary:value];
        [node addChild:aNewNode];
        [aNewNode release];
    } else if (CFGetTypeID(value) == CFArrayGetTypeID()) {
        aNewNode = [[IORegDetailOutlineViewNode alloc] initWithName:(NSString *)key value:@""];
        [aNewNode setType:@"Array"];
        [self populateNode:aNewNode withContentsOfArray:value];
        [node addChild:aNewNode];
        [aNewNode release];
    } else if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
        if ( CFBooleanGetValue( (CFBooleanRef) value) )
            aNewNode = [[IORegDetailOutlineViewNode alloc] initWithName:(NSString *)key value:@"Yes"];
        else
            aNewNode = [[IORegDetailOutlineViewNode alloc] initWithName:(NSString *)key value:@"No"];
        [aNewNode setType:@"Boolean"];
        [node addChild:aNewNode];
        [aNewNode release];
    }
}

- (void)populateNode:(IORegDetailOutlineViewNode *)node withContentsOfDictionary:(NSDictionary *)dictionary {
    NSMutableArray *    keys = [[dictionary allKeys] mutableCopy];
    NSEnumerator *      enumerator;
    NSString *          key;

    [keys sortUsingSelector:@selector(caseInsensitiveCompare:)];
    enumerator = [keys objectEnumerator];
    
    while (key = [enumerator nextObject]) {
        [self populateNode:node withKey:key value:[dictionary objectForKey:key]];
    }
    
    [keys release];
}

- (void)populateNode:(IORegDetailOutlineViewNode *)node withContentsOfArray:(NSArray *)array {
    NSEnumerator *      enumerator = [array objectEnumerator];
    id                  value;
    int                 counter = 0;
    
    while (value = [enumerator nextObject]) {
        [self populateNode:node withKey:[NSString stringWithFormat:@"%d",counter] value:value];
        counter++;
    }
}

- (void)detailDrawerDidOpen:(NSNotification *)notification {
    [self ioregItemSingleClicked:IORegOutputOV];
}

@end
