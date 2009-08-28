//
//  JABShowHelpAction.h
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 8/8/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "JABAction.h"

//------------------------------------------------------------------------------
@interface JABShowHelpAction : JABAction {
	
	NSMutableArray *_helpLines; // output text for help message
}
@property(retain,readwrite) NSMutableArray *helpLines;

- (id) initWithCommandOptions: (NSDictionary *) cmdOpts;
- (void) dealloc;

- (void) doAction;

- (void) addAllHelpLines;
- (void) addHelpLinesForOpCode: (NSInteger) opCode;
- (void) addHelpNoteLines;

@end
