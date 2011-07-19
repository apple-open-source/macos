/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */
//
//  compaction.m
//  Copyright (c) 2010-2011 Apple Inc. All rights reserved.
//

#import "BlackBoxTest.h"

@interface MovableObject : NSObject
@property(readonly) uintptr_t originalSelf;
@property(readonly) BOOL moved;
@end
@implementation MovableObject
- (id)init {
    self = [super init];
    if (self) {
        originalSelf = (uintptr_t)self;
    }
    return self;
}
- (BOOL)moved {
    return (uintptr_t)self != originalSelf;
}
@end

@interface SelfPinnedObject : MovableObject {
    id extra[0];
}
@end

@implementation SelfPinnedObject

+ (id)new {
    return [NSAllocateObject(self, sizeof(id), NULL) init];
}

- (id)init {
    self = [super init];
    extra[0] = self;
    return self;
}
@end

@interface PinnedTest : BlackBoxTest {
    SelfPinnedObject *pinned;
}
@end

@implementation PinnedTest

- (id)init {
    self = [super init];
    pinned = [SelfPinnedObject new];
    return self;
}

- (NSString *)shouldSkip {
    if (getenv("AUTO_ANALYZE_NOTIFICATION") == NULL)
        return @"Need to turn on AUTO_ANALYZE_NOTIFICATION to check pinning.";
    return nil;
}

- (void)performTest {
    // first, analyze pinning, if object is pinned, then it won't bloody well move.
    [self requestCompactionAnalysisWithCompletionCallback:^{
        [self flushStderr];
        uintptr_t address = (uintptr_t)pinned;
        const char *path = getenv("AUTO_ANALYZE_NOTIFICATION");
        printf("looking for pinnedObject %p in %s.\n", pinned, path);
        FILE *f = fopen(path, "r");
        char *line; size_t length;
        while ((line = fgetln(f, &length)) != NULL) {
            line[length - 1] = '\0';
            char *tokens = line, *token;
            while ((token = strsep(&tokens, " ")) != NULL) {
                if (strcmp(token, "->") == 0) {
                    token = strsep(&tokens, " ");
                    if (strtoul(token, NULL, 0) == address) {
                        // scan for the reason.
                        NSString *reason = @"object was pinned.";
                        while ((token = strsep(&tokens, " ")) != NULL) {
                            if (token[0] == '<') {
                                reason = [NSString stringWithFormat:@"object was pinned, reason = %s", token];
                                break;
                            }
                        }
                        [self setTestResult:PASSED message:reason];
                        [self testFinished];
                        fclose(f);
                        return;
                    }
                    break;
                }
            }
        }
        fclose(f);
        [self fail:@"object not pinned as expected."];
        [self testFinished];
    }];
}
- (void)processOutputLine:(NSString *)line {}

@end

@interface CompactionTest : BlackBoxTest {
    MovableObject *movable;
    NSUInteger hashValue;
}
@end

@implementation CompactionTest

- (id)init {
    self = [super init];
    movable = [MovableObject new];
    hashValue = [movable hash];
    return self;
}

- (NSString *)shouldSkip {
    if (getenv("AUTO_COMPACTION_SCRAMBLE") == NULL)
        return @"setenv AUTO_COMPACTION_SCRAMBLE=YES to force compaction.";
    return nil;
}

- (void)performTest {
    // now wait for a compaction to finish, and verify that the object moved.
    [self requestCompactionWithCompletionCallback:^{
        [self flushStderr];
        if (movable.moved && (movable.hash == hashValue))
            [self setTestResult:PASSED message:@"object was compacted, hash value stayed constant."];
        else
            [self fail:@"object not compacted."];
        [self testFinished];
    }];
}
- (void)processOutputLine:(NSString *)line {}

@end
