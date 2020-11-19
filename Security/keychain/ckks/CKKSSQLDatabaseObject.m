/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>
#import "CKKSSQLDatabaseObject.h"
#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/SecItemDb.h"

#import "keychain/ckks/CKKS.h"
#import "CKKSKeychainView.h"

@interface CKKSSQLResult ()
@property (nullable) NSString* stringValue;
@end

@implementation CKKSSQLResult
- (instancetype)init:(NSString* _Nullable)value
{
    if((self = [super init])) {
        _stringValue = value;
    }
    return self;
}

- (BOOL)asBOOL
{
    return [self.stringValue boolValue];
}

- (NSInteger)asNSInteger
{
    return [self.stringValue integerValue];
}

- (NSString* _Nullable)asString
{
    return self.stringValue;
}

- (NSNumber* _Nullable)asNSNumberInteger
{
    if(self.stringValue == nil) {
        return nil;
    }
    return [NSNumber numberWithInteger: [self.stringValue integerValue]];
}

- (NSDate* _Nullable)asISO8601Date
{
    if(self.stringValue == nil) {
        return nil;
    }

    NSISO8601DateFormatter* dateFormat = [[NSISO8601DateFormatter alloc] init];
    return [dateFormat dateFromString:self.stringValue];
}

- (NSData* _Nullable)asBase64DecodedData
{
    if(self.stringValue == nil) {
        return nil;
    }
    return [[NSData alloc] initWithBase64EncodedString:self.stringValue options:0];
}
@end

__thread bool CKKSSQLInTransaction = false;
__thread bool CKKSSQLInWriteTransaction = false;

@implementation CKKSSQLDatabaseObject

+ (bool) saveToDatabaseTable: (NSString*) table row: (NSDictionary*) row connection: (SecDbConnectionRef) dbconn error: (NSError * __autoreleasing *) error {
    __block CFErrorRef cferror = NULL;

#if DEBUG
    NSAssert(CKKSSQLInTransaction, @"Must be in a transaction to perform database writes");
    NSAssert(CKKSSQLInWriteTransaction, @"Must be in a write transaction to perform database writes");
#endif

    bool (^doWithConnection)(SecDbConnectionRef) = ^bool (SecDbConnectionRef dbconn) {
        NSString * columns = [row.allKeys componentsJoinedByString:@", "];
        NSMutableString * values = [[NSMutableString alloc] init];
        for(NSUInteger i = 0; i < [row.allKeys count]; i++) {
            if(i != 0) {
                [values appendString: @",?"];
            } else {
                [values appendString: @"?"];
            }
        }

        NSString *sql = [[NSString alloc] initWithFormat: @"INSERT OR REPLACE into %@ (%@) VALUES (%@);", table, columns, values];

        SecDbPrepare(dbconn, (__bridge CFStringRef) sql, &cferror, ^void (sqlite3_stmt *stmt) {
            [row.allKeys enumerateObjectsUsingBlock:^(id  _Nonnull key, NSUInteger i, BOOL * _Nonnull stop) {
                SecDbBindObject(stmt, (int)(i+1), (__bridge CFStringRef) row[key], &cferror);
            }];

            SecDbStep(dbconn, stmt, &cferror, ^(bool *stop) {
                // don't do anything, I guess?
            });
        });

        return true;
    };

    if(dbconn) {
        doWithConnection(dbconn);
    } else {
        kc_with_dbt(true, &cferror, doWithConnection);
    }

    bool ret = cferror == NULL;

    SecTranslateError(error, cferror);

    return ret;
}

