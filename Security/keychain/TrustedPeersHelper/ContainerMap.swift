/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

import CloudKit
import CloudKit_Private
import CloudKitCode
import CoreData
import Foundation
import InternalSwiftProtobuf

// TODO: merge into CodeConnection

let CuttlefishPushTopicBundleIdentifier = "com.apple.security.cuttlefish"

private let logger = Logger(subsystem: "com.apple.security.trustedpeers", category: "containermap")

struct CKInternalErrorMatcher {
    let code: Int
    let internalCode: Int
    let noL3Error: Bool
}

// Match a CKError/CKInternalError
func ~= (pattern: CKInternalErrorMatcher, value: Error?) -> Bool {
    guard let value = value else {
        return false
    }
    let error = value as NSError
    guard let underlyingError = error.userInfo[NSUnderlyingErrorKey] as? NSError else {
        return false
    }
    guard error.domain == CKErrorDomain && error.code == pattern.code &&
        underlyingError.domain == CKInternalErrorDomain && underlyingError.code == pattern.internalCode else {
        return false
    }
    guard pattern.noL3Error else {
        return true
    }
    return underlyingError.userInfo[NSUnderlyingErrorKey] == nil
}

struct CKErrorMatcher {
    let code: Int
}

// Match a CKError
func ~= (pattern: CKErrorMatcher, value: Error?) -> Bool {
    guard let value = value else {
        return false
    }
    let error = value as NSError
    return error.domain == CKErrorDomain && error.code == pattern.code
}

struct NSURLErrorMatcher {
    let code: Int
}

// Match an NSURLError
func ~= (pattern: NSURLErrorMatcher, value: Error?) -> Bool {
    guard let value = value else {
        return false
    }
    let error = value as NSError
    return error.domain == NSURLErrorDomain && error.code == pattern.code
}

public class RetryingCKCodeService: CuttlefishAPIAsync {

    private let underlyingCKOperationRunner: CKOperationRunner
    private let queue: DispatchQueue

    internal init(retry: CKOperationRunner) {
        self.underlyingCKOperationRunner = retry
        self.queue = DispatchQueue(label: "RetryingCKCodeService", qos: .userInitiated)
    }

    public class func retryableError(error: Error?) -> Bool {
        switch error {
        case NSURLErrorMatcher(code: NSURLErrorTimedOut):
            return true
        case CKErrorMatcher(code: CKError.networkFailure.rawValue):
            return true
        case CKInternalErrorMatcher(code: CKError.serverRejectedRequest.rawValue, internalCode: CKInternalErrorCode.errorInternalServerInternalError.rawValue, noL3Error: false):
            return true
        case CKInternalErrorMatcher(code: CKError.serverRejectedRequest.rawValue, internalCode: CKInternalErrorCode.errorInternalPluginError.rawValue, noL3Error: true):
            return true
        case CuttlefishErrorMatcher(code: CuttlefishErrorCode.retryableServerFailure):
            return true
        case CuttlefishErrorMatcher(code: CuttlefishErrorCode.transactionalFailure):
            return true
        default:
            return false
        }
    }

    public func invokeRetry<RequestType, ResponseType>(deadline: Date, minimumDelay: TimeInterval, functionName: String,
                                                       operationCreator: @escaping () -> CloudKitCode.CKCodeOperation<RequestType, ResponseType>,
                                                       completion: @escaping (Swift.Result<ResponseType, Swift.Error>) -> Swift.Void) {
        let op = operationCreator()

        op.configuration.discretionaryNetworkBehavior = .nonDiscretionary
        op.configuration.automaticallyRetryNetworkFailures = false
        op.configuration.isCloudKitSupportOperation = true
        op.configuration.setApplicationBundleIdentifierOverride(CuttlefishPushTopicBundleIdentifier)

        let requestCompletion = { (requestInfo: CKRequestInfo?) -> Void in
            if let requestUUID = requestInfo?.requestUUID {
                logger.debug("ckoperation request finished: \(functionName, privacy: .public) \(requestUUID, privacy: .public)")
            }
        }
        op.requestCompletedBlock = requestCompletion

        op.codeOperationResultBlock = { response in
            switch response {
            case .success:
                completion(response)
            case .failure(let error):
                if RetryingCKCodeService.retryableError(error: error) {
                    let now = Date()
                    // Check cuttlefish and CKError retry afters.
                    let cuttlefishDelay = CuttlefishRetryAfter(error: error)
                    let ckDelay = CKRetryAfterSecondsForError(error)
                    let delay = max(minimumDelay, cuttlefishDelay, ckDelay)
                    let cutoff = Date(timeInterval: delay, since: now)
                    guard cutoff.compare(deadline) == ComparisonResult.orderedDescending else {
                        Thread.sleep(forTimeInterval: delay)
                        logger.debug("\(functionName, privacy: .public) error: \(String(describing: error), privacy: .public) (retrying, now=\(String(describing: now), privacy: .public), deadline=\(String(describing: deadline), privacy: .public)")

                        self.invokeRetry(deadline: deadline, minimumDelay: minimumDelay, functionName: functionName, operationCreator: operationCreator, completion: completion)
                        return
                    }
                }
                completion(.failure(error))
                return
            }
        }

        // Hack to fool CloudKit into allowing us to use QoS UserInitiated to get our requests off-device in a reasonable amount of time, real solution is tracked in <rdar://problem/49086080>
        self.queue.async {
            op.qualityOfService = .userInitiated
            self.underlyingCKOperationRunner.add(op)
        }
    }

