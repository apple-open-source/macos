//
//  JABDatabase.m
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 8/8/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABDatabase.h"

#import "JABDatabaseQuery.h"
#import "JABLogger.h"

// CONSTANTS

#define JABBER_DB_PATH_DEFAULT @"/private/var/jabberd/sqlite/jabberd2.db"
#define JABBER_DB_FILENAME_DEFAULT @"jabberd2.db"
#define SRVRMGR_JABBER_PREFS_PATH @"/Library/Preferences/com.apple.ichatserver.plist"
#define SMJ_SETTINGSKEY_JABBERD_DB_PATH @"jabberdDatabasePath"

#define QRY_SEL_OWNER_FROM_ACTIVE @"select \"collection-owner\" from active where \"collection-owner\" = \"%@\""
#define QRY_INS_OWNER_INTO_ACTIVE @"insert into active (\"collection-owner\", \"time\") values (\"%@\", \"%d\")"

#define QRY_INS_OWNER_INTO_VCARD @"insert into vcard (\"collection-owner\", \"fn\", \"nickname\", \"url\", \"tel\", \"email\", \"jabberid\", \"mailer\", \"title\",\"role\", \"bday\", \"tz\", \"n-family\", \"n-given\", \"n-middle\", \"n-prefix\", \"n-suffix\", \"adr-street\", \"adr-extadd\", \"adr-pobox\", \"adr-locality\", \"adr-region\", \"adr-pcode\", \"adr-country\", \"geo-lat\", \"geo-lon\", \"org-orgname\", \"org-orgunit\", \"agent-extval\", \"sort-string\", \"desc\", \"note\", \"uid\", \"photo-type\", \"photo-binval\", \"photo-extval\", \"logo-type\", \"logo-binval\", \"logo-extval\", \"sound-phonetic\", \"sound-binval\", \"sound-extval\", \"key-type\", \"key-cred\", \"rev\") values (\"%@\", NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL)"

#define QRY_SEL_OWNER_FROM_ROSTER_ITEMS_MATCHING_OWNER_AND_JID @"select \"collection-owner\" from \"roster-items\" where \"collection-owner\" = \"%@\" and \"jid\" = \"%@\""
#define QRY_INS_OWNER_INTO_ROSTER_ITEMS @"insert into \"roster-items\" (\"collection-owner\", \"jid\", \"name\", \"to\", \"from\", \"ask\") values (\"%@\", \"%@\", \"\", \"1\", \"1\", \"0\")"
#define QRY_DEL_OWNER_FROM_ROSTER_ITEMS_MATCHING_OWNER_AND_JID @"delete from \"roster-items\" where \"collection-owner\" = \"%@\" and \"jid\" = \"%@\""

#define QRY_SEL_OWNER_FROM_ROSTER_GROUPS_MATCHING_OWNER_JID_GROUP @"select \"collection-owner\" from \"roster-groups\" where \"collection-owner\" = \"%@\" and \"jid\" = \"%@\" and \"group\" = \"%@\""
#define QRY_INS_OWNER_INTO_ROSTER_GROUPS @"insert into \"roster-groups\" (\"collection-owner\", \"jid\", \"group\") values (\"%@\", \"%@\", \"%@\")"
#define QRY_DEL_OWNER_FROM_ROSTER_GROUPS_MATCHING_OWNER_JID_GROUP @"delete from \"roster-groups\" where \"collection-owner\" = \"%@\" and \"jid\" = \"%@\" and \"group\" = \"%@\""

#define QRY_UPDATE_TABLE_WHERE_COLUMN @"update \"%@\" set \"%@\" = \"%@\" where \"%@\" = \"%@\""

#define QRY_DEL_TABLE_ALL @"delete from \"%@\""
#define QRY_DEL_TABLE_WHERE_COLUMN @"delete from \"%@\" where \"%@\" = \"%@\""


//------------------------------------------------------------------------------
// Miscellaneous functions
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// callback for SQLITE_BUSY state
static int DBBusyCallback(void *context, int count) 
{
	const int max_tries = 100;
	const int sleep_ms = 100;
	
	JABDatabaseAction *dbAction = (JABDatabaseAction *) context;
	[[dbAction logger] logMsgWithLevel: LOG_NOTICE 
			                    format: @"Database is busy, attempt %i of %i, sleeping for %i ms...", 
	 count, max_tries, sleep_ms];
	
	usleep(sleep_ms);
	
	return (count < 100);
}

