//
//  OCSPCacheUpgradeTests.m
//  Security
//
//

#include <AssertMacros.h>
#import <XCTest/XCTest.h>
#import <Security/SecCertificatePriv.h>
#include <utilities/SecCFWrappers.h>
#include <sqlite3.h>
#import "trust/trustd/SecOCSPRequest.h"
#import "trust/trustd/SecOCSPResponse.h"
#import "trust/trustd/SecOCSPCache.h"

#import "TrustDaemonTestCase.h"
#import "OCSPCacheTests_data.h"

@interface OCSPCacheTests : TrustDaemonTestCase
@end

@implementation OCSPCacheTests

- (void)setUp
{
    /* Delete the OCSP cache DB so we can start fresh */
    SecOCSPCacheDeleteCache();
}

- (BOOL)canReadDB
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _leaf_cert, sizeof(_leaf_cert));
    SecCertificateRef issuer = SecCertificateCreateWithBytes(NULL, _issuer, sizeof(_issuer));
    SecOCSPRequestRef request = SecOCSPRequestCreate(leaf, issuer);
    SecOCSPResponseRef response = SecOCSPCacheCopyMatching(request, NULL);
    CFReleaseNull(leaf);
    CFReleaseNull(issuer);
    SecOCSPRequestFinalize(request);

    if (response) {
        SecOCSPResponseFinalize(response);
        return YES;
    }
    return NO;
}

- (void)writeResponse1ToDB
{
    NSData *responseData = [NSData dataWithBytes:_ocsp_response1 length:sizeof(_ocsp_response1)];
    SecOCSPResponseRef response = SecOCSPResponseCreate((__bridge CFDataRef)responseData);
    /* use a verifyTime within the validity of the ocsp response */
    (void)SecOCSPResponseCalculateValidity(response, 0, 60, 595602000.0); // as a side effect, populates the expire time
    SecOCSPCacheReplaceResponse(NULL, response, NULL, 595602000.0);
    SecOCSPResponseFinalize(response);
}

- (void)writeResponse2ToDB
{
    NSData *responseData = [NSData dataWithBytes:_ocsp_response2 length:sizeof(_ocsp_response2)];
    SecOCSPResponseRef response = SecOCSPResponseCreate((__bridge CFDataRef)responseData);
    (void)SecOCSPResponseCalculateValidity(response, 0, 60, 596180000.0); // as a side effect, populates the expire time
    SecOCSPCacheReplaceResponse(NULL, response, NULL,596180000.0);
    SecOCSPResponseFinalize(response);
}

- (void)replaceResponse
{
    NSData *responseData = [NSData dataWithBytes:_ocsp_response1 length:sizeof(_ocsp_response1)];
    SecOCSPResponseRef response1 = SecOCSPResponseCreate((__bridge CFDataRef)responseData);
    (void)SecOCSPResponseCalculateValidity(response1, 0, 60, 595602000.0); // populate the expire time

    responseData = [NSData dataWithBytes:_ocsp_response2 length:sizeof(_ocsp_response2)];
    SecOCSPResponseRef response2 = SecOCSPResponseCreate((__bridge CFDataRef)responseData);
    (void)SecOCSPResponseCalculateValidity(response2, 0, 60, 596180000.0); // populate the expire time

    SecOCSPCacheReplaceResponse(response1, response2, NULL, 596180000.0);
    SecOCSPResponseFinalize(response1);
    SecOCSPResponseFinalize(response2);
}

- (void)createDBFromSQL:(NSString *)sql
{
    CFStringRef cf_path = SecOCSPCacheCopyPath();
    CFStringPerformWithCString(cf_path, ^(const char *path) {
        /* Create ocsp cahche */
        sqlite3 *db;
        XCTAssertEqual(sqlite3_open(path, &db), SQLITE_OK, "create ocsp cache");
        XCTAssertEqual(sqlite3_exec(db, [sql cStringUsingEncoding:NSUTF8StringEncoding], NULL, NULL, NULL), SQLITE_OK,
           "populate ocsp cache");
        XCTAssertEqual(sqlite3_close_v2(db), SQLITE_OK);

    });
    CFReleaseNull(cf_path);
}

- (int)countEntries
{
    CFStringRef cf_path = SecOCSPCacheCopyPath();
    __block int result = 0;
    CFStringPerformWithCString(cf_path, ^(const char *path) {
        sqlite3 *db = NULL;
        sqlite3_stmt *stmt = NULL;
        XCTAssertEqual(sqlite3_open(path, &db), SQLITE_OK);
        NSString *countResponses = @"SELECT COUNT(responseId) FROM responses;";
        XCTAssertEqual(sqlite3_prepare_v2(db, [countResponses cStringUsingEncoding:NSUTF8StringEncoding],
                                          (int)[countResponses length], &stmt, NULL),
                       SQLITE_OK);
        XCTAssertEqual(sqlite3_step(stmt), SQLITE_ROW);
        result = sqlite3_column_int(stmt, 0);
        XCTAssertEqual(sqlite3_finalize(stmt), SQLITE_OK);
        XCTAssertEqual(sqlite3_close_v2(db), SQLITE_OK);
    });
    CFReleaseNull(cf_path);
    return result;
}

- (void)testNewDatabase
{
    [self writeResponse1ToDB];
    XCTAssert([self canReadDB]);
}

- (void)testNewDatabaseReOpen
{
    [self writeResponse1ToDB];
    XCTAssert([self canReadDB]);
    SecOCSPCacheCloseDB();
    XCTAssert([self canReadDB]);
    [self replaceResponse];
    XCTAssert([self canReadDB]);
}

- (void)testOldDatabaseUpgradeNoContent
{
    [self createDBFromSQL:_oldDBSchema];
    [self writeResponse1ToDB];
    XCTAssert([self canReadDB]);
}

- (void)testOldDatabaseUpgradeWithContent
{
    [self createDBFromSQL:_oldDBSchemaWithContent];
    XCTAssert([self canReadDB]);
    [self replaceResponse];
    XCTAssert([self canReadDB]);
}

- (void)testUpgradedDatabaseNoContent
{
    [self createDBFromSQL:_oldDBSchema];
    XCTAssertFalse([self canReadDB]); // should upgrade the DB
    SecOCSPCacheCloseDB();
    [self writeResponse1ToDB];
    XCTAssert([self canReadDB]);
}

- (void)testUpgradedDatabaseWithContent
{
    [self createDBFromSQL:_oldDBSchemaWithContent];
    XCTAssert([self canReadDB]); // should upgrade the DB
    SecOCSPCacheCloseDB();
    [self replaceResponse];
    XCTAssert([self canReadDB]);
}

- (void)testGCExpiredResponses
{
    [self createDBFromSQL:_oldDBSchemaWithContent]; // since this is an old schema, the certStatus will be CS_NotParsed
    /* don't replace response 1, just add response 2 a week after response 1 expired */
    [self writeResponse2ToDB]; // as a side effect, should GC the expired non-revoked response
    SecOCSPCacheCloseDB();
    XCTAssertEqual([self countEntries], 1);
}

- (void)testNoGCExpiredRevokedResponses
{
    [self writeResponse1ToDB];
    /* don't replace response 1, just add response 2 a week after response 1 expired */
    [self writeResponse2ToDB]; // should not GC the expired revoked response 1
    SecOCSPCacheCloseDB();
    XCTAssertEqual([self countEntries], 2);
}

@end
