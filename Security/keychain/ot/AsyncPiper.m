/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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
#import "AsyncPiper.h"
#import "ObjCImprovements.h"
#import "utilities/debugging.h"

static BOOL g_failPipe = NO;
static BOOL g_failXpcFdWrapping = NO;

@interface AsyncPiper ()
@property NSFileHandle* readHandle;
@property xpc_object_t writeXpcFd;
@property NSMutableData* bigData;
@property dispatch_queue_t queue;
@property dispatch_semaphore_t semaForTestingOnly;
@end

@implementation AsyncPiper

-(instancetype)initWithError:(NSError**)error {
    if (self = [super init]) {
        int fds[2] = {-1, -1};
        int err = g_failPipe ? EPERM : pipe(fds) ? errno : 0;
        if (err) {
            secerror("Could not create pipe: %d", err);
            if (error) {
                *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:nil];
            }
            return nil;
        }
        _readHandle = [[NSFileHandle alloc] initWithFileDescriptor:fds[0] closeOnDealloc:YES];
        _writeXpcFd = g_failXpcFdWrapping ? nil : xpc_fd_create(fds[1]);
        if (_writeXpcFd == nil) {
            err = g_failXpcFdWrapping ? EPERM : errno;
            secerror("Could not box FD: %d", err);
            if (error) {
                *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:nil];
            }
            return nil;
        }
        _bigData = [NSMutableData dataWithCapacity:0];
        _queue = dispatch_queue_create("AsyncPiper", DISPATCH_QUEUE_SERIAL);
        _semaForTestingOnly = nil;
        close(fds[1]);
    }
    return self;
}

// Unfortunately we can't simply read to EOF, because the writeXpcFd xpc_object_t will
// hold the write file descriptor open. And that object cannot go out of scope until
// the call completes, as it's a blocking call. But the remote side needs to write all
// data to complete the call and make the callback. So we must read while the call is
// happening so that the writes on the far side do not become blocked. We know the
// remote side is done sending when we encounter a nul sentinel bytes, which is not
// a valid JSON character.
-(void)moreData {
    dispatch_queue_t queue = self.queue;
    WEAKIFY(self);
    dispatch_async(queue, ^{
        STRONGIFY(self);
        secinfo("AsyncPiper", "Attempting to read data...");
        if (self.semaForTestingOnly) {
            dispatch_semaphore_signal(self.semaForTestingOnly);
        }
        NSData* more = self.readHandle.availableData;
        secinfo("AsyncPiper","Read %u bytes", (unsigned int)[more length]);
        if (more && more.length != 0) {
            [self.bigData appendData:more];
            NSUInteger lastByteIndex = self.bigData.length - 1;
            if (((unsigned char*)[self.bigData bytes])[lastByteIndex] == 0) {
                self.bigData.length = self.bigData.length - 1;
            } else {
                [self moreData];
            }
        }
        if (self.semaForTestingOnly) {
            dispatch_semaphore_signal(self.semaForTestingOnly);
        }
    });
}

// This method also starts reading data until the pipe is closed
-(xpc_object_t)xpcFd {
    [self moreData];
    return self.writeXpcFd;
}

-(void)waitAndReleaseFd_ForTestingOnly {
    self.semaForTestingOnly = dispatch_semaphore_create(0);
    [self moreData];
    dispatch_wait(self.semaForTestingOnly, DISPATCH_TIME_FOREVER);
    self.writeXpcFd = nil;
    dispatch_wait(self.semaForTestingOnly, DISPATCH_TIME_FOREVER);
}

-(NSDictionary*)dictWithError:(NSError**)errorOut {
    __block NSDictionary* theDict;
    __block NSError* error;
    dispatch_sync(self.queue, ^{
        theDict = [NSJSONSerialization JSONObjectWithData:self.bigData options:0 error:&error];
    });
    if (errorOut) {
        *errorOut = error;
    }
    return theDict;
}

@end

@implementation AsyncPiperFailPipeForTesting

-(instancetype)init {
    g_failPipe = YES;
    self = [super init];
    return self;
}

-(void)dealloc {
    g_failPipe = NO;
}

@end

@implementation AsyncPiperFailXpcFdWrappingForTesting

-(instancetype)init {
    g_failXpcFdWrapping = YES;
    self = [super init];
    return self;
}

-(void)dealloc {
    g_failXpcFdWrapping = NO;
}

@end