#pragma mark -
//------------------------------------------------------------------------------
// JABDatabase methods
//------------------------------------------------------------------------------
@implementation JABDatabase

@synthesize dbPath = _dbPath;
@synthesize backupPath = _backupPath;
@synthesize sqlDB = _sqlDB;
@synthesize dbAction = _dbAction;
@synthesize dbQuery = _dbQuery;
@synthesize dbResult = _dbResult;

//------------------------------------------------------------------------------
+ (id) jabDatabaseWithOptions: (NSDictionary *) cmdOpts forAction: (JABDatabaseAction *) dbAction
{
	return [[[self alloc] initWithOptions: cmdOpts forAction: dbAction] autorelease];
}

#pragma mark -
#pragma mark init/dealloc
//------------------------------------------------------------------------------
- (id) initWithOptions: (NSDictionary *) cmdOpts forAction: (JABDatabaseAction *) dbAction
{
	self = [super init];

	self.dbPath = JABBER_DB_PATH_DEFAULT; // default path if not overridden
	// We want to determine the database path based on the servermgr_jabber prefs if
	// it is available.
	NSDictionary *aDict = nil;
	aDict =  [NSDictionary dictionaryWithContentsOfFile: SRVRMGR_JABBER_PREFS_PATH];
	if (nil != aDict) {
		NSString *jabberdDatabasePath = [aDict objectForKey: SMJ_SETTINGSKEY_JABBERD_DB_PATH];
		if (nil != jabberdDatabasePath)
			self.dbPath = jabberdDatabasePath;
	}
	self.dbAction = dbAction; // save reference to master action
#ifdef DEBUG
	NSString *dbPath = [cmdOpts objectForKey: CMDOPT_KEY_DBPATHVAL];
	if (nil != dbPath) // override database path with user value
		self.dbPath = dbPath; 
#endif
	
	// Create a general-purpose query instance for database query methods
	self.dbQuery = [JABDatabaseQuery jabDatabaseQuery: @"" forAction: dbAction];
	
	return self;	
}

- (void) dealloc
{
	[self closeDatabase];

	self.dbQuery = nil;
	self.backupPath = nil;
	self.dbPath = nil;
	
	[super dealloc];
}

#pragma mark -
#pragma mark Special Database Utility Methods
//------------------------------------------------------------------------------
- (BOOL) createBackup
{
	// Creates a copy of the selected jabberd database in the same folder as 
	// the original, with a .bak extension.  If previous .bak files exist in 
	// the folder, newer backups are disambiguated by adding a number to the 
	// file name (e.g. "jabberd2.db-1.bak", jabberd2.db-2.bak, etc).  The path
	// to the most recent backup is saved in the dbBackupPath ivar.
	
	NSFileManager *fm = [NSFileManager defaultManager];
	
	if (![fm fileExistsAtPath: _dbPath])
		return NO;
	
	NSAutoreleasePool *aPool = [[NSAutoreleasePool alloc] init];
	
	NSString *dstPath = [_dbPath stringByAppendingFormat: @".bak"];
	if ([fm fileExistsAtPath: dstPath]) {
		NSInteger nBackup = 1;
		do {
			dstPath = [_dbPath stringByAppendingFormat: @"-%ld.bak", nBackup++];
		} while ([fm fileExistsAtPath: dstPath]);
	}
	
	NSError *fmErr = nil;
	BOOL bOk = [fm copyItemAtPath: _dbPath toPath: dstPath error: &fmErr];
	if (bOk)
		self.backupPath = dstPath;
	else {
		[[_dbAction logger] logStdErrMessage: [NSString stringWithFormat: 
											   @"Database backup failed; err=%ld: %@\n",
											   [fmErr code], [fmErr localizedFailureReason]]];
	}
    
	[aPool drain];
	
	return bOk;
}

#pragma mark -
#pragma mark Database Open/Close Methods
//------------------------------------------------------------------------------
- (void) openDatabase
{
	self.dbResult = SQLITE_OK;

	if (NULL != _sqlDB) 
		return; // already open
	
	if (0 != [_dbAction verboseFlag])
		[[_dbAction logger] logMsgWithLevel: LOG_INFO format: @"Opening database: %@", _dbPath];
	
	// Connect to db
	sqlite3 *db = NULL;
	int result = sqlite3_open([_dbPath UTF8String], &db);
	if (SQLITE_OK != result) {
		[[_dbAction logger] logMsgWithLevel: LOG_ERR format: @"Error: %s - %s", sqlite3_errmsg(db), __PRETTY_FUNCTION__];
		sqlite3_close(db);
		self.dbResult = result;
		return;
	}
#if SQLITE_VERSION_NUMBER >= 3003008
	sqlite3_extended_result_codes(db, 1);
#endif
	sqlite3_busy_handler(db, DBBusyCallback, _dbAction);
	
	self.sqlDB = db;
}

