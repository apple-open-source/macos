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

@_spi(CloudKitPrivate)
import CloudKit
import CloudKit_Private
import CloudKitCode
import CoreData
import Foundation
import InternalSwiftProtobuf

// TODO: merge into CodeConnection

let CuttlefishPushTopicBundleIdentifier = "com.apple.security.cuttlefish"

private let logger = Logger(subsystem: "com.apple.security.trustedpeers", category: "containermap")

struct CKUnderlyingErrorMatcher {
    let code: CKError.Code
    let underlyingCode: CKUnderlyingError.Code
    let noL3Error: Bool
}

// Match a CKError/CKUnderlyingError
func ~= (pattern: CKUnderlyingErrorMatcher, value: Error?) -> Bool {
    guard let value else {
        return false
    }
    switch value {
    case let error as CKError:
        guard let underlyingError = error.underlyingError else {
            return false
        }
        guard error.code == pattern.code &&
                underlyingError.code == pattern.underlyingCode else {
            return false
        }
        guard pattern.noL3Error else {
            return true
        }
        return underlyingError.userInfo[NSUnderlyingErrorKey] == nil
    default:
        return false
    }
}

struct CKErrorNSURLErrorMatcher {
    let code: CKError.Code
    let underlyingCode: Int
}

// Match a CKError/NSError
func ~= (pattern: CKErrorNSURLErrorMatcher, value: Error?) -> Bool {
    guard let value else {
        return false
    }
    switch value {
    case let error as CKError:
        guard error.code == pattern.code else {
            return false
        }
        guard let underlyingError = error.userInfo[NSUnderlyingErrorKey] else {
            return false
        }
        guard let underlyingError = underlyingError as? NSError else {
            return false
        }
        switch underlyingError {
        case NSURLErrorMatcher(code: pattern.underlyingCode):
            return true
        default:
            return false
        }
    default:
        return false
    }
}

struct CKErrorMatcher {
    let code: CKError.Code
}

// Match a CKError
func ~= (pattern: CKErrorMatcher, value: Error?) -> Bool {
    guard let value else {
        return false
    }
    switch value {
    case let error as CKError:
        return error.code == pattern.code
    default:
        return false
    }
}

struct NSURLErrorMatcher {
    let code: Int
}

// Match an NSURLError
func ~= (pattern: NSURLErrorMatcher, value: Error?) -> Bool {
    guard let value else {
        return false
    }
    switch value {
    case let error as NSError:
        return error.domain == NSURLErrorDomain && error.code == pattern.code
    default:
        return false
    }
}

protocol ConfiguredCloudKit {
    func configuredFor(user: TPSpecificUser) -> Bool
}

protocol ConfiguredCuttlefishAPIAsync: CuttlefishAPIAsync, ConfiguredCloudKit {
}

public class RetryingCKCodeService: ConfiguredCuttlefishAPIAsync {

    private let underlyingCKOperationRunner: CKOperationRunner
    private let queue: DispatchQueue

    internal init(retry: CKOperationRunner) {
        self.underlyingCKOperationRunner = retry
        self.queue = DispatchQueue(label: "RetryingCKCodeService", qos: .userInitiated)
    }

    public func configuredFor(user: TPSpecificUser) -> Bool {
        return self.underlyingCKOperationRunner.configuredFor(user: user)
    }

    public class func retryableError(error: Error?) -> Bool {
        switch error {
        case NSURLErrorMatcher(code: NSURLErrorTimedOut),
             NSURLErrorMatcher(code: NSURLErrorNotConnectedToInternet):
            return true
        case CKErrorMatcher(code: CKError.networkFailure):
            return true
        case CKErrorMatcher(code: CKError.serviceUnavailable):
            return true
        case CKErrorMatcher(code: CKError.requestRateLimited):
            return true
        case CKErrorMatcher(code: CKError.zoneBusy):
            return true
        case CKErrorNSURLErrorMatcher(code: CKError.networkUnavailable, underlyingCode: NSURLErrorNetworkConnectionLost):
            return true
        case CKUnderlyingErrorMatcher(code: CKError.serverRejectedRequest, underlyingCode: CKUnderlyingError.serverInternalError, noL3Error: false):
            return true
        case CKUnderlyingErrorMatcher(code: CKError.serverRejectedRequest, underlyingCode: CKUnderlyingError.pluginError, noL3Error: true):
            return true
        case CuttlefishErrorMatcher(code: CuttlefishErrorCode.retryableServerFailure):
            return true
        case CuttlefishErrorMatcher(code: CuttlefishErrorCode.transactionalFailure):
            return true
        default:
            return CKCanRetryForError(error)
        }
    }

