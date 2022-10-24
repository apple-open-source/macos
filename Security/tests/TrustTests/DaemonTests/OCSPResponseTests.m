//
//  OCSPResponseTests.m
//  Security
//
//  Created by Bailey Basile on 4/29/21.
//

#include <AssertMacros.h>
#import <XCTest/XCTest.h>
#import <Security/SecCertificatePriv.h>
#include <utilities/SecCFWrappers.h>
#import "trust/trustd/SecOCSPResponse.h"

#import "TrustDaemonTestCase.h"
#import "OCSPResponseTests_data.h"

const NSString *OCSPResponseParseFailureResources = @"OCSPResponseTests-data/ParseResponseFailureTests";
const NSString *OCSPResponseParseTodoFailureResources = @"OCSPResponseTests-data/ParseResponseTodoFailureTests";
const NSString *OCSPResponseParseSuccessResources = @"OCSPResponseTests-data/ParseResponseSuccessTests";
const NSString *OCSPSingleResponseParseFailureResources = @"OCSPResponseTests-data/ParseSingleResponseFailureTests";
const NSString *OCSPSingleResponseParseSuccessResources = @"OCSPResponseTests-data/ParseSingleResponseSuccessTests";
const NSString *OCSPCTSourcedCorpus =@"OCSPResponseTests-data/CTSourcedCorpus";

@interface OCSPResponseTests : TrustDaemonTestCase
@end

@implementation OCSPResponseTests

- (void)testParseSuccess {
    /* A bunch of OCSP responses with different parsing variations */
    NSArray <NSURL *>* ocspResponseURLs = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".der" subdirectory:(NSString *)OCSPResponseParseSuccessResources];
    XCTAssertTrue([ocspResponseURLs count] > 0, "Unable to find parse test success OCSP responses in bundle.");

    if ([ocspResponseURLs count] > 0) {
        [ocspResponseURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
            NSData *responseData = [NSData dataWithContentsOfURL:url];
            SecOCSPResponseRef response = SecOCSPResponseCreate((__bridge CFDataRef)responseData);
            XCTAssert(NULL != response, "Failed to parse %@", url);
            SecOCSPResponseFinalize(response);
        }];
    }

    NSArray <NSURL *>* ocspDirs = [[NSBundle bundleForClass:[self class]] URLsForResourcesWithExtension:nil subdirectory:(NSString *)OCSPCTSourcedCorpus];
    XCTAssert(ocspDirs.count > 0);
    for (NSURL *dir in ocspDirs) {
        if ([dir pathExtension].length > 0) {
            continue;
        }
        // SHA-1 responses
        NSURL *responseUrl = [NSURL fileURLWithPath:@"ocspResponse.der" relativeToURL:dir];
        NSData *responseData = [NSData dataWithContentsOfURL:responseUrl];
        if (responseData.length > 0) {
            SecOCSPResponseRef response = SecOCSPResponseCreate((__bridge CFDataRef)responseData);
            XCTAssert(NULL != response, "Failed to parse %@", responseUrl);
            SecOCSPResponseFinalize(response);
        }

        // SHA-2 responses
        responseUrl = [NSURL fileURLWithPath:@"ocspResponse.sha256.der" relativeToURL:dir];
        responseData = [NSData dataWithContentsOfURL:responseUrl];
        if (responseData.length > 0) {
            SecOCSPResponseRef response = SecOCSPResponseCreate((__bridge CFDataRef)responseData);
            XCTAssert(NULL != response, "Failed to parse %@", responseUrl);
            SecOCSPResponseFinalize(response);
        }
    }
}

