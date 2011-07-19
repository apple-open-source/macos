//
//  JABActionInfo.m
//  jabber_autobuddy
//
//  Created by Steve Peralta on 7/21/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABActionInfo.h"

#import "JABAction.h"
#import "JABLogger.h"

// CONSTANTS

#ifndef JAB_VERSION_MAJOR
#define JAB_VERSION_MAJOR 10
#endif

#ifndef JAB_VERSION_MINOR
#define JAB_VERSION_MINOR 6
#endif

#ifndef JAB_VERSION_BUILD
#define JAB_VERSION_BUILD 0
#endif

// action names (commands only)
#define ACTIONCLASS_NONE            @""
#define ACTIONCLASS_SHOWUSAGE       @"JABShowHelpAction"
#define ACTIONCLASS_SHOWVERSION     @"JABShowVersionAction"
#define ACTIONCLASS_INITUSER        @"JABInitUserAction"
#define ACTIONCLASS_DELETEUSER      @"JABDeleteUserAction"
#define ACTIONCLASS_ADDBUDDY        @"JABAddRosterUserAction"
#define ACTIONCLASS_REMOVEBUDDY     @"JABDeleteRosterUserAction"
#define ACTIONCLASS_ALLBUDDIES      @"JABMakeAllBuddiesAction"
#define ACTIONCLASS_GROUPBUDDIES    @"JABMakeGroupBuddiesAction"
#define ACTIONCLASS_GROUP_BY_GUID   @"JABMakeGroupBuddiesByGuidAction"
#define ACTIONCLASS_REMOVEGROUP     @"JABRemoveGroupBuddiesAction"
#define ACTIONCLASS_UNGROUP_BY_GUID @"JABRemoveGroupBuddiesByGuidAction"
#define ACTIONCLASS_MOVEDOMAIN      @"JABMoveDomainAction"
#ifdef DEBUG
#define ACTIONCLASS_INITTEST        @"JABInitTestUsersAction"
#define ACTIONCLASS_DELETEALL       @"JABDeleteAllUsersAction"
#endif

// command forms for parsing
#define CMDFORM_S_VERBOSE     @"-V"
#define CMDFORM_L_VERBOSE     @"--verbose"
#define CMDFORM_S_SHOWHELP    @"-h"
#define CMDFORM_S_SHOWHELP2   @"-?"
#define CMDFORM_L_SHOWHELP    @"--help"
#define CMDFORM_S_SHOWVERS    @"-v"
#define CMDFORM_L_SHOWVERS    @"--version"
#define CMDFORM_S_INITUSER    @"-i"
#define CMDFORM_L_INITUSER    @"--inituser"
#define CMDFORM_S_DELETEUSER  @"-d"
#define CMDFORM_L_DELETEUSER  @"--deleteuser"
#define CMDFORM_S_ADDBUDDY    @"-a"
#define CMDFORM_L_ADDBUDDY    @"--addbuddy"
#define CMDFORM_S_REMOVEBUDDY @"-r"
#define CMDFORM_L_REMOVEBUDDY @"--removebuddy"
#define CMDFORM_S_ALLBUDDIES  @"-m"
#define CMDFORM_L_ALLBUDDIES  @"--all"
#define CMDFORM_S_GRPBUDDIES  @"-g"
#define CMDFORM_L_GRPBUDDIES  @"--group"
#define CMDFORM_S_GROUPBYGUID @"-G"
#define CMDFORM_L_GROUPBYGUID @"--group-by-id"
#define CMDFORM_S_UNGROUP     @"-u"
#define CMDFORM_L_UNGROUP     @"--ungroup"
#define CMDFORM_S_UNGROUPGUID @"-U"
#define CMDFORM_L_UNGROUPGUID @"--ungroup-by-id"
#define CMDFORM_S_MOVEDOMAIN  @"-M"
#define CMDFORM_L_MOVEDOMAIN  @"--move-domain"
// commands available only with --debug
#ifdef DEBUG
#define CMDFORM_S_DBPATH      @"-b"
#define CMDFORM_L_DBPATH      @"--database"
#define CMDFORM_S_SHOWSQL     @"-q"
#define CMDFORM_L_SHOWSQL     @"--showsql"
#define CMDFORM_S_NOWRITE     @"-w"
#define CMDFORM_L_NOWRITE     @"--nowrite"
#define CMDFORM_S_SUMMARY     @"-s"
#define CMDFORM_L_SUMMARY     @"--summary"
#define CMDFORM_S_INITTEST    @"-T"
#define CMDFORM_L_INITTEST    @"--inittest"
#define CMDFORM_S_DELALL      @"-X"
#define CMDFORM_L_DELALL      @"--deleteall"
#endif

// required command argument help text
#define OPTINFO_REQARG_JID       @"<JID>"
#define OPTINFO_REQARG_GROUP     @"<GROUP>"
#define OPTINFO_REQARG_GROUP_ID  @"<GROUP ID>"
#define OPTINFO_REQARG_SRCDOMAIN @"<FROM-DOMAIN>"
#define OPTINFO_REQARG_DSTDOMAIN @"<TO-DOMAIN>"
#define OPTINFO_REQARG_DBPATH    @"<PATH>"
#define OPTINFO_REQARG_NUMVAL    @"<#>"
#define OPTINFO_REQARG_PREFIX    @"<PREFIX>"

