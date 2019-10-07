//
//  SecurityPairing.m
//  Security_ios
//
//  Created by Love Hörnquist Åstrand on 2017-02-28.
//

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <Security/SecKeyPriv.h>
#import <Security/SecItemPriv.h>
#import "keychain/SecureObjectSync/SOSAccount.h"
#include "keychain/SecureObjectSync/SOSAccountPriv.h"
#include "keychain/SecureObjectSync/SOSCircle.h"
#import <KeychainCircle/KeychainCircle.h>
#import <XCTest/XCTest.h>
#import "SecCFWrappers.h"
#import "SOSRegressionUtilities.h"
#import "FakeSOSControl.h"

@interface KCPairingTest : XCTestCase

@end

@implementation KCPairingTest

- (void)checkRoundtrip:(KCPairingChannelContext *)c1 check:(NSString *)check
{
    KCPairingChannelContext *c2;
    NSData *data;

    data = [NSKeyedArchiver archivedDataWithRootObject:c1 requiringSecureCoding:TRUE error:NULL];
    XCTAssertNotNil(data, "data should be valid: %@", check);

    NSKeyedUnarchiver* unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
    c2 = [unarchiver decodeObjectOfClass:[KCPairingChannelContext class] forKey:NSKeyedArchiveRootObjectKey];

    XCTAssertEqualObjects(c1, c2, "c1 should be same as c2: %@", check);
}

- (void)testPairingChannelContextValid {
    KCPairingChannelContext *c;

    c = [[KCPairingChannelContext alloc] init];

    [self checkRoundtrip:c check:@"empty"];

    c.intent = KCPairingIntent_Type_None;
    [self checkRoundtrip:c check:@"with intent"];

    c.intent = @"invalid";
}

- (void)testPairingChannelContextInvalid {
    KCPairingChannelContext *c1, *c2;
    NSData *data;

    c1 = [[KCPairingChannelContext alloc] init];
    c1.intent = @"invalid";

    data = [NSKeyedArchiver archivedDataWithRootObject:c1 requiringSecureCoding:TRUE error:NULL];
    XCTAssertNotNil(data, "data should be valid");

    NSKeyedUnarchiver* unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
    c2 = [unarchiver decodeObjectOfClass:[KCPairingChannelContext class] forKey:NSKeyedArchiveRootObjectKey];

    XCTAssertNil(c2, "c2 should be NULL");
}


@end