- (void)testParseFailure {
    /* A bunch of OCSP responses with different parsing errors */
    NSArray <NSURL *>* ocspResponseURLs = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".der" subdirectory:(NSString *)OCSPResponseParseFailureResources];
    XCTAssertTrue([ocspResponseURLs count] > 0, "Unable to find parse test failure OCSP responses in bundle.");

    if ([ocspResponseURLs count] > 0) {
        [ocspResponseURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
            NSData *responseData = [NSData dataWithContentsOfURL:url];
            SecOCSPResponseRef response = SecOCSPResponseCreate((__bridge CFDataRef)responseData);
            XCTAssert(NULL == response, "Successfully parsed %@", url);
        }];
    }
}

- (void)testParseTodoFailure {
    /* Errors that currently parse but shouldn't */
    NSArray <NSURL *>* ocspResponseURLs = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".der" subdirectory:(NSString *)OCSPResponseParseTodoFailureResources];
    XCTAssertTrue([ocspResponseURLs count] > 0, "Unable to find parse test todo failure OCSP responses in bundle.");

    if ([ocspResponseURLs count] > 0) {
        [ocspResponseURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
            NSData *responseData = [NSData dataWithContentsOfURL:url];
            SecOCSPResponseRef response = SecOCSPResponseCreate((__bridge CFDataRef)responseData);
            XCTAssert(NULL != response, "Bug fixed! Now failed to parse %@", url);
            SecOCSPResponseFinalize(response);
        }];
    }
}

- (void)testGetters {
    NSData *responseData = [NSData dataWithBytes:_ocsp_resp1 length:sizeof(_ocsp_resp1)];
    SecOCSPResponseRef response = SecOCSPResponseCreateWithID((__bridge CFDataRef)responseData, 42);
    XCTAssert(NULL != response);

    XCTAssertEqual(SecOCSPResponseGetID(response), 42);
    XCTAssertEqualObjects((__bridge NSData *)SecOCSPResponseGetData(response), responseData);
    XCTAssertEqual(SecOCSPGetResponseStatus(response), OCSPResponseStatusSuccessful);
    XCTAssertEqualWithAccuracy(SecOCSPResponseGetExpirationTime(response), 0, 0.001);
    XCTAssertEqualWithAccuracy(SecOCSPResponseProducedAt(response), 632099181.0, 0.001);

    SecOCSPResponseFinalize(response);
}

- (void)testCalculateValidity {
    NSData *responseData = [NSData dataWithBytes:_ocsp_resp1 length:sizeof(_ocsp_resp1)];
    SecOCSPResponseRef response = SecOCSPResponseCreateWithID((__bridge CFDataRef)responseData, 42);
    XCTAssert(NULL != response);

    XCTAssertFalse(SecOCSPResponseCalculateValidity(response, 0, 0, 0));
    XCTAssertFalse(SecOCSPResponseCalculateValidity(response, 0, 0, 999999999.0));
    XCTAssertTrue(SecOCSPResponseCalculateValidity(response, 0, 0, 632104000.0)); // January 11, 2021 at 4:26:40 PM PST
    XCTAssertEqual(response->expireTime, 632142381.0);

    response->producedAt = 123456789;
    XCTAssertFalse(SecOCSPResponseCalculateValidity(response, 0, 0, 600000000.0));

    XCTAssertTrue(SecOCSPResponseCalculateValidity(response, 50, 0, 632104000.0));
    XCTAssertEqual(response->expireTime, 632104050.0);

    XCTAssertTrue(SecOCSPResponseCalculateValidity(response, 50000000, 0, 632104000.0));
    XCTAssertEqual(response->expireTime, 632142381.0);

    SecOCSPResponseFinalize(response);

    responseData = [NSData dataWithBytes:_ocsp_response_no_next_update length:sizeof(_ocsp_response_no_next_update)];
    response = SecOCSPResponseCreateWithID((__bridge CFDataRef)responseData, 42);
    XCTAssert(NULL != response);
    XCTAssertTrue(SecOCSPResponseCalculateValidity(response, 0, 500000, 663900000.0)); // January 14, 2022 at 4:40:00 PM PST
    XCTAssertEqual(response->expireTime, 663900000.0+500000); // expected to be verifyTime + TTL
    SecOCSPResponseFinalize(response);
}

