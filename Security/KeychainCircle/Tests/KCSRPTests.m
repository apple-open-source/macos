//
//  KeychainCircleTests.m
//  KeychainCircleTests
//
//

#import <XCTest/XCTest.h>
#import <XCTest/XCTestCase_Private.h>

#import "KCSRPContext.h"
#include <corecrypto/ccrng.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccdh_gp.h>

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
#if XCT_MEMORY_TESTING_AVAILABLE
    [self assertNoLeaksInScope:^{
#endif /* XCT_MEMORY_TESTING_AVAILABLE */
        [self negotiateWithUser: @"TestUser"
                     digestInfo: ccsha256_di()
                          group: ccsrp_gp_rfc5054_3072()
                   randomSource: ccrng(NULL)];
#if XCT_MEMORY_TESTING_AVAILABLE
    }];
#endif /* XCT_MEMORY_TESTING_AVAILABLE */
}

- (void) testGetKeyAfterDealloc {
    NSString* user = @"some user";
    NSString* password = @"TryMeAs a ü password, sucka";

    NSData* key1_client = NULL;
    NSData* key2_client = NULL;
    NSData* key1_server = NULL;
    NSData* key2_server = NULL;

    const struct ccdigest_info* di = ccsha256_di();
    ccsrp_const_gp_t group = ccsrp_gp_rfc5054_3072();
    struct ccrng_state* rng = ccrng(NULL);

    @autoreleasepool {
        KCSRPClientContext* client = [[KCSRPClientContext alloc] initWithUser:user
                                                                   digestInfo:di
                                                                        group:group
                                                                 randomSource:rng];

        KCSRPServerContext* server = [[KCSRPServerContext alloc] initWithUser:user
                                                                     password:password
                                                                   digestInfo:di
                                                                        group:group
                                                                 randomSource:rng];

        NSError* error = nil;
        NSData* A_data = [client copyStart:&error];
        XCTAssert(A_data, @"copied start failed (%@)", error);
        error = nil;

        NSData* B_data = [server copyChallengeFor:A_data error: &error];
        XCTAssert(B_data, @"Copied challenge for start failed (%@)", error);
        error = nil;

        key1_server = [server getKey];
        key2_server = [[NSData alloc] initWithData:[server getKey]];
        XCTAssert([key1_server isEqualToData:key2_server], "key1_server and key2_server should be equal");

        NSData* M_data = [client copyResposeToChallenge:B_data
                                               password:password
                                                   salt:server.salt
                                                  error:&error];
        XCTAssert(M_data, @"Copied responseToChallenge failed (%@)", error);
        error = nil;

        NSData* HAMK_data = [server copyConfirmationFor:M_data error:&error];
        XCTAssert(HAMK_data, @"Copied confirmation failed (%@)", error);
        error = nil;

        key1_client = [client getKey];
        key2_client = [[NSData alloc] initWithData:[client getKey]];
        XCTAssert([key1_client isEqualToData:key2_client], "key1_client and key2_client should be equal");
    }
    XCTAssert([key1_server isEqualToData:key2_server], "key1_server and key2_server should be equal");
    XCTAssert([key1_client isEqualToData:key2_client], "key1_client and key2_cilent should be equal");
}

@end
