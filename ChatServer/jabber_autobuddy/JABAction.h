//
//  JABAction.h
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/15/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "JABActionInfo.h"

// constants

// command info keys for [JABAction jabActionWithCommandOptions]
#define CMDOPT_KEY_OPCODE        @"opCode"
#define CMDOPT_KEY_ACTIONCLASS   @"actionClass"
#define CMDOPT_KEY_ACTIONINFO    @"actionInfo"
#define CMDOPT_KEY_JABBERID      @"jabberID"
#define CMDOPT_KEY_GROUPNAME     @"groupName"
#define CMDOPT_KEY_GROUPGUID     @"groupGuid"
#define CMDOPT_KEY_TESTCOUNT     @"testCount"
#define CMDOPT_KEY_TESTPREFIX    @"testPrefix"
#define CMDOPT_KEY_VERBOSE       @"verbose"
#define CMDOPT_KEY_SRCDOMAIN     @"srcDomain"
#define CMDOPT_KEY_DSTDOMAIN     @"dstDomain"

#ifdef DEBUG
// additional options for --debug mode
#define CMDOPT_KEY_DBPATHVAL     @"dbPathVal"
#define CMDOPT_KEY_DBNOWRITEFLAG @"dbNoWrite"
#define CMDOPT_KEY_SHOWSQLFLAG   @"showSQL"
#define CMDOPT_KEY_SUMMARYFLAG   @"showSummary"
#endif

// enums

// result codes for JABAction and derived classes
enum OpResultCodes {
	OPRESULT_OK = 0,
	OPRESULT_FAILED,
	OPRESULT_INVALARGS
};

@class JABActionInfo;
@class JABLogger;

//------------------------------------------------------------------------------
// JABAction base class
//------------------------------------------------------------------------------
@interface JABAction : NSObject {

	NSInteger _operation; // operation ID for this command
	NSString *_targetJid; // target JabberID for selected operations
	NSInteger _verboseFlag; // 1 = enable verbose/detailed output

	JABActionInfo *_actionInfo; // utility reference for metadata lookups
	
	JABLogger *_logger; // for log message handling

	NSInteger _result; // operation result code on completion
}
@property(assign, readwrite) NSInteger operation;
@property(retain, readwrite) NSString *targetJid;
@property(assign,readwrite) NSInteger verboseFlag;
@property(assign, readwrite) JABActionInfo *actionInfo; // ref only - not retained
@property(retain, readwrite) JABLogger *logger;
@property(assign, readwrite) NSInteger result;

+ (id) jabActionWithCommandOptions: (NSDictionary *) cmdOpts;

- (id) initWithCommandOptions: (NSDictionary *) cmdOpts;
- (void) dealloc;

- (void) doCommand;
- (void) doAction;

- (void) logResultWithOkMessage: (NSString *) okMsg errorMessage: (NSString *) errMsg;

@end
