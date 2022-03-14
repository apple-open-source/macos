#if OCTAGON

class OctagonErrorUtilsTest: OctagonTestsBase {
    func testAKErrorRetryable() throws {
        let urlError = NSError(domain: NSURLErrorDomain, code: NSURLErrorTimedOut, userInfo: nil)
        let error = NSError(domain: AKAppleIDAuthenticationErrorDomain, code: 17, userInfo: [NSUnderlyingErrorKey: urlError])
        print(error)
        XCTAssertTrue(error.isRetryable(), "AK/NSURLErrorTimedOut should be retryable")
    }

    func testURLErrorRetryable() throws {
        let error = NSError(domain: NSURLErrorDomain, code: NSURLErrorTimedOut, userInfo: nil)
        print(error)
        XCTAssertTrue(error.isRetryable(), "NSURLErrorTimedOut should be retryable")
    }

    func testCKErrorRetryable() throws {
        let urlError = NSError(domain: NSURLErrorDomain, code: NSURLErrorNotConnectedToInternet, userInfo: nil)
        let ckError = NSError(domain: CKErrorDomain, code: CKError.networkUnavailable.rawValue, userInfo: [NSUnderlyingErrorKey: urlError])
        print(ckError)
        XCTAssertTrue(ckError.isRetryable(), "CK/NSURLErrorNotConnectedToInternet should be retryable")
    }

    func testRetryIntervalCKError() throws {
        let error = NSError(domain: CKErrorDomain, code: 17, userInfo: nil)
        print(error)
        XCTAssertEqual(2, error.retryInterval(), "expect CKError default retry to 2")
    }

    func testRetryIntervalCKErrorRetry() throws {
        let error = NSError(domain: CKErrorDomain, code: 17, userInfo: [CKErrorRetryAfterKey: 17])
        print(error)
        XCTAssertEqual(17, error.retryInterval(), "expect CKError default retry to 17")
    }

    func testRetryIntervalCKErrorRetryBad() throws {
        let error = NSError(domain: CKErrorDomain, code: 17, userInfo: [CKErrorRetryAfterKey: "foo"])
        print(error)
        XCTAssertEqual(2, error.retryInterval(), "expect CKError default retry to 2")
    }

    func testRetryIntervalCKErrorPartial() throws {
        let suberror = NSError(domain: CKErrorDomain, code: 1, userInfo: [CKErrorRetryAfterKey: "4711"])

        let error = NSError(domain: CKErrorDomain, code: CKError.partialFailure.rawValue, userInfo: [CKPartialErrorsByItemIDKey: ["foo": suberror]])
        print(error)
        XCTAssertEqual(4711, error.retryInterval(), "expect CKError default retry to 4711")
    }

    func testRetryIntervalCuttlefish() throws {
        let cuttlefishError = CKPrettyError(domain: CuttlefishErrorDomain,
                                            code: 17,
                                            userInfo: [CuttlefishErrorRetryAfterKey: 101])
        let internalError = CKPrettyError(domain: CKInternalErrorDomain,
                                          code: CKInternalErrorCode.errorInternalPluginError.rawValue,
                                          userInfo: [NSUnderlyingErrorKey: cuttlefishError, ])
        let ckError = CKPrettyError(domain: CKErrorDomain,
                                    code: CKError.serverRejectedRequest.rawValue,
                                    userInfo: [NSUnderlyingErrorKey: internalError,
                                               CKErrorServerDescriptionKey: "Fake: FunctionError domain: CuttlefishError, 17",
                                               ])
        print(ckError)
        XCTAssertEqual(101, ckError.retryInterval(), "cuttlefish retry should be 101")
    }
}

#endif
