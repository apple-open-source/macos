//
//  ExceptionTests.swift
//  EvaluationTests
//
#if false // module issue, see rdar://131315137
#if !os(bridgeOS)
import XCTest
import Dispatch
import Foundation
import Security

class SecTrustExceptionStresser {

    let cert_data = Data(base64Encoded:
        "MIIFSzCCBDOgAwIBAgIQSueVSfqavj8QDxekeOFpCTANBgkqhkiG9w0BAQsFADCB" +
        "kDELMAkGA1UEBhMCR0IxGzAZBgNVBAgTEkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4G" +
        "A1UEBxMHU2FsZm9yZDEaMBgGA1UEChMRQ09NT0RPIENBIExpbWl0ZWQxNjA0BgNV" +
        "BAMTLUNPTU9ETyBSU0EgRG9tYWluIFZhbGlkYXRpb24gU2VjdXJlIFNlcnZlciBD" +
        "QTAeFw0xNTA0MDkwMDAwMDBaFw0xNTA0MTIyMzU5NTlaMFkxITAfBgNVBAsTGERv" +
        "bWFpbiBDb250cm9sIFZhbGlkYXRlZDEdMBsGA1UECxMUUG9zaXRpdmVTU0wgV2ls" +
        "ZGNhcmQxFTATBgNVBAMUDCouYmFkc3NsLmNvbTCCASIwDQYJKoZIhvcNAQEBBQAD" +
        "ggEPADCCAQoCggEBAMIE7PiM7gTCs9hQ1XBYzJMY61yoaEmwIrX5lZ6xKyx2PmzA" +
        "S2BMTOqytMAPgLaw+XLJhgL5XEFdEyt/ccRLvOmULlA3pmccYYz2QULFRtMWhyef" +
        "dOsKnRFSJiFzbIRMeVXk0WvoBj1IFVKtsyjbqv9u/2CVSndrOfEk0TG23U3AxPxT" +
        "uW1CrbV8/q71FdIzSOciccfCFHpsKOo3St/qbLVytH5aohbcabFXRNsKEqveww9H" +
        "dFxBIuGa+RuT5q0iBikusbpJHAwnnqP7i/dAcgCskgjZjFeEU4EFy+b+a1SYQCeF" +
        "xxC7c3DvaRhBB0VVfPlkPz0sw6l865MaTIbRyoUCAwEAAaOCAdUwggHRMB8GA1Ud" +
        "IwQYMBaAFJCvajqUWgvYkOoSVnPfQ7Q6KNrnMB0GA1UdDgQWBBSd7sF7gQs6R2lx" +
        "GH0RN5O8pRs/+zAOBgNVHQ8BAf8EBAMCBaAwDAYDVR0TAQH/BAIwADAdBgNVHSUE" +
        "FjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwTwYDVR0gBEgwRjA6BgsrBgEEAbIxAQIC" +
        "BzArMCkGCCsGAQUFBwIBFh1odHRwczovL3NlY3VyZS5jb21vZG8uY29tL0NQUzAI" +
        "BgZngQwBAgEwVAYDVR0fBE0wSzBJoEegRYZDaHR0cDovL2NybC5jb21vZG9jYS5j" +
        "b20vQ09NT0RPUlNBRG9tYWluVmFsaWRhdGlvblNlY3VyZVNlcnZlckNBLmNybDCB" +
        "hQYIKwYBBQUHAQEEeTB3ME8GCCsGAQUFBzAChkNodHRwOi8vY3J0LmNvbW9kb2Nh" +
        "LmNvbS9DT01PRE9SU0FEb21haW5WYWxpZGF0aW9uU2VjdXJlU2VydmVyQ0EuY3J0" +
        "MCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5jb21vZG9jYS5jb20wIwYDVR0RBBww" +
        "GoIMKi5iYWRzc2wuY29tggpiYWRzc2wuY29tMA0GCSqGSIb3DQEBCwUAA4IBAQBq" +
        "evHa/wMHcnjFZqFPRkMOXxQhjHUa6zbgH6QQFezaMyV8O7UKxwE4PSf9WNnM6i1p" +
        "OXy+l+8L1gtY54x/v7NMHfO3kICmNnwUW+wHLQI+G1tjWxWrAPofOxkt3+IjEBEH" +
        "fnJ/4r+3ABuYLyw/zoWaJ4wQIghBK4o+gk783SHGVnRwpDTysUCeK1iiWQ8dSO/r" +
        "ET7BSp68ZVVtxqPv1dSWzfGuJ/ekVxQ8lEEFeouhN0fX9X3c+s5vMaKwjOrMEpsi" +
        "8TRwz311SotoKQwe6Zaoz7ASH1wq7mcvf71z81oBIgxw+s1F73hczg36TuHvzmWf" +
        "RwxPuzZEaFZcVlmtqoq8"
    )!

