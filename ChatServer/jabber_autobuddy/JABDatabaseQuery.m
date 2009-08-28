//
//  JABDatabaseQuery.m
//  ChatServer2/jabber_autobuddy
//
//  Created by Steve Peralta on 8/7/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABDatabaseQuery.h"

// constants

#define MAX_JID_LEN 512

#define QRY_SEL_ALLOWNERS_FROM_ACTIVE @"select \"collection-owner\" from active"

//------------------------------------------------------------------------------
// JABDatabaseQuery methods
//------------------------------------------------------------------------------
@implementation JABDatabaseQuery

@synthesize queryText = _queryText;
@synthesize dbAction = _dbAction;
@synthesize queryResult = _queryResult;
@synthesize sqlStatement = _sqlStatement;
#ifdef DEBUG
@synthesize queryCanStep = _queryCanStep;
#endif

//------------------------------------------------------------------------------
+ (id) jabDatabaseQuery: (NSString *) queryText forAction: (JABDatabaseAction *) dbAction
{
	return [[[self alloc] initQuery: queryText forAction: dbAction] autorelease];
}

//------------------------------------------------------------------------------
- (id) initQuery: (NSString *) query forAction: (JABDatabaseAction *) dbAction
{
	self = [super init];
	
	self.queryText = query;
	self.dbAction = dbAction; // for reference only, not retained
	
#ifdef DEBUG
	// For queries that modify the database, determine if that operation is enabled.
	[self updateQueryCanStep];
#endif
	
	return self;
}

//------------------------------------------------------------------------------
- (void) dealloc
{
	[self finalizeStatement]; // make sure the active query is terminated
	
	self.queryText = nil;
	
	[super dealloc];
}

//------------------------------------------------------------------------------
- (BOOL) startStatementForSource: (const char *) source line: (int) line
{
#ifdef DEBUG
	if (1 == [_dbAction dbShowSQLFlag]) {
		[[_dbAction logger] logStdErrMessage: [NSString stringWithFormat: 
											   @"%@\n", _queryText]];
	}
	
	// Update queryCanStep here also since the query text may have changed 
	// since the JABDatabaseQuery instance was initialized
	[self updateQueryCanStep];
#endif
	
	char sql_query[MAX_JID_LEN+1024];
	snprintf(sql_query, sizeof(sql_query) - 1, "%s", [_queryText UTF8String]);
	
	sqlite3_stmt *stmt;
	_queryResult = sqlite3_prepare_v2([[_dbAction database] sqlDB], (const char *) sql_query, 
									  strlen(sql_query), &stmt, NULL);
	if (SQLITE_OK != _queryResult) {
		[_dbAction logSqlPrepareErrorForSource: source line: line];
		[_dbAction setResult: OPRESULT_FAILED];
		return NO;
	}
	_sqlStatement = stmt;
	
	return YES;
}

//------------------------------------------------------------------------------
- (void) stepStatement
{
	if (NULL == _sqlStatement)
		return; // missing query
#ifdef DEBUG
	if (1 != _queryCanStep)
		return; // execution disabled for this query
#endif
	
	_queryResult = sqlite3_step(_sqlStatement);
}

//------------------------------------------------------------------------------
- (void) finalizeStatement
{
	if (NULL != _sqlStatement)
		_queryResult = sqlite3_finalize(_sqlStatement);
	_sqlStatement = NULL;
}

//------------------------------------------------------------------------------
- (NSString *) textForColumn: (NSInteger) columnIndex
{
	if (NULL == _sqlStatement)
		return nil; // no active statement
	
	const unsigned char *elem = sqlite3_column_text(_sqlStatement, columnIndex);
	if (NULL == elem) return nil;
	return [NSString stringWithCString: (const char *) elem encoding: NSASCIIStringEncoding];
}

#ifdef DEBUG
//------------------------------------------------------------------------------
- (void) updateQueryCanStep
{
	// Update _queryCanStep based on the requested SQL operation and 
	// the JABDatabaseAction enable flag for writes to the database
	
	if ((nil == _queryText) || (1 > [_queryText length])) {
		_queryCanStep = 0; // invalid query text
		return;
	}
	
	// Check whether the query would result in database modification if executed
	if (!([_queryText hasPrefix: @"insert"] || [_queryText hasPrefix: @"delete"]))  {
		// all non-writing queries are explicitly enabled
		_queryCanStep = 1;
		return;
	}
	
	_queryCanStep = (0 == [_dbAction dbNoWriteFlag]) ? 1 : 0;
}
#endif

@end

#pragma mark -
//------------------------------------------------------------------------------
@implementation JABSelectAllActiveQuery

+ (id) jabSelectAllActiveQueryForAction: (JABDatabaseAction *) dbAction
{
	return [[[self alloc] initQuery: QRY_SEL_ALLOWNERS_FROM_ACTIVE
						  forAction: dbAction] autorelease];
}

//------------------------------------------------------------------------------
- (BOOL) startStatement
{
	[_dbAction setResult: OPRESULT_OK];
	
	if (![super startStatementForSource: __PRETTY_FUNCTION__ line: __LINE__]) {
		[super finalizeStatement];
		[_dbAction setResult: OPRESULT_FAILED];
	}
	
	return (OPRESULT_OK == [_dbAction result]);
}

//------------------------------------------------------------------------------
- (NSString *) getNextActiveJid 
{
	[self stepStatement]; // get next row
	
	BOOL itemsFound = NO;
	switch(_queryResult) {
		case SQLITE_ROW:
			itemsFound = YES;
			break;
		case SQLITE_DONE:
		case SQLITE_OK:
			//			[_dbAction logNoActiveItemsWarningForSource: __PRETTY_FUNCTION__ line: __LINE__];
			break;
		default:
			[_dbAction logUnknownQueryStatusErrorForSource: __PRETTY_FUNCTION__ line: __LINE__];
			[_dbAction setResult: OPRESULT_FAILED];
	} // switch
	if (!itemsFound) return nil;
	
	NSString *ownerJid = nil;
	while (SQLITE_DONE != _queryResult) {
		if (SQLITE_ROW != _queryResult) {
			[_dbAction logUnknownQueryStatusErrorForSource: __PRETTY_FUNCTION__ line: __LINE__];
			[_dbAction setResult: OPRESULT_FAILED];
			break;
		}
		NSString *aJid = [self textForColumn: 0];
		if (MAX_JID_LEN < [aJid length]) {
			[_dbAction logJidLengthWarning: aJid];
			[self stepStatement]; // skip to next result
			continue;
		}	
		ownerJid = aJid; // set return value
		break; // done
	} // while
	
#ifdef DEBUG
	if (OPRESULT_OK == [_dbAction result])
		[_dbAction recordActivity: DBActivityActiveRowSelect];
#endif
	
	return ownerJid;
}

@end