+ (NSString*) makeWhereClause: (NSDictionary*) whereDict {
    if(!whereDict) {
        return @"";
    }
    NSMutableString * whereClause = [[NSMutableString alloc] init];
    __block bool conjunction = false;
    [whereDict enumerateKeysAndObjectsUsingBlock: ^(NSString* key, NSNumber* value, BOOL* stop) {
        if(!conjunction) {
            [whereClause appendFormat: @" WHERE "];
        } else {
            [whereClause appendFormat: @" AND "];
        }

        if([value class] == [CKKSSQLWhereValue class]) {
            CKKSSQLWhereValue* obj = (CKKSSQLWhereValue*)value;
            [whereClause appendFormat: @"%@%@(?)", key, CKKSSQLWhereComparatorAsString(obj.sqlOp)];

        } else if([value class] == [CKKSSQLWhereColumn class]) {
            CKKSSQLWhereColumn* obj = (CKKSSQLWhereColumn*)value;
            [whereClause appendFormat: @"%@%@%@",
             key,
             CKKSSQLWhereComparatorAsString(obj.sqlOp),
             CKKSSQLWhereColumnNameAsString(obj.columnName)];

        } else if([value isMemberOfClass:[CKKSSQLWhereIn class]]) {
            CKKSSQLWhereIn* obj = (CKKSSQLWhereIn*)value;

            NSMutableArray* q = [NSMutableArray arrayWithCapacity:obj.values.count];
            for(NSString* value in obj.values) {
                [q addObject: @"?"];
                (void)value;
            }

            NSString* binds = [q componentsJoinedByString:@", "];

            [whereClause appendFormat:@"%@ IN (%@)", key, binds];

        } else {
            [whereClause appendFormat: @"%@=(?)", key];
        }

        conjunction = true;
    }];
    return whereClause;
}

+ (NSString*) groupByClause: (NSArray*) columns {
    if(!columns) {
        return @"";
    }
    NSMutableString * groupByClause = [[NSMutableString alloc] init];
    __block bool conjunction = false;
    [columns enumerateObjectsUsingBlock: ^(NSString* column, NSUInteger i, BOOL* stop) {
        if(!conjunction) {
            [groupByClause appendFormat: @" GROUP BY "];
        } else {
            [groupByClause appendFormat: @", "];
        }

        [groupByClause appendFormat: @"%@", column];

        conjunction = true;
    }];
    return groupByClause;
}

+ (NSString*)orderByClause: (NSArray*) columns {
    if(!columns || columns.count == 0u) {
        return @"";
    }
    NSMutableString * orderByClause = [[NSMutableString alloc] init];
    __block bool conjunction = false;
    [columns enumerateObjectsUsingBlock: ^(NSString* column, NSUInteger i, BOOL* stop) {
        if(!conjunction) {
            [orderByClause appendFormat: @" ORDER BY "];
        } else {
            [orderByClause appendFormat: @", "];
        }

        [orderByClause appendFormat: @"%@", column];

        conjunction = true;
    }];
    return orderByClause;
}

+ (void)bindWhereClause:(sqlite3_stmt*)stmt whereDict:(NSDictionary*)whereDict cferror:(CFErrorRef*)cferror
{
    __block int whereLocation = 1;

    [whereDict.allKeys enumerateObjectsUsingBlock:^(id  _Nonnull key, NSUInteger i, BOOL * _Nonnull stop) {
        if([whereDict[key] class] == [CKKSSQLWhereValue class]) {
            CKKSSQLWhereValue* obj = (CKKSSQLWhereValue*)whereDict[key];
            SecDbBindObject(stmt, whereLocation, (__bridge CFStringRef)obj.value, cferror);
            whereLocation++;

        } else if([whereDict[key] class] == [CKKSSQLWhereColumn class]) {
            // skip
        } else if([whereDict[key] isMemberOfClass:[CKKSSQLWhereIn class]]) {
            CKKSSQLWhereIn* obj = (CKKSSQLWhereIn*)whereDict[key];

            for(NSString* value in obj.values) {
                SecDbBindObject(stmt, whereLocation, (__bridge CFStringRef)value, cferror);
                whereLocation++;
            }

        } else {
            SecDbBindObject(stmt, whereLocation, (__bridge CFStringRef) whereDict[key], cferror);
            whereLocation++;
        }
    }];
}

