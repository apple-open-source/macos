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
}

#endif
