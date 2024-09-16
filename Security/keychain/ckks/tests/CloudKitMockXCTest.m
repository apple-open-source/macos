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

#if OCTAGON

#import "CloudKitMockXCTest.h"

#import <ApplePushService/ApplePushService.h>
#import <Foundation/Foundation.h>
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#import <CloudKit/CKContainer_Private.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"
#import <OCMock/OCMock.h>
#pragma clang diagnostic pop

#import <TrustedPeers/TrustedPeers.h>
#import <TrustedPeers/TPPBPolicyKeyViewMapping.h>
#import <TrustedPeers/TPDictionaryMatchingRules.h>

#include "keychain/securityd/Regressions/SecdTestKeychainUtilities.h"
#include "utilities/SecFileLocations.h"
#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/SecItemDataSource.h"

#if NO_SERVER
#include "keychain/securityd/spi.h"
#endif

#include <Security/SecureObjectSync/SOSViews.h>

#include "utilities/SecDb.h"
#include "keychain/securityd/SecItemServer.h"
#include "keychain/ckks/CKKS.h"
#include "keychain/ckks/CKKSViewManager.h"
#include "keychain/ckks/CKKSKeychainView.h"
#include "keychain/ckks/CKKSItem.h"
#include "keychain/ckks/CKKSOutgoingQueueEntry.h"
#include "keychain/ckks/CKKSKey.h"
#include "keychain/ckks/CKKSGroupOperation.h"
#include "keychain/ckks/CKKSLockStateTracker.h"
#include "keychain/ckks/CKKSReachabilityTracker.h"

#include "keychain/ot/OT.h"
#include "keychain/ot/OTManager.h"
#import "keychain/ot/ObjCImprovements.h"

#import "tests/secdmockaks/mockaks.h"
#import "utilities/SecTapToRadar.h"

#import "MockCloudKit.h"

#include "keychain/ckks/CKKSAnalytics.h"
#include "Analytics/Clients/SOSAnalytics.h"

#import "keychain/ot/OTAccountsAdapter.h"
#import "keychain/ot/OTAuthKitAdapter.h"

#import "keychain/Sharing/KCSharingSupport.h"

@interface BoolHolder : NSObject
@property bool state;
@end

@implementation BoolHolder
@end

@implementation CloudKitAccount

- (instancetype)initWithAltDSID:(NSString*)altdsid
                        persona:(NSString* _Nullable)persona
                           hsa2:(bool)hsa2
                           demo:(bool)demo
                  accountStatus:(CKAccountStatus)accountStatus
{
    return [self initWithAltDSID:altdsid
                  appleAccountID:[NSString stringWithFormat:@"aaid-%@", altdsid]
                         persona:persona
                            hsa2:hsa2
                            demo:demo
                   accountStatus:accountStatus];
}

- (instancetype)initWithAltDSID:(NSString*)altdsid
                        persona:(NSString* _Nullable)persona
                           hsa2:(bool)hsa2
                           demo:(bool)demo
                  accountStatus:(CKAccountStatus)accountStatus
                      isPrimary:(bool)isPrimary
                isDataSeparated:(bool)isDataSeparated
{
    return [self initWithAltDSID:altdsid
                  appleAccountID:[NSString stringWithFormat:@"aaid-%@", altdsid]
                         persona:persona
                            hsa2:hsa2
                            demo:demo
                   accountStatus:accountStatus
                       isPrimary:isPrimary
                 isDataSeparated:isDataSeparated];
}

- (instancetype)initWithAltDSID:(NSString*)altdsid
                 appleAccountID:(NSString*)appleAccountID
                        persona:(NSString* _Nullable)persona
                           hsa2:(bool)hsa2
                           demo:(bool)demo
                  accountStatus:(CKAccountStatus)accountStatus
{
    return [self initWithAltDSID:altdsid
                  appleAccountID:appleAccountID
                         persona:persona
                            hsa2:hsa2
                            demo:demo
                   accountStatus:accountStatus
                       isPrimary:true
                 isDataSeparated:false];
}

- (instancetype)initWithAltDSID:(NSString*)altdsid
                 appleAccountID:(NSString*)appleAccountID
                        persona:(NSString* _Nullable)persona
                           hsa2:(bool)hsa2
                           demo:(bool)demo
                  accountStatus:(CKAccountStatus)accountStatus
                      isPrimary:(bool)isPrimary
                isDataSeparated:(bool)isDataSeparated
{
    if (!persona) {
        persona = [OTMockPersonaAdapter defaultMockPersonaString];
    }
    if (self = [super init]) {
        _altDSID = [[NSString alloc] initWithString:altdsid];
        _persona = [[NSString alloc] initWithString:persona];
        _appleAccountID = appleAccountID;
        _hsa2 = hsa2;
        _demo = demo;
        _accountStatus = accountStatus;
        _isPrimary = isPrimary;
        _isDataSeparated = isDataSeparated;
    }
    return self;
}
@end

@implementation CKKSTestsMockAccountsAuthKitAdapter

-(instancetype)initWithAltDSID:(NSString* _Nullable)altDSID
                     machineID:(NSString*)machineID
                  otherDevices:(NSSet<NSString*>*)otherDevices
{
    if (self = [super init]) {
        if (!altDSID) {
            altDSID = [NSUUID UUID].UUIDString;
        }
        
        @synchronized (_accounts) {
            _accounts = [NSMutableDictionary dictionary];
            _accounts[altDSID] = [[CloudKitAccount alloc]initWithAltDSID:altDSID persona:nil hsa2:true demo:false accountStatus:CKAccountStatusAvailable];
        }
    
        if (!machineID) {
            machineID = @"fake-machineID";
        }
        _currentMachineID = machineID;
   
        if (!otherDevices) {
            _otherDevices = [NSMutableSet set];
        } else {
           _otherDevices = otherDevices.mutableCopy;
        }
        _excludeDevices = [NSMutableSet set];
        _listeners = [[CKKSListenerCollection<OTAuthKitAdapterNotifier> alloc] initWithName:@"test-authkit"];
        
        _machineIDFetchErrors = [NSMutableArray array];
        
        // By default, you can fetch a list you're not on
        _injectAuthErrorsAtFetchTime = false;
        
    }
    return self;
}

- (void)setAccountStore:(ACAccountStore *)store
{
    //no op
}

+ (TPSpecificUser* _Nullable)userForAuthkit:(CKKSTestsMockAccountsAuthKitAdapter*)authkit
                              containerName:(NSString*)containerName
                                  contextID:(NSString*)contextID
                                      error:(NSError**)error
{
    return [authkit findAccountForCurrentThread:[[OTMockPersonaAdapter alloc]init] optionalAltDSID:nil cloudkitContainerName:containerName octagonContextID:contextID error:error];
}

- (TPSpecificUser * _Nullable)findAccountForCurrentThread:(nonnull id<OTPersonaAdapter>)personaAdapter
                                          optionalAltDSID:(NSString * _Nullable)altDSID
                                    cloudkitContainerName:(nonnull NSString *)cloudkitContainerName
                                         octagonContextID:(nonnull NSString *)octagonContextID
                                                    error:(NSError *__autoreleasing  _Nullable * _Nullable)error
{
    NSString* currentPersonaString = [personaAdapter currentThreadPersonaUniqueString];
    bool currentPersonaIsPrimary = [personaAdapter currentThreadIsForPrimaryiCloudAccount];
    
    if (altDSID) {
        for (CloudKitAccount* account in [self.accounts allValues]) {
            if ([account.altDSID isEqualToString: altDSID]) {
                if ([account.persona isEqualToString:currentPersonaString] || currentPersonaIsPrimary) {
                    NSString* personaUniqueString = nil;
                    // data separated accounts have personas
                    // primary accounts do not have personas
                    if (!account.isPrimary && account.isDataSeparated) {
                        personaUniqueString = account.persona;
                    }
                    return [[TPSpecificUser alloc] initWithCloudkitContainerName:cloudkitContainerName
                                                                octagonContextID:octagonContextID
                                                                  appleAccountID:account.appleAccountID
                                                                         altDSID:altDSID
                                                                isPrimaryPersona:account.isPrimary
                                                             personaUniqueString:personaUniqueString];
                } else {
                    if (error) {
                        *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNoAppleAccount userInfo:@{ NSLocalizedDescriptionKey : @"given altDSID does not match configured altDSID"}];
                    }
                    return nil;
                }
            }
        }
    } else {
        CloudKitAccount* currentPersonaAccount = nil;
        for (CloudKitAccount* account in [self.accounts allValues]) {
            if ([account.persona isEqualToString:currentPersonaString]) {
                currentPersonaAccount = self.accounts[account.altDSID];
            }
        }
        
        if (currentPersonaAccount == nil) {
            if (error) {
                *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNoAppleAccount userInfo:@{ NSLocalizedDescriptionKey : @"No current altDSID"}];
            }
            return nil;
        }
            
        NSString* personaUniqueString = nil;
        // data separated accounts have personas
        // primary accounts do not have personas
        if (!currentPersonaAccount.isPrimary && currentPersonaAccount.isDataSeparated) {
            personaUniqueString = currentPersonaAccount.persona;
        }
        return [[TPSpecificUser alloc]initWithCloudkitContainerName:cloudkitContainerName
                                                   octagonContextID:octagonContextID
                                                     appleAccountID:currentPersonaAccount.appleAccountID
                                                            altDSID:currentPersonaAccount.altDSID
                                                   isPrimaryPersona:currentPersonaAccount.isPrimary
                                                personaUniqueString:personaUniqueString];
    }
    return nil;
}

- (NSArray<TPSpecificUser *> * _Nullable)inflateAllTPSpecificUsers:(nonnull NSString *)cloudkitContainerName
                                                  octagonContextID:(nonnull NSString *)octagonContextID
{
    NSMutableArray<TPSpecificUser*>* activeAccounts = [NSMutableArray array];
   
    for (CloudKitAccount* account in [self.accounts allValues]) {
        NSString* personaUniqueString = nil;
        // data separated accounts have personas
        // primary accounts do not have personas
        if (!account.isPrimary && account.isDataSeparated) {
            personaUniqueString = account.persona;
        }
        TPSpecificUser* activeAccount = [[TPSpecificUser alloc] initWithCloudkitContainerName:cloudkitContainerName
                                                                             octagonContextID:octagonContextID
                                                                               appleAccountID:account.appleAccountID
                                                                                      altDSID:account.altDSID
                                                                             isPrimaryPersona:account.isPrimary
                                                                          personaUniqueString:personaUniqueString];
        
        [activeAccounts addObject:activeAccount];
    }
    
    return activeAccounts;
}