    let read_queue = DispatchQueue(label: "com.apple.trustd.test-read-queue", attributes: [.concurrent])
    let eval_queue = DispatchQueue(label: "com.apple.trustd.test-eval-queue", attributes: [.concurrent])
    let write_queue = DispatchQueue(label: "com.apple.trustd.test-write-queue", attributes: [.concurrent])
    let hostname = "expired.badssl.com"
    let queue_limit = 10
    let seconds_to_live = 5.0

    var trust: SecTrust? = nil
    var policy: SecPolicy? = nil
    var cert: SecCertificate? = nil
    var cur_reads = 0
    var cur_evals = 0
    var cur_writes = 0
    var exception_set = 0
    var timeout_reached = 0

    func setup() {
        var result: OSStatus
        cert = SecCertificateCreateWithData(kCFAllocatorDefault, self.cert_data as CFData)!
        let certs = [cert]
        policy = SecPolicyCreateSSL(true, self.hostname as CFString?)
        result = SecTrustCreateWithCertificates(certs as CFArray, policy, &trust)
        guard result == errSecSuccess else {
            print("setup error: failed to initialize trust")
            exit(1)
        }
        self.read_queue.asyncAfter(deadline: .now() + self.seconds_to_live) {
            self.timeout_reached = 1
        }
    }

    func run() -> Bool {
        setup()
        while (self.timeout_reached == 0) {
            if (self.cur_reads < self.queue_limit) {
                self.cur_reads += 1
                read_queue.async {
                    var exceptions: CFData? = nil
                    exceptions = SecTrustCopyExceptions(self.trust!)
                    if (exceptions != nil) {
                        print("c", terminator: "")
                    } else {
                        print("!", terminator: "")
                    }
                    self.cur_reads -= 1
                }
            }
            if (self.cur_evals < self.queue_limit) {
                self.cur_evals += 1
                eval_queue.async {
                    var result:OSStatus
                    result = SecTrustSetAnchorCertificatesOnly(self.trust!, false)
                    if (result != errSecSuccess) {
                        print("eval: \(result)")
                    }
                    let chain = SecTrustCopyCertificateChain(self.trust!)
                    let count = CFArrayGetCount(chain!)
                    print("\(count)", terminator: "")
                    self.cur_evals -= 1
                }
            }
            if (self.cur_writes < self.queue_limit) {
                self.cur_writes += 1
                write_queue.async {
                    var exceptions: CFData? = nil
                    var ok: Bool? = false
                    guard self.trust != nil else {
                        print("write: no trust reference")
                        self.cur_writes -= 1
                        return
                    }
                    exceptions = SecTrustCopyExceptions(self.trust!)
                    guard exceptions != nil else {
                        print("write: no exceptions")
                        self.cur_writes -= 1
                        return
                    }
                    if (self.exception_set < 1) {
                        ok = SecTrustSetExceptions(self.trust!, exceptions)
                        if (ok == true) {
                            print("s", terminator: "")
                            self.exception_set = 1
                        } else {
                            print("!", terminator: "")
                        }
                    }
                    self.cur_writes -= 1
                }
            }
        }
	    // If we get here without crashing, the test is a success
        print("")
	    print("SUCCESS")
        return true
    }
}

class TrustExceptionSwiftTests: XCTestCase {

    func testExceptionStress() throws {
        let stress = SecTrustExceptionStresser()
        XCTAssertTrue(stress.run())
    }
}

#endif // !bridgeOS
#endif // false