+ (bool) deleteFromTable: (NSString*) table where: (NSDictionary*) whereDict connection:(SecDbConnectionRef) dbconn error: (NSError * __autoreleasing *) error {
    __block CFErrorRef cferror = NULL;

#if DEBUG
    NSAssert(CKKSSQLInTransaction, @"Must be in a transaction to perform database writes");
    NSAssert(CKKSSQLInWriteTransaction, @"Must be in a write transaction to perform database writes");
#endif

    bool (^doWithConnection)(SecDbConnectionRef) = ^bool (SecDbConnectionRef dbconn) {
        NSString* whereClause = [CKKSSQLDatabaseObject makeWhereClause: whereDict];

        NSString * sql = [[NSString alloc] initWithFormat: @"DELETE FROM %@%@;", table, whereClause];
        SecDbPrepare(dbconn, (__bridge CFStringRef) sql, &cferror, ^void (sqlite3_stmt *stmt) {
            [self bindWhereClause:stmt whereDict:whereDict cferror:&cferror];

            SecDbStep(dbconn, stmt, &cferror, ^(bool *stop) {
            });
        });
        return true;
    };

    if(dbconn) {
        doWithConnection(dbconn);
    } else {
        kc_with_dbt(true, &cferror, doWithConnection);
    }

    // Deletes finish in a single step, so if we didn't get an error, the delete 'happened'
    bool status = (cferror == nil);

    if(error) {
        *error = (NSError*) CFBridgingRelease(cferror);
    } else {
        CFReleaseNull(cferror);
    }

    return status;
}

+ (bool)queryDatabaseTable:(NSString*)table
                     where:(NSDictionary*)whereDict
                   columns:(NSArray*)names
                   groupBy:(NSArray*)groupColumns
                   orderBy:(NSArray*)orderColumns
                     limit:(ssize_t)limit
                processRow:(void (^)(NSDictionary<NSString*, CKKSSQLResult*>*)) processRow
                     error:(NSError * __autoreleasing *) error {
    __block CFErrorRef cferror = NULL;

    kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbconn) {
        NSString * columns = [names componentsJoinedByString:@", "];
        NSString * whereClause = [CKKSSQLDatabaseObject makeWhereClause: whereDict];
        NSString * groupByClause = [CKKSSQLDatabaseObject groupByClause: groupColumns];
        NSString * orderByClause = [CKKSSQLDatabaseObject orderByClause: orderColumns];
        NSString * limitClause = (limit > 0 ? [NSString stringWithFormat:@" LIMIT %lu", limit] : @"");

        NSString * sql = [[NSString alloc] initWithFormat: @"SELECT %@ FROM %@%@%@%@%@;", columns, table, whereClause, groupByClause, orderByClause, limitClause];
        SecDbPrepare(dbconn, (__bridge CFStringRef) sql, &cferror, ^void (sqlite3_stmt *stmt) {
            [self bindWhereClause:stmt whereDict:whereDict cferror:&cferror];

            SecDbStep(dbconn, stmt, &cferror, ^(bool *stop) {
                __block NSMutableDictionary<NSString*, CKKSSQLResult*>* row = [[NSMutableDictionary alloc] init];

                [names enumerateObjectsUsingBlock:^(id  _Nonnull name, NSUInteger i, BOOL * _Nonnull stop) {
                    const char * col = (const char *) sqlite3_column_text(stmt, (int)i);
                    row[name] = [[CKKSSQLResult alloc] init:col ? [NSString stringWithUTF8String:col] : nil];
                }];

                processRow(row);
            });
        });
        return true;
    });

    bool ret = (cferror == NULL);
    SecTranslateError(error, cferror);
    return ret;
}

+ (NSString *)quotedString:(NSString *)string
{
    char *quotedMaxField = sqlite3_mprintf("%q", [string UTF8String]);
    if (quotedMaxField == NULL) {
        abort();
    }
    NSString *rstring = [NSString stringWithUTF8String:quotedMaxField];
    sqlite3_free(quotedMaxField);
    return rstring;
}