#define OPTINFO_HTEXT_SHOWUSAGE   @"Displays this message."
#define OPTINFO_HTEXT_SHOWVERSION @"Display program version information."
#define OPTINFO_HTEXT_INITUSER    @"Initialize a user's data store in the jabberd database."
#define OPTINFO_HTEXT_DELUSER     @"Delete a user's data store from the jabberd database."
#define OPTINFO_HTEXT_ADDBUDDY    @"Add a user to the buddy list of all other users in the jabberd database."
#define OPTINFO_HTEXT_REMBUDDY    @"Delete a user from the buddy lists of all other user's in the jabberd database."
#define OPTINFO_HTEXT_ALLBUDDIES  @"Make all existing users buddies."
#define OPTINFO_HTEXT_GRPBUDDIES  @"Make buddies of existing users belonging to the Open Directory group."
#define OPTINFO_HTEXT_GRPBYGUID   @"Make buddies of existing users belonging to the Open Directory group, using the group's GeneratedID."
#define OPTINFO_HTEXT_UNGROUP     @"Remove all buddy pairings for the Open Directory group."
#define OPTINFO_HTEXT_UNGROUPGUID @"Remove all buddy pairings for the Open Directory group, using the group's GeneratedID."
#define OPTINFO_HTEXT_MOVEDOMAIN  @"Move all users from one jabber domain to another."
#define OPTINFO_HTEXT_VERBOSE     @"Enable verbose (detailed) program output to standard error."
#define OPTINFO_HTEXT_DBPATH1     @"DEBUG: Directs SQL queries to the selected database."
#define OPTINFO_HTEXT_DBPATH2     @"       (default: /Library/Server/iChat/Data/sqlite/jabberd2.db)"
#define OPTINFO_HTEXT_SHOWSQL     @"DEBUG: Display all generated SQL queries."
#define OPTINFO_HTEXT_NOWRITE     @"DEBUG: Skip execution of queries which modify the database."
#define OPTINFO_HTEXT_SUMMARY     @"DEBUG: Display a summary of database activity by query kind."
#define OPTINFO_HTEXT_INITTEST    @"DEBUG: Create simulated test users for # number of users to create."
#define OPTINFO_HTEXT_DELALL      @"DEBUG: Delete all active users."

#define OPTINFO_HTEXT_VERSFMT     @"jabber_autobuddy v%d.%d.%d, (c) 2006-2008 Apple, Inc. All rights reserved."
#define OPTINFO_HTEXT_USAGE       @"jabber_autobuddy [options] <command> [command args] [ <command> [command args] ... ]"

#define OPTINFO_NOTEREF_JIDFMT     @"1"
#define OPTINFO_NOTEREF_XORDER     @"2"
#define OPTINFO_NOTEREF_DBONLY     @"3"
#define OPTINFO_NOTEREF_DUPCMDS    @"4"
#define OPTINFO_NOTEREF_SINGLEOPT  @"5"
#define OPTINFO_NOTEREF_SINGLECMD  @"6"
#define OPTINFO_NOTEREF_HELPONLY   @"7"
#define OPTINFO_NOTEREF_PREREQ     @"8"
#define OPTINFO_NOTEREF_DOMAINFMT  @"9"
#define OPTINFO_NOTEREF_USERPREFIX @"10"
#define OPTINFO_NOTEREF_DEBUGOPT   @"11"

// ENUMS

// action IDs for duplicate options or commands
enum DupActionCodes{
	DUP_ALLOW = 0,
	DUP_WARN,
	DUP_ERROR
};

//------------------------------------------------------------------------------
// JABActionInfo methods
//------------------------------------------------------------------------------

@implementation JABActionInfo

@synthesize inputCommands = _inputCommands;
@synthesize optionsInfo = _optionsInfo;
@synthesize actionList = _actionList;
@synthesize globalOptions = _globalOptions;
@synthesize logger = _logger;
@synthesize opResult = _opResult;

//------------------------------------------------------------------------------
+ (id) jabActionInfoWithCommandInput: (NSArray *) inCommands
{
	return [[[self alloc] initWithCommandInput: inCommands] autorelease];
}

//------------------------------------------------------------------------------
- (id) initWithCommandInput: (NSArray *) inCommands
{
	self = [super init];
	
	self.inputCommands = inCommands;
	
	[self initOptionsInfo];

	self.actionList = [NSMutableArray arrayWithCapacity: 0];
	self.globalOptions = [NSMutableDictionary dictionaryWithCapacity: 0];
	
	self.logger = [JABLogger jabLogger];

	return self;
}

//------------------------------------------------------------------------------
- (void) dealloc
{
	self.actionList = nil;
	self.inputCommands = nil;

	[super dealloc];
}