    public func retry<RequestType, ResponseType>(functionName: String, operationCreator: @escaping () -> CloudKitCode.CKCodeOperation<RequestType, ResponseType>, completion: @escaping (Swift.Result<ResponseType, Swift.Error>) -> Swift.Void) {
        let now = Date()
        let deadline = Date(timeInterval: 30, since: now)
        let minimumDelay = TimeInterval(5)
        invokeRetry(deadline: deadline, minimumDelay: minimumDelay, functionName: functionName, operationCreator: operationCreator, completion: completion)
    }

    public func reset( _ request: ResetRequest, completion: @escaping (Swift.Result<ResetResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.ResetOperation(request: request)
        }, completion: completion)
    }
    public func establish(
      _ request: EstablishRequest,
      completion: @escaping (Swift.Result<EstablishResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.EstablishOperation(request: request)
        }, completion: completion)
    }
    public func joinWithVoucher(
      _ request: JoinWithVoucherRequest,
      completion: @escaping (Swift.Result<JoinWithVoucherResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.JoinWithVoucherOperation(request: request)
        }, completion: completion)
    }
    public func updateTrust(
      _ request: UpdateTrustRequest,
      completion: @escaping (Swift.Result<UpdateTrustResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.UpdateTrustOperation(request: request)
        }, completion: completion)
    }
    public func setRecoveryKey(
      _ request: SetRecoveryKeyRequest,
      completion: @escaping (Swift.Result<SetRecoveryKeyResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.SetRecoveryKeyOperation(request: request)
        }, completion: completion)
    }
    public func fetchChanges(
      _ request: FetchChangesRequest,
      completion: @escaping (Swift.Result<FetchChangesResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.FetchChangesOperation(request: request)
        }, completion: completion)
    }
    public func fetchViableBottles(
      _ request: FetchViableBottlesRequest,
      completion: @escaping (Swift.Result<FetchViableBottlesResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.FetchViableBottlesOperation(request: request)
        }, completion: completion)
    }
    public func fetchPolicyDocuments(
      _ request: FetchPolicyDocumentsRequest,
      completion: @escaping (Swift.Result<FetchPolicyDocumentsResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.FetchPolicyDocumentsOperation(request: request)
        }, completion: completion)
    }
    public func validatePeers(
      _ request: ValidatePeersRequest,
      completion: @escaping (Swift.Result<ValidatePeersResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.ValidatePeersOperation(request: request)
        }, completion: completion)
    }
    public func reportHealth(
      _ request: ReportHealthRequest,
      completion: @escaping (Swift.Result<ReportHealthResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.ReportHealthOperation(request: request)
        }, completion: completion)
    }
    public func pushHealthInquiry(
      _ request: PushHealthInquiryRequest,
      completion: @escaping (Swift.Result<PushHealthInquiryResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.PushHealthInquiryOperation(request: request)
        }, completion: completion)
    }
    public func getRepairAction(
      _ request: GetRepairActionRequest,
      completion: @escaping (Swift.Result<GetRepairActionResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.GetRepairActionOperation(request: request)
        }, completion: completion)
    }
    public func getSupportAppInfo(
      _ request: GetSupportAppInfoRequest,
      completion: @escaping (Swift.Result<GetSupportAppInfoResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.GetSupportAppInfoOperation(request: request)
        }, completion: completion)
    }
    public func getClubCertificates(
      _ request: GetClubCertificatesRequest,
      completion: @escaping (Swift.Result<GetClubCertificatesResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.GetClubCertificatesOperation(request: request)
        }, completion: completion)
    }
    public func fetchSosiCloudIdentity(
      _ request: FetchSOSiCloudIdentityRequest,
      completion: @escaping (Swift.Result<FetchSOSiCloudIdentityResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.FetchSosiCloudIdentityOperation(request: request)
        }, completion: completion)
    }
    public func resetAccountCdpcontents(
      _ request: ResetAccountCDPContentsRequest,
      completion: @escaping (Swift.Result<ResetAccountCDPContentsResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.ResetAccountCdpcontentsOperation(request: request)
        }, completion: completion)
    }
    public func addCustodianRecoveryKey(
        _ request: AddCustodianRecoveryKeyRequest,
        completion: @escaping (Result<AddCustodianRecoveryKeyResponse, Error>) -> Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.AddCustodianRecoveryKeyOperation(request: request)
        }, completion: completion)
    }
    public func fetchRecoverableTlkshares(
        _ request: FetchRecoverableTLKSharesRequest,
        completion: @escaping (Result<FetchRecoverableTLKSharesResponse, Error>) -> Void) {
        retry(functionName: #function, operationCreator: {
            return CuttlefishAPI.FetchRecoverableTlksharesOperation(request: request)
        }, completion: completion)
    }
}

