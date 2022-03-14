/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import "CloudKitKeychainSyncingTestsBase.h"

@implementation CloudKitKeychainSyncingTestsBase

- (ZoneKeys*)keychainZoneKeys {
    return self.keys[self.keychainZoneID];
}

- (BOOL)mockPostFollowUpWithContext:(CDPFollowUpContext *)context error:(NSError **)error {
    secnotice("octagon", "mock cdp posting follow up");
    return YES;
}

// Override our base class
- (NSSet<NSString*>*)managedViewList {
    return [NSSet setWithObject:@"keychain"];
}

+ (void)setUp {
    SecCKKSEnable();
    SecCKKSResetSyncing();
    [super setUp];
}

- (void)setUp {
    self.utcCalendar = [NSCalendar calendarWithIdentifier:NSCalendarIdentifierISO8601];
    self.utcCalendar.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"UTC"];

    [super setUp];

    if(SecCKKSIsEnabled()) {
        self.keychainZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"keychain" ownerName:CKCurrentUserDefaultName];
        self.keychainZone = [[FakeCKZone alloc] initZone: self.keychainZoneID];

        [self.ckksZones addObject:self.keychainZoneID];

        // Wait for the ViewManager to be brought up
        XCTAssertEqual(0, [self.injectedManager.completedSecCKKSInitialize wait:20*NSEC_PER_SEC], "No timeout waiting for SecCKKSInitialize");

        self.defaultCKKS = [[CKKSViewManager manager] ckksAccountSyncForContainer:SecCKKSContainerName
                                                                        contextID:OTDefaultContext];

        self.keychainView = [self.defaultCKKS.operationDependencies viewStateForName:@"keychain"];
        XCTAssertNotNil(self.keychainView, "CKKS knows about the keychain view");
        [self.ckksViews addObject:self.keychainView];
    }

    // Check that your environment is set up correctly
    XCTAssertFalse([CKKSManifest shouldSyncManifests], "Manifests syncing is disabled");
    XCTAssertFalse([CKKSManifest shouldEnforceManifests], "Manifests enforcement is disabled");
}


+ (void)tearDown {
    [super tearDown];
    SecCKKSResetSyncing();
}

- (void)tearDown {
    // Fetch status, to make sure we can

    // Can only fetch status from the default persona.
    self.mockPersonaAdapter.isDefaultPersona = YES;

    XCTestExpectation* statusCompletes = [self expectationWithDescription:@"status completes"];
    [self.defaultCKKS rpcStatus:nil
                      fast:NO
                      waitForNonTransientState:CKKSControlStatusDefaultNonTransientStateTimeout
                      reply:^(NSArray<NSDictionary*>* _Nullable status, NSError* _Nullable error) {
        XCTAssertNotNil(status, "Should have some statuses");
        XCTAssertNil(error, "Should have no error fetching status");
        [statusCompletes fulfill];
    }];
    [self waitForExpectations:@[statusCompletes] timeout:20];

    [self.defaultCKKS halt];
    [self.defaultCKKS waitUntilAllOperationsAreFinished];

    self.keychainView = nil;
    self.keychainZoneID = nil;

    [super tearDown];
} 

- (FakeCKZone*)keychainZone {
    return self.zones[self.keychainZoneID];
}

- (void)setKeychainZone: (FakeCKZone*) zone {
    self.zones[self.keychainZoneID] = zone;
}

@end

#endif /* OCTAGON */