//------------------------------------------------------------------------------
- (void) initOptionsInfo
{
	// Initialize internal options/command metadata
	
	// After initialization, the optionsInfo dictionary contains several general 
	// help items and a variable number of option/command metadata and help 
	// note items based on the options and commands supported by the program.
	//
	// General items:
	//
	// OPTINFO_KEY_VERSIONTEXT Display message for program version.
	// OPTINFO_KEY_USAGETEXT   Summary usage text displayed prior to the main 
	//                         body of the help message.
	// OPTINFO_KEY_NOTESTEXT   A list of text items define the help "Notes"
	//                         displayed after the main body of the help message.
	//                         Keys defined for notes text are described below.
	//
	// Option/command items:
	//
	// In addition to the general items, optionsInfo contains one or more 
	// dictionary element for each option or command listed in the main body of 
	// the help message defines. Each element describes the rules, limits, help
	// text, command forms, etc for a single option or command.  The metadata 
	// element is repeating in the optionsInfo dictionary keyed under each 
	// form of the option or command (short and long). The keys for each
	// option/command metadata dictionary are as follows:
	// 
	// OPTINFO_KEY_OPCODE      Numeric identifier for the option or command.
	// 
	// OPTINFO_KEY_ACTIONCLASS String identifier for commands which defines the 
	//                         name of the JABAction sub-class that implements 
	//                         the action. For non-command program options, this 
	//                         string is blank.
	// 
	// OPTINFO_KEY_CMDFORMS    Array containing all input forms (short and long) 
	//                         of the program option or command.
	// 
	// OPTINFO_KEY_DUPACTION   Numeric ID that defines the action to take when a 
	//                         more than one occurrance of the option or command 
	//                         is scanned from the input command line.
	// 
	// OPTINFO_KEY_HELPTEXT    Array of help text lines which describe the 
	//                         meaning and use of the program option or command.
	// 
	// OPTINFO_KEY_OPARGS      [optional] Array of string identifiers used to key 
	//                         each command option when intializing the action 
	//                         handler for commands which require one or more 
	//                         value parameters. The order of the keys corresponds 
	//                         to the position of each paramter following the 
	//                         option or command on the command line.
	// 
	// OPTINFO_KEY_ARGSHELP    [optional] Array of strings used to represent the 
	//                         required arguments for the program option or 
	//                         command in the help text.
	// 
	// OPTINFO_KEY_NOTEREFS    [optional] Array of keys relating the program 
	//                         option or command to specific entries in the notes
	//                         text.
	// 
	// Help note items:
	//
	// The OPTINFO_KEY_NOTESTEXT element of optionsInfo contains one or more
	// dictionary entries to describe individual notes related to the options
	// or commands listed in the main body of the help text.  The keys for 
	// each help note metdata dictionary are as follows:
	// 
	// OPTINFO_KEY_NOTEREF     Unique string identifier associated with the note.
	//
	// OPTINFO_KEY_NOTELINES   Array of text lines to be displayed for the note.
	// 
	
	self.optionsInfo = [NSMutableDictionary dictionaryWithCapacity: 0];

	//
	// Initialize version info and general usage text
	//
	[_optionsInfo setObject: [NSString stringWithFormat: OPTINFO_HTEXT_VERSFMT, 
							  JAB_VERSION_MAJOR, JAB_VERSION_MINOR, JAB_VERSION_BUILD]
					 forKey: OPTINFO_KEY_VERSIONTEXT];
	[_optionsInfo setObject: OPTINFO_HTEXT_USAGE forKey: OPTINFO_KEY_USAGETEXT];
	
	//
	// Initialize options metadata for each defined option/command
	//
	[self initCommandInfoForOpCode: OPCODE_SHOWHELP actionClass: ACTIONCLASS_SHOWUSAGE
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_SHOWHELP,  CMDFORM_S_SHOWHELP2,  CMDFORM_L_SHOWHELP, nil]
						 dupAction: DUP_WARN
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_SHOWUSAGE, nil]
							opArgs: nil
						  argsHelp: nil
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_HELPONLY, nil]];
	
	[self initCommandInfoForOpCode: OPCODE_SHOWVERS actionClass: ACTIONCLASS_SHOWVERSION
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_SHOWVERS, CMDFORM_L_SHOWVERS, nil]
						 dupAction: DUP_WARN
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_SHOWVERSION, nil]
							opArgs: nil
						  argsHelp: nil
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_HELPONLY, nil]];
	
	[self initCommandInfoForOpCode: OPCODE_INITUSER actionClass: ACTIONCLASS_INITUSER
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_INITUSER, CMDFORM_L_INITUSER, nil]
						 dupAction: DUP_ALLOW
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_INITUSER, nil]
							opArgs: [NSArray arrayWithObjects: CMDOPT_KEY_JABBERID, nil]
						  argsHelp: [NSArray arrayWithObjects: OPTINFO_REQARG_JID, nil]
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_JIDFMT, OPTINFO_NOTEREF_XORDER, 
									 OPTINFO_NOTEREF_DBONLY, OPTINFO_NOTEREF_PREREQ, nil]];
	
	[self initCommandInfoForOpCode: OPCODE_DELUSER actionClass: ACTIONCLASS_DELETEUSER
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_DELETEUSER, CMDFORM_L_DELETEUSER, nil]
						 dupAction: DUP_ALLOW
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_DELUSER, nil]
							opArgs: [NSArray arrayWithObjects: CMDOPT_KEY_JABBERID, nil]
						  argsHelp: [NSArray arrayWithObjects: OPTINFO_REQARG_JID, nil]
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_JIDFMT, OPTINFO_NOTEREF_XORDER, 
									 OPTINFO_NOTEREF_DBONLY, OPTINFO_NOTEREF_PREREQ, nil]];
	
	[self initCommandInfoForOpCode: OPCODE_ADDBUDDY actionClass: ACTIONCLASS_ADDBUDDY
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_ADDBUDDY, CMDFORM_L_ADDBUDDY, nil]
						 dupAction: DUP_ALLOW
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_ADDBUDDY, nil]
							opArgs: [NSArray arrayWithObjects: CMDOPT_KEY_JABBERID, nil]
						  argsHelp: [NSArray arrayWithObjects: OPTINFO_REQARG_JID, nil]
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_JIDFMT, 
									 OPTINFO_NOTEREF_XORDER, OPTINFO_NOTEREF_PREREQ, nil]];
	
	[self initCommandInfoForOpCode: OPCODE_REMBUDDY actionClass: ACTIONCLASS_REMOVEBUDDY
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_REMOVEBUDDY, CMDFORM_L_REMOVEBUDDY, nil]
						 dupAction: DUP_ALLOW
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_REMBUDDY, nil]
							opArgs: [NSArray arrayWithObjects: CMDOPT_KEY_JABBERID, nil]
						  argsHelp: [NSArray arrayWithObjects: OPTINFO_REQARG_JID, nil]
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_JIDFMT, 
									 OPTINFO_NOTEREF_XORDER, OPTINFO_NOTEREF_PREREQ, nil]];
	
	[self initCommandInfoForOpCode: OPCODE_ALLBUDDIES actionClass: ACTIONCLASS_ALLBUDDIES
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_ALLBUDDIES, CMDFORM_L_ALLBUDDIES, nil]
						 dupAction: DUP_WARN
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_ALLBUDDIES, nil]
							opArgs: nil
						  argsHelp: nil
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_XORDER, 
									 OPTINFO_NOTEREF_DUPCMDS, OPTINFO_NOTEREF_SINGLECMD, nil]];
	
	[self initCommandInfoForOpCode: OPCODE_GRPBUDDIES actionClass: ACTIONCLASS_GROUPBUDDIES
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_GRPBUDDIES, CMDFORM_L_GRPBUDDIES, nil]
						 dupAction: DUP_WARN
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_GRPBUDDIES, nil]
							opArgs: [NSArray arrayWithObjects: CMDOPT_KEY_GROUPNAME, nil]
						  argsHelp: [NSArray arrayWithObjects: OPTINFO_REQARG_GROUP, nil]
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_XORDER, 
									 OPTINFO_NOTEREF_DUPCMDS, OPTINFO_NOTEREF_SINGLECMD, nil]];

	[self initCommandInfoForOpCode: OPCODE_GROUP_BY_GUID actionClass: ACTIONCLASS_GROUP_BY_GUID
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_GROUPBYGUID, CMDFORM_L_GROUPBYGUID, nil]
						 dupAction: DUP_WARN
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_GRPBYGUID, nil]
							opArgs: [NSArray arrayWithObjects: CMDOPT_KEY_GROUPGUID, nil]
						  argsHelp: [NSArray arrayWithObjects: OPTINFO_REQARG_GROUP_ID, nil]
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_XORDER, 
									 OPTINFO_NOTEREF_DUPCMDS, OPTINFO_NOTEREF_SINGLECMD, nil]];

	[self initCommandInfoForOpCode: OPCODE_UNGROUP actionClass: ACTIONCLASS_REMOVEGROUP
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_UNGROUP, CMDFORM_L_UNGROUP, nil]
						 dupAction: DUP_WARN
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_UNGROUP, nil]
							opArgs: [NSArray arrayWithObjects: CMDOPT_KEY_GROUPNAME, nil]
						  argsHelp: [NSArray arrayWithObjects: OPTINFO_REQARG_GROUP, nil]
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_XORDER, 
									 OPTINFO_NOTEREF_DUPCMDS, OPTINFO_NOTEREF_SINGLECMD, nil]];

	[self initCommandInfoForOpCode: OPCODE_UNGROUP_BY_GUID actionClass: ACTIONCLASS_UNGROUP_BY_GUID
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_UNGROUPGUID, CMDFORM_L_UNGROUPGUID, nil]
						 dupAction: DUP_WARN
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_UNGROUPGUID, nil]
							opArgs: [NSArray arrayWithObjects: CMDOPT_KEY_GROUPGUID, nil]
						  argsHelp: [NSArray arrayWithObjects: OPTINFO_REQARG_GROUP_ID, nil]
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_XORDER, 
									 OPTINFO_NOTEREF_DUPCMDS, OPTINFO_NOTEREF_SINGLECMD, nil]];
	
	[self initCommandInfoForOpCode: OPCODE_MOVEDOMAIN actionClass: ACTIONCLASS_MOVEDOMAIN
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_MOVEDOMAIN, CMDFORM_L_MOVEDOMAIN, nil]
						 dupAction: DUP_WARN
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_MOVEDOMAIN, nil]
							opArgs: [NSArray arrayWithObjects: CMDOPT_KEY_SRCDOMAIN, CMDOPT_KEY_DSTDOMAIN, nil]
						  argsHelp: [NSArray arrayWithObjects: OPTINFO_REQARG_SRCDOMAIN, OPTINFO_REQARG_DSTDOMAIN, nil]
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_DOMAINFMT, nil]];
	
	[self initCommandInfoForOpCode: OPCODE_OPT_VERBOSE actionClass: ACTIONCLASS_NONE
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_VERBOSE, CMDFORM_L_VERBOSE, nil]
						 dupAction: DUP_WARN
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_VERBOSE, nil]
							opArgs: nil
						  argsHelp: nil
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_SINGLEOPT, nil]];
	