- (CloudKitAccount* _Nullable)accountForAltDSID:(NSString*)altDSID {
    return self.accounts[altDSID];
 }

- (BOOL)accountIsDemoAccountByAltDSID:(nonnull NSString *)altDSID error:(NSError *__autoreleasing  _Nullable * _Nullable)error {
    return [self accountForAltDSID:altDSID].demo ? YES : NO;
}

- (BOOL)accountIsCDPCapableByAltDSID:(nonnull NSString *)altDSID {
    return [self accountForAltDSID:altDSID].hsa2 ? YES : NO;
}

- (NSSet<NSString*>*)currentDeviceList {
       // Always succeeds.
    NSMutableSet<NSString*>* set = [[NSMutableSet alloc] initWithObjects:self.currentMachineID, nil];
    [set unionSet:self.otherDevices];
    
    for (NSString* excluded in self.excludeDevices) {
        [set removeObject:excluded];
    }
    
    return set;
}

- (void)fetchCurrentDeviceListByAltDSID:(NSString *)altDSID
                                 flowID:(NSString *)flowID
                        deviceSessionID:(NSString *)deviceSessionID
                                  reply:(void (^)(NSSet<NSString *> * _Nullable machineIDs,
                                                  NSSet<NSString*>* _Nullable userInitiatedRemovals,
                                                  NSSet<NSString*>* _Nullable evictedRemovals,
                                                  NSSet<NSString*>* _Nullable unknownReasonRemovals,
                                                  NSString* _Nullable version, 
                                                  NSString* _Nullable trustedDeviceHash,
                                                  NSString* _Nullable deletedDeviceHash,
                                                  NSNumber* _Nullable trustedDevicesUpdateTimestamp,
                                                  NSError * _Nullable error))complete
{
    self.fetchInvocations += 1;
    if ([self.machineIDFetchErrors count] > 0) {
        NSError* firstError = [self.machineIDFetchErrors objectAtIndex:0];
        complete(nil, nil, nil, nil, nil, nil, nil, nil, firstError);
        [self.machineIDFetchErrors removeObjectAtIndex:0];
        return;
    }
    
    // If this device is actively on the excluded list, return an error
    // But demo accounts can do what they want
    BOOL demo = [self accountForAltDSID:altDSID].demo ? YES : NO;
    if (demo == NO &&
        self.injectAuthErrorsAtFetchTime &&
        [self.excludeDevices containsObject:self.currentMachineID]) {
        complete(nil, nil, nil, nil, nil, nil, nil, nil, [NSError errorWithDomain:AKAppleIDAuthenticationErrorDomain code:-7026 userInfo:@{NSLocalizedDescriptionKey : @"Injected AKAuthenticationErrorNotPermitted error"}]);
        return;
    }
    if (self.fetchCondition) {
        [self.fetchCondition fulfill];
    }
    NSSet<NSString*>* deviceList = [self currentDeviceList];
    NSString* version = [NSString stringWithFormat:@"%lu", (unsigned long)[deviceList hash]];

    if (demo) {
        complete([self currentDeviceList], nil, nil, nil, version, nil, nil, nil, nil);
        return;
    }
    // TODO: fail if !accountPresent
    complete([self currentDeviceList], self.excludeDevices, nil, nil, version, nil, nil, nil, nil);
}

- (NSString * _Nullable)machineID:(NSString*)altDSID 
                           flowID:(NSString* _Nullable)flowID
                  deviceSessionID:(NSString* _Nullable)deviceSessionID
                   canSendMetrics:(BOOL)canSendMetrics
                            error:(NSError *__autoreleasing  _Nullable * _Nullable)error {
    return self.currentMachineID;
}

- (void)registerNotification:(id<OTAuthKitAdapterNotifier>)notifier {
    [self.listeners registerListener:notifier];
}

- (CloudKitAccount* _Nullable)primaryAccount {
    
    for (CloudKitAccount* account in [self.accounts allValues]) {
        if ([account.persona isEqualToString:[OTMockPersonaAdapter defaultMockPersonaString]]) {
            return account;
        }
    }
    return nil;
}

- (NSString* _Nullable) primaryAltDSID {
    return [self primaryAccount].altDSID;
}

- (void)add:(CloudKitAccount*) account {
    @synchronized (self.accounts) {
        self.accounts[account.altDSID] = account;
    }
}

- (void)removeAccountForPersona:(NSString*)persona {
    @synchronized (self.accounts) {
        NSString* copyPersona = [[NSString alloc] initWithString:persona];
        CloudKitAccount* found = nil;
        for (CloudKitAccount* account in [self.accounts allValues]) {
            if ([copyPersona isEqualToString:account.persona]) {
                found = account;
            }
        }
        if (found && found.altDSID) {
            [self.accounts removeObjectForKey:found.altDSID];
        }
    }
}

- (void)removePrimaryAccount {
    CloudKitAccount* primary = [self primaryAccount];
    @synchronized (self.accounts) {
        [self.accounts removeObjectForKey:primary.altDSID];
    }
}

- (void)sendIncompleteNotification {
    [self.listeners iterateListeners:^(id _Nonnull listener) {
        [listener notificationOfMachineIDListChange];
    }];
 }

- (void)addAndSendNotification:(NSString*)machineID  {
    [self.otherDevices addObject:machineID];
    [self.excludeDevices removeObject:machineID];
    
    [self sendAddNotification:machineID altDSID:[self primaryAltDSID]];
 }

- (void)deliverAKDeviceListDeltaMessagePayload:(NSDictionary* _Nullable)notificationDictionary
{
    //no-op
}

- (void)sendAddNotification:(NSString*)machineID altDSID:(NSString*)altDSID {
    [self.listeners iterateListeners:^(id _Nonnull listener) {
        [listener notificationOfMachineIDListChange];
    }];
 }

- (void)removeAndSendNotification:(NSString*)machineID {
    [self.excludeDevices addObject:machineID];
    [self sendRemoveNotification:machineID altDSID:[self primaryAltDSID]];
}

- (void)sendRemoveNotification:(NSString*)machineID altDSID:(NSString*)altDSID {
    [self.listeners iterateListeners:^(id _Nonnull listener) {
        [listener notificationOfMachineIDListChange];
    }];
 }

@end


@interface MockCKContainerObject : NSObject

// Needed from CKContainer
@property (readonly) CKContainerID* containerID;
@property (readonly) CKContainerOptions* options;

// Internal.
@property (readonly) id mockPrivateCloudDatabase;
@property (nullable) id mockContainerExpectations;
@property NSMutableArray<CKEventMetric*>* metrics;

@property (weak) CloudKitMockXCTest* test;
@property (strong) CKKSTestsMockAccountsAuthKitAdapter* accountsAuthkitAdapter;

- (instancetype)initWithContainerID:(CKContainerID*)containerID
                            options:(CKContainerOptions* _Nullable)options
               privateCloudDatabase:(id)privateCloudDatabase
              containerExpectations:(id)mockContainerExpectations
                               test:(CloudKitMockXCTest*)test;
@end

@implementation MockCKContainerObject

- (instancetype)initWithContainerID:(CKContainerID*)containerID
                            options:(CKContainerOptions* _Nullable)options
               privateCloudDatabase:(id)privateCloudDatabase
              containerExpectations:(id)mockContainerExpectations
                               test:(CloudKitMockXCTest*)test
{
    if((self = [super init])) {
        _containerID = containerID;
        _options = options;
        _mockPrivateCloudDatabase = privateCloudDatabase;
        _mockContainerExpectations = mockContainerExpectations;
        _test = test;

        _metrics = [NSMutableArray array];
    }
    return self;
}

- (NSString*)containerIdentifier
{
    return self.containerID.containerIdentifier;
}

- (void)serverPreferredPushEnvironmentWithCompletionHandler:(void (^)(NSString * _Nullable apsPushEnvString, NSError * _Nullable error))completionHandler
{
    completionHandler(@"fake APS push string", nil);
}

- (CKDatabase*)privateCloudDatabase
{
    return self.mockPrivateCloudDatabase;
}

- (void)accountInfoWithCompletionHandler:(void (^)(CKAccountInfo * _Nullable accountInfo, NSError * _Nullable error))completionHandler
{
    [self.test ckcontainerAccountInfoWithCompletionHandler:self.options.accountOverrideInfo.altDSID completionHandler:completionHandler];
}

- (void)submitEventMetric:(CKEventMetric *)eventMetric
{
    // NSMutableArrays are not thread-safe
    @synchronized(self.metrics) {
        [self.metrics addObject:eventMetric];
        
        @try {
            [self.mockContainerExpectations submitEventMetric:eventMetric];
        } @catch (NSException *exception) {
            XCTFail("Received an container exception when trying to add a metric: %@", exception);
        }        
    }
}

- (void)fetchCurrentDeviceIDWithCompletionHandler:(void (^)(NSString * _Nullable deviceID, NSError * _Nullable error))completionHandler
{
    completionHandler([NSString stringWithFormat:@"fake-cloudkit-device-id-%@", self.test.testName], nil);
}

- (void)reloadAccountWithCompletionHandler:(void (^)(NSError *_Nullable error))completionHandler
{
    completionHandler(nil);
}
@end

// Inform OCMock about the internals of CKContainer
@interface CKContainer ()
- (void)_checkSelfCloudServicesEntitlement;
@end

@implementation CKKSTestFailureLogger
- (instancetype)init {
    if((self = [super init])) {
    }
    return self;
}

