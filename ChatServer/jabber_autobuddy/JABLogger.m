//
//  JABLogger.m
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/16/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABLogger.h"

#import "JABAction.h"

#define MAX_LOG_LINE 1024

static const char *_log_level[] = {
	"emergency",
	"alert",
	"critical",
	"error",
	"warning",
	"notice",
	"info",
	"debug"
};

@implementation JABLogger

@synthesize verboseFlag = _verboseFlag;

@synthesize outFile = _outFile;
@synthesize errFile = _errFile;

//------------------------------------------------------------------------------
+ (id) jabLogger
{
	return [[[self alloc] initWithCommandOptions: nil] autorelease];
}

//------------------------------------------------------------------------------
+ (id) jabLoggerWithCommandOptions: (NSDictionary *) cmdOpts
{
	return [[[self alloc] initWithCommandOptions: cmdOpts] autorelease];
}

//------------------------------------------------------------------------------
- (id) initWithCommandOptions: (NSDictionary *) cmdOpts
{
	self = [super init];
	
	self.outFile = [NSFileHandle fileHandleWithStandardOutput];
	self.errFile = [NSFileHandle fileHandleWithStandardError];
	
	if (nil != cmdOpts)
		self.verboseFlag = [[cmdOpts objectForKey: CMDOPT_KEY_VERBOSE] integerValue];
	
	return self;
}

// --------------------------------------------------------------------------------
- (void) dealloc
{
	self.errFile = nil;
	self.outFile = nil;
	
	[super dealloc];
}

//------------------------------------------------------------------------------
- (void) logInfo: (NSString *) aMsg
{
	[self logMsgWithLevel: LOG_INFO format: @"%@", aMsg];
}

//------------------------------------------------------------------------------
- (void) logNotice: (NSString *) aMsg
{
	[self logMsgWithLevel: LOG_NOTICE format: @"%@", aMsg];
}

//------------------------------------------------------------------------------
- (void) logError: (NSString *) aMsg
{
	[self logMsgWithLevel: LOG_ERR format: @"%@", aMsg];
}

//------------------------------------------------------------------------------
- (void) logMsgWithLevel: (int) level format: (NSString *) aFormat, ...
{
	va_list argList;
	va_start(argList, aFormat);
	NSString *logMsg = [[[NSString alloc] initWithFormat: aFormat arguments: argList] autorelease];
	va_end(argList);
	
	// log to syslog
	syslog(level, "%s", [logMsg UTF8String]);
	
	if ((0 == _verboseFlag) && (LOG_WARNING < level))
		return;

	NSMutableString *stdMsg = [NSMutableString stringWithCapacity: 0];

	// prefix the message with a timestamp
	time_t t = time(NULL);
	char *szTime = (char *) ctime(&t);
	szTime[strlen(szTime) - 1] = '\0'; // discard the trailing line end character
	[stdMsg setString: [NSString stringWithUTF8String: szTime]];

	// add the log level descriptive string and caller's message
	[stdMsg appendFormat: @"[%@] %@\n", 
	 [NSString stringWithUTF8String: _log_level[level]], logMsg];
	
	// write the final log message
	if (LOG_ERR == level)
		[self logStdErrMessage: stdMsg];
	else
		[self logStdOutMessage: stdMsg];
	
	return;
}

//------------------------------------------------------------------------------
- (void) logStdOutMessage: (NSString *) aMsg
{
	[_outFile writeData: [aMsg dataUsingEncoding: NSUTF8StringEncoding]];
}

//------------------------------------------------------------------------------
- (void) logStdErrMessage: (NSString *) aMsg
{
	[_errFile writeData: [aMsg dataUsingEncoding: NSUTF8StringEncoding]];
}

@end