//------------------------------------------------------------------------------
- (void) closeDatabase
{
	self.dbResult = SQLITE_OK;
	
	if (NULL == _sqlDB)
		return; // nothing to close
	
	int result = sqlite3_close(_sqlDB);
	if (SQLITE_OK != result)
		[[_dbAction logger] logMsgWithLevel: LOG_ERR 
									 format: @"Error: %s - %s", 
		                                     sqlite3_errmsg(_sqlDB), __PRETTY_FUNCTION__];
	
	self.sqlDB = NULL;
}

#pragma mark -
#pragma mark Custom Query Methods
//------------------------------------------------------------------------------
- (BOOL) verifyActiveJid: (NSString *) aJid expectedResult: (BOOL) expected
{
	self.dbResult = SQLITE_OK;
	
	BOOL bIsActiveJid = NO;
	
	[_dbQuery setQueryText: [NSString stringWithFormat: 
							 QRY_SEL_OWNER_FROM_ACTIVE, 
							 aJid]];
	if (![_dbQuery startStatementForSource: __PRETTY_FUNCTION__  line: __LINE__])
 		return NO; // query creation failed -- abort
	[_dbQuery stepStatement]; // execute query
	self.dbResult = [_dbQuery queryResult];
	switch (_dbResult) {
		case SQLITE_ROW:
			bIsActiveJid = YES;
			break;
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			[_dbAction logUnknownQueryStatusErrorForSource: __PRETTY_FUNCTION__ line: __LINE__];
			self.dbResult = SQLITE_ERROR;
	} // switch
	[_dbQuery finalizeStatement];
	
	if ((SQLITE_ERROR != _dbResult) && (bIsActiveJid != expected)) {
		if (expected)
			[_dbAction logJidNotFoundError: aJid];
		else
			[_dbAction logDuplicateJidWarning: aJid];
	} // if
	
#ifdef DEBUG
	if (SQLITE_ERROR != _dbResult)
		[_dbAction recordActivity: DBActivityActiveRowSelect];
#endif
	
	return bIsActiveJid;
}

//------------------------------------------------------------------------------
- (void) insertActiveItemForOwner: (NSString *) ownerJid source: (const char *) source line: (int) line
{
	self.dbResult = SQLITE_OK;
	
	time_t time_val = time(NULL);
	[_dbQuery setQueryText: [NSString stringWithFormat: 
							 QRY_INS_OWNER_INTO_ACTIVE, 
							 ownerJid, time_val]];
	if (![_dbQuery startStatementForSource: source  line: line])
		return; // query creation failed -- abort
	
	[_dbQuery stepStatement];
	[self checkQueryStatus: _dbQuery source: source line: line];
#ifdef DEBUG
	if (SQLITE_ERROR != _dbResult)
		[_dbAction recordActivity: DBActivityActiveRowInsert];
#endif
	
	[_dbQuery finalizeStatement];
}

#pragma mark -
//------------------------------------------------------------------------------
- (void) insertVcardItemForOwner: (NSString *) ownerJid source: (const char *) source line: (int) line
{
	self.dbResult = SQLITE_OK;
	
	[_dbQuery setQueryText: [NSString stringWithFormat: 
							 QRY_INS_OWNER_INTO_VCARD, 
							 ownerJid]];
	if (![_dbQuery startStatementForSource: source  line: line])
		return; // query creation failed -- abort
	
	[_dbQuery stepStatement];
	[self checkQueryStatus: _dbQuery source: source line: line];
#ifdef DEBUG
	if (SQLITE_ERROR != _dbResult)
		[_dbAction recordActivity: DBActivityVcardRowInsert];
#endif
	
	[_dbQuery finalizeStatement];
}