- (void)testCase:(XCTestCase *)testCase didRecordIssue:(XCTIssue *)issue {
    ckksnotice_global("ckkstests", "XCTest failure: (%@)%@:%lu error: %@ -- %@\n%@",
                      testCase.name,
                      issue.sourceCodeContext.location.fileURL,
                      (unsigned long)issue.sourceCodeContext.location.lineNumber,
                      issue.compactDescription,
                      issue.detailedDescription,
                      issue.sourceCodeContext.callStack);
}
@end

@implementation CloudKitMockXCTest
static CKKSTestFailureLogger* _testFailureLoggerVariable;

+ (void)setUp {
    // Turn on testing
    SecCKKSEnable();
    SecCKKSTestsEnable();
    SecCKKSSetReduceRateLimiting(true);

    self.testFailureLogger = [[CKKSTestFailureLogger alloc] init];

    [[XCTestObservationCenter sharedTestObservationCenter] addTestObserver:self.testFailureLogger];

    [super setUp];

#if NO_SERVER
    securityd_init_local_spi();
#endif
}

+ (void)tearDown {
    [super tearDown];
    [[XCTestObservationCenter sharedTestObservationCenter] removeTestObserver:self.testFailureLogger];
}

+ (CKKSTestFailureLogger*)testFailureLogger {
    return _testFailureLoggerVariable;
}

+ (void)setTestFailureLogger:(CKKSTestFailureLogger*)logger {
    _testFailureLoggerVariable = logger;
}

- (BOOL)isRateLimited:(SecTapToRadar *)ttrRequest
{
    return self.isTTRRatelimited;
}

- (BOOL)askUserIfTTR:(SecTapToRadar *)ttrRequest
{
    return YES;
}

- (void)triggerTapToRadar:(SecTapToRadar *)ttrRequest
{
    [self.ttrExpectation fulfill];
}

- (void)setUpDirectories {
    if (self.dirSetup) {
        return;
    }

    // Create a temporary directory for keychains, analytics, and whatever else.
    self.testDir = [NSString stringWithFormat: @"/tmp/%@.%X", self.testName, arc4random()];
    [[NSFileManager defaultManager] createDirectoryAtPath:[NSString stringWithFormat: @"%@/Library/Keychains", self.testDir] withIntermediateDirectories:YES attributes:nil error:NULL];

    SecSetCustomHomeURLString((__bridge CFStringRef)self.testDir);
    self.dirSetup = YES;
}

- (void)cleanUpDirectories {
    // Bring the database down and delete it

    NSURL* keychainDir = (NSURL*)CFBridgingRelease(SecCopyHomeURL());

    // Only perform the destructive step if the url matches what we expect!
    if([keychainDir.path isEqualToString:self.testDir]) {
        secnotice("ckkstest", "Removing test-specific keychain directory at %@", keychainDir);

        NSError* removeError = nil;
        [[NSFileManager defaultManager] removeItemAtURL:keychainDir error:&removeError];
        XCTAssertNil(removeError, "Should have been able to remove temporary files");
     } else {
         XCTFail("Unsure what happened to the keychain directory URL: %@", keychainDir);
    }
    self.dirSetup = NO;
}

- (void)setUp {
    [super setUp];

    self.testName = [self.name componentsSeparatedByString:@" "][1];
    self.testName = [self.testName stringByReplacingOccurrencesOfString:@"]" withString:@""];

    secnotice("ckkstest", "Beginning test %@", self.testName);

    [self setUpDirectories];

    // All tests start with the same flag set.
    SecCKKSTestResetFlags();
    SecCKKSTestSetDisableSOS(true);
#if KCSHARING
    KCSharingSetDisabledForTests(true);
#endif  // KCSHARING

    self.silentFetchesAllowed = true;
    self.silentZoneDeletesAllowed = false; // Set to true if you want to do any deletes

    __weak __typeof(self) weakSelf = self;
    self.operationQueue = [[NSOperationQueue alloc] init];
    self.operationQueue.maxConcurrentOperationCount = 1;

    self.zones = self.zones ?: [[NSMutableDictionary alloc] init];

    // Static variables are a scourge. Let's reset this one...
    [OctagonAPSReceiver resetGlobalDelegatePortMap];

    self.mockDatabaseExceptionCatcher = OCMStrictClassMock([CKDatabase class]);
    self.mockDatabase = OCMStrictClassMock([CKDatabase class]);
    self.mockContainerExpectations = OCMStrictClassMock([CKContainer class]);
    self.mockContainer = OCMClassMock([CKContainer class]);
    OCMStub([self.mockContainer containerWithIdentifier:[OCMArg isKindOfClass:[NSString class]]]).andReturn(self.mockContainer);
    OCMStub([self.mockContainer alloc]).andReturn(self.mockContainer);
    OCMStub([self.mockContainer initWithContainerID: [OCMArg any] options: [OCMArg any]]).andCall(self, @selector(ckinitWithContainerID:options:));

    // Use two layers of mockDatabase here, so we can both add Expectations and catch the exception (instead of crash) when one fails.
    OCMStub([self.mockDatabaseExceptionCatcher addOperation:[OCMArg any]]).andCall(self, @selector(ckdatabaseAddOperation:));

    self.accountStatus = CKAccountStatusAvailable;
    self.ckAccountStatusFetchError = nil;
    self.iCloudHasValidCredentials = YES;
    self.fakeHSA2AccountStatus = CKKSAccountStatusAvailable;
    
    NSString* newAltDSID = [OTMockPersonaAdapter defaultMockPersonaString];
    NSSet* otherDevices = [[NSSet alloc] initWithObjects:@"MACHINE2", @"MACHINE3", nil];
    self.mockAccountsAuthKitAdapter = [[CKKSTestsMockAccountsAuthKitAdapter alloc]initWithAltDSID:newAltDSID machineID:@"MACHINE1" otherDevices:otherDevices];
    
    // Inject a fake operation dependency so we won't respond with the CloudKit account status immediately
    // The CKKSAccountStateTracker won't send any login/logout calls without that information, so this blocks all CKKS setup
    self.ckaccountHoldOperation = [NSBlockOperation named:@"ckaccount-hold" withBlock:^{
        ckksnotice_global("ckks", "CKKS CK account status test hold released");
    }];
    
    self.mockAccountStateTracker = OCMClassMock([CKKSAccountStateTracker class]);
    OCMStub([self.mockAccountStateTracker getCircleStatus]).andCall(self, @selector(circleStatus));

    // Fake out SOS peers
    // One trusted non-self peer, but it doesn't have any Octagon keys. Your test can change this if it wants.
    // However, note that [self putFakeDeviceStatusInCloudKit:] will likely not do what you want after you change this
    CKKSSOSSelfPeer* currentSelfPeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"local-peer"
                                                                    encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                       signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                         viewList:self.managedViewList];

    self.mockSOSAdapter = [[CKKSMockSOSPresentAdapter alloc] initWithSelfPeer:currentSelfPeer
                                                                 trustedPeers:[NSSet set]
                                                                    essential:YES];

    if(self.mockPersonaAdapter == nil) {
        // Other tests can set one of these up.
        self.mockPersonaAdapter = [[OTMockPersonaAdapter alloc] init];
    }

    OCMStub([self.mockAccountStateTracker fetchCirclePeerID:[OCMArg any]]).andCall(self, @selector(sosFetchCirclePeerID:));

    self.lockStateProvider = [[CKKSMockLockStateProvider alloc] initWithCurrentLockStatus:NO];
    self.aksLockState = false; // Lie and say AKS is always unlocked

    self.mockTTR = OCMClassMock([SecTapToRadar class]);
    OCMStub([self.mockTTR isRateLimited:[OCMArg any]]).andCall(self, @selector(isRateLimited:));
    OCMStub([self.mockTTR askUserIfTTR:[OCMArg any]]).andCall(self, @selector(askUserIfTTR:));
    OCMStub([self.mockTTR triggerTapToRadar:[OCMArg any]]).andCall(self, @selector(triggerTapToRadar:));
    self.isTTRRatelimited = true;

    self.mockFakeCKModifyRecordZonesOperation = OCMClassMock([FakeCKModifyRecordZonesOperation class]);
    OCMStub([self.mockFakeCKModifyRecordZonesOperation ckdb]).andReturn(self.zones);
    OCMStub([self.mockFakeCKModifyRecordZonesOperation shouldFailModifyRecordZonesOperation]).andCall(self, @selector(shouldFailModifyRecordZonesOperation));

    OCMStub([self.mockFakeCKModifyRecordZonesOperation ensureZoneDeletionAllowed:[OCMArg any]]).andCall(self, @selector(ensureZoneDeletionAllowed:));

    self.mockFakeCKModifySubscriptionsOperation = OCMClassMock([FakeCKModifySubscriptionsOperation class]);
    OCMStub([self.mockFakeCKModifySubscriptionsOperation ckdb]).andReturn(self.zones);

    self.mockFakeCKFetchRecordZoneChangesOperation = OCMClassMock([FakeCKFetchRecordZoneChangesOperation class]);
    OCMStub([self.mockFakeCKFetchRecordZoneChangesOperation ckdb]).andReturn(self.zones);
    OCMStub([self.mockFakeCKFetchRecordZoneChangesOperation isNetworkReachable]).andCall(self, @selector(isNetworkReachable));

    self.mockFakeCKFetchRecordsOperation = OCMClassMock([FakeCKFetchRecordsOperation class]);
    OCMStub([self.mockFakeCKFetchRecordsOperation ckdb]).andReturn(self.zones);

    self.mockFakeCKQueryOperation = OCMClassMock([FakeCKQueryOperation class]);
    OCMStub([self.mockFakeCKQueryOperation ckdb]).andReturn(self.zones);


    OCMStub([self.mockDatabase addOperation: [OCMArg checkWithBlock:^BOOL(id obj) {
        __strong __typeof(self) strongSelf = weakSelf;
        BOOL matches = NO;
        if ([obj isKindOfClass: [FakeCKFetchRecordZoneChangesOperation class]]) {
            if(strongSelf.silentFetchesAllowed) {
                matches = YES;

                FakeCKFetchRecordZoneChangesOperation *frzco = (FakeCKFetchRecordZoneChangesOperation *)obj;
                [frzco addNullableDependency:strongSelf.ckFetchHoldOperation];
                [strongSelf.operationQueue addOperation: frzco];
            }
        }
        return matches;
    }]]);

    OCMStub([self.mockDatabase addOperation: [OCMArg checkWithBlock:^BOOL(id obj) {
        __strong __typeof(self) strongSelf = weakSelf;
        BOOL matches = NO;
        if ([obj isKindOfClass: [FakeCKFetchRecordsOperation class]]) {
            if(strongSelf.silentFetchesAllowed) {
                matches = YES;

                FakeCKFetchRecordsOperation *ffro = (FakeCKFetchRecordsOperation *)obj;
                [ffro addNullableDependency:strongSelf.ckFetchHoldOperation];
                [strongSelf.operationQueue addOperation: ffro];
            }
        }
        return matches;
    }]]);

    OCMStub([self.mockDatabase addOperation: [OCMArg checkWithBlock:^BOOL(id obj) {
        __strong __typeof(self) strongSelf = weakSelf;
        BOOL matches = NO;
        if ([obj isKindOfClass: [FakeCKQueryOperation class]]) {
            if(strongSelf.silentFetchesAllowed) {
                matches = YES;

                FakeCKQueryOperation *fqo = (FakeCKQueryOperation *)obj;
                [fqo addNullableDependency:strongSelf.ckFetchHoldOperation];
                [strongSelf.operationQueue addOperation: fqo];
            }
        }
        return matches;
    }]]);

    OCMStub([self.mockDatabase addOperation: [OCMArg checkWithBlock:^BOOL(id obj) {
        __strong __typeof(self) strongSelf = weakSelf;
        BOOL matches = NO;
        if ([obj isKindOfClass: [FakeCKModifyRecordZonesOperation class]]) {
            FakeCKModifyRecordZonesOperation *frzco = (FakeCKModifyRecordZonesOperation *)obj;
            [frzco addNullableDependency:strongSelf.ckModifyRecordZonesHoldOperation];
            [strongSelf.operationQueue addOperation: frzco];
            matches = YES;
        }
        return matches;
    }]]);

    OCMStub([self.mockDatabase addOperation: [OCMArg checkWithBlock:^BOOL(id obj) {
        __strong __typeof(self) strongSelf = weakSelf;
        BOOL matches = NO;
        if ([obj isKindOfClass: [FakeCKModifySubscriptionsOperation class]]) {
            FakeCKModifySubscriptionsOperation *frzco = (FakeCKModifySubscriptionsOperation *)obj;
            [frzco addNullableDependency:strongSelf.ckModifySubscriptionsHoldOperation];
            [strongSelf.operationQueue addOperation: frzco];
            matches = YES;
        }
        return matches;
    }]]);

    self.testZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"testzone" ownerName:CKCurrentUserDefaultName];

    // We don't want to use class mocks here, because they don't play well with partial mocks
    CKKSCloudKitClassDependencies* cloudKitClassDependencies = [[CKKSCloudKitClassDependencies alloc] initWithFetchRecordZoneChangesOperationClass:[FakeCKFetchRecordZoneChangesOperation class]
                                                                                                                        fetchRecordsOperationClass:[FakeCKFetchRecordsOperation class]
                                                                                                                               queryOperationClass:[FakeCKQueryOperation class]
                                                                                                                 modifySubscriptionsOperationClass:[FakeCKModifySubscriptionsOperation class]
                                                                                                                   modifyRecordZonesOperationClass:[FakeCKModifyRecordZonesOperation class]
                                                                                                                                apsConnectionClass:[FakeAPSConnection class]
                                                                                                                         nsnotificationCenterClass:[FakeNSNotificationCenter class]
                                                                                                              nsdistributednotificationCenterClass:[FakeNSDistributedNotificationCenter class]
                                                                                                                                     notifierClass:[FakeCKKSNotifier class]];
    self.injectedOTManager = [self setUpOTManager:cloudKitClassDependencies];
    self.cloudKitClassDependencies = cloudKitClassDependencies;
    [OTManager resetManager:false to:self.injectedOTManager];

    self.mockCKKSViewManager = OCMPartialMock(self.injectedOTManager.viewManager);
    self.injectedManager = self.mockCKKSViewManager;

    self.defaultCKKS = [self.injectedManager ckksAccountSyncForContainer:SecCKKSContainerName
                                                               contextID:OTDefaultContext];
    XCTAssertNotNil(self.defaultCKKS, "Should have a default CKKS");
    
    __block NSString* ckID = nil;
    [self.defaultCKKS.container fetchCurrentDeviceIDWithCompletionHandler:^(NSString * _Nullable deviceID, NSError * _Nullable error) {
        XCTAssertNotNil(deviceID, "deviceID should not be nil");
        XCTAssertNil(error, "error should be nil");
        ckID = deviceID;
    }];
    
    _ckDeviceID = ckID;
    
    OCMStub([self.mockCKKSViewManager syncBackupAndNotifyAboutSync]);
    OCMStub([self.mockCKKSViewManager waitForTrustReady]).andReturn(YES);
    
    // Lie and say network is available
    [self.reachabilityTracker setNetworkReachability:true];

    SecKeychainDbReset(NULL);

    // Actually load the database.
    kc_with_dbt(true, NULL, ^bool (SecDbConnectionRef dbt) { return false; });

    if(!self.disableConfigureCKKSViewManagerWithViews) {
        // Normally, the Octagon state machine calls this. But, since we won't be running that, help it out.
        // CKKS might try to take a DB lock, so do this after the DB load above
        [self.defaultCKKS setCurrentSyncingPolicy:self.viewSortingPolicyForManagedViewList];
    }
}

