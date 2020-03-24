//
//  MockCuttlefish.swift
//  TrustedPeersHelperUnitTests
//
//  Created by Ben Williamson on 5/1/18.
//

import Foundation
import XCTest

enum Handler {
    case reset((ResetRequest, @escaping (ResetResponse?, Error?) -> Void) -> Void)
    case establish((EstablishRequest, @escaping (EstablishResponse?, Error?) -> Void) -> Void)
    case joinWithVoucher((JoinWithVoucherRequest, @escaping (JoinWithVoucherResponse?, Error?) -> Void) -> Void)
    case updateTrust((UpdateTrustRequest, @escaping (UpdateTrustResponse?, Error?) -> Void) -> Void)
    case setRecoveryKey((SetRecoveryKeyRequest, @escaping (SetRecoveryKeyResponse?, Error?) -> Void) -> Void)
    case fetchChanges((FetchChangesRequest, @escaping (FetchChangesResponse?, Error?) -> Void) -> Void)
    case fetchViableBottles((FetchViableBottlesRequest, @escaping (FetchViableBottlesResponse?, Error?) -> Void) -> Void)
    case fetchPolicyDocuments((FetchPolicyDocumentsRequest,
        @escaping (FetchPolicyDocumentsResponse?, Error?) -> Void) -> Void)
}

class MockCuttlefishAPIAsyncClient: CuttlefishAPIAsync {

    var handlers: [Handler] = []
    var index: Int = 0

    func expect(_ handler: Handler) {
        self.handlers.append(handler)
    }

    func next() -> Handler {
        if index >= handlers.count {
            XCTFail("MockCuttlefish: Not expecting any more calls.")
            self.dump()
            abort()
        }
        return handlers[index]
    }

    func dump() {
        print("---")
        for i in 0 ..< handlers.count {
            print("\(i) \(i == index ? "->" : "  ") \(handlers[i])")
        }
        print("---")
    }

    func reset(_ request: ResetRequest, completion: @escaping (ResetResponse?, Error?) -> Void) {
        print("MockCuttlefish: reset called")
        if case .reset(let f) = next() {
            index += 1
            f(request, completion)
        } else {
            XCTFail("Unexpected: reset")
            print("Unexpected: reset")
            self.dump()
            abort()
        }
    }

    func establish(_ request: EstablishRequest, completion: @escaping (EstablishResponse?, Error?) -> Void) {
        print("MockCuttlefish: establish called")
        if case .establish(let f) = next() {
            index += 1
            f(request, completion)
        } else {
            XCTFail("Unexpected: establish")
            print("Unexpected: establish")
            self.dump()
            abort()
        }
    }

    func joinWithVoucher(_ request: JoinWithVoucherRequest,
                         completion: @escaping (JoinWithVoucherResponse?, Error?) -> Void) {
        print("MockCuttlefish: joinWithVoucher called")
        if case .joinWithVoucher(let f) = next() {
            index += 1
            f(request, completion)
        } else {
            XCTFail("Unexpected: joinWithVoucher")
            print("Unexpected: joinWithVoucher")
            self.dump()
            abort()
        }
    }

    func updateTrust(_ request: UpdateTrustRequest, completion: @escaping (UpdateTrustResponse?, Error?) -> Void) {
        print("MockCuttlefish: updateTrust called")
        if case .updateTrust(let f) = next() {
            index += 1
            f(request, completion)
        } else {
            XCTFail("Unexpected: updateTrust")
            print("Unexpected: updateTrust")
            self.dump()
            abort()
        }
    }

    func setRecoveryKey(_ request: SetRecoveryKeyRequest,
                        completion: @escaping (SetRecoveryKeyResponse?, Error?) -> Void) {
        print("MockCuttlefish: setRecoveryKey called")
        if case .setRecoveryKey(let f) = next() {
            index += 1
            f(request, completion)
        } else {
            XCTFail("Unexpected: setRecoveryKey")
            print("Unexpected: setRecoveryKey")
            self.dump()
            abort()
        }
    }

    func fetchChanges(_ request: FetchChangesRequest, completion: @escaping (FetchChangesResponse?, Error?) -> Void) {
        print("MockCuttlefish: fetchChanges called")
        if case .fetchChanges(let f) = next() {
            index += 1
            f(request, completion)
        } else {
            XCTFail("Unexpected: fetchChanges")
            print("Unexpected: fetchChanges")
            self.dump()
            abort()
        }
    }

    func fetchViableBottles(_ request: FetchViableBottlesRequest, completion: @escaping (FetchViableBottlesResponse?, Error?) -> Void) {
        print("MockCuttlefish: fetchViableBottles called")
        if case .fetchViableBottles(let f) = next() {
            index += 1
            f(request, completion)
        } else {
            XCTFail("Unexpected: fetchViableBottles")
            self.dump()
            abort()
        }
    }

    func fetchPolicyDocuments(_ request: FetchPolicyDocumentsRequest,
                              completion: @escaping (FetchPolicyDocumentsResponse?, Error?) -> Void) {
        print("MockCuttlefish: fetchPolicyDocuments called")
        if case .fetchPolicyDocuments(let f) = next() {
            index += 1
            f(request, completion)
        } else {
            XCTFail("Unexpected: fetchPolicyDocuments")
            print("Unexpected: fetchPolicyDocuments")
            self.dump()
            abort()
        }
    }

    func validatePeers(_: ValidatePeersRequest, completion: @escaping (ValidatePeersResponse?, Error?) -> Void) {
        completion(ValidatePeersResponse(), nil)
    }
    func reportHealth(_: ReportHealthRequest, completion: @escaping (ReportHealthResponse?, Error?) -> Void) {
        print("MockCuttlefish: reportHealth called")
        completion(ReportHealthResponse(), nil)
    }
    func pushHealthInquiry(_: PushHealthInquiryRequest, completion: @escaping (PushHealthInquiryResponse?, Error?) -> Void) {
        completion(PushHealthInquiryResponse(), nil)
    }
    func getRepairAction(_: GetRepairActionRequest, completion: @escaping (GetRepairActionResponse?, Error?) -> Void) {
        completion(GetRepairActionResponse(), nil)

    }
    func getSupportAppInfo(_: GetSupportAppInfoRequest, completion: @escaping (GetSupportAppInfoResponse?, Error?) -> Void) {
        completion(GetSupportAppInfoResponse(), nil)
    }
    func getClubCertificates(_: GetClubCertificatesRequest, completion: @escaping (GetClubCertificatesResponse?, Error?) -> Void) {
        completion(GetClubCertificatesResponse(), nil)
    }
}