- (void)testCopySingleResponse {
    NSData *responseData = [NSData dataWithBytes:_ocsp_sha1_resp1 length:sizeof(_ocsp_sha1_resp1)];
    SecOCSPResponseRef response = SecOCSPResponseCreateWithID((__bridge CFDataRef)responseData, 42);
    /* No request */
    SecOCSPSingleResponseRef singleResponse = SecOCSPResponseCopySingleResponse(response, NULL);
    XCTAssertEqual(singleResponse, NULL);

    /* Testing-style request (from bytes) */
    NSData *requestData = [NSData dataWithBytes:_ocsp_sha1_req1 length:sizeof(_ocsp_sha1_req1)];
    SecOCSPRequestRef request = SecOCSPRequestCreateWithData((__bridge CFDataRef)requestData);
    singleResponse = SecOCSPResponseCopySingleResponse(response, request);
    XCTAssertNotEqual(singleResponse, NULL);
    SecOCSPSingleResponseDestroy(singleResponse);
    singleResponse = NULL;

    /* Request with original certs */
    SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, _ocsp_sha1_cert, sizeof(_ocsp_sha1_cert));
    SecCertificateRef issuer = SecCertificateCreateWithBytes(NULL, _ocsp_sha1_issuer, sizeof(_ocsp_sha1_issuer));
    request->certificate = CFRetainSafe(cert);
    request->issuer = CFRetainSafe(issuer);
    singleResponse = SecOCSPResponseCopySingleResponse(response, request);
    XCTAssertNotEqual(singleResponse, NULL);
    SecOCSPSingleResponseDestroy(singleResponse);

    /* serial number mismatch */
    NSMutableData *mismatchRequest = [NSMutableData dataWithData:requestData];
    [mismatchRequest resetBytesInRange:NSMakeRange(66, 2)]; // location within serial number
    request = SecOCSPRequestCreateWithData((__bridge CFDataRef)mismatchRequest);
    singleResponse = SecOCSPResponseCopySingleResponse(response, request);
    XCTAssertEqual(singleResponse, NULL);
    SecOCSPRequestFinalize(request);
    SecOCSPResponseFinalize(response);

    /* unsupported hash algorithm */
    NSMutableData *unknownHashAlg = [NSMutableData dataWithData:responseData];
    [unknownHashAlg resetBytesInRange:NSMakeRange(90, 2)]; // location within certID hash algorithm
    response = SecOCSPResponseCreateWithID((__bridge CFDataRef)unknownHashAlg, 42);
    request = SecOCSPRequestCreateWithData((__bridge CFDataRef)requestData);
    request->certificate = CFRetainSafe(cert);
    request->issuer = CFRetainSafe(issuer);
    singleResponse = SecOCSPResponseCopySingleResponse(response, request);
    XCTAssertEqual(singleResponse, NULL);

    SecOCSPRequestFinalize(request);
    SecOCSPResponseFinalize(response);
    CFReleaseNull(cert);
    CFReleaseNull(issuer);
}

- (void)testCopySigners {
    NSData *responseData = [NSData dataWithBytes:_ocsp_resp1 length:sizeof(_ocsp_resp1)];
    SecOCSPResponseRef response = SecOCSPResponseCreateWithID((__bridge CFDataRef)responseData, 42);
    XCTAssert(NULL != response);

    NSArray *signers = CFBridgingRelease(SecOCSPResponseCopySigners(response));
    XCTAssertNotNil(signers);
    XCTAssertEqual(signers.count, 2);

    SecOCSPResponseFinalize(response);
}