#ifdef DEBUG
	[self initCommandInfoForOpCode: OPCODE_OPT_DBPATH actionClass: ACTIONCLASS_NONE
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_DBPATH, CMDFORM_L_DBPATH, nil]
						 dupAction: DUP_WARN
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_DBPATH1, OPTINFO_HTEXT_DBPATH2, nil]
							opArgs: [NSArray arrayWithObjects: CMDOPT_KEY_DBPATHVAL, nil]
						  argsHelp: [NSArray arrayWithObjects: OPTINFO_REQARG_DBPATH, nil]
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_DEBUGOPT, nil]];
	
	[self initCommandInfoForOpCode: OPCODE_OPT_SHOWSQL actionClass: ACTIONCLASS_NONE
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_SHOWSQL, CMDFORM_L_SHOWSQL, nil]
						 dupAction: DUP_WARN
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_SHOWSQL, nil]
							opArgs: nil
						  argsHelp: nil
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_DEBUGOPT, nil]];
	
	[self initCommandInfoForOpCode: OPCODE_OPT_NOWRITE actionClass: ACTIONCLASS_NONE
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_NOWRITE, CMDFORM_L_NOWRITE, nil]
						 dupAction: DUP_WARN
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_NOWRITE, nil]
							opArgs: nil
						  argsHelp: nil
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_DEBUGOPT, nil]];
	
	[self initCommandInfoForOpCode: OPCODE_OPT_SUMMARY actionClass: ACTIONCLASS_NONE
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_SUMMARY, CMDFORM_L_SUMMARY, nil]
						 dupAction: DUP_WARN
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_SUMMARY, nil]
							opArgs: nil
						  argsHelp: nil
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_DEBUGOPT, nil]];
	
	[self initCommandInfoForOpCode: OPCODE_INITTEST actionClass: ACTIONCLASS_INITTEST
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_INITTEST, CMDFORM_L_INITTEST, nil]
						 dupAction: DUP_ERROR
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_INITTEST, nil]
							opArgs: [NSArray arrayWithObjects: CMDOPT_KEY_TESTCOUNT, CMDOPT_KEY_TESTPREFIX, nil]
						  argsHelp: [NSArray arrayWithObjects: OPTINFO_REQARG_NUMVAL, OPTINFO_REQARG_PREFIX, nil]
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_USERPREFIX, OPTINFO_NOTEREF_DEBUGOPT, nil]];
	
	[self initCommandInfoForOpCode: OPCODE_DELALL actionClass: ACTIONCLASS_DELETEALL
					  commandForms: [NSArray arrayWithObjects: CMDFORM_S_DELALL, CMDFORM_L_DELALL, nil]
						 dupAction: DUP_WARN
						  helpText: [NSArray arrayWithObjects: OPTINFO_HTEXT_DELALL, nil]
							opArgs: [NSArray arrayWithObjects: CMDOPT_KEY_TESTPREFIX, nil]
						  argsHelp: [NSArray arrayWithObjects: OPTINFO_REQARG_PREFIX, nil]
						  noteRefs: [NSArray arrayWithObjects: OPTINFO_NOTEREF_USERPREFIX, OPTINFO_NOTEREF_DEBUGOPT, nil]];
