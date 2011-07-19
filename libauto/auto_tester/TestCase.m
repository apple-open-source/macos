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
//  TestCase.m
//  Copyright (c) 2009-2011 Apple Inc. All rights reserved.
//

#import "TestCase.h"
#import "Environment.h"
#import <objc/objc-auto.h>
#import <objc/objc-class.h>
#import <unistd.h>

#define assertTestQueue() do { \
if (dispatch_get_current_queue() != _testQueue) { \
    NSLog(@"%@ invoked on incorrect dispatch queue", NSStringFromSelector(_cmd)); abort(); \
}} while (0)

@implementation TestFinalizer : NSObject

- (id)init
{
    self = [super init];
    if (self) {
        _allocatingTest = [TestCase currentTestCase];
    }
    return self;
}

- (void)finalize
{
    TestCase *tc = [TestCase currentTestCase];
    // Here is a convenient place to test objc_is_finalized()
    if (!objc_is_finalized(self)) {
        [tc fail:[NSString stringWithFormat:@"objc_is_finalized(self) returns false during -finalize. self = %@, allocating = %@, running = %@", [self className], [_allocatingTest className], [tc className]]];
    }
    if (tc == _allocatingTest) {
        [tc didFinalize:self];
    }
    [super finalize];
}
@end

@implementation TestCase

static TestCase *_currentTestCase;

+ (id)currentTestCase
{
    return _currentTestCase;
}

+ (BOOL)isConcreteTestCase
{
    Class tc = [TestCase class];
    IMP startImp = [tc instanceMethodForSelector:@selector(performTest)];
    if ([self instanceMethodForSelector:@selector(performTest)] != startImp) {
        return YES;
    }
    return NO;
}

static NSInteger sortClassesByName(id cls1, id cls2, void* context) {
    return strcmp(class_getName(cls1), class_getName(cls2));
}

+ (NSArray *)testClasses
{
    static NSArray *_testClasses = nil;
    // Dynamically find all the subclasses of AutoTestScript
    if (_testClasses == nil) {
        NSMutableArray *testClasses = [[NSMutableArray alloc] init];
        int numClasses;
        Class * classes = NULL;
        Class tc = [TestCase class];
        IMP startImp = [tc instanceMethodForSelector:@selector(performTest)];
        
        classes = NULL;
        numClasses = objc_getClassList(NULL, 0);
        
        if (numClasses > 0 )
        {
            classes = (Class *)malloc(sizeof(Class) * numClasses);
            numClasses = objc_getClassList(classes, numClasses);
            int i;
            for (i=0; i<numClasses; i++) {
                // Some fumkiness to weed out weird clases like NSZombie and Object without actually messaging them.
                Class sc = class_getSuperclass(classes[i]);
                if (sc) {
                    Class ssc = class_getSuperclass(sc);
                    if (ssc) {
                        if ([classes[i] isSubclassOfClass:tc]) {
                            if ([classes[i] isConcreteTestCase])
                                [testClasses addObject:classes[i]];
                        }
                    }
                }
            }
            free(classes);
        }
        _testClasses = [testClasses sortedArrayUsingFunction:sortClassesByName context:NULL];
    }
    return _testClasses;
}

+ (void)initialize
{
    if (self == [TestCase class]) {
        Auto::Environment::initialize();
    }
}

+ (NSString *)resultStringForResult:(TestResult)resultCode
{
    NSString *result;
    switch (resultCode) {
        case PASSED:
            result = @"PASSED";
            break;
        case FAILED:
            result = @"FAILED";
            break;
        case SKIPPED:
            result = @"SKIPPED";
            break;
        default:
            result = @"UNKNOWN";
            break;
    }
    return result;
}


- (id)init
{
    self = [super init];
    if (self) {
        NSString *label = [NSString stringWithFormat:@"Test Case: %@", [self className]];
        _testQueue = dispatch_queue_create([label UTF8String], NULL);
        _result = PENDING;
        pthread_mutex_init(&_mutex, NULL);
        pthread_cond_init(&_cond, NULL);
        _orig_fd = -1;
    }
    return self;
}

