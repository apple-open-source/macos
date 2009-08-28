//
//  JABAction.m
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/15/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABAction.h"

#import "JABActionInfo.h"
#import "JABDatabaseAction.h"
#import "JABLogger.h"

//------------------------------------------------------------------------------
@implementation JABAction

@synthesize operation = _operation;
@synthesize targetJid = _targetJid;
@synthesize verboseFlag = _verboseFlag;
@synthesize actionInfo = _actionInfo;
@synthesize logger = _logger;
@synthesize result = _result;

//------------------------------------------------------------------------------
+ (id) jabActionWithCommandOptions: (NSDictionary *) cmdOpts
{
	NSString *className = [cmdOpts objectForKey: CMDOPT_KEY_ACTIONCLASS];
	if (nil == className) return nil;
	Class aClass = NSClassFromString(className);
	if (nil == aClass)
		return nil; // unknown command
	return [[[aClass alloc] initWithCommandOptions: cmdOpts] autorelease];
}

//------------------------------------------------------------------------------
- (id) initWithCommandOptions: (NSDictionary *) cmdOpts
{
	self = [super init];

	self.operation = [[cmdOpts objectForKey: CMDOPT_KEY_OPCODE] integerValue];
	self.targetJid = [cmdOpts objectForKey: CMDOPT_KEY_JABBERID];

	self.verboseFlag = [[cmdOpts objectForKey: CMDOPT_KEY_VERBOSE] integerValue];

	self.actionInfo = [cmdOpts objectForKey: CMDOPT_KEY_ACTIONINFO]; // ref only - not retained
	
	self.logger = [JABLogger jabLoggerWithCommandOptions: cmdOpts];
	
	return self;
}

//------------------------------------------------------------------------------
- (void) dealloc
{
	self.logger = nil;
	self.targetJid = nil;

	[super dealloc];
}

//------------------------------------------------------------------------------
- (void) doCommand
{
	if (OPCODE_UNKNOWN == self.operation)
		return; // unknown command -- abort

	self.result = OPRESULT_OK;
	
	// execute command action
	[self doAction]; 

}

//------------------------------------------------------------------------------
- (void) doAction
{
	// base class has no default action - must be implemented in derived classes
}

//------------------------------------------------------------------------------
- (void) logResultWithOkMessage: (NSString *) okMsg errorMessage: (NSString *) errMsg
{
	NSString *resultMsg = nil;
	NSString *appendFmt = nil;
	if (OPRESULT_OK == self.result) {
		resultMsg = @"Operation completed. ";
		appendFmt = okMsg;
	}
	else {
		resultMsg = @"Error: operation failed. ";
		appendFmt = errMsg;
	}

	NSMutableString *logMsg = [NSMutableString stringWithCapacity: 0];
	[logMsg setString: resultMsg];
	if (nil != _targetJid)
		[logMsg appendString: appendFmt];
	else
	[logMsg appendFormat: appendFmt, _targetJid];
	[_logger logInfo: logMsg];
}

@end