#endif	
	
	//
	// Initialize help notes
	//
	[self initNotesText: [NSArray arrayWithObjects: 
						  @"JID is a Jabber ID in the form <username>@<hostname>, where:", 
						  @"  username   is the short name (record name) from any Open Directory",
						  @"             account in the server's directory search path.",
						  @"  hostname   is be a domain (or realm) that the local jabberd service ",
						  @"             is configured to host.",
						  nil]
				 forRef: OPTINFO_NOTEREF_JIDFMT];
	
	[self initNotesText: [NSArray arrayWithObjects: 
						  @"Commands which modify the jabberd database may be used multiple times",
						  @"in the same execution and in any order.  However, when used this way, the",
						  @"commands are evaluated in the following order:",
						  [NSString stringWithFormat: @"  * all %@ commands", CMDFORM_L_DELETEUSER],
						  [NSString stringWithFormat: @"  * all %@ commands", CMDFORM_L_INITUSER],
						  [NSString stringWithFormat: @"  * all %@ commands", CMDFORM_L_REMOVEBUDDY],
						  [NSString stringWithFormat: @"  * all %@ commands", CMDFORM_L_ADDBUDDY],
						  [NSString stringWithFormat: @"  * the %@, %@ or %@ command", 
						   CMDFORM_L_ALLBUDDIES, CMDFORM_L_GRPBUDDIES, CMDFORM_L_UNGROUP],
						  nil]
				 forRef: OPTINFO_NOTEREF_XORDER];
	
	[self initNotesText: [NSArray arrayWithObjects: 
						  [NSString stringWithFormat: 
						   @"The %@ and %@ commands only modify the local jabberd ", 
						   CMDFORM_L_INITUSER, CMDFORM_L_DELETEUSER],
						  @"database for the selected users.  These command do not affect user",
						  @"account data stored in the directory.",
						  nil]
				 forRef: OPTINFO_NOTEREF_DBONLY];
	
	[self initNotesText: [NSArray arrayWithObjects: 
						  [NSString stringWithFormat: 
						   @"The %@, %@ and %@ commands may not be used in the same execution.", 
						   CMDFORM_L_ALLBUDDIES, CMDFORM_L_GRPBUDDIES, CMDFORM_L_GROUPBYGUID, CMDFORM_L_UNGROUP, CMDFORM_L_UNGROUPGUID],
						  nil]
				 forRef: OPTINFO_NOTEREF_DUPCMDS];
	
	[self initNotesText: [NSArray arrayWithObjects: 
						  [NSString stringWithFormat: 
						   @"Multiple uses of the %@ option are treated as a single use of that option.", 
						   CMDFORM_L_VERBOSE ],
						  nil]
				 forRef: OPTINFO_NOTEREF_SINGLEOPT];
	
	[self initNotesText: [NSArray arrayWithObjects: 
						  [NSString stringWithFormat: 
						   @"Multiple uses of the %@, %@ or %@ commands are treated as a ", 
						   CMDFORM_L_ALLBUDDIES, CMDFORM_L_GRPBUDDIES, CMDFORM_L_GROUPBYGUID, CMDFORM_L_UNGROUP, CMDFORM_L_UNGROUPGUID],
						  @"single use of each command.",
						  nil]
				 forRef: OPTINFO_NOTEREF_SINGLECMD];
	
	[self initNotesText: [NSArray arrayWithObjects: 
						  [NSString stringWithFormat: 
						   @"Use of the %@ or %@ commands in combination with other commands will ", 
						   CMDFORM_L_SHOWHELP, CMDFORM_L_SHOWVERS],
						  @"only display the help or version text.  All other commands will be ignored for",
						  @"that execution.",
						  nil]
				 forRef: OPTINFO_NOTEREF_HELPONLY];
	
	[self initNotesText: [NSArray arrayWithObjects: 
						  [NSString stringWithFormat: 
						   @"The %@ command is a prerequisite for the  %@, %@", 
						   CMDFORM_L_INITUSER, CMDFORM_L_DELETEUSER, CMDFORM_L_ADDBUDDY],
						  [NSString stringWithFormat: @"and %@ commands.", CMDFORM_L_REMOVEBUDDY],
						  nil]
				 forRef: OPTINFO_NOTEREF_PREREQ];
	
	[self initNotesText: [NSArray arrayWithObjects: 
						  @"The format for domains is the same as for host names ",
						  @"(ex: example.com, chat.example.com, etc).",
						  nil]
				 forRef: OPTINFO_NOTEREF_DOMAINFMT];
	
#ifdef DEBUG
	[self initNotesText: [NSArray arrayWithObjects: 
						  @"Commands that take a prefix argument restrict the operation to JIDs ",
						  [NSString stringWithFormat: 
						   @"matching the prefix string.  A value of '%@' is used if no particular ",
						   OPTINFO_PREFIX_USEDEFAULT ],
						  @"prefix is desired.  For example:",
						  [NSString stringWithFormat: @"  %@ 10 test", CMDFORM_L_INITTEST],
						  @"      initializes 10 users in the form 'test1.example.com', etc.",
						  [NSString stringWithFormat: @"  %@ 10 %@", CMDFORM_L_INITTEST, OPTINFO_PREFIX_USEDEFAULT],
						  [NSString stringWithFormat: 
						   @"      initializes 10 users with the default prefix (ex: '%@1.example.com').", 
						   TESTUSER_DEFAULTPREFIX ],
						  [NSString stringWithFormat: @"  %@ test", CMDFORM_L_DELALL],
						  @"      deletes active users matching the prefix 'test'.",
						  [NSString stringWithFormat: @"  %@ %@", CMDFORM_L_DELALL, OPTINFO_PREFIX_USEDEFAULT],
						  @"      deletes all active users in the database.",
						  nil]
				 forRef: OPTINFO_NOTEREF_USERPREFIX];

	[self initNotesText: [NSArray arrayWithObjects: 
						  @"Commands or options designated 'DEBUG:' are only available",
						  @"in the DEBUG build of the program.",
						  nil]
				 forRef: OPTINFO_NOTEREF_DEBUGOPT];
