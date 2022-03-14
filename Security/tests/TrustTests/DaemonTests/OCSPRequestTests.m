//
//  OCSPRequestTests.m
//  Security
//

#include <AssertMacros.h>
#import <XCTest/XCTest.h>
#import <Security/SecCertificatePriv.h>
#include <utilities/SecCFWrappers.h>

#import "trust/trustd/SecOCSPRequest.h"

#import "TrustDaemonTestCase.h"
#import "OCSPRequestTests_data.h"

@interface OCSPRequestTests : TrustDaemonTestCase
@end

@implementation OCSPRequestTests

- (void)testCreate {
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _ocsp_request_leaf, sizeof(_ocsp_request_leaf));
    SecCertificateRef ca = SecCertificateCreateWithBytes(NULL, _ocsp_request_ca, sizeof(_ocsp_request_ca));

    SecOCSPRequestRef request = SecOCSPRequestCreate(leaf, ca);
    XCTAssert(NULL != request);
    SecOCSPRequestFinalize(request);

    CFReleaseNull(leaf);
    CFReleaseNull(ca);
}

- (void)testGetDER {
    XCTAssert(NULL == SecOCSPRequestGetDER(NULL));

    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _ocsp_request_leaf, sizeof(_ocsp_request_leaf));
    SecCertificateRef ca = SecCertificateCreateWithBytes(NULL, _ocsp_request_ca, sizeof(_ocsp_request_ca));

    SecOCSPRequestRef request = SecOCSPRequestCreate(leaf, ca);
    XCTAssert(NULL != request);

    CFDataRef requestDER = SecOCSPRequestGetDER(request);
    XCTAssert(NULL != requestDER);

    SecOCSPRequestRef decodedRequest = SecOCSPRequestCreateWithDataStrict(requestDER);
    XCTAssert(NULL != decodedRequest);

    //SecOCSPRequestFinalize(decodedRequest);
    SecOCSPRequestFinalize(request);
    CFReleaseNull(leaf);
    CFReleaseNull(ca);
}

@end