- (void)testCopySigner {
    // Signer in response and responder ID  identified by hash
    NSData *responseData = [NSData dataWithBytes:_ocsp_resp1 length:sizeof(_ocsp_resp1)];
    SecOCSPResponseRef response = SecOCSPResponseCreateWithID((__bridge CFDataRef)responseData, 42);
    XCTAssert(NULL != response);

    SecCertificateRef signer = SecOCSPResponseCopySigner(response, NULL);
    XCTAssertNotEqual(signer, NULL);
    CFReleaseNull(signer);
    SecOCSPResponseFinalize(response);

    // Change a byte in the signature so that the issuer signature check fails
    NSMutableData *badData = [responseData mutableCopy];
    [badData resetBytesInRange:NSMakeRange(300, 1)];
    response = SecOCSPResponseCreate((__bridge CFDataRef)badData);
    XCTAssert(NULL != response);
    signer = SecOCSPResponseCopySigner(response, NULL);
    XCTAssertEqual(signer, NULL);
    CFReleaseNull(signer);
    SecOCSPResponseFinalize(response);

    // Change a byte in one of the certificates, so it fails to parse
    badData = [responseData mutableCopy];
    [badData resetBytesInRange:NSMakeRange(330, 1)];
    response = SecOCSPResponseCreate((__bridge CFDataRef)badData);
    XCTAssert(NULL == response);

    // Signer not in response
    responseData = [NSData dataWithBytes:_ocsp_no_certs_response length:sizeof(_ocsp_no_certs_response)];
    response = SecOCSPResponseCreate((__bridge CFDataRef)responseData);
    XCTAssert(NULL != response);
    signer = SecOCSPResponseCopySigner(response, NULL);
    XCTAssertEqual(signer, NULL);

    SecCertificateRef issuer = (__bridge SecCertificateRef)[self SecCertificateCreateFromPEMResource:@"digicert_sha2_issuer" subdirectory:(NSString *)OCSPResponseParseSuccessResources];
    signer = SecOCSPResponseCopySigner(response, issuer);
    XCTAssertEqual(signer, issuer);

    CFReleaseNull(issuer);
    CFReleaseNull(signer);
    SecOCSPResponseFinalize(response);

    // Signer in response, responder ID identified by name
    responseData = [NSData dataWithBytes:_ocsp_response_no_next_update length:sizeof(_ocsp_response_no_next_update)];
    response = SecOCSPResponseCreate((__bridge CFDataRef)responseData);
    XCTAssert(NULL != response);
    signer = SecOCSPResponseCopySigner(response, NULL);
    XCTAssertNotEqual(signer, NULL);
    CFReleaseNull(signer);
    SecOCSPResponseFinalize(response);

    // Change response to have different responder ID
    badData = [responseData mutableCopy];
    [badData resetBytesInRange:NSMakeRange(48, 1)];
    response = SecOCSPResponseCreate((__bridge CFDataRef)badData);
    XCTAssert(NULL != response);
    signer = SecOCSPResponseCopySigner(response, NULL);
    XCTAssertEqual(signer, NULL);
    CFReleaseNull(signer);
    SecOCSPResponseFinalize(response);
}

- (void)testIsWeakHash {
    NSData *responseData = [NSData dataWithBytes:_ocsp_resp1 length:sizeof(_ocsp_resp1)];
    SecOCSPResponseRef response = SecOCSPResponseCreateWithID((__bridge CFDataRef)responseData, 42);
    XCTAssert(NULL != response);
    XCTAssertFalse(SecOCSPResponseIsWeakHash(response));
    SecOCSPResponseFinalize(response);

    responseData = [NSData dataWithBytes:_ocsp_sha1_resp1 length:sizeof(_ocsp_sha1_resp1)];
    response = SecOCSPResponseCreateWithID((__bridge CFDataRef)responseData, 42);
    XCTAssert(NULL != response);
    XCTAssert(SecOCSPResponseIsWeakHash(response));
    SecOCSPResponseFinalize(response);
}

@end

@interface OCSPSingleResponseTests : TrustDaemonTestCase
@end

@implementation OCSPSingleResponseTests

