//
//  KeychainCircleTests.m
//  KeychainCircleTests
//
//

#import <XCTest/XCTest.h>

#import "KCSRPContext.h"
#include <corecrypto/ccrng.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccdh_gp.h>
#include <CommonCrypto/CommonRandomSPI.h>

@interface KCSRPTests : XCTestCase

@end

@implementation KCSRPTests

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void) negotiateWithUser: (NSString*) user
                digestInfo: (const struct ccdigest_info*) di
                     group: (ccsrp_const_gp_t) group
              randomSource: (struct ccrng_state *) rng {

    NSString* password = @"TryMeAs a ü password, sucka";

    KCSRPClientContext * client = [[KCSRPClientContext alloc] initWithUser: user
                                                            digestInfo: di
                                                                 group: group
                                                          randomSource: rng];
    XCTAssert([client getKey] == NULL, @"No key yet");
    XCTAssert(![client isAuthenticated], @"Not yet authenticated");

    XCTAssert(client, @"No KCSRPClientContext created");

    KCSRPServerContext * server = [[KCSRPServerContext alloc] initWithUser:user
                                                              password:password
                                                            digestInfo:di
                                                                 group:group
                                                          randomSource:rng];


    XCTAssert(server, @"No KCSRPServerContext created");

    XCTAssert([server getKey] == NULL, @"No key yet");

    NSError* error = nil;

    NSData* A_data = [client copyStart:&error];
    XCTAssert(A_data, @"copied start failed (%@)", error);
    error = nil;

    XCTAssert([client getKey] == NULL, @"Shouldn't have key");
    XCTAssert(![client isAuthenticated], @"Not yet authenticated");

    NSData* B_data = [server copyChallengeFor:A_data error: &error];
    XCTAssert(B_data, @"Copied challenge for start failed (%@)", error);
    error = nil;

    XCTAssert([server getKey] != NULL, @"Should have key");
    XCTAssert(![server isAuthenticated], @"Not yet authenticated");

    NSData* M_data = [client copyResposeToChallenge:B_data
                                           password:password
                                               salt:server.salt
                                              error:&error];
    XCTAssert(M_data, @"Copied responseToChallenge failed (%@)", error);
    error = nil;

    XCTAssert([client getKey] != NULL, @"Don't have key");
    XCTAssert(![client isAuthenticated], @"Not yet authenticated");

    NSData* HAMK_data = [server copyConfirmationFor:M_data error:&error];
    XCTAssert(HAMK_data, @"Copied confirmation failed (%@)", error);
    error = nil;

    XCTAssert([server getKey] != NULL, @"Don't have key");
    XCTAssert([server isAuthenticated], @"Not yet authenticated");

    bool verified = [client verifyConfirmation:HAMK_data error:&error];
    XCTAssert(verified, @"Verification failed (%@)", error);
    error = nil;

    XCTAssert([client getKey] != NULL, @"Don't have key");
    XCTAssert([client isAuthenticated], @"Should be authenticated");


}

- (void)testNegotiation {
    [self negotiateWithUser: @"TestUser"
                 digestInfo: ccsha256_di()
                      group: ccsrp_gp_rfc5054_3072()
               randomSource: ccDRBGGetRngState()];
}

@end
