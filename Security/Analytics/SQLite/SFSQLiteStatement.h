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

#if __OBJC2__

#import <Foundation/Foundation.h>
#import <sqlite3.h>

@class SFSQLite;

@interface SFSQLiteStatement : NSObject {
    __weak SFSQLite* _SQLite;
    NSString* _SQL;
    sqlite3_stmt* _handle;
    BOOL _reset;
    NSMutableArray* _temporaryBoundObjects;
}

- (id)initWithSQLite:(SFSQLite *)SQLite SQL:(NSString *)SQL handle:(sqlite3_stmt *)handle;

@property (nonatomic, readonly, weak)     SFSQLite     *SQLite;
@property (nonatomic, readonly, strong)   NSString       *SQL;
@property (nonatomic, readonly, assign)   sqlite3_stmt   *handle;

@property (nonatomic, assign, getter=isReset) BOOL reset;

- (BOOL)step;
- (void)reset;

- (void)finalizeStatement;

- (void)bindInt:(SInt32)value atIndex:(NSUInteger)index;
- (void)bindInt64:(SInt64)value atIndex:(NSUInteger)index;
- (void)bindDouble:(double)value atIndex:(NSUInteger)index;
- (void)bindBlob:(NSData *)value atIndex:(NSUInteger)index;
- (void)bindText:(NSString *)value atIndex:(NSUInteger)index;
- (void)bindNullAtIndex:(NSUInteger)index;
- (void)bindValue:(id)value atIndex:(NSUInteger)index;
- (void)bindValues:(NSArray *)values;

- (NSUInteger)columnCount;
- (int)columnTypeAtIndex:(NSUInteger)index;
- (NSString *)columnNameAtIndex:(NSUInteger)index;

- (SInt32)intAtIndex:(NSUInteger)index;
- (SInt64)int64AtIndex:(NSUInteger)index;
- (double)doubleAtIndex:(NSUInteger)index;
- (NSData *)blobAtIndex:(NSUInteger)index;
- (NSString *)textAtIndex:(NSUInteger)index;
- (id)objectAtIndex:(NSUInteger)index;
- (NSArray *)allObjects;
- (NSDictionary *)allObjectsByColumnName;

@end

#endif