    public func functionNameToEvent(functionName: String) -> String {
        let index = functionName.firstIndex(of: "(")

        if let index {
            let subStringOfFunctionName = functionName.prefix(upTo: index)
            return "com.apple.security." + subStringOfFunctionName
        } else {
            return "com.apple.security." + functionName
        }
    }

    public func invokeRetry<RequestType, ResponseType>(deadline: Date,
                                                       minimumDelay: TimeInterval,
                                                       functionName: String,
                                                       deviceSessionID: String?,
                                                       flowID: String?,
                                                       attemptNumber: Int,
                                                       startTime: Date,
                                                       operationCreator: @escaping () -> CloudKitCode.CKCodeOperation<RequestType, ResponseType>,
                                                       completion: @escaping (Swift.Result<ResponseType, Swift.Error>) -> Swift.Void) {
        let op = operationCreator()

        op.configuration.isCloudKitSupportOperation = true
        op.configuration.setApplicationBundleIdentifierOverride(CuttlefishPushTopicBundleIdentifier)

        let requestCompletion = { (requestInfo: CKRequestInfo?) -> Void in
            if let requestUUID = requestInfo?.requestUUID {
                logger.info("ckoperation request finished: \(functionName, privacy: .public) \(requestUUID, privacy: .public)")
            }
        }
        op.requestCompletedBlock = requestCompletion

        // we only want to send Cuttlefish metrics if there's an associated flowID
        var shouldSendMetrics: Bool = false
        if let flowID, !flowID.isEmpty, let deviceSessionID, !deviceSessionID.isEmpty {
            shouldSendMetrics = true
        }

        let event = AAFAnalyticsEventSecurity(keychainCircleMetrics: nil,
                                              altDSID: self.underlyingCKOperationRunner.altDSID(),
                                              flowID: flowID,
                                              deviceSessionID: deviceSessionID,
                                              eventName: self.functionNameToEvent(functionName: functionName),
                                              testsAreEnabled: soft_MetricsOverrideTestsAreEnabled(),
                                              canSendMetrics: shouldSendMetrics,
                                              category: kSecurityRTCEventCategoryAccountDataAccessRecovery)

        op.codeOperationResultBlock = { response in
            switch response {
            case .success:
                event.addMetrics([kSecurityRTCFieldRetryAttemptCount: attemptNumber,
                                 kSecurityRTCFieldTotalRetryDuration: Date().timeIntervalSince(startTime), ])
                SecurityAnalyticsReporterRTC.sendMetric(withEvent: event, success: true, error: nil)

                completion(response)
            case .failure(let error):
                event.addMetrics([kSecurityRTCFieldRetryAttemptCount: attemptNumber,
                                 kSecurityRTCFieldTotalRetryDuration: Date().timeIntervalSince(startTime), ])
                SecurityAnalyticsReporterRTC.sendMetric(withEvent: event, success: false, error: error)

                if RetryingCKCodeService.retryableError(error: error) {
                    let now = Date()
                    // Check cuttlefish and CKError retry afters.
                    let cuttlefishDelay = CuttlefishRetryAfter(error: error)
                    let ckDelay = CKRetryAfterSecondsForError(error)
                    let delay = max(minimumDelay, cuttlefishDelay, ckDelay)
                    let cutoff = Date(timeInterval: delay, since: now)
                    guard cutoff.compare(deadline) == ComparisonResult.orderedDescending else {
                        Thread.sleep(forTimeInterval: delay)
                        logger.info("\(functionName, privacy: .public) error: \(String(describing: error), privacy: .public) (retrying, now=\(String(describing: now), privacy: .public), deadline=\(String(describing: deadline), privacy: .public)")

                        self.invokeRetry(deadline: deadline, minimumDelay: minimumDelay, functionName: functionName, deviceSessionID: deviceSessionID, flowID: flowID, attemptNumber: attemptNumber + 1, startTime: startTime, operationCreator: operationCreator, completion: completion)
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

    public func retry<RequestType, ResponseType>(functionName: String,
                                                 deviceSessionID: String?,
                                                 flowID: String?,
                                                 operationCreator: @escaping () -> CloudKitCode.CKCodeOperation<RequestType, ResponseType>, completion: @escaping (Swift.Result<ResponseType, Swift.Error>) -> Swift.Void) {

        let now = Date()
        let deadline = Date(timeInterval: 30, since: now)
        let minimumDelay = TimeInterval(5)
        invokeRetry(deadline: deadline,
                    minimumDelay: minimumDelay,
                    functionName: functionName,
                    deviceSessionID: deviceSessionID,
                    flowID: flowID,
                    attemptNumber: 1,
                    startTime: Date(),
                    operationCreator: operationCreator,
                    completion: completion)
    }

    public func reset( _ request: ResetRequest, completion: @escaping (Swift.Result<ResetResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.ResetOperation(request: request)
        }, completion: completion)
    }
    public func establish(
      _ request: EstablishRequest,
      completion: @escaping (Swift.Result<EstablishResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.EstablishOperation(request: request)
        }, completion: completion)
    }
    public func joinWithVoucher(
      _ request: JoinWithVoucherRequest,
      completion: @escaping (Swift.Result<JoinWithVoucherResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.JoinWithVoucherOperation(request: request)
        }, completion: completion)
    }
    public func updateTrust(
      _ request: UpdateTrustRequest,
      completion: @escaping (Swift.Result<UpdateTrustResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.UpdateTrustOperation(request: request)
        }, completion: completion)
    }
    public func setRecoveryKey(
      _ request: SetRecoveryKeyRequest,
      completion: @escaping (Swift.Result<SetRecoveryKeyResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.SetRecoveryKeyOperation(request: request)
        }, completion: completion)
    }
    public func fetchChanges(
      _ request: FetchChangesRequest,
      completion: @escaping (Swift.Result<FetchChangesResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.FetchChangesOperation(request: request)
        }, completion: completion)
    }
    public func fetchViableBottles(
      _ request: FetchViableBottlesRequest,
      completion: @escaping (Swift.Result<FetchViableBottlesResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.FetchViableBottlesOperation(request: request)
        }, completion: completion)
    }
    public func fetchPolicyDocuments(
      _ request: FetchPolicyDocumentsRequest,
      completion: @escaping (Swift.Result<FetchPolicyDocumentsResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.FetchPolicyDocumentsOperation(request: request)
        }, completion: completion)
    }
    public func getRepairAction(
      _ request: GetRepairActionRequest,
      completion: @escaping (Swift.Result<GetRepairActionResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.GetRepairActionOperation(request: request)
        }, completion: completion)
    }
    public func getSupportAppInfo(
      _ request: GetSupportAppInfoRequest,
      completion: @escaping (Swift.Result<GetSupportAppInfoResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.GetSupportAppInfoOperation(request: request)
        }, completion: completion)
    }
    public func resetAccountCdpcontents(
      _ request: ResetAccountCDPContentsRequest,
      completion: @escaping (Swift.Result<ResetAccountCDPContentsResponse, Swift.Error>) -> Swift.Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.ResetAccountCdpcontentsOperation(request: request)
        }, completion: completion)
    }
    public func addCustodianRecoveryKey(
        _ request: AddCustodianRecoveryKeyRequest,
        completion: @escaping (Result<AddCustodianRecoveryKeyResponse, Error>) -> Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.AddCustodianRecoveryKeyOperation(request: request)
        }, completion: completion)
    }
    public func fetchRecoverableTlkshares(
        _ request: FetchRecoverableTLKSharesRequest,
        completion: @escaping (Result<FetchRecoverableTLKSharesResponse, Error>) -> Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.FetchRecoverableTlksharesOperation(request: request)
        }, completion: completion)
    }

    public func removeRecoveryKey(_ request: RemoveRecoveryKeyRequest,
                                  completion: @escaping (Result<RemoveRecoveryKeyResponse, Error>) -> Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.RemoveRecoveryKeyOperation(request: request)
        }, completion: completion)
    }

    public func performAtoprvactions(_ request: PerformATOPRVActionsRequest,
                                     completion: @escaping (Result<PerformATOPRVActionsResponse, any Error>) -> Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.PerformAtoprvactionsOperation(request: request)
        }, completion: completion)
    }

    public func fetchCurrentItem(_ request: CurrentItemFetchRequest, completion: @escaping (Result<CurrentItemFetchResponse, any Error>) -> Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.FetchCurrentItemOperation(request: request)
        }, completion: completion)
    }

    public func fetchPcsidentityByPublicKey(_ request: DirectPCSIdentityFetchRequest, completion: @escaping (Result<DirectPCSIdentityFetchResponse, any Error>) -> Void) {
        retry(functionName: #function, deviceSessionID: request.metrics.deviceSessionID, flowID: request.metrics.flowID, operationCreator: {
            return CuttlefishAPI.FetchPcsidentityByPublicKeyOperation(request: request)
        }, completion: completion)
    }
}

