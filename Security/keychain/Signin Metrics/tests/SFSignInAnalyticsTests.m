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

#import <XCTest/XCTest.h>
#import "keychain/Signin Metrics/SFSignInAnalytics.h"
#import "keychain/Signin Metrics/SFSignInAnalytics+Internal.h"
#import "keychain/ot/OTDefines.h"
#import "SFAnalytics+Signin.h"
#import <Security/SecureObjectSync/SOSCloudCircle.h>

static NSInteger _testnum;
static NSString* _path;

@interface SFSignInAnalyticsTester : SFSignInAnalytics
-(instancetype)init;
@end

@implementation SFSignInAnalyticsTester

+ (NSString*)databasePath {
    return _path;
}

-(instancetype)init
{
    self = [super initWithSignInUUID:[NSUUID UUID].UUIDString category:@"CoreCDP" eventName:@"signin"];

    return self;
}

@end


@interface SignInAnalyticsTests : XCTestCase
@property (nonatomic) SFSignInAnalyticsTester *metric;
@end

@implementation SignInAnalyticsTests


- (void)setUp {
    [super setUp];
    _testnum = 0;
    self.continueAfterFailure = NO;
    _path = [@"/tmp" stringByAppendingFormat:@"/test_%ld.db", (long)++_testnum];
    _metric = [[SFSignInAnalyticsTester alloc] init];
    XCTAssertNotNil(_metric, "SignInAnalyticsTester object should not be nil");
}

- (void)tearDown
{
    dispatch_async([SFSIALoggerObject logger].queue, ^{
        [[SFSIALoggerObject logger].database executeSQL:@"delete from all_events"];
        [[SFSIALoggerObject logger].database executeSQL:@"delete from soft_failures"];
        [[SFSIALoggerObject logger].database executeSQL:@"delete from hard_failures"];
    });

    [[SFSIALoggerObject logger] removeState];
    _metric = nil;
    [super tearDown];
}

- (void)testStop
{
    sleep(2);
    NSDictionary *attributes = @{@"success": @YES,
                                 @"takenFlow" : @"restore",
                                 };
    XCTAssertNotNil(attributes, "attributes dictionary should exist");
    
    [_metric stopWithAttributes:attributes];

    NSArray* results = [[SFSIALoggerObject logger].database allEvents];

    XCTAssertEqual([results count], 2, @"should have 2 results");
    
}

- (void)testCancel
{
    [_metric cancel];
}

- (void)testLogError
{
    NSError* error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorBottleID userInfo:@{NSLocalizedDescriptionKey: @"Failed to deserialize bottle peer"}];
    [_metric logRecoverableError:error];
    NSArray* results = [[SFSIALoggerObject logger].database softFailures];
    XCTAssertEqual([results count], 1, @"should have 1 results");
}

- (void)testCreateNewSubtask
{
    SFSignInAnalytics* child = [_metric newSubTaskForEvent:@"restore"];
    XCTAssertNotNil(child, "child should be created");
    [[SFSIALoggerObject logger] removeState];
    child = nil;
}


- (void)testCreateNewSubtaskAndStop
{
    SFSignInAnalytics* child = [_metric newSubTaskForEvent:@"restore"];

    sleep(2);
    NSDictionary *attributes = @{@"success": @YES,
                                 @"takenFlow" : @"piggyback",
                                 };

    [child stopWithAttributes:attributes];
    
    XCTAssertNotNil(child, "child should be created");

    NSArray* results = [[SFSIALoggerObject logger].database allEvents];

    XCTAssertEqual([results count], 2, @"should have 2 results");

    [[SFSIALoggerObject logger] removeState];
    child = nil;
}

- (void)testStopAfterCancel
{
    sleep(2);
    NSDictionary *attributes = @{@"success": @YES,
                                 @"takenFlow" : @"piggyback",
                                 };
    XCTAssertNotNil(attributes, "attributes dictionary should exist");

    [_metric cancel];

    [_metric stopWithAttributes:attributes];

    NSArray *allEvents = [[SFSIALoggerObject logger].database allEvents];

    XCTAssertEqual([allEvents count], 0, @"should have 0 things logged");
}