- (CKContainer*)ckinitWithContainerID:(CKContainerID*)containerID options:(CKContainerOptions*)options
{
    return (CKContainer*)[[MockCKContainerObject alloc] initWithContainerID:containerID
                                                                    options:options
                                                       privateCloudDatabase:self.mockDatabaseExceptionCatcher
                                                      containerExpectations:self.mockContainerExpectations
                                                                       test:self];
}

- (OTManager*)setUpOTManager:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
{
    return [[OTManager alloc] initWithSOSAdapter:self.mockSOSAdapter
                                lockStateTracker:[[CKKSLockStateTracker alloc] initWithProvider:self.lockStateProvider]
                                  personaAdapter:self.mockPersonaAdapter
                       cloudKitClassDependencies:cloudKitClassDependencies];
}

- (SOSAccountStatus*)circleStatus {
    NSError* error = nil;
    SOSCCStatus status = [self.mockSOSAdapter circleStatus:&error];
    return [[SOSAccountStatus alloc] init:status error:error];
}

- (bool)aksLockState
{
    return self.lockStateProvider.aksCurrentlyLocked;
}

- (void)setAksLockState:(bool)aksLockState
{
    ckksnotice_global("ckkstests", "Setting mock AKS lock state to: %@", (aksLockState ? @"locked" : @"unlocked"));
    self.mockSOSAdapter.aksLocked = aksLockState ? YES : NO;
    self.lockStateProvider.aksCurrentlyLocked = aksLockState;
}

- (bool)isNetworkReachable {
    return self.reachabilityTracker.currentReachability;
}

- (void)sosFetchCirclePeerID:(void (^)(NSString* _Nullable peerID, NSError* _Nullable error))callback
{
    if(callback) {
        // If we're in circle, come up with a fake circle id. Otherwise, return an error.
        if(self.mockSOSAdapter.circleStatus == kSOSCCInCircle) {
            callback(self.mockSOSAdapter.selfPeer.peerID, nil);
        } else {
            callback(nil, [NSError errorWithDomain:@"securityd"
                                              code:errSecInternalError
                                          userInfo:@{NSLocalizedDescriptionKey:@"no account, no circle id"}]);
        }
    }
}

- (void)ckcontainerAccountInfoWithCompletionHandler:(NSString*)altDSID completionHandler:(void (^)(CKAccountInfo * _Nullable accountInfo, NSError * _Nullable error))completionHandler
{
    __weak __typeof(self) weakSelf = self;
    
    if (OctagonSupportsPersonaMultiuser()) {
        NSBlockOperation* fulfillBlock = [NSBlockOperation named:@"account-info-completion" withBlock: ^{
            __strong __typeof(self) blockStrongSelf = weakSelf;
            CloudKitAccount* ckAccount = blockStrongSelf.mockAccountsAuthKitAdapter.accounts[altDSID];
            CKAccountInfo* account = nil;
            NSError* ckAccountStatusFetchError = nil;
            if (ckAccount == nil) {
                FakeCKAccountInfo *fakeAccount = [[FakeCKAccountInfo alloc] init];
                fakeAccount.accountStatus = blockStrongSelf.accountStatus;
                fakeAccount.hasValidCredentials = blockStrongSelf.iCloudHasValidCredentials;
                fakeAccount.accountPartition = CKAccountPartitionTypeProduction;
                account = (CKAccountInfo *)fakeAccount;
                ckAccountStatusFetchError = blockStrongSelf.ckAccountStatusFetchError;
            } else {
                FakeCKAccountInfo *fakeAccount = [[FakeCKAccountInfo alloc] init];
                fakeAccount.accountStatus = ckAccount.accountStatus;
                fakeAccount.hasValidCredentials = ckAccount.iCloudHasValidCredentials;
                fakeAccount.accountPartition = CKAccountPartitionTypeProduction;
                account = (CKAccountInfo *)fakeAccount;
                ckAccountStatusFetchError = ckAccount.ckAccountStatusFetchError;
            }
            if(completionHandler) {
                completionHandler(account, ckAccountStatusFetchError);
            }
        }];
        
        [fulfillBlock addNullableDependency:self.ckaccountHoldOperation];
        [self.operationQueue addOperation:fulfillBlock];
    } else {
        NSBlockOperation* fulfillBlock = [NSBlockOperation named:@"account-info-completion" withBlock: ^{
            __strong __typeof(self) blockStrongSelf = weakSelf;
            FakeCKAccountInfo *fakeAccount = [[FakeCKAccountInfo alloc] init];
            fakeAccount.accountStatus = blockStrongSelf.accountStatus;
            fakeAccount.hasValidCredentials = blockStrongSelf.iCloudHasValidCredentials;
            fakeAccount.accountPartition = CKAccountPartitionTypeProduction;
            CKAccountInfo *account = (CKAccountInfo *)fakeAccount;
            
            if(completionHandler) {
                completionHandler(account, blockStrongSelf.ckAccountStatusFetchError);
            }
        }];
        
        [fulfillBlock addNullableDependency:self.ckaccountHoldOperation];
        [self.operationQueue addOperation:fulfillBlock];
    }
}