+ (bool)queryMaxValueForField:(NSString*)maxField
                      inTable:(NSString*)table
                        where:(NSDictionary*)whereDict
                      columns:(NSArray*)names
                   processRow:(void (^)(NSDictionary<NSString*, CKKSSQLResult*>*))processRow
{
    __block CFErrorRef cferror = NULL;

    kc_with_dbt(false, &cferror, ^bool(SecDbConnectionRef dbconn) {
        NSString *quotedMaxField = [self quotedString:maxField];
        NSString *quotedTable = [self quotedString:table];

        NSMutableArray<NSString *>* quotedNames = [NSMutableArray array];
        for (NSString *element in names) {
            [quotedNames addObject:[self quotedString:element]];
        }

        NSString* columns = [[quotedNames componentsJoinedByString:@", "] stringByAppendingFormat:@", %@", quotedMaxField];
        NSString* whereClause = [CKKSSQLDatabaseObject makeWhereClause:whereDict];
        
        NSString* sql = [[NSString alloc] initWithFormat:@"SELECT %@ FROM %@%@", columns, quotedTable, whereClause];
        SecDbPrepare(dbconn, (__bridge CFStringRef)sql, &cferror, ^(sqlite3_stmt* stmt) {
            [self bindWhereClause:stmt whereDict:whereDict cferror:&cferror];
            
            SecDbStep(dbconn, stmt, &cferror, ^(bool*stop) {
                __block NSMutableDictionary<NSString*, CKKSSQLResult*>* row = [[NSMutableDictionary alloc] init];
                
                [names enumerateObjectsUsingBlock:^(id  _Nonnull name, NSUInteger i, BOOL * _Nonnull stop) {
                    const char * col = (const char *) sqlite3_column_text(stmt, (int)i);
                    row[name] = [[CKKSSQLResult alloc] init:col ? [NSString stringWithUTF8String:col] : nil];
                }];
                
                processRow(row);
            });
        });
        
        return true;
    });
    
    bool ret = (cferror == NULL);
    return ret;
}

+ (BOOL)performCKKSTransaction:(CKKSDatabaseTransactionResult (^)(void))block
{
    CFErrorRef cferror = NULL;
    bool ok = kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbconn) {
        CFErrorRef cferrorInternal = NULL;
        bool ret = kc_transaction_type(dbconn, kSecDbExclusiveRemoteCKKSTransactionType, &cferrorInternal, ^bool{
            CKKSDatabaseTransactionResult result = CKKSDatabaseTransactionRollback;

            CKKSSQLInTransaction = true;
            CKKSSQLInWriteTransaction = true;
            result = block();
            CKKSSQLInWriteTransaction = false;
            CKKSSQLInTransaction = false;
            return result == CKKSDatabaseTransactionCommit;
        });
        if(cferrorInternal) {
            ckkserror_global("ckkssql",  "error performing database transaction, major problems ahead: %@", cferrorInternal);
        }
        CFReleaseNull(cferrorInternal);
        return ret;
    });

    if(cferror) {
        ckkserror_global("ckkssql",  "error performing database operation, major problems ahead: %@", cferror);
    }
    CFReleaseNull(cferror);
    return ok;
}

+ (BOOL)performCKKSReadonlyTransaction:(void(^)(void))block
{
    CFErrorRef cferror = NULL;
    bool ok = kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbconn) {
        CFErrorRef cferrorInternal = NULL;
        bool ret = kc_transaction_type(dbconn, kSecDbNormalTransactionType, &cferrorInternal, ^bool{
            CKKSSQLInTransaction = true;
            block();
            CKKSSQLInTransaction = false;
            return true;
        });
        if(cferrorInternal) {
            ckkserror_global("ckkssql",  "error performing database transaction, major problems ahead: %@", cferrorInternal);
        }
        CFReleaseNull(cferrorInternal);
        return ret;
    });

    if(cferror) {
        ckkserror_global("ckkssql",  "error performing database operation, major problems ahead: %@", cferror);
    }
    CFReleaseNull(cferror);
    return ok;
}

#pragma mark - Instance methods

- (bool) saveToDatabase: (NSError * __autoreleasing *) error {
    return [self saveToDatabaseWithConnection:nil error: error];
}

