//
//  ExtensionSelector.m
//  USBProber
//
//  Created by local on 8/25/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import "ExtensionSelector.h"

@implementation ExtensionSelector
@synthesize extensionSelectionButton, theSavePanel, itemDictionary;

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) 
    {
        // Initialization code here.
        NSPopUpButton *myButtonToBe;
        NSRect newFrame;
        newFrame.origin.x = 10.0;
        newFrame.origin.y = 10.0;
        newFrame.size.height = 22;
        newFrame.size.width = 220;
        myButtonToBe = [[NSPopUpButton alloc] initWithFrame:newFrame];
        self.extensionSelectionButton = [myButtonToBe autorelease];
        [self.extensionSelectionButton setTarget:self];
        [self.extensionSelectionButton setAction:@selector(popupAction:)];
        
    }
    
    return self;
}

- (void)drawRect:(NSRect)dirtyRect
{
    // Drawing code here.
}


- (void)viewDidMoveToSuperview
{
    NSRect currentFrame = self.frame;
    NSRect superViewFrame = [self.superview frame];
    NSRect textViewFrame;
    textViewFrame.origin.x = 10.0;
    textViewFrame.origin.y = 5.0;
    textViewFrame.size.height = 22;
    textViewFrame.size.width = 70;
    NSTextView *theLabel = [[NSTextView alloc] initWithFrame:textViewFrame];
    [theLabel setString:@"File Type:"];
    [theLabel setEditable:NO];
    [theLabel setDrawsBackground:NO];
    [self addSubview:[theLabel autorelease]];
    textViewFrame = [self.extensionSelectionButton frame];
    textViewFrame.origin.x = textViewFrame.origin.x + 75;
    [self.extensionSelectionButton setFrame:textViewFrame];
    currentFrame.size.height = 40;
    currentFrame.size.width = superViewFrame.size.width;
    [self setFrame:currentFrame];
    [self addSubview:self.extensionSelectionButton];
    [[self superview] setAutoresizesSubviews:YES];
    [self setAutoresizingMask:NSViewWidthSizable | NSViewMaxXMargin];
}

-(void)populatePopuButtonWithArray:(NSDictionary *)addItems
{
    [self.extensionSelectionButton removeAllItems];
    [self.extensionSelectionButton addItemsWithTitles:[addItems allKeys]];
    self.itemDictionary = addItems;
}

-(void)setCurrentSelection:(NSString *)currentSelection
{
    [self.extensionSelectionButton setTitle:currentSelection];
}

-(IBAction)popupAction:(id)sender
{
    NSString *nameField = [[self.theSavePanel nameFieldStringValue] stringByDeletingPathExtension];
    [self.theSavePanel setNameFieldStringValue:[nameField stringByAppendingPathExtension:[self.itemDictionary valueForKey:[self.extensionSelectionButton titleOfSelectedItem]]]];
}

-(void)dealloc
{
    self.extensionSelectionButton = nil;
    self.theSavePanel = nil;
    self.itemDictionary = nil;
    [super dealloc];
}

@end