- (void)testCTSourcedResponses {
    /* Lists of CT sourced responses with single responses were generated with
     for dir in `ls`; do if `openssl ocsp -text -noverify -respin $dir/ocspResponse.der 2>&1 | grep -q "Responses"`; then echo $dir; fi; done > singleSha1OcspResponses.list
     for dir in `ls`; do if `openssl ocsp -text -noverify -respin $dir/ocspResponse.sha256.der 2>&1 | grep -q "Responses"`; then echo $dir; fi; done > singleSha256OcspResponses.list
     */
    NSArray <NSURL *>*singleResponseLists = [[NSBundle bundleForClass:[self class]] URLsForResourcesWithExtension:@"list" subdirectory:(NSString *)OCSPCTSourcedCorpus];
    XCTAssert(singleResponseLists.count > 0);
    for (NSURL *listUrl in singleResponseLists) {
        NSString *dirsString = [NSString stringWithContentsOfURL:listUrl encoding:NSUTF8StringEncoding error:nil];
        NSArray <NSString *>*dirs = [dirsString componentsSeparatedByString:@"\n"];
        for (NSString *dir in dirs) {
            NSString *filename = [[listUrl relativeString] containsString:@"Sha256"] ? @"ocspResponse.sha256" : @"ocspResponse";
            NSURL *respURL = [[NSBundle bundleForClass:[self class]] URLForResource:filename withExtension:@"der" subdirectory:[NSString stringWithFormat:@"%@/%@", OCSPCTSourcedCorpus, dir]];
            filename = [[listUrl relativeString] containsString:@"Sha256"] ? @"ocspRequest.sha256" : @"ocspRequest";
            NSURL *reqURL = [[NSBundle bundleForClass:[self class]] URLForResource:filename withExtension:@"der" subdirectory:[NSString stringWithFormat:@"%@/%@", OCSPCTSourcedCorpus, dir]];

            NSData *responseData = [NSData dataWithContentsOfURL:respURL];
            if (responseData.length <= 0) { continue; }
            SecOCSPResponseRef response = SecOCSPResponseCreate((__bridge CFDataRef)responseData);
            XCTAssert(NULL != response, "Failed to parse response %@", respURL);

            NSData *requestData = [NSData dataWithContentsOfURL:reqURL];
            if (requestData.length <= 0) { continue; }
            SecOCSPRequestRef request = SecOCSPRequestCreateWithData((__bridge CFDataRef)requestData);
            XCTAssert(NULL != request, "Failed to create request %@", respURL);

            SecOCSPSingleResponseRef singleResponse = SecOCSPResponseCopySingleResponse(response, request);
            if (!singleResponse) {
                // Fallback case where the response certId doesn't match the request but does match the certs (e.g. different hash algorithms)
                NSURL *issuerURL = [[NSBundle bundleForClass:[self class]] URLForResource:@"issuer.crt" withExtension:@"pem" subdirectory:[NSString stringWithFormat:@"%@/%@", OCSPCTSourcedCorpus, dir]];
                NSArray <NSURL *>* certUrls = [[NSBundle bundleForClass:[self class]] URLsForResourcesWithExtension:@"crt.pem" subdirectory:[NSString stringWithFormat:@"%@/%@", OCSPCTSourcedCorpus, dir]];

                SecCertificateRef issuer = SecCertificateCreateWithPEM(NULL, (__bridge CFDataRef)[NSData dataWithContentsOfURL:issuerURL]);
                SecCertificateRef leaf = NULL;
                for (NSURL *certUrl in certUrls) {
                    if ([[certUrl relativeString] containsString:@"issuer"]) {
                        continue;
                    }
                    leaf = SecCertificateCreateWithPEM(NULL, (__bridge CFDataRef)[NSData dataWithContentsOfURL:certUrl]);
                }

                if (issuer && leaf) {
                    SecOCSPRequestFinalize(request);
                    request = SecOCSPRequestCreate(leaf, issuer);
                    singleResponse = SecOCSPResponseCopySingleResponse(response, request);
                }

                CFReleaseNull(issuer);
                CFReleaseNull(leaf);
            }

            XCTAssert(NULL != singleResponse, "Failed to parse single response: %@", respURL);
            SecOCSPSingleResponseDestroy(singleResponse);
            SecOCSPResponseFinalize(response);
            SecOCSPRequestFinalize(request);
        }
    }
}