- (void)finalize
{
    dispatch_release(_testQueue);
    [super finalize];
}


- (void)setCompletionCallback:(void (^)(void))completionCallback
{
    _completionCallback = Block_copy(completionCallback);
    _completionCallbackQueue = dispatch_get_current_queue();
    dispatch_retain(_completionCallbackQueue);
}

- (void)invokeCompletionCallback
{
    assertTestQueue();
    // safe to call this multiple times
    // do not really do it until we are really done
    if (_completionCallback && !_testThread && _outputCompleteCalled && _result != IN_PROGRESS) {
        if (_completionCallback) {
            _currentTestCase = nil;
            dispatch_async(_completionCallbackQueue, _completionCallback);
            Block_release(_completionCallback);
            dispatch_release(_completionCallbackQueue);
            _completionCallback = nil;
        }
    }
}

- (TestResult)result
{
    return _result;
}

- (NSString *)resultString
{
    TestResult r = [self result];
    NSString *result;
    if (_resultMessage)
        result = [NSString stringWithFormat:@"%@: %@", [TestCase resultStringForResult:r], _resultMessage];
    else
        result = [TestCase resultStringForResult:r];
    return result;
}

- (NSString *)testOutput
{
    return _testOutput;
}

- (void)setTestResult:(TestResult)result message:(NSString *)msg
{
    BOOL badTransition = NO;
    // sanity check that the result only changes in expected ways
    switch (_result) {
        case PENDING:
            if (result != IN_PROGRESS && result != SKIPPED)
                badTransition = YES;
            break;
        case IN_PROGRESS:
            if (result != PASSED && result != FAILED)
                badTransition = YES;
            break;
        case PASSED:
            // allow the result to change from passed->failed to simplify post-test output handling
            if (result != FAILED)
                badTransition = YES;
            break;
        case SKIPPED:
            badTransition = YES;
            break;
        case FAILED:
            if (result != FAILED)
                badTransition = YES;
            break;
        default:
            badTransition = YES;
    }
    if (badTransition) {
        // we might not be able to allocate here
        // peg the result to failed to get attention
        result = FAILED; 
        msg = @"Bogus test result transition";
    }
    _result = result;
    if (_resultMessage == nil)
        _resultMessage = msg;
}

- (BOOL)resurrectionIsFatal
{
    return Auto::Environment::resurrection_is_fatal ? YES : NO;
}

- (void)monitorOutput:(id)unused
{
    dispatch_async(_testQueue, ^{[self performTest];});
    char buf[512];
    NSMutableString *stringBuf = [NSMutableString string];
    int bytes;
    do {
        bytes = read(_fds[0], buf, sizeof(buf)-1);
        if (bytes > 0 && ![self failed]) {
            buf[bytes] = 0;
            [stringBuf appendFormat:@"%s", buf];
            if (!_testOutput)
                _testOutput = [NSMutableString string];
            [_testOutput appendFormat:@"%s", buf];
            NSRange newline;
            do {
                newline = [stringBuf rangeOfString:@"\n"];
                if (newline.location != NSNotFound) {
                    NSString *line = [stringBuf substringWithRange:NSMakeRange(0, newline.location)];
                    [stringBuf deleteCharactersInRange:NSMakeRange(0, newline.location+1)];
                    if ([line length] > 0) {
                        dispatch_async(_testQueue, ^{if (![self failed]) [self processOutputLine:line];});
                    }
                }
            } while (newline.location != NSNotFound);
        }
    } while (bytes > 0);
    if ([stringBuf length] > 0 && ![self failed]) {
        dispatch_async(_testQueue, ^{[self processOutputLine:stringBuf];});
    }
    dispatch_async(_testQueue, ^{
        [self outputComplete];     
    });
}

- (BOOL)shouldMonitorOutput
{
    return YES;
}

- (void)beginMonitoringOutput
{
    assertTestQueue();

    if ([self shouldMonitorOutput]) {
        // take over stderr
        if (pipe(_fds) == -1) {
            abort();
        }
        
        _orig_fd = dup(2);
        if (_orig_fd == -1) {
            abort();
        }
        if (dup2(_fds[1], 2) == -1) {
            abort();
        }
        
        [NSThread detachNewThreadSelector:@selector(monitorOutput:) toTarget:self withObject:nil];
    } else {
        dispatch_async(_testQueue, ^{[self performTest];});
    }
}

