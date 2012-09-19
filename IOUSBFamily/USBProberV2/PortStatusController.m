/*
 * Copyright © 2010-2012 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#import "PortStatusController.h"

@interface PortStatusController (Private)


- (BOOL)logFile;
- (void) refreshStart;
- (void) refreshStop;
- (void)expandOutlineViewItems;

@end

@implementation PortStatusController

- init 
{
    if (self = [super init]) 
	{
        _rootNode = [[OutlineViewNode alloc] initWithName:@"Root" value:@"Root"];
		
        _infoGatherer = [[PortStatusGatherer alloc] initWithListener:self rootNode:_rootNode];

	}

	return self;
}

- (void)dealloc 
{
    [_rootNode release];
	[_infoGatherer release];

	[super dealloc];
}



- (IBAction)Refresh:(id)sender
{
	[_infoGatherer gatherStatus ];

    [PortStatusOutputOV reloadData];
	
    [self expandOutlineViewItems];
}


- (void)awakeFromNib 
{
	[self Refresh:self];

    if ( [[NSUserDefaults standardUserDefaults] boolForKey:@"PortStatusAutoRefresh"] == NO)
	{
        [fResfreshAutomatically setState:NSOffState];
    }
	else
	{
        [fResfreshAutomatically setState:NSOnState];
		[self refreshStart];
    }
	
    [PortStatusOutputOV setTarget:self];
    [PortStatusOutputOV setAction:@selector(portStatusItemSingleClicked:)];
    [PortStatusOutputOV setDoubleAction:@selector(portStatusItemDoubleClicked:)];
	
	// doesn't work!?
	[PortStatusOutputOV setFont:[NSFont fontWithName:@"Monaco" size:10]];

    [self expandOutlineViewItems];
}


// refreshEvent
//       used for auto refresh
//
- (void) refreshEvent
{
	[self Refresh:self];
}

- (void) refreshStart
{
	if( refreshTimer == NULL)
	{
		// poll set to every second
		refreshTimer = [NSTimer scheduledTimerWithTimeInterval: 10.0 target: self selector: @selector(refreshEvent) userInfo: self repeats: YES];
	}
}

- (void) refreshStop
{
	[refreshTimer invalidate];
	refreshTimer = NULL;
}

// refreshAutomaticallyAction
//		enable and disable refreshing
//
- (IBAction)refreshAutomaticallyAction:(id)sender
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    

	if( [sender state] == NSOnState )
	{
		[self refreshStart];
        [defaults setBool:YES forKey:@"PortStatusAutoRefresh"];
	}
	else
	{
		[self refreshStop];
        [defaults setBool:NO forKey:@"PortStatusAutoRefresh"];
	}
}


- (IBAction)SaveOutput:(id)sender
{
    NSSavePanel *sp = [NSSavePanel savePanel];
    [sp setAllowedFileTypes:[NSArray arrayWithObjects:@"txt", @"plist", nil]];
    [sp setDirectoryURL:[NSURL URLWithString:NSHomeDirectory()]];
    [sp setNameFieldStringValue:@"USBPortStatus"];
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


/////////////////////////////////////////////////////////////////////////////////////
#pragma mark -
#pragma mark DataSouce and NSOutLineView Interface

- (void)expandOutlineViewItems 
{
    NSEnumerator *enumerator;
    id thisNode;
    
    enumerator = [[_rootNode children] objectEnumerator];
    while (thisNode = [enumerator nextObject]) 
	{
		[PortStatusOutputOV expandItem:thisNode];

#if 0		
		NSEnumerator *childEnumerator = [[thisNode children] objectEnumerator];
		id thisChild;
		while(thisChild = [childEnumerator nextObject]) 
		{
			[PortStatusOutputOV expandItem:thisChild];
		}
#endif
    }
}
		
- (id)outlineView:(NSOutlineView *)ov child:(NSInteger)index ofItem:(id)item
{
    if (ov == PortStatusOutputOV) {
        if (item == nil) 
		{
            return [_rootNode childAtIndex:index];
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
    if (ov == PortStatusOutputOV) 
{
        if (item == nil) 
		{
            return [_rootNode childrenCount];
        }
        return [item childrenCount];
    }
	
    return 0;
}
	
- (id)outlineView:(NSOutlineView *)ov objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
	
    if (ov == PortStatusOutputOV) 
	{
        return [item value];
    }
    return nil;
}
	
- (void)portStatusItemSingleClicked:(id)sender 
{
}

- (void)portStatusItemDoubleClicked:(id)sender 
{
    [sender itemDoubleClicked];
}



@end