- (void)ckdatabaseAddOperation:(NSOperation*)op {
    @try {
        [self.mockDatabase addOperation:op];
    } @catch (NSException *exception) {
        XCTFail("Received an database exception: %@", exception);
    }
}

- (NSError* _Nullable)shouldFailModifyRecordZonesOperation {
    NSError* error = self.nextModifyRecordZonesError;
    if(error) {
        self.nextModifyRecordZonesError = nil;
        return error;
    }
    return nil;
}

- (void)ensureZoneDeletionAllowed:(FakeCKZone*)zone {
    XCTAssertTrue(self.silentZoneDeletesAllowed, "Should be allowing zone deletes");
}

- (CKKSAccountStateTracker*)accountStateTracker {
    return self.injectedOTManager.accountStateTracker;
}

-(CKKSLockStateTracker*)lockStateTracker {
    return self.injectedOTManager.lockStateTracker;
}

-(CKKSReachabilityTracker*)reachabilityTracker {
    return self.injectedManager.reachabilityTracker;
}

-(NSSet*)managedViewList {
    return (NSSet*) CFBridgingRelease(SOSViewCopyViewSet(kViewSetCKKS));
}

- (TPSyncingPolicy*)viewSortingPolicyForManagedViewList
{
    return [self viewSortingPolicyForManagedViewListWithUserControllableViews:[NSSet set]
                                                    syncUserControllableViews:TPPBPeerStableInfoUserControllableViewStatus_ENABLED];
}
 
- (TPSyncingPolicy*)viewSortingPolicyForManagedViewListWithUserControllableViews:(NSSet<NSString*>*)ucv
                                                       syncUserControllableViews:(TPPBPeerStableInfoUserControllableViewStatus)syncUserControllableViews
{
    NSMutableArray<TPPBPolicyKeyViewMapping*>* rules = [NSMutableArray array];

    for(NSString* viewName in self.managedViewList) {
        TPPBPolicyKeyViewMapping* mapping = [[TPPBPolicyKeyViewMapping alloc] init];
        mapping.view = viewName;
        mapping.matchingRule = [TPDictionaryMatchingRule fieldMatch:@"vwht"
                                                         fieldRegex:[NSString stringWithFormat:@"^%@$", viewName]];

        [rules addObject:mapping];
    }

    TPSyncingPolicy* policy = [[TPSyncingPolicy alloc] initWithModel:@"test-policy"
                                                             version:[[TPPolicyVersion alloc] initWithVersion:1 hash:@"fake-policy-for-views"]
                                                            viewList:[self managedViewList]
                                                       priorityViews:[NSSet set]
                                               userControllableViews:ucv
                                           syncUserControllableViews:syncUserControllableViews
                                                viewsToPiggybackTLKs:[NSSet set]
                                                      keyViewMapping:rules
                                                  isInheritedAccount:NO];
    return policy;
}

-(void)expectCKFetch {
    [self expectCKFetchAndRunBeforeFinished: nil];
}

-(void)expectCKFetchAndRunBeforeFinished: (void (^)(void))blockAfterFetch {
    [self expectCKFetchWithFilter:^BOOL(FakeCKFetchRecordZoneChangesOperation * op) {
        return YES;
    }
                runBeforeFinished:blockAfterFetch];
}

- (void)expectCKFetchForZones:(NSSet<CKRecordZoneID*>*)expectedZoneIDs
            runBeforeFinished:(void (^)(void))blockAfterFetch
{
    [self expectCKFetchWithFilter:^BOOL(FakeCKFetchRecordZoneChangesOperation * _Nonnull frzco) {
        NSSet<CKRecordZoneID*>* fetchedZoneIDs = [NSSet setWithArray:frzco.recordZoneIDs];
        XCTAssertEqualObjects(fetchedZoneIDs, expectedZoneIDs, "Should fetch intended view set");
        return YES;
    }
                runBeforeFinished:blockAfterFetch];
}

- (void)expectCKFetchWithFilter:(BOOL (^)(FakeCKFetchRecordZoneChangesOperation*))operationMatch
              runBeforeFinished:(void (^)(void))blockAfterFetch
{
    // Create an object for the block to retain and modify
    BoolHolder* runAlready = [[BoolHolder alloc] init];

    __weak __typeof(self) weakSelf = self;
    [[self.mockDatabase expect] addOperation: [OCMArg checkWithBlock:^BOOL(id obj) {
        __strong __typeof(self) strongSelf = weakSelf;
        if(runAlready.state) {
            return NO;
        }

        secnotice("fakecloudkit", "Received an operation (%@), checking if it's a fetch changes", obj);
        BOOL matches = NO;
        if ([obj isKindOfClass: [FakeCKFetchRecordZoneChangesOperation class]]) {
            FakeCKFetchRecordZoneChangesOperation *frzco = (FakeCKFetchRecordZoneChangesOperation *)obj;
            matches = operationMatch(frzco);
            runAlready.state = true;

            secnotice("fakecloudkit", "Running fetch changes: %@", obj);
            frzco.blockAfterFetch = blockAfterFetch;
            [frzco addNullableDependency: strongSelf.ckFetchHoldOperation];
            [strongSelf.operationQueue addOperation: frzco];
        }
        return matches;
    }]];
}

-(void)expectCKFetchByRecordID {
    // Create an object for the block to retain and modify
    BoolHolder* runAlready = [[BoolHolder alloc] init];

    __weak __typeof(self) weakSelf = self;
    [[self.mockDatabase expect] addOperation: [OCMArg checkWithBlock:^BOOL(id obj) {
        __strong __typeof(self) strongSelf = weakSelf;
        if(runAlready.state) {
            return NO;
        }
        BOOL matches = NO;
        if ([obj isKindOfClass: [FakeCKFetchRecordsOperation class]]) {
            matches = YES;
            runAlready.state = true;

            FakeCKFetchRecordsOperation *ffro = (FakeCKFetchRecordsOperation *)obj;
            [ffro addNullableDependency: strongSelf.ckFetchHoldOperation];
            [strongSelf.operationQueue addOperation: ffro];
        }
        return matches;
    }]];
}


-(void)expectCKFetchByQuery {
    // Create an object for the block to retain and modify
    BoolHolder* runAlready = [[BoolHolder alloc] init];

    __weak __typeof(self) weakSelf = self;
    [[self.mockDatabase expect] addOperation: [OCMArg checkWithBlock:^BOOL(id obj) {
        __strong __typeof(self) strongSelf = weakSelf;
        if(runAlready.state) {
            return NO;
        }
        BOOL matches = NO;
        if ([obj isKindOfClass: [FakeCKQueryOperation class]]) {
            matches = YES;
            runAlready.state = true;

            FakeCKQueryOperation *fqo = (FakeCKQueryOperation *)obj;
            [fqo addNullableDependency: strongSelf.ckFetchHoldOperation];
            [strongSelf.operationQueue addOperation: fqo];
        }
        return matches;
    }]];
}

- (void)startCKKSSubsystem {
    if(self.fakeHSA2AccountStatus != CKKSAccountStatusUnknown) {
        [self.accountStateTracker setCDPCapableiCloudAccountStatus:self.fakeHSA2AccountStatus];
    }
    [self startCKAccountStatusMock];
}

- (void)startCKAccountStatusMock {
    // Note: currently, based on how we're mocking up the zone creation and zone subscription operation,
    // they will 'fire' before this method is called. It's harmless, since the mocks immediately succeed
    // and return; it's just a tad confusing.
    if([self.ckaccountHoldOperation isPending]) {
        [self.operationQueue addOperation: self.ckaccountHoldOperation];
    }

    [self.accountStateTracker performInitialDispatches];
}

-(void)holdCloudKitModifications {
    XCTAssertFalse([self.ckModifyHoldOperation isPending], "Shouldn't already be a pending cloudkit modify hold operation");
    self.ckModifyHoldOperation = [NSBlockOperation blockOperationWithBlock:^{
        ckksnotice_global("ckks", "Released CloudKit modification hold.");
    }];
}
-(void)releaseCloudKitModificationHold {
    if([self.ckModifyHoldOperation isPending]) {
        [self.operationQueue addOperation: self.ckModifyHoldOperation];
    }
}

-(void)holdCloudKitFetches {
    XCTAssertFalse([self.ckFetchHoldOperation isPending], "Shouldn't already be a pending cloudkit fetch hold operation");
    ckksnotice_global("ckksfetcher", "Holding CloudKit fetches.");
    self.ckFetchHoldOperation = [NSBlockOperation blockOperationWithBlock:^{
        ckksnotice_global("ckksfetcher", "Released CloudKit fetch hold.");
    }];
}
-(void)releaseCloudKitFetchHold {
    if([self.ckFetchHoldOperation isPending]) {
        [self.operationQueue addOperation: self.ckFetchHoldOperation];
    }
}

-(void)holdCloudKitModifyRecordZones {
    XCTAssertFalse([self.ckModifyRecordZonesHoldOperation isPending], "Shouldn't already be a pending cloudkit zone create hold operation");
    ckksnotice_global("ckkszonemodifier", "Holding CloudKit zone modifications.");
    self.ckModifyRecordZonesHoldOperation = [NSBlockOperation blockOperationWithBlock:^{
        ckksnotice_global("ckkszonemodifier", "Released CloudKit zone modification hold.");
    }];
}
-(void)releaseCloudKitModifyRecordZonesHold {
    if([self.ckModifyRecordZonesHoldOperation isPending]) {
        [self.operationQueue addOperation: self.ckModifyRecordZonesHoldOperation];
    }
}