#endif
}

//------------------------------------------------------------------------------
- (void) initCommandInfoForOpCode: (NSInteger) opCode actionClass: (NSString *) actionClass
					 commandForms: (NSArray *) commandForms dupAction: (NSInteger) dupAction
						 helpText: (NSArray *) helpText opArgs: (NSArray *) opArgs 
						 argsHelp: (NSArray *) argsHelp noteRefs: (NSArray *) noteRefs
{
	NSMutableDictionary *cmdInfo = [NSMutableDictionary dictionaryWithCapacity: 0]; 
	[cmdInfo setObject: [NSNumber numberWithInteger: opCode] forKey: OPTINFO_KEY_OPCODE];
	[cmdInfo setObject: actionClass forKey: OPTINFO_KEY_ACTIONCLASS];
	[cmdInfo setObject: commandForms forKey: OPTINFO_KEY_CMDFORMS];
	[cmdInfo setObject: [NSNumber numberWithInteger: dupAction] forKey: OPTINFO_KEY_DUPACTION];
	[cmdInfo setObject: helpText forKey: OPTINFO_KEY_HELPTEXT];
	if (nil != opArgs)
		[cmdInfo setObject: opArgs forKey: OPTINFO_KEY_OPARGS];
	if (nil != argsHelp)
		[cmdInfo setObject: argsHelp forKey: OPTINFO_KEY_ARGSHELP];
	if (nil != noteRefs)
		[cmdInfo setObject: noteRefs forKey: OPTINFO_KEY_NOTEREFS];
	NSDictionary *commandInfo = [NSDictionary dictionaryWithDictionary: cmdInfo];
	
	for (NSString *cmdForm in commandForms)
		[_optionsInfo setObject: commandInfo forKey: cmdForm];
}

//------------------------------------------------------------------------------
- (void) initNotesText: (NSArray *) noteLines forRef: (NSString *) noteRef
{
	NSMutableArray *notesText = [_optionsInfo objectForKey: OPTINFO_KEY_NOTESTEXT];
	if (nil == notesText) {
		notesText = [NSMutableArray arrayWithCapacity: 0];
		[_optionsInfo setObject: notesText forKey: OPTINFO_KEY_NOTESTEXT];
	}

	NSDictionary *noteItem = [NSDictionary dictionaryWithObjectsAndKeys: 
							  noteRef, OPTINFO_KEY_NOTEREF,
							  noteLines, OPTINFO_KEY_NOTELINES,
							  nil];
	[notesText addObject: noteItem];
}

//------------------------------------------------------------------------------
- (BOOL) processCommandInput
{
	// Scan the input arguments and create one or more cmdOpts entries, 
	// according to the parameters defined for this command in optsInfo.

	BOOL bOk = YES;
	
	NSEnumerator *argsEnum = [self.inputCommands objectEnumerator];
	[argsEnum nextObject]; // skip executable name

	NSString *anArg = nil;
	while (nil != (anArg = [argsEnum nextObject])) {
		
		// Find the command metadata for the input argument
		NSDictionary *optInfo = [self.optionsInfo objectForKey: anArg];
		if (nil == optInfo) {
			bOk = [self showInvalidArgError: anArg];
			break;
		}
		
		// Determine if this is an duplicate and what action to take (if any)
		NSInteger opCode = [[optInfo objectForKey: OPTINFO_KEY_OPCODE] integerValue];
		if (nil != [self findActionItemsForOpCode: opCode]) {
			// process duplicate entry
			NSInteger dupAction = [[optInfo objectForKey: OPTINFO_KEY_DUPACTION] integerValue];
			bOk = [self handleDuplicateArg: anArg withAction: dupAction];
			if (!bOk) break;
		}
		
		// Copy optsInfo data to new cmdOpts dict, adding required values as needed
		// NOTE: At this point, the globalOptions flags may not have been parsed, so we'll 
		//       add those items to the command options later when the action is performed
		NSMutableDictionary *cmdOpts = [NSMutableDictionary dictionaryWithCapacity: 0];
		[cmdOpts setObject: self forKey: CMDOPT_KEY_ACTIONINFO];
		[cmdOpts setObject: [optInfo objectForKey: OPTINFO_KEY_OPCODE] forKey: CMDOPT_KEY_OPCODE];
		[cmdOpts setObject: [optInfo objectForKey: OPTINFO_KEY_ACTIONCLASS] forKey: CMDOPT_KEY_ACTIONCLASS];
		NSArray *cmdArgs = [optInfo objectForKey: OPTINFO_KEY_OPARGS];
		for (NSString *argKey in cmdArgs) {
			// Get the cmdOpts key for the expected argument value
			NSString *argVal = [argsEnum nextObject];
			if (nil == argVal) {
				bOk = [self showMissingOptionValue: anArg argKey: argKey];
				break;
			}
			// Save the argument value in cmdOpts
			[cmdOpts setObject: argVal forKey: argKey];
		}
		if (!bOk) break;

		// Add item to master list of parsed commands
		// Note that some "actions" in the list represent global options.
		// These options will be merged with the command-specific options
		// when the command actions are performed.
		[self.actionList addObject: [NSDictionary dictionaryWithDictionary: cmdOpts]];
	}
	
	if (bOk && (1 > [self.actionList count]))
		bOk = [self showMissingArgsError];

	if (!bOk)
		[self showUsage];
	
	return bOk;
}

