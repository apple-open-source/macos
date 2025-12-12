//
//  CRLiteIngestionTests.m
//

#import <XCTest/XCTest.h>
#import <Foundation/Foundation.h>
#import <sqlite3.h>
#import "trust/trustd/SecRevocationDb.h"
#import "trust/trustd/trustd_spi.h"

#import "TrustDaemonTestCase.h"

@interface CRLiteIngestionTests : TrustDaemonInitializationTestCase
@property NSData *updateData;
@end

@implementation CRLiteIngestionTests


- (void)setUp
{
    [super setUp];

    self.updateData = [NSData dataWithContentsOfURL:[[NSBundle bundleForClass:[self class]] URLForResource:@"v0-recent" withExtension:@"bin" subdirectory:@"CRLiteTests-data"]];
}

- (void)testCRLiteLoadUpdate
{
#if TARGET_OS_BRIDGE || TARGET_OS_WATCH
    XCTSkip();
#endif

    XCTAssertNotNil(self.updateData);
    
    trustd_init_server();

    /* Set the Valid generation to 8 */
    SecRevocationDbSetGeneration(8);

    /* Check that we can ingest the test update */
    SecValidUpdateVerifyAndIngest((__bridge CFDataRef)self.updateData, CFSTR("https://valid-qa.apple.com/carry"), true);

    /* Reset Valid DB */
    __block CFErrorRef error = NULL;
    SecRevocationDbFullReset(&error);
    XCTAssertNil((__bridge NSError*)error, "Should be no error resetting the Valid DB");
}
@end