- (void)testParseSuccess {
    NSArray <NSURL *>* ocspResponseURLs = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".der" subdirectory:(NSString *)OCSPSingleResponseParseSuccessResources];
    XCTAssertTrue([ocspResponseURLs count] > 0, "Unable to find parse test success OCSP responses in bundle.");

    if ([ocspResponseURLs count] > 0) {
        [ocspResponseURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
            NSData *responseData = [NSData dataWithContentsOfURL:url];
            SecOCSPResponseRef response = SecOCSPResponseCreate((__bridge CFDataRef)responseData);
            XCTAssert(NULL != response, "Failed to parse response %@", url);
            NSData *requestData = [NSData dataWithContentsOfURL:[url URLByAppendingPathExtension:@"req"]];
            SecOCSPRequestRef request = SecOCSPRequestCreateWithData((__bridge CFDataRef)requestData);
            XCTAssert(NULL != request, "Failed to parse request %@", url);

            SecOCSPSingleResponseRef singleResponse = SecOCSPResponseCopySingleResponse(response, request);
            XCTAssert(NULL != singleResponse, "Failed to parse single response: %@", url);
            SecOCSPSingleResponseDestroy(singleResponse);
            SecOCSPResponseFinalize(response);
            SecOCSPRequestFinalize(request);
        }];
    }
}

- (void)testParseFailure {
    NSArray <NSURL *>* ocspResponseURLs = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".der" subdirectory:(NSString *)OCSPSingleResponseParseFailureResources];
    XCTAssertTrue([ocspResponseURLs count] > 0, "Unable to find parse test failure OCSP responses in bundle.");

    if ([ocspResponseURLs count] > 0) {
        [ocspResponseURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
            NSData *responseData = [NSData dataWithContentsOfURL:url];
            SecOCSPResponseRef response = SecOCSPResponseCreate((__bridge CFDataRef)responseData);
            XCTAssert(NULL != response, "Failed to parse %@", url);
            if (!response) {
                return;
            }
            NSData *requestData = [NSData dataWithContentsOfURL:[url URLByAppendingPathExtension:@"req"]];
            SecOCSPRequestRef request = SecOCSPRequestCreateWithData((__bridge CFDataRef)requestData);
            XCTAssert(NULL != request, "Failed to parse request %@", url);

            SecOCSPSingleResponseRef singleResponse = SecOCSPResponseCopySingleResponse(response, request);
            XCTAssert(NULL == singleResponse, "Successfully parsed single response: %@", url);
            SecOCSPResponseFinalize(response);
            SecOCSPRequestFinalize(request);
        }];
    }
}

- (void)testParseTodoFailure {
    /* Errors that currently parse but shouldn't */
    NSArray <NSURL *>* ocspResponseURLs = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".der" subdirectory:(NSString *)OCSPResponseParseTodoFailureResources];
    XCTAssertTrue([ocspResponseURLs count] > 0, "Unable to find parse test todo failure OCSP responses in bundle.");

    if ([ocspResponseURLs count] > 0) {
        [ocspResponseURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
            NSData *responseData = [NSData dataWithContentsOfURL:url];
            SecOCSPResponseRef response = SecOCSPResponseCreate((__bridge CFDataRef)responseData);
            XCTAssert(NULL != response, "Failed to parse response %@", url);
            NSData *requestData = [NSData dataWithContentsOfURL:[url URLByAppendingPathExtension:@"req"]];
            SecOCSPRequestRef request = SecOCSPRequestCreateWithData((__bridge CFDataRef)requestData);
            XCTAssert(NULL != request, "Failed to parse request %@", url);

            SecOCSPSingleResponseRef singleResponse = SecOCSPResponseCopySingleResponse(response, request);
            XCTAssert(NULL != singleResponse, "Bug fixed! Now failed to parse single response: %@", url);
            SecOCSPSingleResponseDestroy(singleResponse);
            SecOCSPResponseFinalize(response);
            SecOCSPRequestFinalize(request);
        }];
    }
}