- (void)testStopAfterStop
{
    sleep(2);
    NSDictionary *attributes = @{@"success": @YES,
                                 @"takenFlow" : @"piggyback",
                                 };
    XCTAssertNotNil(attributes, "attributes dictionary should exist");

    [_metric stopWithAttributes:attributes];

    [_metric stopWithAttributes:attributes];

    NSArray *allEvents = [[SFSIALoggerObject logger].database allEvents];

    XCTAssertEqual([allEvents count], 2, @"should have 2 things logged");
}

-(void)testSignInComplete
{
    NSDictionary* attributes = [NSDictionary dictionary];
    [_metric stopWithAttributes:attributes];

    [_metric signInCompleted];

    NSArray *allEvents = [[SFSIALoggerObject logger].database allEvents];
    XCTAssertNotNil(allEvents, "array should not be nil");
    XCTAssertTrue(allEvents && [allEvents count] > 0, "array should not be nil and contain an entry");

    NSDictionary *dependencyEntry = [allEvents objectAtIndex:[allEvents count]-1];
    XCTAssertTrue(dependencyEntry && [dependencyEntry count] > 0, "dictionary should not be nil and contain an entry");

    NSArray *chains = [dependencyEntry objectForKey:@"dependencyChains"];

    XCTAssertEqual([chains count], 1, "should be one list");

    XCTAssertTrue([chains containsObject:_metric.signin_uuid], "should contain 1 uuid");
}

-(void)testSingleChainDependencyList
{
    SFSignInAnalytics* child1 = [_metric newSubTaskForEvent:@"piggyback"];
    XCTAssertNotNil(child1, "child1 should be created");

    SFSignInAnalytics* child2 = [child1 newSubTaskForEvent:@"initialsync"];
    XCTAssertNotNil(child2, "child2 should be created");

    SFSignInAnalytics* child3 = [child2 newSubTaskForEvent:@"backup"];
    XCTAssertNotNil(child3, "child3 should be created");

    SFSignInAnalytics* child4 = [child3 newSubTaskForEvent:@"processing one ring"];
    XCTAssertNotNil(child4, "child4 should be created");

    [_metric signInCompleted];

    NSString *expectedChain = [NSString stringWithFormat:@"%@, %@, %@, %@, %@", child1.signin_uuid, child1.my_uuid, child2.my_uuid, child3.my_uuid, child4.my_uuid];

    NSArray *allEvents = [[SFSIALoggerObject logger].database allEvents];
    XCTAssertNotNil(allEvents, "should not be nil");
    XCTAssertTrue([allEvents count] > 0, "should be events");

    NSDictionary *dependencyEntry = [allEvents objectAtIndex:[allEvents count]-1];
    NSArray *chains = [dependencyEntry objectForKey:@"dependencyChains"];

    XCTAssertEqual([chains count], 1, "should be one list");
    XCTAssertTrue([expectedChain isEqualToString:[chains objectAtIndex:0]], "chains should be the same");

    child1 = nil;
    child2 = nil;
    child3 = nil;
    child4 = nil;

    child1 = nil;
    child2 = nil;
    child3 = nil;
    child4 = nil;
}

