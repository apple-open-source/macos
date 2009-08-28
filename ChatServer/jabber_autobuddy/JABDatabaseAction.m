//
//  JABDatabaseAction.m
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/16/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABDatabaseAction.h"

#import "JABDatabase.h"
#import "JABLogger.h"

#import "errno.h"

// CONSTANTS

#define ROUTER_PID_FILE @"/var/run/jabberd/router.pid"

// TYPEDEFS & ENUMS

typedef struct {
	NSInteger f_iType;
	const char *f_szLabel;
} DBActivityLabel;

// GLOBAL DATA

const DBActivityLabel DB_ACTIVITY_LABELS[] = {
	{DBActivityActiveRowSelect, "SELECT ACTIVE"},
	{DBActivityActiveRowInsert, "INSERT ACTIVE"},
	{DBActivityVcardRowInsert, "INSERT VCARD"},
	{DBActivityRosterItemsRowSelect, "SELECT ROSTER-ITEMS"},
	{DBActivityRosterItemsRowInsert, "INSERT ROSTER-ITEMS"},
	{DBActivityRosterItemsRowDelete, "DELETE ROSTER-ITEMS"},
	{DBActivityRosterGroupsRowSelect, "SELECT ROSTER-GROUPS"},
	{DBActivityRosterGroupsRowInsert, "INSERT ROSTER-GROUPS"},
	{DBActivityRosterGroupsRowDelete, "DELETE ROSTER-GROUPS"},
	{DBActivityMatchedRowUpdate, "UPDATE MATCHED ROWS"},
	{DBActivityUnknown, NULL} // END OF TABLE
};

//------------------------------------------------------------------------------
// JABDatabaseAction methods
//------------------------------------------------------------------------------
@implementation JABDatabaseAction

@synthesize database = _database;
#ifdef DEBUG
@synthesize dbNoWriteFlag = _dbNoWriteFlag;
@synthesize dbShowSQLFlag = _dbShowSQLFlag;
@synthesize dbSummaryFlag = _dbSummaryFlag;
@synthesize activityStats = _activityStats;
#endif

//------------------------------------------------------------------------------
- (id) initWithCommandOptions: (NSDictionary *) cmdOpts
{
	self = [super initWithCommandOptions: cmdOpts];
	

#ifdef DEBUG
	// set up the options for this command
	self.dbNoWriteFlag = [[cmdOpts objectForKey: CMDOPT_KEY_DBNOWRITEFLAG] integerValue];
	self.dbShowSQLFlag = [[cmdOpts objectForKey: CMDOPT_KEY_SHOWSQLFLAG] integerValue];
	self.dbSummaryFlag = [[cmdOpts objectForKey: CMDOPT_KEY_SUMMARYFLAG] integerValue];

	// initialize activity stats
	self.activityStats = malloc(sizeof(NSInteger) * DBActivityNumTypes);
	NSInteger iType = DBActivityActiveRowSelect;
	for ( ; DBActivityNumTypes > iType; iType++)
		_activityStats[iType] = 0;	
#endif
	
	self.database = [JABDatabase jabDatabaseWithOptions: cmdOpts 
											  forAction: self];

	return self;
}

//------------------------------------------------------------------------------
- (void) dealloc
{
#ifdef DEBUG
	if (NULL != _activityStats)
		free(_activityStats);
#endif
	
	self.database = nil;
	
	[super dealloc];
}

#pragma mark -
//------------------------------------------------------------------------------
- (void) doAction
{
	// Verify all required parameters are initialized
	if ([self requiresJid] && (nil == _targetJid)) {
		[self.logger logMsgWithLevel: LOG_ERR format: @"Error: missing target JID - %s", __PRETTY_FUNCTION__];
		self.result = OPRESULT_INVALARGS;
		return; // cannot execute without required arguments
	}
	
	// Open the database for operations
	[_database openDatabase];
	if (SQLITE_OK != [_database dbResult]) {
		self.result = OPRESULT_FAILED;
		return; // operation failed
	}

	// perform the custom database action
	[self doDBAction];

	// clean up
	[_database closeDatabase];

#ifdef DEBUG
	// display database activity summary
	if ((OPRESULT_OK == _result) && (0 != _dbSummaryFlag)) {
		NSMutableArray *outLines = [NSMutableArray arrayWithCapacity: 0];
		[outLines addObject: @"-------------------------------------"];
		[outLines addObject: @"Database activity summary:"];
		 NSInteger iCount = 0;
		 for ( ; ; iCount++) {
			 NSInteger iType = DB_ACTIVITY_LABELS[iCount].f_iType;
			 if (DBActivityUnknown == iType) break; // end of table
			 [outLines addObject: [NSString stringWithFormat:
								   @" %-20s : %ld", 
								   DB_ACTIVITY_LABELS[iCount].f_szLabel, 
								   _activityStats[iType]]];
		 } // for
		[outLines addObject: @"-------------------------------------"];
		for (NSString *line in outLines)
			[_logger logStdErrMessage: [NSString stringWithFormat: @"%@\n", line]];
	} // if dbSummaryFlag
#endif
}

