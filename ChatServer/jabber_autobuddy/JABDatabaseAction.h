//
//  JABDatabaseAction.h
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/16/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABAction.h"

#import "JABDatabase.h"
#import "JABLogger.h"

enum {
	DBActivityUnknown = -1,
	DBActivityActiveRowSelect = 0,
	DBActivityActiveRowInsert,
	DBActivityVcardRowInsert,
	DBActivityRosterItemsRowSelect,
	DBActivityRosterItemsRowInsert,
	DBActivityRosterItemsRowDelete,
	DBActivityRosterGroupsRowSelect,
	DBActivityRosterGroupsRowInsert,
	DBActivityRosterGroupsRowDelete,
	DBActivityMatchedRowUpdate,
	DBActivityNumTypes // must be last item in enum
} DatabaseActivityType;

//------------------------------------------------------------------------------
// JABDatabaseAction base class
//------------------------------------------------------------------------------
@interface JABDatabaseAction : JABAction {
	
	JABDatabase *_database; // instance of database for this action

#ifdef DEBUG
	NSInteger _dbNoWriteFlag; // allow/supress queries that modify the database
	NSInteger _dbShowSQLFlag; // enable display of generated SQL queries
	NSInteger _dbSummaryFlag; // enable query statistics gathering/display
	NSInteger *_activityStats; // cache for database activity stats
#endif
}
@property(retain, readwrite) JABDatabase *database;
#ifdef DEBUG
@property(assign, readwrite) NSInteger dbNoWriteFlag;
@property(assign, readwrite) NSInteger dbShowSQLFlag;
@property(assign, readwrite) NSInteger dbSummaryFlag;
@property(assign, readwrite) NSInteger *activityStats;
#endif

- (id) initWithCommandOptions: (NSDictionary *) cmdOpts;
- (void) dealloc;

- (void) doAction;
- (BOOL) requiresJid;
- (void) doDBAction;
- (BOOL) checkDatabaseStatus;

- (BOOL) isJabberdRunning;

#ifdef DEBUG
- (void) recordActivity: (NSInteger) iActivityType;
#endif

- (void) logDuplicateJidWarning: (NSString *) aJid;
- (void) logJidLengthWarning: (NSString *) aJid;
- (void) logJidNotFoundError: (NSString *) aJid;
- (void) logMalformedJidError: (NSString *) aJid;

- (void) logSqlPrepareErrorForSource: (const char *) source line: (int) line;
- (void) logUnknownQueryStatusErrorForSource: (const char *) source line: (int) line;
- (void) logDirectoryOpenFailedErrorForSource: (const char *) source line: (int) line;

@end