- (bool) saveToDatabaseWithConnection: (SecDbConnectionRef) conn error: (NSError * __autoreleasing *) error {
    // Todo: turn this into a transaction

    NSDictionary* currentWhereClause = [self whereClauseToFindSelf];

    // First, if we were loaded from the database and the where clause has changed, delete the old record.
    if(self.originalSelfWhereClause && ![self.originalSelfWhereClause isEqualToDictionary: currentWhereClause]) {
        secdebug("ckkssql", "Primary key changed; removing old row at %@", self.originalSelfWhereClause);
        if(![CKKSSQLDatabaseObject deleteFromTable:[[self class] sqlTable] where: self.originalSelfWhereClause connection:conn error: error]) {
            return false;
        }
    }

    bool ok = [CKKSSQLDatabaseObject saveToDatabaseTable: [[self class] sqlTable]
                                                     row: [self sqlValues]
                                              connection: conn
                                                   error: error];

    if(ok) {
        secdebug("ckkssql", "Saved %@", self);
    } else {
        secdebug("ckkssql", "Couldn't save %@: %@", self, error ? *error : @"unknown");
    }
    return ok;
}

- (bool) deleteFromDatabase: (NSError * __autoreleasing *) error {
    bool ok = [CKKSSQLDatabaseObject deleteFromTable:[[self class] sqlTable] where: [self whereClauseToFindSelf] connection:nil error: error];

    if(ok) {
        secdebug("ckkssql", "Deleted %@", self);
    } else {
        secdebug("ckkssql", "Couldn't delete %@: %@", self, error ? *error : @"unknown");
    }
    return ok;
}

+ (bool) deleteAll: (NSError * __autoreleasing *) error {
    bool ok = [CKKSSQLDatabaseObject deleteFromTable:[self sqlTable] where: nil connection:nil error: error];

    if(ok) {
        secdebug("ckkssql", "Deleted all %@", self);
    } else {
        secdebug("ckkssql", "Couldn't delete all %@: %@", self, error ? *error : @"unknown");
    }
    return ok;
}

+ (instancetype) fromDatabaseWhere: (NSDictionary*) whereDict error: (NSError * __autoreleasing *) error {
    id ret = [self tryFromDatabaseWhere: whereDict error:error];

    if(!ret && error) {
        *error = [NSError errorWithDomain:@"securityd"
                                     code:errSecItemNotFound
                                 userInfo:@{NSLocalizedDescriptionKey:
                                                [NSString stringWithFormat: @"%@ does not exist in database where %@", [self class], whereDict]}];
    }

    return ret;
}

+ (instancetype _Nullable) tryFromDatabaseWhere: (NSDictionary*) whereDict error: (NSError * __autoreleasing *) error {
    __block id ret = nil;

    [CKKSSQLDatabaseObject queryDatabaseTable: [self sqlTable]
                                        where: whereDict
                                      columns: [self sqlColumns]
                                      groupBy: nil
                                      orderBy:nil
                                        limit: -1
                                   processRow: ^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                   ret = [[self fromDatabaseRow: row] memoizeOriginalSelfWhereClause];
                               }
                                        error: error];

    return ret;
}

+ (NSArray*) all: (NSError * __autoreleasing *) error {
    return [self allWhere: nil error:error];
}

+ (NSArray*) allWhere: (NSDictionary*) whereDict error: (NSError * __autoreleasing *) error {
    __block NSMutableArray* items = [[NSMutableArray alloc] init];

    [CKKSSQLDatabaseObject queryDatabaseTable: [self sqlTable]
                                        where: whereDict
                                      columns: [self sqlColumns]
                                      groupBy: nil
                                      orderBy:nil
                                        limit: -1
                                   processRow: ^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       [items addObject: [[self fromDatabaseRow: row] memoizeOriginalSelfWhereClause]];
                                   }
                                        error: error];

    return items;
}

+ (NSArray*)fetch: (size_t)count error: (NSError * __autoreleasing *) error {
    return [self fetch: count where:nil orderBy:nil error:error];
}

+ (NSArray*)fetch: (size_t)count where:(NSDictionary*)whereDict error: (NSError * __autoreleasing *) error {
    return [self fetch: count where:whereDict orderBy:nil error:error];
}

