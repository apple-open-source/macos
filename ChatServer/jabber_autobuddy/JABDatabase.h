//
//  JABDatabase.h
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 8/8/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <sqlite3.h>

// enums & typdefs

typedef struct sqlite3 sqlite3;

@class JABDatabaseAction;
@class JABDatabaseQuery;

@interface JABDatabase : NSObject {

	NSString *_dbPath; // path to target database
	NSString *_backupPath; // path to last database backup
	sqlite3 *_sqlDB; // open database reference for read/write operation
	
	JABDatabaseAction *_dbAction; // reference to master
	JABDatabaseQuery *_dbQuery; // internal query instance
	NSInteger _dbResult; // result code for last database operation
}
@property(retain, readwrite) NSString *dbPath;
@property(retain, readwrite) NSString *backupPath;
@property(assign, readwrite) sqlite3 *sqlDB;
@property(retain, readwrite) JABDatabaseAction *dbAction;
@property(retain, readwrite) JABDatabaseQuery *dbQuery;
@property(assign, readwrite) NSInteger dbResult;

+ (id) jabDatabaseWithOptions: (NSDictionary *) cmdOpts forAction: (JABDatabaseAction *) dbAction;

// init/dealloc

- (id) initWithOptions: (NSDictionary *) cmdOpts forAction: (JABDatabaseAction *) dbAction;
- (void) dealloc;

// Special Database Utility Methods

- (BOOL) createBackup;

// Database Open/Close Methods

- (void) openDatabase;
- (void) closeDatabase;

// Custom Query Methods

- (BOOL) verifyActiveJid: (NSString *) aJid expectedResult: (BOOL) expected;
- (void) insertActiveItemForOwner: (NSString *) ownerJid source: (const char *) source line: (int) line;

- (void) insertVcardItemForOwner: (NSString *) ownerJid source: (const char *) source line: (int) line;

- (BOOL) verifyRosterItemForOwner: (NSString *) ownerJid andBuddy: (NSString *) ownerJid;
- (void) insertRosterItemForOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid
						   source: (const char *) source line: (int) line;
- (void) deleteRosterItemForOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid
						   source: (const char *) source line: (int) line;

- (BOOL) verifyRosterGroup: (NSString *) grpName forOwner: (NSString *) ownerJid andBuddy: (NSString *) ownerJid;
- (void) insertRosterGroup: (NSString *) grpName forOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid
					source: (const char *) source line: (int) line;
- (void) deleteRosterGroup: (NSString *) grpName forOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid
					source: (const char *) source line: (int) line;

// Generic Query Methods

- (void) updateTable: (NSString *) tableName column: (NSString *) setCol 
			oldValue: (NSString *) oldVal newValue: (NSString *) newVal
			  source: (const char *) source line: (int) line;
- (void) deleteAllRowsFromTable: (NSString *) tableName source: (const char *) source line: (int) line;
- (void) deleteRowFromTable: (NSString *) tableName whereColumn: (NSString *) colName equalsValue: (NSString *) aVal 
					 source: (const char *) source line: (int) line;

// Status/Error Utility Methods

- (BOOL) checkQueryStatus: (JABDatabaseQuery *) dbQuery source: (const char *) source line: (int) line;
- (NSString *) getDatabaseErrorMessage;

@end
