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

#ifndef DatabaseObject_h
#define DatabaseObject_h

#include <utilities/SecDb.h>
#include <securityd/SecDbItem.h>

#define CKKSNilToNSNull(obj) ({ id o = (obj); o ? o : [NSNull null]; })
#define CKKSNSNullToNil(obj) ({ id o = (obj); ([o isEqual: [NSNull null]]) ? nil : o; })

#define CKKSIsNull(x) ({ id y = (x); ((y == nil) || ([y isEqual: [NSNull null]])); })
#define CKKSUnbase64NullableString(x) (!CKKSIsNull(x) ? [[NSData alloc] initWithBase64EncodedString:x options:0] : nil)

@interface CKKSSQLDatabaseObject : NSObject <NSCopying> {

}

@property (copy) NSDictionary<NSString*,NSString*>* originalSelfWhereClause;

- (bool) saveToDatabase: (NSError * __autoreleasing *) error;
- (bool) saveToDatabaseWithConnection: (SecDbConnectionRef) conn error: (NSError * __autoreleasing *) error;
- (bool) deleteFromDatabase: (NSError * __autoreleasing *) error;
+ (bool) deleteAll: (NSError * __autoreleasing *) error;

// Load the object from the database, and error if it doesn't exist
+ (instancetype) fromDatabaseWhere: (NSDictionary*) whereDict error: (NSError * __autoreleasing *) error;

// Load the object from the database, and return nil if it doesn't exist
+ (instancetype) tryFromDatabaseWhere: (NSDictionary*) whereDict error: (NSError * __autoreleasing *) error;

+ (NSArray*) all: (NSError * __autoreleasing *) error;
+ (NSArray*) allWhere: (NSDictionary*) whereDict error: (NSError * __autoreleasing *) error;

// Like all() above, but with limits on how many will return
+ (NSArray*)fetch:(size_t)count error: (NSError * __autoreleasing *) error;
+ (NSArray*)fetch:(size_t)count where:(NSDictionary*)whereDict error: (NSError * __autoreleasing *) error;
+ (NSArray*)fetch: (size_t)count where:(NSDictionary*)whereDict orderBy:(NSArray*) orderColumns error: (NSError * __autoreleasing *) error;


+ (bool) saveToDatabaseTable: (NSString*) table row: (NSDictionary*) row connection: (SecDbConnectionRef) dbconn error: (NSError * __autoreleasing *) error;
+ (bool) deleteFromTable: (NSString*) table where: (NSDictionary*) whereDict connection:(SecDbConnectionRef) dbconn error: (NSError * __autoreleasing *) error;

+ (bool) queryDatabaseTable:(NSString*) table
                      where:(NSDictionary*) whereDict
                    columns:(NSArray*) names
                    groupBy:(NSArray*) groupColumns
                    orderBy:(NSArray*) orderColumns
                      limit:(ssize_t)limit
                 processRow:(void (^)(NSDictionary*)) processRow
                      error:(NSError * __autoreleasing *) error;

+ (bool)queryMaxValueForField:(NSString*)maxField inTable:(NSString*)table where:(NSDictionary*)whereDict columns:(NSArray*)names processRow:(void (^)(NSDictionary*))processRow;

// Note: if you don't use the SQLDatabase methods of loading yourself,
//  make sure you call this directly after loading.
- (instancetype) memoizeOriginalSelfWhereClause;

#pragma mark - Subclasses must implement the following:

// Given a row from the database, make this object
+ (instancetype) fromDatabaseRow: (NSDictionary*) row;

// Return the columns, in order, that this row wants to fetch
+ (NSArray<NSString*>*) sqlColumns;

// Return the table name for objects of this class
+ (NSString*) sqlTable;

// Return the columns and values, in order, that this row wants to save
- (NSDictionary<NSString*,NSString*>*) sqlValues;

// Return a set of key-value pairs that will uniquely find This Row in the table
- (NSDictionary<NSString*,NSString*>*) whereClauseToFindSelf;

- (instancetype)copyWithZone:(NSZone *)zone;
@end

// Helper class to use with where clauses
// If you pass in one of these instead of a concrete value, its substring will be used directly, instead of binding the value as a named parameter
@interface CKKSSQLWhereObject : NSObject
@property NSString* sqlOp;
@property NSString* contents;
- (instancetype) initWithOperation:(NSString*)op string: (NSString*) str;
+ (instancetype) op:(NSString*)op string:(NSString*) str;
+ (instancetype)op:(NSString*) op stringValue: (NSString*) str; // Will add single quotes around your value.
@end

#endif /* DatabaseObject_h */