-(void)testMultipleChildrenPerEvent
{
    SFSignInAnalytics* child1 = [_metric newSubTaskForEvent:@"piggyback"];
    XCTAssertNotNil(child1, "child1 should be created");

    SFSignInAnalytics* child2 = [child1 newSubTaskForEvent:@"initialsync"];
    XCTAssertNotNil(child2, "child2 should be created");

    SFSignInAnalytics* child3 = [child1 newSubTaskForEvent:@"backup"];
    XCTAssertNotNil(child3, "child3 should be created");

    SFSignInAnalytics* child4 = [child1 newSubTaskForEvent:@"processing one ring"];
    XCTAssertNotNil(child4, "child4 should be created");

    SFSignInAnalytics* child5 = [child2 newSubTaskForEvent:@"processing second ring"];
    XCTAssertNotNil(child5, "child5 should be created");

    SFSignInAnalytics* child6 = [child2 newSubTaskForEvent:@"processing third ring"];
    XCTAssertNotNil(child6, "child6 should be created");

    SFSignInAnalytics* child7 = [child2 newSubTaskForEvent:@"processing fourth ring"];
    XCTAssertNotNil(child7, "child7 should be created");

    SFSignInAnalytics* child8 = [child7 newSubTaskForEvent:@"processing fifth ring"];
    XCTAssertNotNil(child8, "child8 should be created");

    SFSignInAnalytics* child9 = [child7 newSubTaskForEvent:@"processing one ring"];
    XCTAssertNotNil(child9, "child9 should be created");

    NSString *expectedChain = [NSString stringWithFormat:@"%@, %@, %@, %@", _metric.signin_uuid, child1.my_uuid, child2.my_uuid, child5.my_uuid];

    NSString *expectedChain1 = [NSString stringWithFormat:@"%@, %@, %@", _metric.signin_uuid, child1.my_uuid, child3.my_uuid];

    NSString *expectedChain2 = [NSString stringWithFormat:@"%@, %@, %@", _metric.signin_uuid, child1.my_uuid, child4.my_uuid];

    NSString *expectedChain3 = [NSString stringWithFormat:@"%@, %@, %@, %@", _metric.signin_uuid, child1.my_uuid, child2.my_uuid, child5.my_uuid];

    NSString *expectedChain4 = [NSString stringWithFormat:@"%@, %@, %@, %@, %@", _metric.signin_uuid, child1.my_uuid, child2.my_uuid, child7.my_uuid, child8.my_uuid];

    NSString *expectedChain5 = [NSString stringWithFormat:@"%@, %@, %@, %@, %@", _metric.signin_uuid, child1.my_uuid, child2.my_uuid, child7.my_uuid, child9.my_uuid];


    [_metric signInCompleted];

    NSArray *allEvents = [[SFSIALoggerObject logger].database allEvents];
    XCTAssertNotNil(allEvents, "array is not nil");
    XCTAssertTrue([allEvents count] > 0, "array should not be empty");

    NSDictionary *dependencyEntry = [allEvents objectAtIndex:[allEvents count]-1];
    NSArray *chains = [dependencyEntry objectForKey:@"dependencyChains"];

    XCTAssertEqual([chains count], 6, "should be one list");

    XCTAssertTrue([chains containsObject:expectedChain], "chains should contain expectedChain");
    XCTAssertTrue([chains containsObject:expectedChain1], "chains should contain expectedChain1");
    XCTAssertTrue([chains containsObject:expectedChain2], "chains should contain expectedChain2");
    XCTAssertTrue([chains containsObject:expectedChain3], "chains should contain expectedChain3");
    XCTAssertTrue([chains containsObject:expectedChain4], "chains should contain expectedChain4");
    XCTAssertTrue([chains containsObject:expectedChain5], "chains should contain expectedChain5");

    [[SFSIALoggerObject logger] removeState];

    child1 = nil;
    child2 = nil;
    child3 = nil;
    child4 = nil;
    child5 = nil;
    child6 = nil;
    child7 = nil;
    child8 = nil;
    child9 = nil;

}

-(void)testSOSCCWaitForInitialSync
{
    CFErrorRef error = nil;
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    XCTAssertNotNil(archiver, "should not be nil");
    [_metric encodeWithCoder:archiver];
    CFDataRef parentData = (__bridge CFDataRef)archiver.encodedData;
    XCTAssertNotNil((__bridge NSData*)parentData, "should not be nil");
    bool worked = SOSCCWaitForInitialSyncWithAnalytics(parentData, &error);
    XCTAssertTrue(worked, "should have worked");

    [_metric signInCompleted];

    NSArray *allEvents = [[SFSIALoggerObject logger].database allEvents];
    XCTAssertNotNil(allEvents, "array is not nil");
    XCTAssertEqual([allEvents count], 1, "array should not contain 1 entry");

    NSDictionary *dependencyEntry = [allEvents objectAtIndex:[allEvents count]-1];
    NSArray *chains = [dependencyEntry objectForKey:@"dependencyChains"];
    XCTAssertNotNil(chains, "chains is not nil");
    XCTAssertEqual([chains count], 1, "array should not contain 1 entry");
}

