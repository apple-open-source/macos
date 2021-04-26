//
//  PinningDbTests.m
//

#import <XCTest/XCTest.h>
#import <Foundation/Foundation.h>
#import <sqlite3.h>
#import "trust/trustd/SecPinningDb.h"

#import "TrustDaemonTestCase.h"

@interface PinningDbInitializationTests : TrustDaemonInitializationTestCase
@end

@implementation PinningDbInitializationTests

#if !TARGET_OS_BRIDGE
- (void)testSchemaUpgrade
{
    /* Create a "pinningDB" with a large content version number but older schema version */
    char *schema_v2 =   "PRAGMA foreign_keys=OFF; "
                        "PRAGMA user_version=2; "
                        "BEGIN TRANSACTION; "
                        "CREATE TABLE admin(key TEXT PRIMARY KEY NOT NULL,ival INTEGER NOT NULL,value BLOB); "
                        "INSERT INTO admin VALUES('version',2147483647,NULL); " // Version as INT_MAX
                        "CREATE TABLE rules( policyName TEXT NOT NULL,"
                                            "domainSuffix TEXT NOT NULL,"
                                            "labelRegex TEXT NOT NULL,"
                                            "policies BLOB NOT NULL,"
                                            "UNIQUE(policyName, domainSuffix, labelRegex)); "
                        "COMMIT;";
    NSURL *pinningDbPath = [SecPinningDb pinningDbPath];
    sqlite3 *handle = nil;
    int sqlite_result = sqlite3_open_v2([pinningDbPath fileSystemRepresentation], &handle,
                                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    XCTAssertEqual(sqlite_result, SQLITE_OK);
    XCTAssert(SQLITE_OK == sqlite3_exec(handle, schema_v2, NULL, NULL, NULL));
    XCTAssert(SQLITE_OK == sqlite3_close(handle));

    /* Initialize the Pinning DB -- schema should get upgraded and populated with system content version */
    SecPinningDb *pinningDb = [[SecPinningDb alloc] init];
    XCTAssert(SecDbPerformRead([pinningDb db], NULL, ^(SecDbConnectionRef dbconn) {
        NSNumber *contentVersion = [pinningDb getContentVersion:dbconn error:NULL];
        NSNumber *schemaVersion = [pinningDb getSchemaVersion:dbconn error:NULL];
        XCTAssert([contentVersion intValue] < INT_MAX && [contentVersion intValue] > 0);
        XCTAssertEqualObjects(schemaVersion, @(PinningDbSchemaVersion));
    }));
}

- (void)testContentUpgradeFromFile
{
    /* initialize a DB with the system content version */
    SecPinningDb *pinningDb = [[SecPinningDb alloc] init];
    XCTAssert(SecDbPerformRead([pinningDb db], NULL, ^(SecDbConnectionRef dbconn) {
        NSNumber *contentVersion = [pinningDb getContentVersion:dbconn error:NULL];
        XCTAssert([contentVersion intValue] < INT_MAX && [contentVersion intValue] > 0);
    }));

    /* update it using a test plist with INT_MAX version */
    NSURL *pinningPlist = [[NSBundle bundleForClass:[self class]] URLForResource:@"PinningDB_vINT_MAX" withExtension:nil
                                                                 subdirectory:@"TestTrustdInitialization-data"];
    XCTAssert([pinningDb installDbFromURL:pinningPlist error:nil]);
    XCTAssert(SecDbPerformRead([pinningDb db], NULL, ^(SecDbConnectionRef dbconn) {
        NSNumber *contentVersion = [pinningDb getContentVersion:dbconn error:NULL];
        XCTAssertEqual([contentVersion intValue], INT_MAX);
    }));

    /* update one more time with the same content version */
    XCTAssert([pinningDb installDbFromURL:pinningPlist error:nil]);
}
#else // !TARGET_OS_BRIDGE
/* BridgeOS doesn't have security_certificates project so there is no baseline pinning plist */
- (void)testSkipTests
{
    XCTAssert(true);
}
#endif // !TARGET_OS_BRIDGE

@end