//------------------------------------------------------------------------------
- (BOOL) requiresJid
{
	// Indicate whether the action requires the targetJid to be set
	// base class provides default answer, must be overridden in derived
	// classes for specific requirements for the action.
	return YES; // most actions require this
}

//------------------------------------------------------------------------------
- (void) doDBAction
{
	// base class has no default action - must be implemented in derived classes
}

//------------------------------------------------------------------------------
- (BOOL) checkDatabaseStatus
{
	self.result = (SQLITE_ERROR != [_database dbResult]) ?  OPRESULT_OK : OPRESULT_FAILED;
	return (OPRESULT_OK == _result);
}

//------------------------------------------------------------------------------
- (BOOL) isJabberdRunning
{
	// Check whether the jabberd database process(es) are running.  Since there
	// is no "jabberd" process in Jabber 2.x, we'll verify the service status
	// using the "router" process ID.

	NSFileManager *fm = [NSFileManager defaultManager];
	
	// get active process ID from pid file
	if (![fm fileExistsAtPath: ROUTER_PID_FILE])
		return NO;
	
	NSString *aStr = [NSString stringWithContentsOfFile: ROUTER_PID_FILE
											   encoding: NSASCIIStringEncoding
												  error: nil];
	if ((nil == aStr) || (1 > [aStr length]))
		return NO;
	
	BOOL jabberRunning = NO;
	NSArray *stringParts = [aStr componentsSeparatedByCharactersInSet: 
							[NSCharacterSet whitespaceCharacterSet]];
	for (NSString *aPart in stringParts) {
		if (1 > [aPart length]) continue; // ignore blank parts
		NSInteger routerPid = [aPart integerValue];
		if (1 > routerPid) continue; // ignore non-number parts
		int status = kill(routerPid, 0); // test process running
		switch (status) {
			case -1:
				if (ESRCH != errno) {
					[_logger logStdErrMessage: 
					 [NSString stringWithFormat: 
					  @"Warning -- unable to determine jabberd process status; err=%d: %s\n",
					  errno, strerror(errno)]];
					jabberRunning = YES;
				}
				break;
			default:
				jabberRunning = YES;
		}
		break;
	}
	
	return jabberRunning;
}

#pragma mark -
#ifdef DEBUG
//------------------------------------------------------------------------------
- (void) recordActivity: (NSInteger) iActivityType
{
	if ((DBActivityActiveRowSelect > iActivityType) || (DBActivityNumTypes <= iActivityType))
		return; // index out of range
	_activityStats[iActivityType] += 1;
}
#endif

#pragma mark -
//------------------------------------------------------------------------------
- (void) logNoActiveItemsWarningForSource: (const char *) source line: (int) line
{
	if (1 == _verboseFlag)
		[self.logger logMsgWithLevel: LOG_NOTICE 
		                      format: @"Notice: Did not find any collection-owner items in active table"];
}

//------------------------------------------------------------------------------
- (void) logDuplicateJidWarning: (NSString *) aJid
{
	if (1 == _verboseFlag)
		[self.logger logMsgWithLevel: LOG_WARNING 
		                      format: @"Warning: Initialization skipped. Reason: JID already exists in active table: \"%@\"", 
		                              aJid];
}

//------------------------------------------------------------------------------
- (void) logJidLengthWarning: (NSString *) aJid
{
	if (1 == _verboseFlag)
		[self.logger logMsgWithLevel: LOG_WARNING 
		                      format: @"Warning: Operation skipped. Reason: JID exceeds maximum valid size: \"%@\"", 
						              aJid];
}

//------------------------------------------------------------------------------
- (void) logJidNotFoundError: (NSString *) aJid
{
	[self.logger logMsgWithLevel: LOG_ERR 
						  format: @"Error: Could not find input JID (\"%@\") in database.  Use -i argument to initialize user.", 
	                              aJid];
}

//------------------------------------------------------------------------------
- (void) logMalformedJidError: (NSString *) aJid
{
	[self.logger logMsgWithLevel: LOG_ERR 
	                      format: @"Error: Operation cannot be performed with malformed JID: %@.", 
	                              aJid];
}

//------------------------------------------------------------------------------
- (void) logSqlPrepareErrorForSource: (const char *) source line: (int) line
{
	NSString *dbErrMsg = [_database getDatabaseErrorMessage];
	[self.logger logMsgWithLevel: LOG_ERR 
						  format: @"Error: (sqlite3_prepare_v2): %@ - %s:%d", 
	                              dbErrMsg, source, line];
}

//------------------------------------------------------------------------------
- (void) logUnknownQueryStatusErrorForSource: (const char *) source line: (int) line
{
	NSString *dbErrMsg = [_database getDatabaseErrorMessage];
	[self.logger logMsgWithLevel: LOG_ERR 
	                      format: @"Error: Unknown status of query: %@ - %s:%d", 
	                              dbErrMsg, source, line];
}

//------------------------------------------------------------------------------
- (void) logDirectoryOpenFailedErrorForSource: (const char *) source line: (int) line
{
	[self.logger logMsgWithLevel: LOG_ERR 
	                      format: @"Error: Failed to open directory for search - %s:%d", 
	                              source, line];
}

@end