#pragma mark -
//------------------------------------------------------------------------------
- (BOOL) verifyRosterItemForOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid
{
	self.dbResult = SQLITE_OK;
	
	BOOL bExists = NO;
	
	[_dbQuery setQueryText: [NSString stringWithFormat: 
						   QRY_SEL_OWNER_FROM_ROSTER_ITEMS_MATCHING_OWNER_AND_JID, 
						   ownerJid, buddyJid]];
	if (![_dbQuery startStatementForSource: __PRETTY_FUNCTION__  line: __LINE__])
 		return NO; // query creation failed -- abort
	[_dbQuery stepStatement]; // execute query
	self.dbResult = [_dbQuery queryResult];
	switch (_dbResult) {
		case SQLITE_ROW:
			bExists = YES;
			break;
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			[_dbAction logUnknownQueryStatusErrorForSource: __PRETTY_FUNCTION__ line: __LINE__];
			self.dbResult = SQLITE_ERROR;
	} // switch
	[_dbQuery finalizeStatement];
	
#ifdef DEBUG
	if (SQLITE_ERROR != _dbResult)
		[_dbAction recordActivity: DBActivityRosterItemsRowSelect];
#endif

	return bExists;
}

//------------------------------------------------------------------------------
- (void) insertRosterItemForOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid
						   source: (const char *) source line: (int) line 
{
	self.dbResult = SQLITE_OK;
	
	// Execute the insert query for the new roster-item entry
	[_dbQuery setQueryText: [NSString stringWithFormat: 
							 QRY_INS_OWNER_INTO_ROSTER_ITEMS, 
							 ownerJid, buddyJid]];
	if (![_dbQuery startStatementForSource: source  line: line])
		return; // query creation failed -- abort
	
	[_dbQuery stepStatement];
	[self checkQueryStatus: _dbQuery source: source line: line];
	
#ifdef DEBUG
	if (SQLITE_ERROR != _dbResult)
		[_dbAction recordActivity: DBActivityRosterItemsRowInsert];
#endif
	
	[_dbQuery finalizeStatement];
}

//------------------------------------------------------------------------------
- (void) deleteRosterItemForOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid
						   source: (const char *) source line: (int) line
{
	self.dbResult = SQLITE_OK;
	
	// Execute the insert query for the new roster-item entry
	[_dbQuery setQueryText: [NSString stringWithFormat: 
							 QRY_DEL_OWNER_FROM_ROSTER_ITEMS_MATCHING_OWNER_AND_JID, 
							 ownerJid, buddyJid]];
	if (![_dbQuery startStatementForSource: source  line: line])
		return; // query creation failed -- abort
	
	[_dbQuery stepStatement];
	[self checkQueryStatus: _dbQuery source: source line: line];
	
#ifdef DEBUG
	if (SQLITE_ERROR != _dbResult)
		[_dbAction recordActivity: DBActivityRosterItemsRowDelete];
#endif
	
	[_dbQuery finalizeStatement];
}

#pragma mark -
//------------------------------------------------------------------------------
- (BOOL) verifyRosterGroup: (NSString *) grpName forOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid
{
	self.dbResult = SQLITE_OK;
	
	BOOL bExists = NO;
	
	[_dbQuery setQueryText: [NSString stringWithFormat: 
							 QRY_SEL_OWNER_FROM_ROSTER_GROUPS_MATCHING_OWNER_JID_GROUP, 
							 ownerJid, buddyJid, grpName]];
	if (![_dbQuery startStatementForSource: __PRETTY_FUNCTION__  line: __LINE__])
 		return NO; // query creation failed -- abort
	[_dbQuery stepStatement]; // execute query
	self.dbResult = [_dbQuery queryResult];
	switch (_dbResult) {
		case SQLITE_ROW:
			bExists = YES;
			break;
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			[_dbAction logUnknownQueryStatusErrorForSource: __PRETTY_FUNCTION__ line: __LINE__];
			self.dbResult = SQLITE_ERROR;
	} // switch
	[_dbQuery finalizeStatement];
	
#ifdef DEBUG
	if (SQLITE_ERROR != _dbResult)
		[_dbAction recordActivity: DBActivityRosterGroupsRowSelect];
#endif
	
	return bExists;
}

//------------------------------------------------------------------------------
- (void) insertRosterGroup: (NSString *) grpName forOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid
					source: (const char *) source line: (int) line
{
	self.dbResult = SQLITE_OK;
	
	// Execute the insert query for the new roster-item entry
	[_dbQuery setQueryText: [NSString stringWithFormat: 
							 QRY_INS_OWNER_INTO_ROSTER_GROUPS, 
							 ownerJid, buddyJid, grpName]];
	if (![_dbQuery startStatementForSource: source  line: line])
		return; // query creation failed -- abort
	
	[_dbQuery stepStatement];
	[self checkQueryStatus: _dbQuery source: source line: line];
	
#ifdef DEBUG
	if (SQLITE_ERROR != _dbResult)
		[_dbAction recordActivity: DBActivityRosterGroupsRowInsert];
#endif
	
	[_dbQuery finalizeStatement];
}