-(void)testSOSCCRemoveThisDeviceFromCircle
{
    CFErrorRef error = nil;
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    XCTAssertNotNil(archiver, "should not be nil");
    [_metric encodeWithCoder:archiver];
    CFDataRef parentData = (__bridge CFDataRef)archiver.encodedData;
    XCTAssertNotNil((__bridge NSData*)parentData, "should not be nil");
    bool worked = SOSCCRemoveThisDeviceFromCircleWithAnalytics(parentData, &error);
    XCTAssertTrue(worked, "should have worked");

    [_metric signInCompleted];

    NSArray *allEvents = [[SFSIALoggerObject logger].database allEvents];
    XCTAssertNotNil(allEvents, "array is not nil");
    XCTAssertEqual([allEvents count], 1, "array should not contain 1 entry");

    NSDictionary *dependencyEntry = [allEvents objectAtIndex:[allEvents count]-1];
    NSArray *chains = [dependencyEntry objectForKey:@"dependencyChains"];
    XCTAssertNotNil(chains, "chains is not nil");
    XCTAssertEqual([chains count], 1, "array should not contain 1 entry");
}

-(void)testSOSCCRequestToJoinCircle
{
    CFErrorRef error = nil;
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    XCTAssertNotNil(archiver, "should not be nil");
    [_metric encodeWithCoder:archiver];
    CFDataRef parentData = (__bridge CFDataRef)archiver.encodedData;
    XCTAssertNotNil((__bridge NSData*)parentData, "should not be nil");
    SOSCCRequestToJoinCircleWithAnalytics(parentData, &error);

    [_metric signInCompleted];

    NSArray *allEvents = [[SFSIALoggerObject logger].database allEvents];
    XCTAssertNotNil(allEvents, "array is not nil");
    XCTAssertEqual([allEvents count], 1, "array should not contain 1 entry");

    NSDictionary *dependencyEntry = [allEvents objectAtIndex:[allEvents count]-1];
    NSArray *chains = [dependencyEntry objectForKey:@"dependencyChains"];
    XCTAssertNotNil(chains, "chains is not nil");
    XCTAssertEqual([chains count], 1, "array should not contain 1 entry");
}

-(void)testSOSCCRequestToJoinCircleAfterRestore
{
    CFErrorRef error = nil;
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    XCTAssertNotNil(archiver, "should not be nil");
    [_metric encodeWithCoder:archiver];
    CFDataRef parentData = (__bridge CFDataRef)archiver.encodedData;
    XCTAssertNotNil((__bridge NSData*)parentData, "should not be nil");
    SOSCCRequestToJoinCircleAfterRestoreWithAnalytics(parentData, &error);

    [_metric signInCompleted];

    NSArray *allEvents = [[SFSIALoggerObject logger].database allEvents];
    XCTAssertNotNil(allEvents, "array is not nil");
    XCTAssertEqual([allEvents count], 1, "array should not contain 1 entry");

    NSDictionary *dependencyEntry = [allEvents objectAtIndex:[allEvents count]-1];
    NSArray *chains = [dependencyEntry objectForKey:@"dependencyChains"];
    XCTAssertNotNil(chains, "chains is not nil");
    XCTAssertEqual([chains count], 1, "array should not contain 1 entry");
}

-(void)testSOSCCRemovePeersFromCircle
{
    CFErrorRef error = nil;
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    XCTAssertNotNil(archiver, "should not be nil");
    [_metric encodeWithCoder:archiver];
    CFDataRef parentData = (__bridge CFDataRef)archiver.encodedData;
    XCTAssertNotNil((__bridge NSData*)parentData, "should not be nil");
    NSArray* peers = [NSArray array];
    SOSCCRemovePeersFromCircleWithAnalytics((__bridge CFArrayRef)peers, parentData, &error);

    [_metric signInCompleted];

    NSArray *allEvents = [[SFSIALoggerObject logger].database allEvents];
    XCTAssertNotNil(allEvents, "array is not nil");
    XCTAssertEqual([allEvents count], 1, "array should not contain 1 entry");

    NSDictionary *dependencyEntry = [allEvents objectAtIndex:[allEvents count]-1];
    NSArray *chains = [dependencyEntry objectForKey:@"dependencyChains"];
    XCTAssertNotNil(chains, "chains is not nil");
    XCTAssertEqual([chains count], 1, "array should not contain 1 entry");
}