//------------------------------------------------------------------------------
- (NSInteger) performRequestedActions
{
	// Perform all actions in the list according to priority of execution

	// Showing help and/or version disables all other options, so check for this
	BOOL bShowHelp = (nil != [self findActionItemsForOpCode: OPCODE_SHOWHELP]);
	
	// Execute all database commands from the list.  To prevent confusion, 
	// all database commands are executed in order of precendence.
	do { // not a loop

		// display version info first if requested
		if (OPRESULT_OK != [self performActionsForOpCode: OPCODE_SHOWVERS]) break;

		// help and/or version commands execute then exits
		if (OPRESULT_OK != [self performActionsForOpCode: OPCODE_SHOWHELP]) break;
		if (bShowHelp) break; // done

#ifdef DEBUG
		// special commands for unit testing...
		if (OPRESULT_OK != [self performActionsForOpCode: OPCODE_DELALL]) break;
		if (OPRESULT_OK != [self performActionsForOpCode: OPCODE_INITTEST]) break;
#endif		

		// execute the remaining commands in order or precedence
		if (OPRESULT_OK != [self performActionsForOpCode: OPCODE_DELUSER]) break;
		if (OPRESULT_OK != [self performActionsForOpCode: OPCODE_INITUSER]) break;
		if (OPRESULT_OK != [self performActionsForOpCode: OPCODE_REMBUDDY]) break;
		if (OPRESULT_OK != [self performActionsForOpCode: OPCODE_ADDBUDDY]) break;
		if (OPRESULT_OK != [self performActionsForOpCode: OPCODE_ALLBUDDIES]) break;
		if (OPRESULT_OK != [self performActionsForOpCode: OPCODE_GRPBUDDIES]) break;
		if (OPRESULT_OK != [self performActionsForOpCode: OPCODE_GROUP_BY_GUID]) break;
		if (OPRESULT_OK != [self performActionsForOpCode: OPCODE_UNGROUP]) break;
		if (OPRESULT_OK != [self performActionsForOpCode: OPCODE_UNGROUP_BY_GUID]) break;
		if (OPRESULT_OK != [self performActionsForOpCode: OPCODE_MOVEDOMAIN]) break;
	
	} while (0); // not a loop

	return self.opResult;
}

//------------------------------------------------------------------------------
- (NSInteger) performActionsForOpCode: (NSInteger) opCode;
{
	// Exectute all actions from the list that match the selected opCode.  
	// If an error occurs in any command, execution is terminated and the 
	// error result returned.
	
	NSArray *matchList = [self findActionItemsForOpCode: opCode];
	if (nil == matchList) return OPRESULT_OK;

	self.opResult = OPRESULT_OK;
	
	for (NSDictionary *cmdOpts in matchList) {
		[self performActionForCmdOpts: cmdOpts];
		if (OPRESULT_OK != self.opResult)
			break;  // command execution error -- halt processing
	} // for.. in
	
	return self.opResult;
}

//------------------------------------------------------------------------------
- (NSInteger) performActionForCmdOpts: (NSDictionary *) cmdOpts;
{
	// Merge global settings into the action-specific options
	NSDictionary *actionOpts = [self actionOptsForCommandOpts: cmdOpts];
	
	// Create an action for the command and exectute
	JABAction *aCmd = [JABAction jabActionWithCommandOptions: actionOpts];
	[aCmd doCommand];
	self.opResult = [aCmd result];

	return self.opResult;
}

//------------------------------------------------------------------------------
- (NSDictionary *) actionOptsForCommandOpts: (NSDictionary *) cmdOpts;
{
	// Create the final "action" options by merging all global options
	// into the action-specific command options.

	NSMutableDictionary *actionOpts = [cmdOpts mutableCopy];

	if (nil != [self findActionItemsForOpCode: OPCODE_OPT_VERBOSE])
		[actionOpts setObject: [NSNumber numberWithInteger: 1] forKey: CMDOPT_KEY_VERBOSE];

#ifdef DEBUG
	if (nil != [self findActionItemsForOpCode: OPCODE_OPT_NOWRITE])
		[actionOpts setObject: [NSNumber numberWithInteger: 1] forKey: CMDOPT_KEY_DBNOWRITEFLAG];
	
	if (nil != [self findActionItemsForOpCode: OPCODE_OPT_SHOWSQL])
		[actionOpts setObject: [NSNumber numberWithInteger: 1] forKey: CMDOPT_KEY_SHOWSQLFLAG];
	
	if (nil != [self findActionItemsForOpCode: OPCODE_OPT_SUMMARY])
		[actionOpts setObject: [NSNumber numberWithInteger: 1] forKey: CMDOPT_KEY_SUMMARYFLAG];
	
	NSArray *items = [self findActionItemsForOpCode: OPCODE_OPT_DBPATH];
	if (nil != items) {
		NSDictionary *anOpt = [items objectAtIndex: 0];
		if (nil != anOpt)
			[actionOpts setObject: [anOpt objectForKey: CMDOPT_KEY_DBPATHVAL] forKey: CMDOPT_KEY_DBPATHVAL];
	}
#endif
	
	return actionOpts;
}

//------------------------------------------------------------------------------
- (NSDictionary *) findOptionInfoForOpCode: (NSInteger) opCode
{
	NSDictionary *anItem = nil;
	
	// Search the options info data for the entry matching opCode
	for (NSString *itemKey in [self.optionsInfo allKeys]) {
		NSObject *anObj = [self.optionsInfo objectForKey: itemKey];
		if (![anObj isKindOfClass: [NSDictionary class]])
			continue; // ignore non-dict entries
		NSDictionary *aDict = (NSDictionary *) anObj;
		NSInteger aCode = [[aDict objectForKey: OPTINFO_KEY_OPCODE] integerValue];
		if (aCode == opCode) {
			anItem = aDict;
			break;
		} // if
	} // for..in
	
	return anItem;
}

