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
#include <securityd/SecItemServer.h>

#import "keychain/ckks/CKKS.h"
#import "CKKSKeychainView.h"

@implementation CKKSSQLDatabaseObject

+ (bool) saveToDatabaseTable: (NSString*) table row: (NSDictionary*) row connection: (SecDbConnectionRef) dbconn error: (NSError * __autoreleasing *) error {
    __block CFErrorRef cferror = NULL;

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

        if([value class] == [CKKSSQLWhereObject class]) {
            // Use this string verbatim
            CKKSSQLWhereObject* whereob = (CKKSSQLWhereObject*) value;
            [whereClause appendFormat: @"%@%@%@", key, whereob.sqlOp, whereob.contents];
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

+ (bool) deleteFromTable: (NSString*) table where: (NSDictionary*) whereDict connection:(SecDbConnectionRef) dbconn error: (NSError * __autoreleasing *) error {
    __block CFErrorRef cferror = NULL;

    bool (^doWithConnection)(SecDbConnectionRef) = ^bool (SecDbConnectionRef dbconn) {
        NSString* whereClause = [CKKSSQLDatabaseObject makeWhereClause: whereDict];

        NSString * sql = [[NSString alloc] initWithFormat: @"DELETE FROM %@%@;", table, whereClause];
        SecDbPrepare(dbconn, (__bridge CFStringRef) sql, &cferror, ^void (sqlite3_stmt *stmt) {
            __block int whereObjectsSkipped = 0;

            [whereDict.allKeys enumerateObjectsUsingBlock:^(id  _Nonnull key, NSUInteger i, BOOL * _Nonnull stop) {
                if([whereDict[key] class] != [CKKSSQLWhereObject class]) {
                    SecDbBindObject(stmt, (int)(i+1-whereObjectsSkipped), (__bridge CFStringRef) whereDict[key], &cferror);
                } else {
                    whereObjectsSkipped += 1;
                }
            }];

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

+ (bool) queryDatabaseTable:(NSString*) table
                      where:(NSDictionary*) whereDict
                    columns:(NSArray*) names
                    groupBy:(NSArray*) groupColumns
                    orderBy:(NSArray*) orderColumns
                      limit:(ssize_t)limit
                 processRow:(void (^)(NSDictionary*)) processRow
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
            __block int whereObjectsSkipped = 0;
            [whereDict.allKeys enumerateObjectsUsingBlock:^(id  _Nonnull key, NSUInteger i, BOOL * _Nonnull stop) {
                if([whereDict[key] class] != [CKKSSQLWhereObject class]) {
                    SecDbBindObject(stmt, (int)(i+1-whereObjectsSkipped), (__bridge CFStringRef) whereDict[key], &cferror);
                } else {
                    whereObjectsSkipped += 1;
                }
            }];

            SecDbStep(dbconn, stmt, &cferror, ^(bool *stop) {
                __block NSMutableDictionary* row = [[NSMutableDictionary alloc] init];

                [names enumerateObjectsUsingBlock:^(id  _Nonnull name, NSUInteger i, BOOL * _Nonnull stop) {
                    const char * col = (const char *) sqlite3_column_text(stmt, (int)i);
                    row[name] = col ? [NSString stringWithUTF8String:col] : [NSNull null];
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

+ (bool)queryMaxValueForField:(NSString*)maxField inTable:(NSString*)table where:(NSDictionary*)whereDict columns:(NSArray*)names processRow:(void (^)(NSDictionary*))processRow
{
    __block CFErrorRef cferror = NULL;
    
    kc_with_dbt(false, &cferror, ^bool(SecDbConnectionRef dbconn) {
        NSString* columns = [[names componentsJoinedByString:@", "] stringByAppendingFormat:@", %@", maxField];
        NSString* whereClause = [CKKSSQLDatabaseObject makeWhereClause:whereDict];
        
        NSString* sql = [[NSString alloc] initWithFormat:@"SELECT %@ FROM %@%@", columns, table, whereClause];
        SecDbPrepare(dbconn, (__bridge CFStringRef)sql, &cferror, ^(sqlite3_stmt* stmt) {
            [whereDict.allKeys enumerateObjectsUsingBlock:^(id  _Nonnull key, NSUInteger i, BOOL* _Nonnull stop) {
                if ([whereDict[key] class] != [CKKSSQLWhereObject class]) {
                    SecDbBindObject(stmt, (int)(i+1), (__bridge CFStringRef) whereDict[key], &cferror);
                }
            }];
            
            SecDbStep(dbconn, stmt, &cferror, ^(bool*stop) {
                __block NSMutableDictionary* row = [[NSMutableDictionary alloc] init];
                
                [names enumerateObjectsUsingBlock:^(id  _Nonnull name, NSUInteger i, BOOL * _Nonnull stop) {
                    const char * col = (const char *) sqlite3_column_text(stmt, (int)i);
                    row[name] = col ? [NSString stringWithUTF8String:col] : [NSNull null];
                }];
                
                processRow(row);
            });
        });
        
        return true;
    });
    
    bool ret = (cferror == NULL);
    return ret;
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
                                   processRow: ^(NSDictionary* row) {
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
                                   processRow: ^(NSDictionary* row) {
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
                                   processRow: ^(NSDictionary* row) {
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

+ (instancetype) fromDatabaseRow:(NSDictionary *)row {
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

#pragma mark - CKKSSQLWhereObject

@implementation CKKSSQLWhereObject
- (instancetype)initWithOperation:(NSString*)op string: (NSString*) str {
    if(self = [super init]) {
        _sqlOp = op;
        _contents = str;
    }
    return self;
}

+ (instancetype)op:(NSString*) op string: (NSString*) str {
    return [[CKKSSQLWhereObject alloc] initWithOperation:op string: str];
}

+ (instancetype)op:(NSString*) op stringValue: (NSString*) str {
    return [[CKKSSQLWhereObject alloc] initWithOperation:op string:[NSString stringWithFormat:@"'%@'", str]];
}


@end