- (void)testCalculateValidity {
    NSData *responseData = [NSData dataWithBytes:_ocsp_sha1_resp1 length:sizeof(_ocsp_sha1_resp1)];
    SecOCSPResponseRef response = SecOCSPResponseCreateWithID((__bridge CFDataRef)responseData, 42);
    NSData *requestData = [NSData dataWithBytes:_ocsp_sha1_req1 length:sizeof(_ocsp_sha1_req1)];
    SecOCSPRequestRef request = SecOCSPRequestCreateWithData((__bridge CFDataRef)requestData);
    SecOCSPSingleResponseRef singleResponse = SecOCSPResponseCopySingleResponse(response, request);
    XCTAssertNotEqual(singleResponse, NULL);

    XCTAssert(SecOCSPSingleResponseCalculateValidity(singleResponse, 0, 645100000.0)); // June 11, 2021 at 3:26:40 AM PDT
    XCTAssertFalse(SecOCSPSingleResponseCalculateValidity(singleResponse, 0, 0));
    XCTAssertFalse(SecOCSPSingleResponseCalculateValidity(singleResponse, 0, 999999999.0));

    /* use thisUpdate + ttl instead */
    singleResponse->nextUpdate = 0.0;
    XCTAssert(SecOCSPSingleResponseCalculateValidity(singleResponse, 7200, 643800000.0)); // 2021-05-27 02:20:00 PDT

    SecOCSPSingleResponseDestroy(singleResponse);
    SecOCSPResponseFinalize(response);
    SecOCSPRequestFinalize(request);
}

- (void)testCopySCTs {
    /* No SCTs */
    NSData *responseData = [NSData dataWithBytes:_ocsp_sha1_resp1 length:sizeof(_ocsp_sha1_resp1)];
    SecOCSPResponseRef response = SecOCSPResponseCreateWithID((__bridge CFDataRef)responseData, 42);
    NSData *requestData = [NSData dataWithBytes:_ocsp_sha1_req1 length:sizeof(_ocsp_sha1_req1)];
    SecOCSPRequestRef request = SecOCSPRequestCreateWithData((__bridge CFDataRef)requestData);
    SecOCSPSingleResponseRef singleResponse = SecOCSPResponseCopySingleResponse(response, request);
    XCTAssertNotEqual(singleResponse, NULL);
    XCTAssertEqual(SecOCSPSingleResponseCopySCTs(singleResponse), NULL);
    SecOCSPSingleResponseDestroy(singleResponse);
    SecOCSPResponseFinalize(response);
    SecOCSPRequestFinalize(request);

    /* SCTs present */
    responseData = [NSData dataWithBytes:_scts_response length:sizeof(_scts_response)];
    response = SecOCSPResponseCreateWithID((__bridge CFDataRef)responseData, 42);
    requestData = [NSData dataWithBytes:_scts_request length:sizeof(_scts_request)];
    request = SecOCSPRequestCreateWithData((__bridge CFDataRef)requestData);
    singleResponse = SecOCSPResponseCopySingleResponse(response, request);
    XCTAssertNotEqual(singleResponse, NULL);
    XCTAssertNotEqual(SecOCSPSingleResponseCopySCTs(singleResponse), NULL);
    SecOCSPSingleResponseDestroy(singleResponse);
    SecOCSPResponseFinalize(response);
    SecOCSPRequestFinalize(request);
}

@end