//------------------------------------------------------------------------------
- (void) deleteRosterGroup: (NSString *) grpName forOwner: (NSString *) ownerJid 
				  andBuddy: (NSString *) buddyJid
					source: (const char *) source line: (int) line
{
	self.dbResult = SQLITE_OK;
	
	// Execute the insert query for the new roster-item entry
	[_dbQuery setQueryText: [NSString stringWithFormat: 
							 QRY_DEL_OWNER_FROM_ROSTER_GROUPS_MATCHING_OWNER_JID_GROUP, 
							 ownerJid, buddyJid, grpName]];
	if (![_dbQuery startStatementForSource: source  line: line])
		return; // query creation failed -- abort
	
	[_dbQuery stepStatement];
	[self checkQueryStatus: _dbQuery source: source line: line];
	
#ifdef DEBUG
	if (SQLITE_ERROR != _dbResult)
		[_dbAction recordActivity: DBActivityRosterGroupsRowDelete];
#endif
	
	[_dbQuery finalizeStatement];
}

#pragma mark -
#pragma mark Generic Query Methods
//------------------------------------------------------------------------------
- (void) updateTable: (NSString *) tableName column: (NSString *) column
		  oldValue: (NSString *) oldVal newValue: (NSString *) newVal
			  source: (const char *) source line: (int) line
{
	self.dbResult = SQLITE_OK;
	
	// Execute the insert query for the new roster-item entry
	[_dbQuery setQueryText: [NSString stringWithFormat: 
							 QRY_UPDATE_TABLE_WHERE_COLUMN, 
							 tableName, column, newVal, column, oldVal]];
	if (![_dbQuery startStatementForSource: source  line: line])
		return; // query creation failed -- abort
	
	[_dbQuery stepStatement];
	[self checkQueryStatus: _dbQuery source: source line: line];
	
#ifdef DEBUG
	if (SQLITE_ERROR != _dbResult)
		[_dbAction recordActivity: DBActivityMatchedRowUpdate];
#endif
	
	[_dbQuery finalizeStatement];
}

//------------------------------------------------------------------------------
- (void) deleteAllRowsFromTable: (NSString *) tableName source: (const char *) source line: (int) line
{
	self.dbResult = SQLITE_OK;
	
	// Execute the delete query for the selected table
	[_dbQuery setQueryText: [NSString stringWithFormat: 
							 QRY_DEL_TABLE_ALL, 
							 tableName]];
	if (![_dbQuery startStatementForSource: source  line: line])
		return; // query creation failed -- abort
	
	[_dbQuery stepStatement];
	[self checkQueryStatus: _dbQuery source: source line: line];
	
	[_dbQuery finalizeStatement];
}

//------------------------------------------------------------------------------
- (void) deleteRowFromTable: (NSString *) tableName whereColumn: (NSString *) colName equalsValue: (NSString *) aVal 
					 source: (const char *) source line: (int) line
{
	self.dbResult = SQLITE_OK;
	
	// Execute the deletion list
	[_dbQuery setQueryText: [NSString stringWithFormat: 
							 QRY_DEL_TABLE_WHERE_COLUMN, 
							 tableName, colName, aVal]];
	if (![_dbQuery startStatementForSource: source line: line])
		return; // query creation failed -- abort
	[_dbQuery stepStatement]; // execute query
	[self checkQueryStatus: _dbQuery source: source line: line];
	[_dbQuery finalizeStatement];
}

#pragma mark -
#pragma mark Status/Error Utility Methods
//------------------------------------------------------------------------------
- (BOOL) checkQueryStatus: (JABDatabaseQuery *) aQuery source: (const char *) source line: (int) line
{
	self.dbResult = SQLITE_OK;
	
	switch([aQuery queryResult]) {
		case SQLITE_ROW:
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			[_dbAction logUnknownQueryStatusErrorForSource: source line: line];
			self.dbResult = SQLITE_ERROR;
	}
	
	return (SQLITE_ERROR == _dbResult);
}

//------------------------------------------------------------------------------
- (NSString *) getDatabaseErrorMessage
{
	const char *err_msg = sqlite3_errmsg(_sqlDB);
	return [NSString stringWithCString: err_msg encoding: NSASCIIStringEncoding];
}

@end