-(void)holdCloudKitModifySubscription {
    XCTAssertFalse([self.ckModifySubscriptionsHoldOperation isPending], "Shouldn't already be a pending cloudkit subscription hold operation");
    self.ckModifySubscriptionsHoldOperation = [NSBlockOperation blockOperationWithBlock:^{
        ckksnotice_global("ckks", "Released CloudKit zone create hold.");
    }];
}
-(void)releaseCloudKitModifySubscriptionHold {
    if([self.ckModifySubscriptionsHoldOperation isPending]) {
        [self.operationQueue addOperation: self.ckModifySubscriptionsHoldOperation];
    }
}

- (void)expectCKModifyItemRecords: (NSUInteger) expectedNumberOfRecords currentKeyPointerRecords: (NSUInteger) expectedCurrentKeyRecords zoneID: (CKRecordZoneID*) zoneID {
    [self expectCKModifyItemRecords:expectedNumberOfRecords
           currentKeyPointerRecords:expectedCurrentKeyRecords
                             zoneID:zoneID
                          checkItem:nil];
}

- (void)expectCKModifyItemRecords: (NSUInteger) expectedNumberOfRecords currentKeyPointerRecords: (NSUInteger) expectedCurrentKeyRecords zoneID: (CKRecordZoneID*) zoneID checkItem: (BOOL (^)(CKRecord*)) checkItem {
    [self expectCKModifyItemRecords:expectedNumberOfRecords
                     deletedRecords:0
           currentKeyPointerRecords:expectedCurrentKeyRecords
                             zoneID:zoneID
                          checkItem:checkItem];
}

- (void)expectCKModifyItemRecords:(NSUInteger)expectedNumberOfModifiedRecords
                   deletedRecords:(NSUInteger)expectedNumberOfDeletedRecords
         currentKeyPointerRecords:(NSUInteger)expectedCurrentKeyRecords
                           zoneID:(CKRecordZoneID*)zoneID
                        checkItem:(BOOL (^)(CKRecord*))checkItem {
    [self expectCKModifyItemRecords:expectedNumberOfModifiedRecords
                     deletedRecords:expectedNumberOfDeletedRecords
           currentKeyPointerRecords:expectedCurrentKeyRecords
                             zoneID:zoneID
                          checkItem:checkItem
         expectedOperationGroupName:nil];
}

- (void)expectCKModifyItemRecords:(NSUInteger)expectedNumberOfModifiedRecords
                   deletedRecords:(NSUInteger)expectedNumberOfDeletedRecords
         currentKeyPointerRecords:(NSUInteger)expectedCurrentKeyRecords
                           zoneID:(CKRecordZoneID*)zoneID
                        checkItem:(BOOL (^ _Nullable)(CKRecord*))checkItem
       expectedOperationGroupName:(NSString* _Nullable)operationGroupName
{
    // We're updating the device state type on every update, so add it in here
    NSMutableDictionary* expectedRecords = [@{SecCKRecordItemType: [NSNumber numberWithUnsignedInteger: expectedNumberOfModifiedRecords],
                                              SecCKRecordCurrentKeyType: [NSNumber numberWithUnsignedInteger: expectedCurrentKeyRecords],
                                              SecCKRecordDeviceStateType: [NSNumber numberWithUnsignedInt: 1],
                                              } mutableCopy];

    NSDictionary* deletedRecords = nil;
    if(expectedNumberOfDeletedRecords != 0) {
        deletedRecords = @{SecCKRecordItemType: [NSNumber numberWithUnsignedInteger: expectedNumberOfDeletedRecords]};
    }

    [self expectCKModifyRecords:expectedRecords
        deletedRecordTypeCounts:deletedRecords
                         zoneID:zoneID
            checkModifiedRecord: ^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: SecCKRecordItemType] && checkItem) {
                    return checkItem(record);
                } else {
                    return YES;
                }
            }
          inspectOperationGroup:operationGroupName != nil ? ^(CKOperationGroup* group) {
        XCTAssertEqualObjects(group.name, operationGroupName, "Should have expected group name");
    } : nil
           runAfterModification:nil];
}



- (void)expectCKModifyKeyRecords:(NSUInteger)expectedNumberOfRecords
        currentKeyPointerRecords:(NSUInteger)expectedCurrentKeyRecords
                 tlkShareRecords:(NSUInteger)expectedTLKShareRecords
                          zoneID:(CKRecordZoneID*)zoneID
{
    return [self expectCKModifyKeyRecords:expectedNumberOfRecords
                 currentKeyPointerRecords:expectedCurrentKeyRecords
                          tlkShareRecords:expectedTLKShareRecords
                                   zoneID:zoneID
                      checkModifiedRecord:nil];
}

- (void)expectCKModifyKeyRecords:(NSUInteger)expectedNumberOfRecords
        currentKeyPointerRecords:(NSUInteger)expectedCurrentKeyRecords
                 tlkShareRecords:(NSUInteger)expectedTLKShareRecords
                          zoneID:(CKRecordZoneID*)zoneID
             checkModifiedRecord:(BOOL (^_Nullable)(CKRecord*))checkModifiedRecord
{
    NSNumber* nkeys = [NSNumber numberWithUnsignedInteger: expectedNumberOfRecords];
    NSNumber* ncurrentkeys = [NSNumber numberWithUnsignedInteger: expectedCurrentKeyRecords];
    NSNumber* ntlkshares = [NSNumber numberWithUnsignedInteger: expectedTLKShareRecords];

    [self expectCKModifyRecords:@{SecCKRecordIntermediateKeyType: nkeys,
                                  SecCKRecordCurrentKeyType: ncurrentkeys,
                                  SecCKRecordTLKShareType: ntlkshares,
                                  }
        deletedRecordTypeCounts:nil
                         zoneID:zoneID
            checkModifiedRecord:checkModifiedRecord
          inspectOperationGroup:nil
           runAfterModification:nil];
}

- (void)expectCKModifyRecords:(NSDictionary<NSString*, NSNumber*>* _Nullable) expectedRecordTypeCounts
      deletedRecordTypeCounts:(NSDictionary<NSString*, NSNumber*>* _Nullable) expectedDeletedRecordTypeCounts
                       zoneID:(CKRecordZoneID*) zoneID
          checkModifiedRecord:(BOOL (^ _Nullable)(CKRecord*)) checkModifiedRecord
         runAfterModification:(void (^ _Nullable) (void))afterModification
{
    [self expectCKModifyRecords:expectedRecordTypeCounts
        deletedRecordTypeCounts:expectedDeletedRecordTypeCounts
                         zoneID:zoneID
            checkModifiedRecord:checkModifiedRecord
          inspectOperationGroup:nil
           runAfterModification:afterModification];
}