protocol CKOperationRunner {
    func add<RequestType, ResponseType>(_ operation: CloudKitCode.CKCodeOperation<RequestType, ResponseType>) where RequestType: InternalSwiftProtobuf.Message, ResponseType: InternalSwiftProtobuf.Message
}

class CuttlefishCKCodeOperationRunner: CKOperationRunner {
    private let underlyingCodeService: CKCodeService

    private let ckContainer: CKContainer

    init(containerName: String) {
        let containerOptions = CKContainerOptions()
        containerOptions.bypassPCSEncryption = true
        let containerID = CKContainer.containerID(forContainerIdentifier: containerName)
        self.ckContainer = CKContainer(containerID: containerID, options: containerOptions)
        // Cuttlefish is using its own push topic.
        // To register for this push topic, we need to issue CK operations with a specific bundle identifier
        self.ckContainer.options.setApplicationBundleIdentifierOverride(CuttlefishPushTopicBundleIdentifier)

        let ckDatabase = self.ckContainer.privateCloudDatabase
        self.underlyingCodeService = self.ckContainer.codeService(named: "Cuttlefish", databaseScope: ckDatabase.databaseScope)
    }

    func add<RequestType, ResponseType>(_ operation: CKCodeOperation<RequestType, ResponseType>) where RequestType: Message, ResponseType: Message {
        self.underlyingCodeService.add(operation)
    }
}

protocol ContainerNameToCKOperationRunner {
    func client(containerName: String) -> CKOperationRunner
}

class CuttlefishCKOperationRunnerCreator: ContainerNameToCKOperationRunner {
    func client(containerName: String) -> CKOperationRunner {
        return CuttlefishCKCodeOperationRunner(containerName: containerName)
    }
}

// A collection of Containers.
// When a Container of a given name is requested, it is created if it did not already exist.
// Methods may be invoked concurrently.
class ContainerMap {
    private let queue = DispatchQueue(label: "com.apple.security.TrustedPeersHelper.ContainerMap")

    let ckCodeOperationRunnerCreator: ContainerNameToCKOperationRunner
    let darwinNotifier: CKKSNotifier.Type

    init (ckCodeOperationRunnerCreator: ContainerNameToCKOperationRunner,
          darwinNotifier: CKKSNotifier.Type) {
        self.ckCodeOperationRunnerCreator = ckCodeOperationRunnerCreator
        self.darwinNotifier = darwinNotifier
    }
    // Only access containers while executing on queue
    private var containers: [ContainerName: Container] = [:]

    func findOrCreate(name: ContainerName) throws -> Container {
        return try queue.sync {
            if let container = self.containers[name] {
                return container
            } else {
                // Set up Core Data stack
                let persistentStoreURL = ContainerMap.urlForPersistentStore(name: name)
                let description = NSPersistentStoreDescription(url: persistentStoreURL)

                // Wrap whatever we're given in a magically-retrying layer
                let ckCodeOperationRunner = self.ckCodeOperationRunnerCreator.client(containerName: name.container)
                let retryingCuttlefish = RetryingCKCodeService(retry: ckCodeOperationRunner)

                let container = try Container(name: name,
                                              persistentStoreDescription: description,
                                              darwinNotifier: self.darwinNotifier,
                                              cuttlefish: retryingCuttlefish)
                self.containers[name] = container
                return container
            }
        }
    }

    static func urlForPersistentStore(name: ContainerName) -> URL {
        let filename = name.container + "-" + name.context + ".TrustedPeersHelper.db"
        return SecCopyURLForFileInKeychainDirectory(filename as CFString) as URL
    }

    // To be called via test only
    func removeAllContainers() {
        queue.sync {
            self.containers.removeAll()
        }
    }

    func removeContainer(name: ContainerName) {
        _ = queue.sync {
            self.containers.removeValue(forKey: name)
        }
    }

    func deleteAllPersistentStores() throws {
        try queue.sync {
            try self.containers.forEach {
                try $0.value.deletePersistentStore()
            }
        }
    }
}