- (void)endMonitoringOutput
{
    assertTestQueue();
    // restore file descriptors
    if (_orig_fd != -1) {
        close(_fds[0]);
        close(_fds[1]);
        if (dup2(_orig_fd, 2)==-1)
            abort();
        _orig_fd = -1;
        _fds[0] = -1;
        _fds[1] = -1;
    } else {
        dispatch_async(_testQueue, ^{
            [self outputComplete];     
        });
    }
}

- (void)flushStderr
{
    assertTestQueue();
    [self endMonitoringOutput];
}

- (void)outputComplete;
{
    assertTestQueue();
    _outputCompleteCalled = YES;
    [self invokeCompletionCallback];
}


- (void)startTest
{
    NSLog(@"Running: %@", [self className]);
    _currentTestCase = self;
    [self setTestResult:IN_PROGRESS message:nil];
    [self runThreadLocalCollection];
    dispatch_async(_testQueue, ^{[self beginMonitoringOutput];});
}

- (void)performTest
{
    [self fail:[NSString stringWithFormat:@"%@ not overridden in test class: %@", NSStringFromSelector(_cmd), [self className]]];
}

- (NSString *)shouldSkip
{
    return nil;
}

- (void)processOutputLine:(NSString *)line
{
    NSString *collectionLog = @"[scan + freeze + finalize + reclaim]";
    NSRange r = [line rangeOfString:collectionLog];
    if (r.location != NSNotFound) {
        NSFileHandle *err;
        if (_orig_fd != -1)
            err = [[NSFileHandle alloc] initWithFileDescriptor:_orig_fd closeOnDealloc:NO];
        else
            err = [NSFileHandle fileHandleWithStandardError];
        [err writeData:[[line stringByAppendingString:@"\n"] dataUsingEncoding:NSMacOSRomanStringEncoding]];
    } else {
        [self fail:[NSString stringWithFormat:@"Unexpected console output: %@", line]];
    }
}

- (void)testFinished
{
    assertTestQueue();
    [self stopTestThread];
    [self endMonitoringOutput];
    [self invokeCompletionCallback];
}

- (void)passed
{
    if (![self failed]) {
        // resumptively set result to passed. may still change to failed based on output processing
        [self setTestResult:PASSED message:nil];
    }
}

- (void)fail:(NSString *)msg
{
    [self setTestResult:FAILED message:msg];
}


/* Misc convenience methods */

- (BOOL)failed
{
    return [self result] == FAILED;
}

- (void)collectWithOptions:(auto_zone_options_t)options
{
    auto_zone_collect([self auto_zone], options);
}

- (void)collectWithOptions:(auto_zone_options_t)options completionCallback:(void (^)(void))callback
{
    auto_zone_collect_and_notify([self auto_zone], options, _testQueue, callback);
}

- (void)runThreadLocalCollection
{
    [self collectWithOptions:AUTO_ZONE_COLLECT_LOCAL_COLLECTION];
}

- (void)requestAdvisoryCollectionWithCompletionCallback:(void (^)(void))callback;
{
    [self collectWithOptions:AUTO_ZONE_COLLECT_NO_OPTIONS completionCallback:callback];
}

- (void)requestGenerationalCollectionWithCompletionCallback:(void (^)(void))callback;
{
    [self collectWithOptions:AUTO_ZONE_COLLECT_GENERATIONAL_COLLECTION completionCallback:callback];
}

- (void)requestFullCollectionWithCompletionCallback:(void (^)(void))callback;
{
    [self collectWithOptions:AUTO_ZONE_COLLECT_FULL_COLLECTION completionCallback:callback];
}

- (void)requestExhaustiveCollectionWithCompletionCallback:(void (^)(void))callback;
{
    [self collectWithOptions:AUTO_ZONE_COLLECT_EXHAUSTIVE_COLLECTION completionCallback:callback];
}

