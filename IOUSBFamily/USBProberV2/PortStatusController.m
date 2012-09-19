//
//  PortStatusController.m
//  USBProber
//
//  Created by Russvogel on 10/14/10.
//  Copyright 2010 Apple. All rights reserved.
//

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
