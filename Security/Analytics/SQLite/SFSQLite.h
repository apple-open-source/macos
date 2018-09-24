/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

// Header exposed for unit testing only

#if __OBJC2__

#import <Foundation/Foundation.h>
#import <sqlite3.h>

@class SFSQLiteStatement;

typedef SInt64 SFSQLiteRowID;
@class SFSQLite;

NSArray *SFSQLiteJournalSuffixes(void);

typedef NS_ENUM(NSInteger, SFSQLiteSynchronousMode) {
    SFSQLiteSynchronousModeOff = 0,
    SFSQLiteSynchronousModeNormal = 1, // default
    SFSQLiteSynchronousModeFull = 2
};

@protocol SFSQLiteDelegate <NSObject>
@property (nonatomic, readonly) SInt32 userVersion;

- (BOOL)migrateDatabase:(SFSQLite *)db fromVersion:(SInt32)version;
@end

// Wrapper around the SQLite API. Typically subclassed to add table accessor methods.
@interface SFSQLite : NSObject {
@private
    id<SFSQLiteDelegate> _delegate;
    NSString* _path;
    NSString* _schema;
    NSString* _schemaVersion;
    NSMutableDictionary* _statementsBySQL;
    NSString* _objectClassPrefix;
    SFSQLiteSynchronousMode _synchronousMode;
    SInt32 _userVersion;
    sqlite3* _db;
    NSUInteger _openCount;
    NSDateFormatter* _dateFormatter;
#if DEBUG
    NSMutableDictionary* _unitTestOverrides;
#endif
    BOOL _hasMigrated;
    BOOL _corrupt;
    BOOL _traced;
}

- (instancetype)initWithPath:(NSString *)path schema:(NSString *)schema;
    
@property (nonatomic, readonly, strong) NSString   *path;
@property (nonatomic, readonly, strong) NSString   *schema;
@property (nonatomic, readonly, strong) NSString   *schemaVersion;
@property (nonatomic, strong)           NSString   *objectClassPrefix;
@property (nonatomic, assign)           SInt32     userVersion;
@property (nonatomic, assign)           SFSQLiteSynchronousMode synchronousMode;
@property (nonatomic, readonly)         BOOL       isOpen;
@property (nonatomic, readonly)         BOOL       hasMigrated;
@property (nonatomic, assign)           BOOL       traced;

@property (nonatomic, strong) id<SFSQLiteDelegate> delegate;

#if DEBUG
@property (nonatomic, strong) NSDictionary* unitTestOverrides;
#endif

// Open/close the underlying database file read/write. Initially, the database is closed.
- (void)open;
- (BOOL)openWithError:(NSError **)error;
- (void)close;

// Remove the database file.
- (void)remove;

// Database exclusive transaction operations.
- (void)begin;
- (void)end;
- (void)rollback;

// Database maintenance.
- (void)analyze;
- (void)vacuum;

// The rowID assigned to the last record inserted into the database.
- (SFSQLiteRowID)lastInsertRowID;

// returns the number of rows modified, inserted or deleted by the most recently completed INSERT, UPDATE or DELETE statement on the database connection
- (int)changes;

// Execute one-or-more queries. Use prepared statements for anything performance critical.
- (BOOL)executeSQL:(NSString *)format, ... NS_FORMAT_FUNCTION(1, 2);
- (BOOL)executeSQL:(NSString *)format arguments:(va_list)args NS_FORMAT_FUNCTION(1, 0);

// Prepared statement pool accessors. Statements must be reset after they're used.
- (SFSQLiteStatement *)statementForSQL:(NSString *)SQL;
- (void)removeAllStatements;

// Accessors for all the tables created in the database.
- (NSArray *)allTableNames;
- (void)dropAllTables;

// Generic key/value properties set in the database.
- (NSString *)propertyForKey:(NSString *)key;
- (void)setProperty:(NSString *)value forKey:(NSString *)key;
- (NSDate *)datePropertyForKey:(NSString *)key;
- (void)setDateProperty:(NSDate *)value forKey:(NSString *)key;
- (void)removePropertyForKey:(NSString *)key;

// Date the cache was created.
- (NSDate *)creationDate;

// Convience calls that generate and execute statements.
- (NSArray *)selectAllFrom:(NSString *)tableName where:(NSString *)whereSQL bindings:(NSArray *)bindings;
- (NSArray *)select:(NSArray *)columns from:(NSString *)tableName;
- (NSArray *)select:(NSArray *)columns from:(NSString *)tableName where:(NSString *)whereSQL bindings:(NSArray *)bindings;
- (void)select:(NSArray *)columns from:(NSString *)tableName where:(NSString *)whereSQL bindings:(NSArray *)bindings orderBy:(NSArray *)orderBy limit:(NSNumber *)limit block:(void (^)(NSDictionary *resultDictionary, BOOL *stop))block;
- (void)selectFrom:(NSString *)tableName where:(NSString *)whereSQL bindings:(NSArray *)bindings orderBy:(NSArray *)orderBy limit:(NSNumber *)limit block:(void (^)(NSDictionary *resultDictionary, BOOL *stop))block;
- (NSUInteger)selectCountFrom:(NSString *)tableName  where:(NSString *)whereSQL bindings:(NSArray *)bindings;
- (SFSQLiteRowID)insertOrReplaceInto:(NSString *)tableName values:(NSDictionary *)valuesByColumnName;
- (void)deleteFrom:(NSString *)tableName where:(NSString *)whereSQL bindings:(NSArray *)bindings;
- (void)update:(NSString *)tableName set:(NSString *)setSQL where:(NSString *)whereSQL bindings:(NSArray *)whereBindings limit:(NSNumber *)limit;
- (void)deleteFrom:(NSString *)tableName matchingValues:(NSDictionary *)valuesByColumnName;
- (NSSet<NSString*> *)columnNamesForTable:(NSString*)tableName;

- (SInt32)dbUserVersion;

@end

#endif