- (void)expectCKModifyRecords:(NSDictionary<NSString*, NSNumber*>* _Nullable)expectedRecordTypeCounts
      deletedRecordTypeCounts:(NSDictionary<NSString*, NSNumber*>* _Nullable)expectedDeletedRecordTypeCounts
                       zoneID:(CKRecordZoneID*) zoneID
          checkModifiedRecord:(BOOL (^)(CKRecord*)) checkModifiedRecord
        inspectOperationGroup:(void (^ _Nullable)(CKOperationGroup* _Nullable))inspectOperationGroup
         runAfterModification:(void (^) (void))afterModification
{
    __weak __typeof(self) weakSelf = self;

    // Create an object for the block to retain and modify
    BoolHolder* runAlready = [[BoolHolder alloc] init];

    secnotice("fakecloudkit", "expecting an operation matching modifications: %@ deletions: %@",
              expectedRecordTypeCounts, expectedDeletedRecordTypeCounts);

    [[self.mockDatabase expect] addOperation:[OCMArg checkWithBlock:^BOOL(id obj) {
        secnotice("fakecloudkit", "Received an operation (%@), checking if it's a modification", obj);
        __block bool matches = false;
        if(runAlready.state) {
            secnotice("fakecloudkit", "Run already, skipping");
            return NO;
        }

        if ([obj isKindOfClass:[CKModifyRecordsOperation class]]) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            XCTAssertNotNil(strongSelf, "self exists");

            CKModifyRecordsOperation *op = (CKModifyRecordsOperation *)obj;
            matches = true;

            NSMutableDictionary<NSString*, NSNumber*>* modifiedRecordTypeCounts = [[NSMutableDictionary alloc] init];
            NSMutableDictionary<NSString*, NSNumber*>* deletedRecordTypeCounts = [[NSMutableDictionary alloc] init];

            // First: check if it matches. If it does, _then_ execute the operation.
            // Supports single-zone atomic writes only

            if(!op.atomic) {
                // We only care about atomic operations
                secnotice("fakecloudkit", "Not an atomic operation; quitting: %@", op);
                return NO;
            }

            FakeCKZone* zone = strongSelf.zones[zoneID];
            XCTAssertNotNil(zone, "Have a zone for these records");
            if (zone == nil) {
                secnotice("fakecloudkit", "zone not found, failing");
                return NO;
            }

            __block BOOL result = YES;
            dispatch_sync(zone.queue, ^{

                for(CKRecord* record in op.recordsToSave) {
                    if(![record.recordID.zoneID isEqual: zoneID]) {
                        secnotice("fakecloudkit", "Modified record zone ID mismatch: %@ %@", zoneID, record.recordID.zoneID);
                        result = NO;
                        return;
                    }

                    NSError* recordError = [zone errorFromSavingRecord:record otherConcurrentWrites:op.recordsToSave];
                    if(recordError) {
                        secnotice("fakecloudkit", "Record zone rejected record write: %@ %@", recordError, record);
                        XCTFail(@"Record zone rejected record write: %@ %@", recordError, record);
                        result = NO;
                        return;
                    }

                    NSNumber* currentCountNumber = modifiedRecordTypeCounts[record.recordType];
                    NSUInteger currentCount = currentCountNumber ? [currentCountNumber unsignedIntegerValue] : 0;
                    modifiedRecordTypeCounts[record.recordType] = [NSNumber numberWithUnsignedInteger: currentCount + 1];
                }

                for(CKRecordID* recordID in op.recordIDsToDelete) {
                    if(![recordID.zoneID isEqual: zoneID]) {
                        matches = false;
                        secnotice("fakecloudkit", "Deleted record zone ID mismatch: %@ %@", zoneID, recordID.zoneID);
                    }

                    // Find the object in CloudKit, and record its type
                    CKRecord* record = strongSelf.zones[zoneID].currentDatabase[recordID];
                    if(record) {
                        NSNumber* currentCountNumber = deletedRecordTypeCounts[record.recordType];
                        NSUInteger currentCount = currentCountNumber ? [currentCountNumber unsignedIntegerValue] : 0;
                        deletedRecordTypeCounts[record.recordType] = [NSNumber numberWithUnsignedInteger: currentCount + 1];
                    }
                }

                NSMutableDictionary* filteredExpectedRecordTypeCounts = [expectedRecordTypeCounts mutableCopy];
                for(NSString* key in filteredExpectedRecordTypeCounts.allKeys) {
                    if([filteredExpectedRecordTypeCounts[key] isEqual: [NSNumber numberWithInt:0]]) {
                        filteredExpectedRecordTypeCounts[key] = nil;
                    }
                }
                filteredExpectedRecordTypeCounts[SecCKRecordManifestType] = modifiedRecordTypeCounts[SecCKRecordManifestType];
                filteredExpectedRecordTypeCounts[SecCKRecordManifestLeafType] = modifiedRecordTypeCounts[SecCKRecordManifestLeafType];

                // Inspect that we have exactly the same records as we expect
                if(expectedRecordTypeCounts) {
                    matches &= !![modifiedRecordTypeCounts isEqual: filteredExpectedRecordTypeCounts];
                    if(!matches) {
                        secnotice("fakecloudkit", "Record number mismatch: attempted:%@ expected:%@", modifiedRecordTypeCounts, filteredExpectedRecordTypeCounts);
                        result = NO;
                        return;
                    }
                } else {
                    matches &= op.recordsToSave.count == 0u;
                    if(!matches) {
                        secnotice("fakecloudkit", "Record number mismatch: attempted:%@ expected:0", modifiedRecordTypeCounts);
                        result = NO;
                        return;
                    }
                }
                if(expectedDeletedRecordTypeCounts) {
                    matches &= !![deletedRecordTypeCounts  isEqual: expectedDeletedRecordTypeCounts];
                    if(!matches) {
                        secnotice("fakecloudkit", "Deleted record number mismatch: attempted:%@ expected:%@", deletedRecordTypeCounts, expectedDeletedRecordTypeCounts);
                        result = NO;
                        return;
                    }
                } else {
                    matches &= op.recordIDsToDelete.count == 0u;
                    if(!matches) {
                        secnotice("fakecloudkit", "Deleted record number mismatch: attempted:%@ expected:0", deletedRecordTypeCounts);
                        result = NO;
                        return;
                    }
                }

                // We have the right number of things, and their etags match. Ensure that they have the right etags
                if(matches && checkModifiedRecord) {
                    // Clearly we have the right number of things. Call checkRecord on them...
                    for(CKRecord* record in op.recordsToSave) {
                        matches &= !!(checkModifiedRecord(record));
                        if(!matches) {
                            secnotice("fakecloudkit", "Check record reports NO: %@ 0", record);
                            result = NO;
                            return;
                        }
                    }
                }

                if(matches) {
                    if(inspectOperationGroup) {
                        inspectOperationGroup(op.group);
                    }

                    // Emulate cloudkit and schedule the operation for execution. Be sure to wait for this operation
                    // if you'd like to read the data from this write.
                    NSBlockOperation* ckop = [NSBlockOperation named:@"cloudkit-write" withBlock: ^{
                        @synchronized(zone.currentDatabase) {
                            if(zone.blockBeforeWriteOperation) {
                                zone.blockBeforeWriteOperation();
                            }

                            NSMutableArray* savedRecords = [[NSMutableArray alloc] init];
                            for(CKRecord* record in op.recordsToSave) {
                                CKRecord* reflectedRecord = [record copy];
                                reflectedRecord.modificationDate = [NSDate date];

                                [zone addToZone: reflectedRecord];

                                [savedRecords addObject:reflectedRecord];
                                op.perRecordSaveBlock(reflectedRecord.recordID, reflectedRecord, nil);
                            }
                            for(CKRecordID* recordID in op.recordIDsToDelete) {
                                // I don't believe CloudKit fails an operation if you delete a record that's not there, so:
                                [zone deleteCKRecordIDFromZone: recordID];
                            }

                            if(afterModification) {
                                afterModification();
                            }

                            op.modifyRecordsCompletionBlock(savedRecords, op.recordIDsToDelete, nil);
                            [op transitionToFinished];
                        }
                    }];
                    [ckop addNullableDependency:strongSelf.ckModifyHoldOperation];
                    [strongSelf.operationQueue addOperation: ckop];
                }
            });
            if(result != YES) {
                return result;
            }
        }
        if(matches) {
            runAlready.state = true;
        }
        return matches ? YES : NO;
    }]];
}

- (void)failNextZoneCreation:(CKRecordZoneID*)zoneID {
    XCTAssertNil(self.zones[zoneID], "Zone does not exist yet");
    self.zones[zoneID] = [[FakeCKZone alloc] initZone: zoneID];
    self.zones[zoneID].creationError = [[NSError alloc] initWithDomain:CKErrorDomain
                                                                  code:CKErrorNetworkUnavailable
                                                              userInfo:@{
        CKErrorRetryAfterKey: @(0.5),
    }];
}

// Report success, but don't actually create the zone.
// This way, you can find ZoneNotFound errors later on
- (void)failNextZoneCreationSilently:(CKRecordZoneID*)zoneID {
    XCTAssertNil(self.zones[zoneID], "Zone does not exist yet");
    self.zones[zoneID] = [[FakeCKZone alloc] initZone: zoneID];
    self.zones[zoneID].failCreationSilently = true;
}

- (void)failNextZoneSubscription:(CKRecordZoneID*)zoneID {
    XCTAssertNotNil(self.zones[zoneID], "Zone exists");
    self.zones[zoneID].subscriptionError = [[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkUnavailable userInfo:@{}];
}

- (void)failNextZoneSubscription:(CKRecordZoneID*)zoneID withError:(NSError*)error {
    XCTAssertNotNil(self.zones[zoneID], "Zone exists");
    self.zones[zoneID].subscriptionError = error;
}

- (void)persistentlyFailNextZoneSubscription:(CKRecordZoneID*)zoneID withError:(NSError*)error {
    XCTAssertNotNil(self.zones[zoneID], "Zone exists");
    self.zones[zoneID].persistentSubscriptionError = error;
}

- (void)failNextCKAtomicModifyItemRecordsUpdateFailure:(CKRecordZoneID*)zoneID {
    [self failNextCKAtomicModifyItemRecordsUpdateFailure:zoneID blockAfterReject:nil];
}

- (void)failNextCKAtomicModifyItemRecordsUpdateFailure:(CKRecordZoneID*)zoneID blockAfterReject: (void (^)(void))blockAfterReject {
    [self failNextCKAtomicModifyItemRecordsUpdateFailure:zoneID blockAfterReject:blockAfterReject withError:nil];
}

- (void)failNextCKAtomicModifyItemRecordsUpdateFailure:(CKRecordZoneID*)zoneID blockAfterReject: (void (^)(void))blockAfterReject withError:(NSError*)error {
    __weak __typeof(self) weakSelf = self;

    [[self.mockDatabase expect] addOperation:[OCMArg checkWithBlock:^BOOL(id obj) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        __block bool rejected = false;
        if ([obj isKindOfClass:[CKModifyRecordsOperation class]]) {
            CKModifyRecordsOperation *op = (CKModifyRecordsOperation *)obj;

            if(!op.atomic) {
                // We only care about atomic operations
                return NO;
            }

            // We want to only match zone updates pertaining to this zone
            for(CKRecord* record in op.recordsToSave) {
                if(![record.recordID.zoneID isEqual: zoneID]) {
                    return NO;
                }
            }

            FakeCKZone* zone = strongSelf.zones[zoneID];
            XCTAssertNotNil(zone, "Have a zone for these records");

            rejected = true;

            if(error) {
                [strongSelf rejectWrite: op withError:error];
            } else {
                NSMutableDictionary<CKRecordID*, NSError*>* failedRecords = [[NSMutableDictionary alloc] init];
                [strongSelf rejectWrite: op failedRecords:failedRecords];
            }

            if(blockAfterReject) {
                blockAfterReject();
            }
        }
        return rejected ? YES : NO;
    }]];
}

- (void)expectCKAtomicModifyItemRecordsUpdateFailure:(CKRecordZoneID*)zoneID {
    __weak __typeof(self) weakSelf = self;

    [[self.mockDatabase expect] addOperation:[OCMArg checkWithBlock:^BOOL(id obj) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        __block bool rejected = false;
        if ([obj isKindOfClass:[CKModifyRecordsOperation class]]) {
            CKModifyRecordsOperation *op = (CKModifyRecordsOperation *)obj;

            secnotice("fakecloudkit", "checking for expectCKAtomicModifyItemRecordsUpdateFailure");

            if(!op.atomic) {
                // We only care about atomic operations
                secnotice("fakecloudkit", "expectCKAtomicModifyItemRecordsUpdateFailure: update not atomic");
                return NO;
            }

            // We want to only match zone updates pertaining to this zone
            for(CKRecord* record in op.recordsToSave) {
                if(![record.recordID.zoneID isEqual: zoneID]) {
                    secnotice("fakecloudkit", "expectCKAtomicModifyItemRecordsUpdateFailure: %@ is not %@", record.recordID.zoneID, zoneID);
                    return NO;
                }
            }

            FakeCKZone* zone = strongSelf.zones[zoneID];
            XCTAssertNotNil(zone, "Have a zone for these records");
            if (zone == nil) {
                return NO;
            }

            NSMutableDictionary<CKRecordID*, NSError*>* failedRecords = [[NSMutableDictionary alloc] init];

            @synchronized(zone.currentDatabase) {
                for(CKRecord* record in op.recordsToSave) {
                    // Check if we should allow this transaction
                    NSError* recordSaveError = [zone errorFromSavingRecord: record];
                    if(recordSaveError) {
                        failedRecords[record.recordID] = recordSaveError;
                        rejected = true;
                    }
                }
            }

            if(rejected) {
                [strongSelf rejectWrite: op failedRecords:failedRecords];
            } else {
                secnotice("fakecloudkit", "expectCKAtomicModifyItemRecordsUpdateFailure: doesn't seem like an error to us");
            }
        }
        return rejected ? YES : NO;
    }]];
}

