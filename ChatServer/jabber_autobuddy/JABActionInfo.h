//
//  JABActionInfo.h
//  jabber_autobuddy
//
//  Created by Steve Peralta on 7/21/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

// constants

// options info keys for [JABActionInfo jabActionInfoGetOptionsInfo]
#define OPTINFO_KEY_VERSIONTEXT   @"versionText"
#define OPTINFO_KEY_USAGETEXT     @"usageText"
#define OPTINFO_KEY_NOTESTEXT     @"notesText"

#define OPTINFO_KEY_OPCODE        @"opCode"
#define OPTINFO_KEY_ACTIONCLASS   @"actionClass"
#define OPTINFO_KEY_CMDFORMS      @"cmdForms"
#define OPTINFO_KEY_DUPACTION     @"dupAction"
#define OPTINFO_KEY_HELPTEXT      @"helpText"
#define OPTINFO_KEY_OPARGS        @"opArgs"
#define OPTINFO_KEY_ARGSHELP      @"argsHelp"
#define OPTINFO_KEY_NOTEREFS      @"noteRefs"

#define OPTINFO_KEY_NOTEREF       @"noteRef"
#define OPTINFO_KEY_NOTELINES     @"noteLines"

#define OPTINFO_PREFIX_USEDEFAULT @"-"
#define TESTUSER_DEFAULTPREFIX    @"u"

// enums

// operation codes for program options and commands
enum OperationCodes {
	OPCODE_UNKNOWN  = 0,
	OPCODE_SHOWHELP,
	OPCODE_SHOWVERS,
	OPCODE_INITUSER,
	OPCODE_DELUSER,
	OPCODE_ADDBUDDY,
	OPCODE_REMBUDDY,
	OPCODE_ALLBUDDIES,
	OPCODE_GRPBUDDIES,
	OPCODE_GROUP_BY_GUID,
	OPCODE_UNGROUP,
	OPCODE_UNGROUP_BY_GUID,
	OPCODE_MOVEDOMAIN,
	OPCODE_OPT_VERBOSE = 99,
#ifdef DEBUG
	OPCODE_OPT_DBPATH,
	OPCODE_OPT_SHOWSQL,
	OPCODE_OPT_NOWRITE,
	OPCODE_OPT_SUMMARY,
	OPCODE_INITTEST,
	OPCODE_DELALL,
#endif
	OPCODE_LAST = 9999,
};

@class JABLogger;

@interface JABActionInfo : NSObject {

	NSArray *_inputCommands; // list of unprocessed commands from input
	NSMutableDictionary *_optionsInfo; // metadata for all valid command arguments
	NSMutableArray *_actionList; // list of parsed actions from command input
	NSMutableDictionary *_globalOptions; // program options passed to each command when executed

	JABLogger *_logger; // log manaager instance

	NSInteger _opResult; // result of last [JABAction doCommand]
}
@property(retain,readwrite) NSArray *inputCommands;
@property(retain,readwrite) NSMutableDictionary *optionsInfo;
@property(retain,readwrite) NSMutableArray *actionList;
@property(retain,readwrite) NSMutableDictionary *globalOptions;
@property(retain,readwrite) JABLogger *logger;
@property(assign,readwrite) NSInteger opResult;

+ (id) jabActionInfoWithCommandInput: (NSArray *) inCommands;

- (id) initWithCommandInput: (NSArray *) inCommands;
- (void) dealloc;

- (void) initOptionsInfo;
- (void) initCommandInfoForOpCode: (NSInteger) opCode actionClass: (NSString *) actionClass
					 commandForms: (NSArray *) commandForms dupAction: (NSInteger) dupAction
						 helpText: (NSArray *) helpText opArgs: (NSArray *) opArgs 
						 argsHelp: (NSArray *) argsHelp noteRefs: (NSArray *) noteRefs;
- (void) initNotesText: (NSArray *) noteLines forRef: (NSString *) noteRef;

- (BOOL) processCommandInput;

- (NSInteger) performRequestedActions;
- (NSInteger) performActionsForOpCode: (NSInteger) opCode;
- (NSInteger) performActionForCmdOpts: (NSDictionary *) cmdOpts;

- (NSDictionary *) actionOptsForCommandOpts: (NSDictionary *) cmdOpts;

- (NSDictionary *) findOptionInfoForOpCode: (NSInteger) opCode;
- (NSArray *) findActionItemsForOpCode: (NSInteger) opCode;

- (BOOL) checkActionConflicts;
- (BOOL) checkActionConflictForOpCode: (NSInteger) oc1 andOpCode: (NSInteger) oc2;
- (BOOL) handleDuplicateArg: (NSString *) inArg withAction: (NSInteger) dupAction;

- (void) showUsage;
- (BOOL) showCommandConflictError: (NSArray *) actionItems reason: (NSString *) reason;
- (BOOL) showInvalidArgError: (NSString *) inArg;
- (BOOL) showMissingOptionValue: (NSString *) inArg argKey: (NSString *) argKey;
- (BOOL) showMissingArgsError;

@end
