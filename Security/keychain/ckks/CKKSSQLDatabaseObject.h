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

#include "keychain/securityd/SecDbItem.h"
#include <utilities/SecDb.h>

#define CKKSNilToNSNull(obj)   \
    ({                         \
        id o = (obj);          \
        o ? o : [NSNull null]; \
    })

#define CKKSIsNull(x)                                \
    ({                                               \
        id y = (x);                                  \
        ((y == nil) || ([y isEqual:[NSNull null]])); \
    })
#define CKKSUnbase64NullableString(x) (!CKKSIsNull(x) ? [[NSData alloc] initWithBase64EncodedString:x options:0] : nil)

NS_ASSUME_NONNULL_BEGIN

// A holder of a possibly-present string value.
// Also includes some convenience methods for parsing the string in different manners.
// If the value is nil, then the convenience methods return nil (or false/0).
@interface CKKSSQLResult : NSObject
- (instancetype)init:(NSString* _Nullable)value;

- (BOOL)asBOOL;
- (NSInteger)asNSInteger;
- (NSString* _Nullable)asString;
- (NSNumber* _Nullable)asNSNumberInteger;
- (NSDate* _Nullable)asISO8601Date;
- (NSData* _Nullable)asBase64DecodedData;
@end

// These thread-local variables should be set by your database layer
// This ensures that all SQL write operations are performed under the protection of a transaction.
// Use [CKKSSQLDatabaseObject +performCKKSTransaction] to get one of these if you don't have any other mechanism.
extern __thread bool CKKSSQLInTransaction;
extern __thread bool CKKSSQLInWriteTransaction;

typedef NS_ENUM(uint8_t, CKKSDatabaseTransactionResult) {
    CKKSDatabaseTransactionRollback = 0,
    CKKSDatabaseTransactionCommit = 1,
};

// A database provider must provide these operations.
@protocol CKKSDatabaseProviderProtocol
- (void)dispatchSyncWithSQLTransaction:(CKKSDatabaseTransactionResult (^)(void))block;
- (void)dispatchSyncWithReadOnlySQLTransaction:(void (^)(void))block;

// Used to maintain lock ordering.
- (BOOL)insideSQLTransaction;
@end

@interface CKKSSQLDatabaseObject : NSObject <NSCopying>

@property (copy) NSDictionary<NSString*, NSString*>* originalSelfWhereClause;

- (bool)saveToDatabase:(NSError* _Nullable __autoreleasing* _Nullable)error;
- (bool)saveToDatabaseWithConnection:(SecDbConnectionRef _Nullable)conn
                               error:(NSError* _Nullable __autoreleasing* _Nullable)error;
- (bool)deleteFromDatabase:(NSError* _Nullable __autoreleasing* _Nullable)error;
+ (bool)deleteAll:(NSError* _Nullable __autoreleasing* _Nullable)error;

// Load the object from the database, and error if it doesn't exist
+ (instancetype _Nullable)fromDatabaseWhere:(NSDictionary*)whereDict error:(NSError* _Nullable __autoreleasing* _Nullable)error;

// Load the object from the database, and return nil if it doesn't exist
+ (instancetype _Nullable)tryFromDatabaseWhere:(NSDictionary*)whereDict
                                         error:(NSError* _Nullable __autoreleasing* _Nullable)error;

+ (NSArray*)all:(NSError* _Nullable __autoreleasing* _Nullable)error;
+ (NSArray*)allWhere:(NSDictionary* _Nullable)whereDict error:(NSError* _Nullable __autoreleasing* _Nullable)error;

// Like all() above, but with limits on how many will return
+ (NSArray*)fetch:(size_t)count error:(NSError* _Nullable __autoreleasing* _Nullable)error;
+ (NSArray*)fetch:(size_t)count
            where:(NSDictionary* _Nullable)whereDict
            error:(NSError* _Nullable __autoreleasing* _Nullable)error;
+ (NSArray*)fetch:(size_t)count
            where:(NSDictionary* _Nullable)whereDict
          orderBy:(NSArray* _Nullable)orderColumns
            error:(NSError* _Nullable __autoreleasing* _Nullable)error;


+ (bool)saveToDatabaseTable:(NSString*)table
                        row:(NSDictionary*)row
                 connection:(SecDbConnectionRef _Nullable)dbconn
                      error:(NSError* _Nullable __autoreleasing* _Nullable)error;