-(void)rejectWrite:(CKModifyRecordsOperation*)op withError:(NSError*)error {
    // Emulate cloudkit and schedule the operation for execution. Be sure to wait for this operation
    // if you'd like to read the data from this write.
    NSBlockOperation* ckop = [NSBlockOperation named:@"cloudkit-reject-write-error" withBlock: ^{
        op.modifyRecordsCompletionBlock(nil, nil, error);
        [op transitionToFinished];
    }];
    [ckop addNullableDependency: self.ckModifyHoldOperation];
    [self.operationQueue addOperation: ckop];
}

-(void)rejectWrite:(CKModifyRecordsOperation*)op failedRecords:(NSMutableDictionary<CKRecordID*, NSError*>*)failedRecords {
    // Add the batch request failed errors
    for(CKRecord* record in op.recordsToSave) {
        NSError* exists = failedRecords[record.recordID];
        if(!exists) {
            // TODO: might have important userInfo, but we're not mocking that yet
            failedRecords[record.recordID] = [[NSError alloc] initWithDomain: CKErrorDomain code: CKErrorBatchRequestFailed userInfo: @{}];
        }
    }

    NSError* error = [[NSError alloc] initWithDomain: CKErrorDomain code: CKErrorPartialFailure userInfo: @{CKPartialErrorsByItemIDKey: failedRecords}];

    // Emulate cloudkit and schedule the operation for execution. Be sure to wait for this operation
    // if you'd like to read the data from this write.
    NSBlockOperation* ckop = [NSBlockOperation named:@"cloudkit-reject-write" withBlock: ^{
        op.modifyRecordsCompletionBlock(nil, nil, error);
        [op transitionToFinished];
    }];
    [ckop addNullableDependency: self.ckModifyHoldOperation];
    [self.operationQueue addOperation: ckop];
}

- (void)expectCKDeleteItemRecords:(NSUInteger)expectedNumberOfRecords
                           zoneID:(CKRecordZoneID*)zoneID
{
    return [self expectCKDeleteItemRecords:expectedNumberOfRecords
                                    zoneID:zoneID
                expectedOperationGroupName:nil];
}

- (void)expectCKDeleteItemRecords:(NSUInteger)expectedNumberOfRecords
                           zoneID:(CKRecordZoneID*)zoneID
       expectedOperationGroupName:(NSString* _Nullable)operationGroupName
{
    // We're updating the device state type on every update, so add it in here
    NSMutableDictionary* expectedRecords = [@{
                                              SecCKRecordDeviceStateType: [NSNumber numberWithUnsignedInteger:expectedNumberOfRecords],
                                              } mutableCopy];

    [self expectCKModifyRecords:expectedRecords
        deletedRecordTypeCounts:@{SecCKRecordItemType: [NSNumber numberWithUnsignedInteger: expectedNumberOfRecords]}
                         zoneID:zoneID
            checkModifiedRecord:nil
          inspectOperationGroup:operationGroupName != nil ? ^(CKOperationGroup* group) {
        XCTAssertEqualObjects(group.name, operationGroupName, "Should have expected group name");
    } : nil
           runAfterModification:nil];
}

-(void)waitForCKModifications {
    // CloudKit modifications are put on the local queue.
    // This is heavyweight but should suffice.
    [self.operationQueue waitUntilAllOperationsAreFinished];
}

- (void)tearDown {
    secnotice("ckkstest", "Ending test %@", self.testName);

    [self.mockPersonaAdapter prepareThreadForKeychainAPIUseForPersonaIdentifier:nil];

    if(SecCKKSIsEnabled()) {
        if (OctagonSupportsPersonaMultiuser()) {
            for (CloudKitAccount* account in [self.mockAccountsAuthKitAdapter.accounts allValues]) {
                account.accountStatus = CKAccountStatusCouldNotDetermine;
            }
        }
        
        // If the test never initialized the account state, don't call status later
        bool callStatus = [self.ckaccountHoldOperation isFinished];
        [self.ckaccountHoldOperation cancel];
        self.ckaccountHoldOperation = nil;

        // Ensure we don't have any blocking operations left
        [self.operationQueue cancelAllOperations];
        [self waitForCKModifications];

        XCTAssertEqual(0, [self.injectedManager.completedSecCKKSInitialize wait:20*NSEC_PER_SEC],
            "Timeout did not occur waiting for SecCKKSInitialize");

        // Ensure that we can fetch zone status for all zones
        if(callStatus) {
            // We can only fetch status from the default persona
            self.mockPersonaAdapter.isDefaultPersona = YES;
            
            NSArray<TPSpecificUser *> *activeAccounts = [self.mockAccountsAuthKitAdapter inflateAllTPSpecificUsers:OTCKContainerName octagonContextID:OTDefaultContext];

            for (TPSpecificUser* account in activeAccounts) {
                
                // when tests supports multiple ckks objects, this should go away so we can check on the status of each ckks
                if ([account.altDSID isEqualToString:[OTMockPersonaAdapter defaultMockPersonaString]]) {
                    XCTestExpectation *statusReturned = [self expectationWithDescription:@"status returned"];
                    [self.injectedManager rpcStatus:nil fast:NO waitForNonTransientState:CKKSControlStatusDefaultNonTransientStateTimeout reply:^(NSArray<NSDictionary *> *result, NSError *error) {
                        XCTAssertNil(error, "Should be no error fetching status");
                        [statusReturned fulfill];
                    }];
                    [self waitForExpectations: @[statusReturned] timeout:20];
                }
            }

            // Make sure this happens before teardown.
            XCTAssertEqual(0, [self.accountStateTracker.finishedInitialDispatches wait:20*NSEC_PER_SEC], "Account state tracker initialized itself");

            dispatch_group_t accountChangesDelivered = [self.accountStateTracker checkForAllDeliveries];
            XCTAssertEqual(0, dispatch_group_wait(accountChangesDelivered, dispatch_time(DISPATCH_TIME_NOW, 10*NSEC_PER_SEC)), "Account state tracker finished delivering everything");
        }
    }

    [self.injectedManager.accountTracker.fetchCKAccountStatusScheduler cancel];
    [self.injectedManager haltAll];
    [self.injectedManager dropAllActors];
    [self.defaultCKKS halt];

    [super tearDown];

    // We're tearing down the tests; things that are non-nil in the tests should be thrown away
    // We can't rely on the test class being released, unforunately...
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"

    self.defaultCKKS = nil;
    self.injectedManager = nil;

    self.lockStateProvider = nil;
#pragma clang diagnostic pop

    [self.mockCKKSViewManager stopMocking];
    self.mockCKKSViewManager = nil;

    self.injectedOTManager.viewManager = nil;

    [self.injectedOTManager clearAllContexts];
    self.injectedOTManager = nil;
    [OTManager resetManager:true to:nil];

    [self.mockAccountStateTracker stopMocking];
    self.mockAccountStateTracker = nil;

    [self.mockFakeCKModifyRecordZonesOperation stopMocking];
    self.mockFakeCKModifyRecordZonesOperation = nil;

    [self.mockFakeCKModifySubscriptionsOperation stopMocking];
    self.mockFakeCKModifySubscriptionsOperation = nil;

    [self.mockFakeCKFetchRecordZoneChangesOperation stopMocking];
    self.mockFakeCKFetchRecordZoneChangesOperation = nil;

    [self.mockFakeCKFetchRecordsOperation stopMocking];
    self.mockFakeCKFetchRecordsOperation = nil;

    [self.mockFakeCKQueryOperation stopMocking];
    self.mockFakeCKQueryOperation = nil;

    [self.mockDatabase stopMocking];
    self.mockDatabase = nil;

    [self.mockDatabaseExceptionCatcher stopMocking];
    self.mockDatabaseExceptionCatcher = nil;

    [self.mockContainer stopMocking];
    self.mockContainer = nil;

    [self.mockTTR stopMocking];
    self.mockTTR = nil;
    self.ttrExpectation = nil;
    self.isTTRRatelimited = true;

    self.zones = nil;

    _mockSOSAdapter = nil;
    _mockOctagonAdapter = nil;
    _mockPersonaAdapter = nil;

    // Force-close the analytics DBs so we can clean out the test directory
    [[CKKSAnalytics logger] removeState];
    [[SOSAnalytics logger] removeState];

    SecItemDataSourceFactoryReleaseAll();
    SecKeychainDbForceClose();
    SecKeychainDbReset(NULL);

    [self cleanUpDirectories];

    SecCKKSTestResetFlags();
#if KCSHARING
    KCSharingSetDisabledForTests(false);
#endif  // KCSHARING
}

- (CKKSKey*)fakeTLK:(CKRecordZoneID*)zoneID
          contextID:(NSString*)contextID
{
    CKKSKey* key = [[CKKSKey alloc] initSelfWrappedWithAESKey:[[CKKSAESSIVKey alloc] initWithBase64: @"uImdbZ7Zg+6WJXScTnRBfNmoU1UiMkSYxWc+d1Vuq3IFn2RmTRkTdWTe3HmeWo1pAomqy+upK8KHg2PGiRGhqg=="]
                                                    contextID:contextID
                                                         uuid:[[NSUUID UUID] UUIDString]
                                                     keyclass:SecCKKSKeyClassTLK
                                                        state: SecCKKSProcessedStateLocal
                                                       zoneID:zoneID
                                              encodedCKRecord: nil
                                                   currentkey: true];
    [key CKRecordWithZoneID: zoneID];
    return key;
}

- (NSError*)ckInternalServerExtensionError:(NSInteger)code description:(NSString*)desc {
    return [FakeCKZone internalPluginError:@"CloudkitKeychainService" code:code description:desc];
}

@end

#endif
