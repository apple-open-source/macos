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

#import <Foundation/NSKeyedArchiver_Private.h>
#import "SFSQLite.h"
#import "SFSQLiteStatement.h"
#import "SFObjCType.h"
#import "utilities/debugging.h"

@interface SFSQLiteStatement ()
@property (nonatomic, strong) NSMutableArray *temporaryBoundObjects;
@end
@implementation SFSQLiteStatement

@synthesize SQLite = _SQLite;
@synthesize SQL = _SQL;
@synthesize handle = _handle;
@synthesize reset = _reset;
@synthesize temporaryBoundObjects = _temporaryBoundObjects;

- (id)initWithSQLite:(SFSQLite *)SQLite SQL:(NSString *)SQL handle:(sqlite3_stmt *)handle {
    if ((self = [super init])) {
        _SQLite = SQLite;
        _SQL = SQL;
        _handle = handle;
        _reset = YES;
    }
    return self;
}

- (void)finalizeStatement {
    if (!_reset) {
        secerror("sfsqlite: Statement not reset after last use: \"%@\"", _SQL);
        return;
    }
    if (sqlite3_finalize(_handle)) {
        secerror("sfsqlite: Error finalizing prepared statement: \"%@\"", _SQL);
        return;
    }
}

- (void)resetAfterStepError
{
    if (!_reset) {
        (void)sqlite3_reset(_handle); // we expect this to return an error
        (void)sqlite3_clear_bindings(_handle);
        [_temporaryBoundObjects removeAllObjects];
        _reset = YES;
    }
}

- (BOOL)step {
    if (_reset) {
        _reset = NO;
    }
    
    int rc = sqlite3_step(_handle);
    if ((rc & 0x00FF) == SQLITE_ROW) {
        return YES;
    } else if ((rc & 0x00FF) == SQLITE_DONE) {
        return NO;
    } else {
        [self resetAfterStepError];
        secerror("sfsqlite: Failed to step (%d): \"%@\"", rc, _SQL);
        return NO;
    }
}

- (void)reset {
    if (!_reset) {
        if (sqlite3_reset(_handle)) {
            secerror("sfsqlite: Error resetting prepared statement: \"%@\"", _SQL);
            return;
        }
        
        if (sqlite3_clear_bindings(_handle)) {
            secerror("sfsqlite: Error clearing prepared statement bindings: \"%@\"", _SQL);
            return;
        }
        [_temporaryBoundObjects removeAllObjects];
        _reset = YES;
    }
}

- (void)bindInt:(SInt32)value atIndex:(NSUInteger)index {
    if (!_reset) {
        secerror("sfsqlite: Statement is not reset: \"%@\"", _SQL);
        return;
    }
    
    if (sqlite3_bind_int(_handle, (int)index+1, value)) {
        secerror("sfsqlite: Error binding int at %ld: \"%@\"", (unsigned long)index, _SQL);
        return;
    }
}

- (void)bindInt64:(SInt64)value atIndex:(NSUInteger)index {
    if (!_reset) {
        secerror("sfsqlite: Statement is not reset: \"%@\"", _SQL);
        return;
    }
    
    if (sqlite3_bind_int64(_handle, (int)index+1, value)) {
        secerror("sfsqlite: Error binding int64 at %ld: \"%@\"", (unsigned long)index, _SQL);
        return;
    }
}

- (void)bindDouble:(double)value atIndex:(NSUInteger)index {
    if (!_reset) {
        secerror("sfsqlite: Statement is not reset: \"%@\"", _SQL);
        return;
    }
    
    if (sqlite3_bind_double(_handle, (int)index+1, value)) {
        secerror("sfsqlite: Error binding double at %ld: \"%@\"", (unsigned long)index, _SQL);
        return;
    }
}

- (void)bindBlob:(NSData *)value atIndex:(NSUInteger)index {
    if (!_reset) {
        secerror("sfsqlite: Statement is not reset: \"%@\"", _SQL);
        return;
    }
    
    if (value) {
        NS_VALID_UNTIL_END_OF_SCOPE NSData *arcSafeValue = value;
        if (sqlite3_bind_blob(_handle, (int)index+1, [arcSafeValue bytes], (int)[arcSafeValue length], NULL)) {
            secerror("sfsqlite: Error binding blob at %ld: \"%@\"", (unsigned long)index, _SQL);
            return;
        }
    } else {
        [self bindNullAtIndex:index];
    }
}

- (void)bindText:(NSString *)value atIndex:(NSUInteger)index {
    if (!_reset) {
        secerror("sfsqlite: Statement is not reset: \"%@\"", _SQL);
        return;
    }

    if (value) {
        NS_VALID_UNTIL_END_OF_SCOPE NSString *arcSafeValue = value;
        if (sqlite3_bind_text(_handle, (int)index+1, [arcSafeValue UTF8String], -1, NULL)) {
            secerror("sfsqlite: Error binding text at %ld: \"%@\"", (unsigned long)index, _SQL);
            return;
        }
    } else {
        [self bindNullAtIndex:index];
    }
}

- (void)bindNullAtIndex:(NSUInteger)index {
    int rc = sqlite3_bind_null(_handle, (int)index+1);
    if ((rc & 0x00FF) != SQLITE_OK) {
        secerror("sfsqlite: sqlite3_bind_null error");
        return;
    }
}

- (id)retainedTemporaryBoundObject:(id)object
{
    if (!_temporaryBoundObjects) {
        _temporaryBoundObjects = [NSMutableArray new];
    }
    [_temporaryBoundObjects addObject:object];
    return object;
}