protocol CKOperationRunner {
    func add<RequestType, ResponseType>(_ operation: CloudKitCode.CKCodeOperation<RequestType, ResponseType>) where RequestType: InternalSwiftProtobuf.Message, ResponseType: InternalSwiftProtobuf.Message

    func configuredFor(user: TPSpecificUser) -> Bool

    func altDSID() -> String?
}

class CuttlefishCKCodeOperationRunner: CKOperationRunner {
    private let underlyingCodeService: CKCodeService

    private let ckContainer: CKContainer

    init(container: CKContainer) {
        self.ckContainer = container
        // Cuttlefish is using its own push topic.
        // To register for this push topic, we need to issue CK operations with a specific bundle identifier
        self.ckContainer.options.setApplicationBundleIdentifierOverride(CuttlefishPushTopicBundleIdentifier)

        let ckDatabase = self.ckContainer.privateCloudDatabase
        self.underlyingCodeService = self.ckContainer.codeService(named: "Cuttlefish", databaseScope: ckDatabase.databaseScope)
    }

    func altDSID() -> String? {
        return self.ckContainer.options.accountOverrideInfo?.altDSID
    }

    func add<RequestType, ResponseType>(_ operation: CKCodeOperation<RequestType, ResponseType>) where RequestType: InternalSwiftProtobuf.Message, ResponseType: InternalSwiftProtobuf.Message {
        self.underlyingCodeService.add(operation)
    }