-(void)testSOSCCViewSet
{
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    XCTAssertNotNil(archiver, "should not be nil");
    [_metric encodeWithCoder:archiver];
    CFDataRef parentData = (__bridge CFDataRef)archiver.encodedData;
    XCTAssertNotNil((__bridge NSData*)parentData, "should not be nil");
    CFSetRef enabledViews = nil;
    CFSetRef disabledViews = nil;
    SOSCCViewSetWithAnalytics(enabledViews, disabledViews, parentData);

    [_metric signInCompleted];

    NSArray *allEvents = [[SFSIALoggerObject logger].database allEvents];
    XCTAssertNotNil(allEvents, "array is not nil");
    XCTAssertEqual([allEvents count], 1, "array should not contain 1 entry");

    NSDictionary *dependencyEntry = [allEvents objectAtIndex:[allEvents count]-1];
    NSArray *chains = [dependencyEntry objectForKey:@"dependencyChains"];
    XCTAssertNotNil(chains, "chains is not nil");
    XCTAssertEqual([chains count], 1, "array should not contain 1 entry");
}

-(void)testSOSCCSetUserCredentialsAndDSID
{
    CFErrorRef error = nil;
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    XCTAssertNotNil(archiver, "should not be nil");
    [_metric encodeWithCoder:archiver];
    CFDataRef parentData = (__bridge CFDataRef)archiver.encodedData;
    XCTAssertNotNil((__bridge NSData*)parentData, "should not be nil");
    CFStringRef label = nil;
    CFDataRef password = nil;
    CFStringRef dsid = nil;
    SOSCCSetUserCredentialsAndDSIDWithAnalytics(label, password, dsid, parentData, &error);

    [_metric signInCompleted];

    NSArray *allEvents = [[SFSIALoggerObject logger].database allEvents];
    XCTAssertNotNil(allEvents, "array is not nil");
    XCTAssertEqual([allEvents count], 1, "array should not contain 1 entry");

    NSDictionary *dependencyEntry = [allEvents objectAtIndex:[allEvents count]-1];
    NSArray *chains = [dependencyEntry objectForKey:@"dependencyChains"];
    XCTAssertNotNil(chains, "chains is not nil");
    XCTAssertEqual([chains count], 1, "array should not contain 1 entry");
}

-(void)testSOSCCResetToEmpty
{
    CFErrorRef error = nil;
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    XCTAssertNotNil(archiver, "should not be nil");
    [_metric encodeWithCoder:archiver];
    CFDataRef parentData = (__bridge CFDataRef)archiver.encodedData;
    XCTAssertNotNil((__bridge NSData*)parentData, "should not be nil");
    SOSCCResetToEmptyWithAnalytics(parentData, &error);

    [_metric signInCompleted];

    NSArray *allEvents = [[SFSIALoggerObject logger].database allEvents];
    XCTAssertNotNil(allEvents, "array is not nil");
    XCTAssertEqual([allEvents count], 1, "array should not contain 1 entry");

    NSDictionary *dependencyEntry = [allEvents objectAtIndex:[allEvents count]-1];
    NSArray *chains = [dependencyEntry objectForKey:@"dependencyChains"];
    XCTAssertNotNil(chains, "chains is not nil");
    XCTAssertEqual([chains count], 1, "array should not contain 1 entry");
}

- (void)testMultipleDBConnections
{
    NSError* error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorBottleID userInfo:@{NSLocalizedDescriptionKey: @"Failed to deserialize bottle peer"}];
    dispatch_queue_t test_queue = dispatch_queue_create("com.apple.security.signin.tests", DISPATCH_QUEUE_CONCURRENT_WITH_AUTORELEASE_POOL);

    for(int i = 0; i < 1000; i++){
        SFSignInAnalytics *test1 = [_metric newSubTaskForEvent:@"connection1"];
        SFSignInAnalytics *test2 = [_metric newSubTaskForEvent:@"connection2"];

        dispatch_async(test_queue, ^{
            [test1 logRecoverableError:error];
        });
        dispatch_async(test_queue, ^{
            [test2 stopWithAttributes:nil];
        });
        dispatch_async(test_queue, ^{
            [self->_metric logRecoverableError:error];
        });
    }
}
@end