//------------------------------------------------------------------------------
- (NSArray *) findActionItemsForOpCode: (NSInteger) opCode
{
	// Return a list of all action items that match the selected opCode
	NSMutableArray *matchItems = [NSMutableArray arrayWithCapacity: 0];
	
	for (NSDictionary *cmdOpts in self.actionList) {
		NSInteger aCode = [[cmdOpts objectForKey: CMDOPT_KEY_OPCODE] integerValue];
		if (opCode == aCode) {
			if (nil == matchItems)
				matchItems = [NSMutableArray arrayWithCapacity: 0];
			[matchItems addObject: cmdOpts];
		} // if opCode
	} // for..in
	
	return (0 < [matchItems count]) ? matchItems : nil;
}

//------------------------------------------------------------------------------
- (BOOL) checkActionConflicts
{
	BOOL bConflict = NO;

	do { // not a loop
		bConflict = [self checkActionConflictForOpCode: OPCODE_ALLBUDDIES 
											andOpCode: OPCODE_GRPBUDDIES];
		if (bConflict) break;
		bConflict = [self checkActionConflictForOpCode: OPCODE_ALLBUDDIES 
											andOpCode: OPCODE_UNGROUP];
		if (bConflict) break;
		bConflict = [self checkActionConflictForOpCode: OPCODE_GRPBUDDIES 
											andOpCode: OPCODE_UNGROUP];
		if (bConflict) break;
		bConflict = [self checkActionConflictForOpCode: OPCODE_ALLBUDDIES 
											andOpCode: OPCODE_UNGROUP_BY_GUID];
		if (bConflict) break;
		bConflict = [self checkActionConflictForOpCode: OPCODE_ALLBUDDIES 
											andOpCode: OPCODE_GROUP_BY_GUID];
		if (bConflict) break;
		bConflict = [self checkActionConflictForOpCode: OPCODE_GROUP_BY_GUID 
											andOpCode: OPCODE_UNGROUP_BY_GUID];
		if (bConflict) break;
		bConflict = [self checkActionConflictForOpCode: OPCODE_GRPBUDDIES 
											andOpCode: OPCODE_UNGROUP_BY_GUID];
		if (bConflict) break;
		bConflict = [self checkActionConflictForOpCode: OPCODE_UNGROUP 
											andOpCode: OPCODE_GROUP_BY_GUID];
	} while (0); // not a loop

	return bConflict;
}

//------------------------------------------------------------------------------
- (BOOL) checkActionConflictForOpCode: (NSInteger) oc1 andOpCode: (NSInteger) oc2
{
	// Check for conflicting actions scheduled for the selected op codes

	NSDictionary *a1 = [[self findActionItemsForOpCode: oc1] objectAtIndex: 0]; 
	NSDictionary *a2 = [[self findActionItemsForOpCode: oc2] objectAtIndex: 0]; 

	BOOL bConflict = ((nil != a1) && (nil != a2));
	if (bConflict) {
		[self showCommandConflictError: [NSArray arrayWithObjects: a1, a2, nil]
								reason: @"may not be used in the same execution"];
		[self showUsage];
	}
	
	return bConflict;
}

//------------------------------------------------------------------------------
- (void) showUsage
{
	// Generate a one-off help command to display after error messages
	NSDictionary *cmdOpts = 
		[NSDictionary dictionaryWithObjectsAndKeys: 
		 self, CMDOPT_KEY_ACTIONINFO,
		 [NSNumber numberWithInteger: OPCODE_SHOWHELP], CMDOPT_KEY_OPCODE,
		 ACTIONCLASS_SHOWUSAGE, CMDOPT_KEY_ACTIONCLASS,
		 nil];
	[self performActionForCmdOpts: cmdOpts];
}

//------------------------------------------------------------------------------
- (BOOL) handleDuplicateArg: (NSString *) inArg withAction: (NSInteger) dupAction
{
	BOOL bOk = YES;
	
	NSString *fmt = nil;
	switch (dupAction) {
		case DUP_ERROR: 
			fmt = @"Error: command option (%s) may be used only once per invocation.\n\n";
			bOk = NO;
			break;
		case DUP_WARN: // display warning, but allow duplicate to be processed
			fmt = @"Warning: duplicate argument (%s) ignored.\n\n";
			break;
		default: break; // DUP_ALLOW
	}
	if (nil != fmt)
		[_logger logStdErrMessage: [NSString stringWithFormat: fmt, inArg]];

	return bOk;
}

//------------------------------------------------------------------------------
- (BOOL) showCommandConflictError: (NSArray *) actionItems reason: (NSString *) reason
{
	// report conflicting command
	
	// Get the short command forms for each command
	NSDictionary *action1 = [actionItems objectAtIndex: 0];
	NSString *cf1 = [[action1 objectForKey: OPTINFO_KEY_CMDFORMS] objectAtIndex: 0];
	
	NSDictionary *action2 = [actionItems objectAtIndex: 1];
	NSString *cf2 = [[action2 objectForKey: OPTINFO_KEY_CMDFORMS] objectAtIndex: 0];
	
	NSString *errMsg = [NSString stringWithFormat: 
						@"Error: conflicting commands: (\"%@\", \"%@\"): %@.\n\n", 
						cf1, cf2, reason];
	[_logger logStdErrMessage: errMsg];
	return NO;
}

//------------------------------------------------------------------------------
- (BOOL) showInvalidArgError: (NSString *) inArg
{
	// report unexpected command line argument 
	[_logger logStdErrMessage: [NSString stringWithFormat: 
								@"Error: Invalid command line argument \"%@\".\n\n", 
								inArg]];

	return NO;
}

//------------------------------------------------------------------------------
- (BOOL) showMissingOptionValue: (NSString *) inArg argKey: (NSString *) argKey
{
	// report missing option value required by the command
	[_logger logStdErrMessage: [NSString stringWithFormat:
								@"Error: Missing value for option %@ (%@).\n\n", 
								inArg, argKey]];

	return NO;
}
//------------------------------------------------------------------------------
- (BOOL) showMissingArgsError
{
	// report lack of input arguments for the program
	[_logger logStdErrMessage: @"Error: command invoked without arguments.\n\n"];

	return NO;
}

@end