    func configuredFor(user: TPSpecificUser) -> Bool {
        if let altDSID = self.ckContainer.options.accountOverrideInfo?.altDSID {
            if user.altDSID == altDSID {
                return true
            } else {
                logger.info("Mismatch between configured CKContainer (altDSID \(altDSID, privacy: .public) and requested user \(user.altDSID, privacy: .public)")
                return false
            }
        } else {
            if user.isPrimaryAccount {
                return true
            } else {
                logger.info("Mismatch between primary CKContainer and requested user \(user, privacy: .public)")
                return false
            }
        }
    }
}

protocol ContainerNameToCKOperationRunner {
    func client(user: TPSpecificUser) -> CKOperationRunner
}

class CuttlefishCKOperationRunnerCreator: ContainerNameToCKOperationRunner {
    func client(user: TPSpecificUser) -> CKOperationRunner {
        return CuttlefishCKCodeOperationRunner(container: user.makeCKContainer())
    }
}

// A collection of Containers.
// When a Container of a given name is requested, it is created if it did not already exist.
// Methods may be invoked concurrently.
class ContainerMap {
    private let queue = DispatchQueue(label: "com.apple.security.TrustedPeersHelper.ContainerMap")

    let ckCodeOperationRunnerCreator: ContainerNameToCKOperationRunner
    let darwinNotifier: CKKSNotifier.Type
    let personaAdapter: OTPersonaAdapter
    let managedConfigurationAdapter: OTManagedConfigurationAdapter

    init (ckCodeOperationRunnerCreator: ContainerNameToCKOperationRunner,
          darwinNotifier: CKKSNotifier.Type,
          personaAdapter: OTPersonaAdapter,
          managedConfigurationAdapter: OTManagedConfigurationAdapter) {
        self.ckCodeOperationRunnerCreator = ckCodeOperationRunnerCreator
        self.darwinNotifier = darwinNotifier
        self.personaAdapter = personaAdapter
        self.managedConfigurationAdapter = managedConfigurationAdapter
    }
    // Only access containers while executing on queue
    private var containers: [ContainerName: Container] = [:]

    func findOrCreate(user: TPSpecificUser?) throws -> Container {
        guard let user = user else {
            throw ContainerError.noSpecifiedUser
        }

        let containerName = ContainerName(container: user.cloudkitContainerName, context: user.octagonContextID)

        return try queue.sync {
            if let container = self.containers[containerName] {
                guard container.configuredFor(user: user) else {
                    throw ContainerError.configuredContainerDoesNotMatchSpecifiedUser(user)
                }

                self.personaAdapter.prepareThreadForKeychainAPIUse(forPersonaIdentifier: user.personaUniqueString)
                return container
            } else {
                // Set up Core Data stack
                let persistentStoreURL = try ContainerMap.urlForPersistentStore(name: containerName)
                let description = NSPersistentStoreDescription(url: persistentStoreURL)

                // Wrap whatever we're given in a magically-retrying layer
                let ckCodeOperationRunner = self.ckCodeOperationRunnerCreator.client(user: user)
                let retryingCuttlefish = RetryingCKCodeService(retry: ckCodeOperationRunner)

                let container = try Container(name: containerName,
                                              persistentStoreDescription: description,
                                              darwinNotifier: self.darwinNotifier,
                                              managedConfigurationAdapter: self.managedConfigurationAdapter,
                                              cuttlefish: retryingCuttlefish)
                self.containers[containerName] = container

                self.personaAdapter.prepareThreadForKeychainAPIUse(forPersonaIdentifier: user.personaUniqueString)
                return container
            }
        }
    }

    static func urlForPersistentStore(name: ContainerName) throws -> URL {
        let filename = name.container + "-" + name.context + ".TrustedPeersHelper.db"
        let url = SecCopyURLForFileInUserScopedKeychainDirectory(filename as CFString) as URL?
        guard let url else {
            throw ContainerError.unableToCreateDirectory
        }
        return url
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