- (void)bindValue:(id)value atIndex:(NSUInteger)index {
    if ([value isKindOfClass:[NSNumber class]]) {
        SFObjCType *type = [SFObjCType typeForValue:value];
        if (type.isIntegerNumber) {
            if (type.size <= 4) {
                [self bindInt:[value intValue] atIndex:index];
            } else {
                [self bindInt64:[value longLongValue] atIndex:index];
            }
        } else {
            NSAssert(type.isFloatingPointNumber, @"Expected number type to be either integer or floating point");
            NSAssert(type.code == SFObjCTypeDouble || type.code == SFObjCTypeFloat, @"Unexpected floating point number type: %@", type);
            [self bindDouble:[value doubleValue] atIndex:index];
        }
    } else if ([value isKindOfClass:[NSData class]]) {
        [self bindBlob:value atIndex:index];
    } else if ([value isKindOfClass:[NSUUID class]]) {
        uuid_t uuid;
        [(NSUUID *)value getUUIDBytes:uuid];
        [self bindBlob:[self retainedTemporaryBoundObject:[NSData dataWithBytes:uuid length:sizeof(uuid_t)]] atIndex:index];
    } else if ([value isKindOfClass:[NSString class]]) {
        [self bindText:value atIndex:index];
    } else if ([value isKindOfClass:[NSNull class]]) {
        [self bindNullAtIndex:index];
    } else if ([value isKindOfClass:[NSDate class]]) {
        [self bindDouble:[(NSDate *)value timeIntervalSinceReferenceDate] atIndex:index];
    } else if ([value isKindOfClass:[NSError class]]) {
        [self bindBlob:[self retainedTemporaryBoundObject:[NSKeyedArchiver archivedDataWithRootObject:value requiringSecureCoding:YES error:nil]] atIndex:index];
    } else if ([value isKindOfClass:[NSURL class]]) {
        [self bindText:[self retainedTemporaryBoundObject:[value absoluteString]] atIndex:index];
    } else {
        secerror("sfsqlite: Can't bind object of type %@", [value class]);
        return;
    }
}

- (void)bindValues:(NSArray *)values {
    for (NSUInteger i = 0; i < values.count; i++) {
        [self bindValue:values[i] atIndex:i];
    }
}

- (NSUInteger)columnCount {
    NSAssert(!_reset, @"Statement is reset: \"%@\"", _SQL);
    
    return sqlite3_column_count(_handle);
}

- (int)columnTypeAtIndex:(NSUInteger)index {
    NSAssert(!_reset, @"Statement is reset: \"%@\"", _SQL);
    
    return sqlite3_column_type(_handle, (int)index);
}

- (NSString *)columnNameAtIndex:(NSUInteger)index {
    NSAssert(!_reset, @"Statement is reset: \"%@\"", _SQL);
    
    return @(sqlite3_column_name(_handle, (int)index));
}

- (SInt32)intAtIndex:(NSUInteger)index {
    NSAssert(!_reset, @"Statement is reset: \"%@\"", _SQL);
    
    return sqlite3_column_int(_handle, (int)index);
}

- (SInt64)int64AtIndex:(NSUInteger)index {
    NSAssert(!_reset, @"Statement is reset: \"%@\"", _SQL);
    
    return sqlite3_column_int64(_handle, (int)index);
}

- (double)doubleAtIndex:(NSUInteger)index {
    NSAssert(!_reset, @"Statement is reset: \"%@\"", _SQL);
    
    return sqlite3_column_double(_handle, (int)index);
}

- (NSData *)blobAtIndex:(NSUInteger)index {
    NSAssert(!_reset, @"Statement is reset: \"%@\"", _SQL);
    
    const void *bytes = sqlite3_column_blob(_handle, (int)index);
    if (bytes) {
        int length = sqlite3_column_bytes(_handle, (int)index);
        return [NSData dataWithBytes:bytes length:length];
    } else {
        return nil;
    }
}

- (NSString *)textAtIndex:(NSUInteger)index {
    NSAssert(!_reset, @"Statement is reset: \"%@\"", _SQL);
    
    const char *text = (const char *)sqlite3_column_text(_handle, (int)index);
    if (text) {
        return @(text);
    } else {
        return nil;
    }
}

- (id)objectAtIndex:(NSUInteger)index {
    int type = [self columnTypeAtIndex:index];
    switch (type) {
        case SQLITE_INTEGER:
            return @([self int64AtIndex:index]);
            
        case SQLITE_FLOAT:
            return @([self doubleAtIndex:index]);
            
        case SQLITE_TEXT:
            return [self textAtIndex:index];
            
        case SQLITE_BLOB:
            return [self blobAtIndex:index];
            
        case SQLITE_NULL:
            return nil;
            
        default:
            secerror("sfsqlite: Unexpected column type: %d", type);
            return nil;
    }
}

- (NSArray *)allObjects {
    NSUInteger columnCount = [self columnCount];
    NSMutableArray *objects = [NSMutableArray arrayWithCapacity:columnCount];
    for (NSUInteger i = 0; i < columnCount; i++) {
        objects[i] = [self objectAtIndex:i] ?: [NSNull null];
    }
    return objects;
}

- (NSDictionary *)allObjectsByColumnName {
    NSUInteger columnCount = [self columnCount];
    NSMutableDictionary *objectsByColumnName = [NSMutableDictionary dictionaryWithCapacity:columnCount];
    for (NSUInteger i = 0; i < columnCount; i++) {
        NSString *columnName = [self columnNameAtIndex:i];
        id object = [self objectAtIndex:i];
        if (object) {
            objectsByColumnName[columnName] = object;
        }
    }
    return objectsByColumnName;
}

@end

#endif
