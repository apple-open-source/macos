//
//  JABShowHelpAction.m
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 8/8/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABShowHelpAction.h"

#import "JABLogger.h"

// CONSTANTS

#define HELP_LINE_INDENT1  @"    "
#define HELP_LINE_INDENT2  @"        "

#define NOTE_LINE_INDENT1  @"    "
#define NOTE_LINE_INDENT2  @"     "

//------------------------------------------------------------------------------
// JABShowHelpAction methods
//------------------------------------------------------------------------------
@implementation JABShowHelpAction

@synthesize helpLines = _helpLines;

//------------------------------------------------------------------------------
- (id) initWithCommandOptions: (NSDictionary *) cmdOpts
{
	self = [super initWithCommandOptions: cmdOpts];

	self.helpLines = [NSMutableArray arrayWithCapacity: 0];
	[self addAllHelpLines];
	
	return self;
}

- (void) dealloc
{
	self.helpLines = nil;
	
	[super dealloc];
}

//------------------------------------------------------------------------------
- (void) doAction 
{
	// display the help message lines
	for (NSString *line in _helpLines) {
		[_logger logStdErrMessage: [NSString stringWithFormat: @"%@\n", line]];
	} // for..in
}

//------------------------------------------------------------------------------
- (void) addAllHelpLines 
{
	// Assemble the help text from the pre-defined options metadata
	
	// Add the usage section
	NSDictionary *optsInfo = [_actionInfo optionsInfo];
	[_helpLines addObject: @"Usage:"];
	[_helpLines addObject: [NSString stringWithFormat: @"%@%@", HELP_LINE_INDENT1,
							[optsInfo objectForKey: OPTINFO_KEY_USAGETEXT]]];
	[_helpLines addObject: @" "]; // finish section line break
	
	// Add the options section
	[_helpLines addObject: @"Options: short-form(s)  long-form"];
	[self addHelpLinesForOpCode: OPCODE_SHOWHELP];
	[self addHelpLinesForOpCode: OPCODE_SHOWVERS];
	[self addHelpLinesForOpCode: OPCODE_OPT_VERBOSE];
#ifdef DEBUG
	[self addHelpLinesForOpCode: OPCODE_OPT_DBPATH];
	[self addHelpLinesForOpCode: OPCODE_OPT_NOWRITE];
	[self addHelpLinesForOpCode: OPCODE_OPT_SHOWSQL];
	[self addHelpLinesForOpCode: OPCODE_OPT_SUMMARY];
#endif
	[_helpLines addObject: @" "]; // finish section line break
	
	// Add the commands section
	[_helpLines addObject: @"Commands: short-form(s)  long-form [ <command-arg> ...]"];
	[self addHelpLinesForOpCode: OPCODE_INITUSER];
	[self addHelpLinesForOpCode: OPCODE_DELUSER];
	[self addHelpLinesForOpCode: OPCODE_ADDBUDDY];
	[self addHelpLinesForOpCode: OPCODE_REMBUDDY];
	[self addHelpLinesForOpCode: OPCODE_ALLBUDDIES];
	[self addHelpLinesForOpCode: OPCODE_GRPBUDDIES];
	[self addHelpLinesForOpCode: OPCODE_GROUP_BY_GUID];
	[self addHelpLinesForOpCode: OPCODE_UNGROUP];
	[self addHelpLinesForOpCode: OPCODE_UNGROUP_BY_GUID];
	[self addHelpLinesForOpCode: OPCODE_MOVEDOMAIN];
#ifdef DEBUG
	[self addHelpLinesForOpCode: OPCODE_INITTEST];
	[self addHelpLinesForOpCode: OPCODE_DELALL];
#endif
	[_helpLines addObject: @" "]; // finish section line break

	// Add the help notes section
	[self addHelpNoteLines];
	[_helpLines addObject: @" "]; // finish section line break
}

//------------------------------------------------------------------------------
- (void) addHelpLinesForOpCode: (NSInteger) opCode
{
	// Add the help text for the optionsInfo entry matching 
	// the selected operation to the array of help lines
	
	NSDictionary *cmdOpts = [_actionInfo findOptionInfoForOpCode: opCode];
	// Add a line to describe the valid the option/command formats (e.g. "-h", "--help")
	// On the same line, add the descriptions for any required arguments
	NSMutableString *outLine = [NSMutableString stringWithCapacity: 0];
	[outLine appendString: HELP_LINE_INDENT1]; // insert column padding
	for (NSString *cmdForm in [cmdOpts objectForKey: OPTINFO_KEY_CMDFORMS])
		[outLine appendFormat: @"%@  ", cmdForm]; // add command key forms
	for (NSString *argDesc in [cmdOpts objectForKey: OPTINFO_KEY_ARGSHELP])
		[outLine appendFormat: @"%@  ", argDesc]; // add argument descriptions
	[_helpLines addObject: [NSString stringWithString: outLine]];
	
	// Add all help text line(s) for this command
	NSArray *helpLines = [cmdOpts objectForKey: OPTINFO_KEY_HELPTEXT];
	BOOL firstLine = YES;
	for (NSString *line in helpLines) {
		[outLine setString: @""];
		[outLine appendFormat: @"%@%@", HELP_LINE_INDENT2, line];
		if (firstLine) {
			NSArray *noteRefs = [cmdOpts objectForKey: OPTINFO_KEY_NOTEREFS];
			if ((nil != noteRefs) && (0 < [noteRefs count])) {
				[outLine appendString: @" ["];
				BOOL firstRef = YES;
				for (NSString *noteRef in noteRefs) {
					if (!firstRef) [outLine appendString: @","];
					[outLine appendString: noteRef];
					if (firstRef) firstRef = NO;
				}
				[outLine appendString: @"]"];
			}
		}
		[_helpLines addObject: [NSString stringWithString: outLine]];
		if (firstLine) firstLine = NO;
	}
}

//------------------------------------------------------------------------------
- (void) addHelpNoteLines
{
	[_helpLines addObject: @"Notes:"];

	NSArray *noteItems = [[_actionInfo optionsInfo] objectForKey: OPTINFO_KEY_NOTESTEXT];
	NSString *noteIndexFmt = (9 < [noteItems count]) ? @"[%2ld] " : @"[%ld] ";
	NSString *notLineIndent = (9 < [noteItems count]) ? NOTE_LINE_INDENT2 : NOTE_LINE_INDENT1;

	for (NSDictionary *noteInfo in noteItems) {
		NSString *noteRef = [noteInfo objectForKey: OPTINFO_KEY_NOTEREF];
		NSArray *noteLines = [noteInfo objectForKey: OPTINFO_KEY_NOTELINES];
		BOOL firstLine = YES;
		for (NSString *aLine in noteLines) {
			NSMutableString *outLine = [NSMutableString stringWithCapacity: 0];
			if (firstLine) {
				[outLine appendFormat: noteIndexFmt, [noteRef integerValue]];
				firstLine = NO;
			}
			else [outLine appendString: notLineIndent];
			[outLine appendString: aLine];
			[_helpLines addObject: [NSString stringWithString: outLine]];
		} // for..in noteLines
	} // for..in noteKeys
}

@end