+ (NSArray*)fetch:(size_t)count
            where:(NSDictionary*)whereDict
          orderBy:(NSArray*) orderColumns
            error:(NSError * __autoreleasing *) error {
    __block NSMutableArray* items = [[NSMutableArray alloc] init];

    [CKKSSQLDatabaseObject queryDatabaseTable: [self sqlTable]
                                        where: whereDict
                                      columns: [self sqlColumns]
                                      groupBy:nil
                                      orderBy:orderColumns
                                        limit: (ssize_t) count
                                   processRow: ^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       [items addObject: [[self fromDatabaseRow: row] memoizeOriginalSelfWhereClause]];
                                   }
                                        error: error];

    return items;
}

- (instancetype) memoizeOriginalSelfWhereClause {
    _originalSelfWhereClause = [self whereClauseToFindSelf];
    return self;
}

#pragma mark - Subclass methods

+ (instancetype)fromDatabaseRow:(NSDictionary<NSString *, CKKSSQLResult*>*)row {
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"A subclass must override %@", NSStringFromSelector(_cmd)]
                                 userInfo:nil];
}

+ (NSString*) sqlTable {
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"A subclass must override %@", NSStringFromSelector(_cmd)]
                                 userInfo:nil];
}

+ (NSArray<NSString*>*) sqlColumns {
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"A subclass must override %@", NSStringFromSelector(_cmd)]
                                 userInfo:nil];
}

- (NSDictionary<NSString*,NSString*>*) sqlValues {
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"A subclass must override %@", NSStringFromSelector(_cmd)]
                                 userInfo:nil];
}

- (NSDictionary<NSString*,NSString*>*) whereClauseToFindSelf {
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"A subclass must override %@", NSStringFromSelector(_cmd)]
                                 userInfo:nil];
}

- (instancetype)copyWithZone:(NSZone *)zone {
    CKKSSQLDatabaseObject *dbCopy = [[[self class] allocWithZone:zone] init];
    dbCopy->_originalSelfWhereClause = _originalSelfWhereClause;
    return dbCopy;
}
@end

NSString* CKKSSQLWhereComparatorAsString(CKKSSQLWhereComparator comparator)
{
    switch(comparator) {
        case CKKSSQLWhereComparatorEquals:
            return @"=";
        case CKKSSQLWhereComparatorNotEquals:
            return @"<>";
        case CKKSSQLWhereComparatorGreaterThan:
            return @">";
        case CKKSSQLWhereComparatorLessThan:
            return @"<";
    }
}

NSString* CKKSSQLWhereColumnNameAsString(CKKSSQLWhereColumnName columnName)
{
    switch(columnName) {
        case CKKSSQLWhereColumnNameUUID:
            return @"uuid";
        case CKKSSQLWhereColumnNameParentKeyUUID:
            return @"parentKeyUUID";
    }
}

#pragma mark - CKKSSQLWhereColumn

@implementation CKKSSQLWhereColumn
- (instancetype)initWithOperation:(CKKSSQLWhereComparator)op columnName:(CKKSSQLWhereColumnName)column
{
    if((self = [super init])) {
        _sqlOp = op;
        _columnName = column;
    }
    return self;
}
+ (instancetype)op:(CKKSSQLWhereComparator)op column:(CKKSSQLWhereColumnName)columnName
{
    return [[CKKSSQLWhereColumn alloc] initWithOperation:op columnName:columnName];
}
@end

#pragma mark - CKKSSQLWhereObject

@implementation CKKSSQLWhereValue
- (instancetype)initWithOperation:(CKKSSQLWhereComparator)op value:(NSString*)value
{
    if((self = [super init])) {
        _sqlOp = op;
        _value = value;
    }
    return self;
}
+ (instancetype)op:(CKKSSQLWhereComparator)op value:(NSString*)value
{
    return [[CKKSSQLWhereValue alloc] initWithOperation:op value:value];

}
@end

#pragma mark - CKKSSQLWhereIn

@implementation CKKSSQLWhereIn : NSObject
- (instancetype)initWithValues:(NSArray<NSString*>*)values
{
    if((self = [super init])) {
         _values = values;
    }
    return self;
}
@end