+ (bool)deleteFromTable:(NSString*)table
                  where:(NSDictionary* _Nullable)whereDict
             connection:(SecDbConnectionRef _Nullable)dbconn
                  error:(NSError* _Nullable __autoreleasing* _Nullable)error;

+ (bool)queryDatabaseTable:(NSString*)table
                     where:(NSDictionary* _Nullable)whereDict
                   columns:(NSArray*)names
                   groupBy:(NSArray* _Nullable)groupColumns
                   orderBy:(NSArray* _Nullable)orderColumns
                     limit:(ssize_t)limit
                processRow:(void (^)(NSDictionary<NSString*, CKKSSQLResult*>*))processRow
                     error:(NSError* _Nullable __autoreleasing* _Nullable)error;

+ (bool)queryMaxValueForField:(NSString*)maxField
                      inTable:(NSString*)table
                        where:(NSDictionary* _Nullable)whereDict
                      columns:(NSArray*)names
                   processRow:(void (^)(NSDictionary<NSString*, CKKSSQLResult*>*))processRow;

// Note: if you don't use the SQLDatabase methods of loading yourself,
//  make sure you call this directly after loading.
- (instancetype)memoizeOriginalSelfWhereClause;

+ (NSString *)quotedString:(NSString *)string;

+ (BOOL)performCKKSTransaction:(CKKSDatabaseTransactionResult (^)(void))block;

#pragma mark - Subclasses must implement the following:

// Given a row from the database, make this object
+ (instancetype _Nullable)fromDatabaseRow:(NSDictionary<NSString *, CKKSSQLResult *>*)row;

// Return the columns, in order, that this row wants to fetch
+ (NSArray<NSString*>*)sqlColumns;

// Return the table name for objects of this class
+ (NSString*)sqlTable;

// Return the columns and values, in order, that this row wants to save
- (NSDictionary<NSString*, NSString*>*)sqlValues;

// Return a set of key-value pairs that will uniquely find This Row in the table
- (NSDictionary<NSString*, NSString*>*)whereClauseToFindSelf;

//- (instancetype)copyWithZone:(NSZone* _Nullable)zone;
@end

// Helper class to use with where clauses
// If you pass in one of these in a where dictionary instead of a concrete value, columnName will be
// used directly, instead of binding as a named parameter. Therefore, it's essential to use
// compile-time constants for both fields.

typedef NS_ENUM(uint64_t, CKKSSQLWhereComparator) {
    CKKSSQLWhereComparatorEquals = 1,
    CKKSSQLWhereComparatorNotEquals = 2,
    CKKSSQLWhereComparatorGreaterThan = 3,
    CKKSSQLWhereComparatorLessThan = 4,
};

NSString* CKKSSQLWhereComparatorAsString(CKKSSQLWhereComparator comparator);

// This typedef is to ensure that CKKSSQLWhereColumn can only ever produce static strings
typedef NS_ENUM(uint64_t, CKKSSQLWhereColumnName) {
    CKKSSQLWhereColumnNameUUID = 1,
    CKKSSQLWhereColumnNameParentKeyUUID = 2,
};
NSString* CKKSSQLWhereColumnNameAsString(CKKSSQLWhereColumnName columnName);

@interface CKKSSQLWhereColumn : NSObject
@property CKKSSQLWhereComparator sqlOp;
@property CKKSSQLWhereColumnName columnName;
- (instancetype)initWithOperation:(CKKSSQLWhereComparator)op columnName:(CKKSSQLWhereColumnName)column;
+ (instancetype)op:(CKKSSQLWhereComparator)op column:(CKKSSQLWhereColumnName)columnName;
@end

// Unlike CKKSSQLWhereColumn, this will insert the value as a parameter in a prepared statement
// but gives you the flexbility to inject a sqlOp. sqlOp must be a compile-time constant.
@interface CKKSSQLWhereValue : NSObject
@property CKKSSQLWhereComparator sqlOp;
@property NSString* value;
- (instancetype)initWithOperation:(CKKSSQLWhereComparator)op value:(NSString*)value;
+ (instancetype)op:(CKKSSQLWhereComparator)op value:(NSString*)value;
@end

@interface CKKSSQLWhereIn : NSObject
@property NSArray<NSString*>* values;
- (instancetype)initWithValues:(NSArray<NSString*>*)values;
@end


NS_ASSUME_NONNULL_END
