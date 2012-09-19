//
//  ExtensionSelector.h
//  USBProber
//
//  Created by local on 8/25/11.
//  Copyright 2011 Apple Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface ExtensionSelector : NSView
{
    NSPopUpButton *extensionSelectionButton;
    NSSavePanel *theSavePanel;
    NSDictionary *itemDictionary;
}
@property (nonatomic, retain) NSPopUpButton *extensionSelectionButton;
@property (nonatomic, retain) NSSavePanel *theSavePanel;
@property (nonatomic, retain) NSDictionary *itemDictionary;

-(void)populatePopuButtonWithArray:(NSDictionary *)addItems;
-(void)setCurrentSelection:(NSString *)currentSelection;
@end