- (void)requestCompactionWithCompletionCallback:(void (^)(void))callback {
    auto_zone_compact([self auto_zone], AUTO_ZONE_COMPACT_NO_OPTIONS, _testQueue, callback);
}

- (void)requestCompactionAnalysisWithCompletionCallback:(void (^)(void))callback {
    auto_zone_compact([self auto_zone], AUTO_ZONE_COMPACT_ANALYZE, _testQueue, callback);
}

- (void)clearStack
{
    objc_clear_stack(0);
}

- (void)didFinalize:(TestFinalizer *)finalizer
{
}

- (vm_address_t)disguise:(void *)pointer;
{
    return (vm_address_t)((intptr_t)pointer + 1);
}


- (void *)undisguise:(vm_address_t)disguisedPointer;
{
    return (void *)((intptr_t)disguisedPointer - 1);
}

- (BOOL)block:(void *)block isInList:(void **)blockList count:(uint32_t)count
{
    BOOL found = NO;
    uint32_t i;
    for (i = 0; i < count && !found; i++)
        found = (blockList[i] == block);
    return found;
}

- (auto_zone_t *)auto_zone
{
    return objc_collectableZone();
}







/* Test thread support */


#define STACK_BUFFER_SIZE 16
- (void)testThread
{
    pthread_mutex_lock(&_mutex);
    _testThread = pthread_self();
    SEL s;
    id stackBuffer[STACK_BUFFER_SIZE];
    bzero(stackBuffer, sizeof(stackBuffer));
    _disguisedStackBuffer = [self disguise:&stackBuffer[0]];
    do {
        while (_selector == nil)
            pthread_cond_wait(&_cond, &_mutex);
        s = _selector;
        if (s != (SEL)-1) {
            [self performSelector:s];
            [self clearStack];
        }
        _selector = nil;
        pthread_cond_signal(&_cond);
    } while (s != (SEL)-1);
    _disguisedStackBuffer = 0;
    _testThread = NULL;
    pthread_mutex_unlock(&_mutex);
}

void *test_thread_loop(void *arg) {
    objc_registerThreadWithCollector();
    TestCase *testCase = (TestCase *)arg;
    CFRelease(testCase);
    [testCase testThread];
    return NULL;
}

- (void)startTestThread
{
    if (_testThread != NULL) {
        NSLog(@"%@: attempted to start test thread twice", [self className]);
        exit(-1);
    }
    CFRetain(self);
    pthread_mutex_lock(&_mutex);
    if (pthread_create(&_testThread, NULL, test_thread_loop, self)) {
        NSLog(@"pthread_create() failed");
        exit(-1);
    }
    _selector = @selector(self);
    while (_selector != nil)
        pthread_cond_wait(&_cond, &_mutex);
    pthread_mutex_unlock(&_mutex);
}

- (void)stopTestThread
{
    pthread_mutex_lock(&_mutex);
    if (_testThread != NULL) {
        while (_selector != nil)
            pthread_cond_wait(&_cond, &_mutex);
        _selector = (SEL)-1;
        pthread_cond_signal(&_cond);
        while (_selector != nil)
            pthread_cond_wait(&_cond, &_mutex);
    }
    pthread_mutex_unlock(&_mutex);
}

- (void)performSelectorOnTestThread:(SEL)cmd
{
    if (!_testThread)
        [self startTestThread];
    pthread_mutex_lock(&_mutex);
    while (_selector != nil)
        pthread_cond_wait(&_cond, &_mutex);
    _selector = cmd;
    pthread_cond_signal(&_cond);
    while (_selector != nil)
        pthread_cond_wait(&_cond, &_mutex);
    pthread_mutex_unlock(&_mutex);
}

- (id *)testThreadStackBuffer
{
    if (pthread_self() != _testThread) {
        NSLog(@"%@ can only be called on the test thread", self);
        __builtin_trap();
    }
    return (id *)[self undisguise:_disguisedStackBuffer];
}

- (uintptr_t)testThreadStackBufferSize
{
    return STACK_BUFFER_SIZE;
}

@end
