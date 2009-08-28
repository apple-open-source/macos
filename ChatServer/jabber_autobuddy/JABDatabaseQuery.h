//
//  JABDatabaseQuery.h
//  ChatServer2/jabber_autobuddy
//
//  Created by Steve Peralta on 8/7/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "JABDatabaseAction.h"
#import "JABDatabase.h"

//------------------------------------------------------------------------------
// JABDatabaseQuery class
//------------------------------------------------------------------------------
@interface JABDatabaseQuery : NSObject {
	
	NSString *_queryText; // query text for SQL statement
	JABDatabaseAction *_dbAction; // for reference to database
	int _queryResult; // result from last query operation
	
	sqlite3_stmt *_sqlStatement; // cache for active SQL statement(s)
	
#ifdef DEBUG
	NSInteger _queryCanStep; // 1 == ok to step the query (and possibly modify the database)
#endif
}
@property(retain, readwrite) NSString *queryText;
@property(assign, readwrite) JABDatabaseAction *dbAction;
@property(assign, readwrite) int queryResult;
@property(assign, readwrite) sqlite3_stmt *sqlStatement;
#ifdef DEBUG
@property(assign, readwrite) NSInteger queryCanStep;
#endif

+ (id) jabDatabaseQuery: (NSString *) queryText forAction: (JABDatabaseAction *) dbAction;

- (id) initQuery: (NSString *) query forAction: (JABDatabaseAction *) dbAction;
- (void) dealloc;

- (BOOL) startStatementForSource: (const char *) source line: (int) line;
- (void) stepStatement;
- (void) finalizeStatement;
- (NSString *) textForColumn: (NSInteger) columnIndex;

#ifdef DEBUG
- (void) updateQueryCanStep;
#endif

@end

//------------------------------------------------------------------------------
// JABSelectAllActiveQuery class
//------------------------------------------------------------------------------
@interface JABSelectAllActiveQuery : JABDatabaseQuery {
	
}
+ (id) jabSelectAllActiveQueryForAction: (JABDatabaseAction *) dbAction;

- (BOOL) startStatement;
- (NSString *) getNextActiveJid;
@end
