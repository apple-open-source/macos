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
import CloudKitCodeProtobuf
import CoreData
import Foundation

// TODO: merge into CodeConnection

let CuttlefishPushTopicBundleIdentifier = "com.apple.security.cuttlefish"

public class RetryingInvocable: CloudKitCode.Invocable {
    private let underlyingInvocable: CloudKitCode.Invocable

    internal init(retry: CloudKitCode.Invocable) {
        self.underlyingInvocable = retry
    }

    private func retryableError(error: Error?) -> Bool {
        switch error {
        case let error as NSError where error.domain == NSURLErrorDomain && error.code == NSURLErrorTimedOut:
            return true
        case let error as NSError where error.domain == CKErrorDomain && error.code == CKError.networkFailure.rawValue:
            return true
        case CuttlefishErrorMatcher(code: CuttlefishErrorCode.retryableServerFailure):
            return true
        default:
            return false
        }
    }

    public func invoke<RequestType, ResponseType>(function: String,
                                                  request: RequestType,
                                                  completion: @escaping (ResponseType?, Error?) -> Void) where RequestType : Message, ResponseType : Message {
        let now = Date()
        let deadline = Date(timeInterval: 30, since: now)
        let delay = TimeInterval(5)

        self.invokeRetry(function: function,
                         request: request,
                         deadline: deadline,
                         delay: delay,
                         completion: completion)
    }

    private func invokeRetry<RequestType: Message, ResponseType: Message>(
        function: String,
        request: RequestType,
        deadline: Date,
        delay: TimeInterval,
        completion: @escaping (ResponseType?, Error?) -> Void) {

        self.underlyingInvocable.invoke(function: function,
                                        request: request) { (response: ResponseType?, error: Error?) in
            if self.retryableError(error: error) {
                let now = Date()
                let cutoff = Date(timeInterval: delay, since: now)
                guard cutoff.compare(deadline) == ComparisonResult.orderedDescending else {
                    Thread.sleep(forTimeInterval: delay)
                    os_log("%{public}@ error: %{public}@ (retrying, now=%{public}@, deadline=%{public}@)", log: tplogDebug,
                           function,
                           "\(String(describing: error))",
                           "\(String(describing: now))",
                           "\(String(describing: deadline))")
                    self.invokeRetry(function: function,
                                     request: request,
                                     deadline: deadline,
                                     delay: delay,
                                     completion: completion)
                    return
                }
            }
            completion(response, error)
        }
    }
}

public class MyCodeConnection: CloudKitCode.Invocable {
    private let serviceName: String
    private let container: CKContainer
    private let databaseScope: CKDatabase.Scope
    private let local: Bool
    private let queue: DispatchQueue

    internal init(service: String, container: CKContainer, databaseScope: CKDatabase.Scope, local: Bool) {
        self.serviceName = service
        self.container = container
        self.databaseScope = databaseScope
        self.local = local
        self.queue = DispatchQueue(label: "MyCodeConnection", qos: .userInitiated)
    }

    /// Temporary stand-in until xcinverness moves to a swift-grpc plugin.
    /// Intended to be used by protoc-generated code only
    public func invoke<RequestType: Message, ResponseType: Message>(
        function: String, request: RequestType,
        completion: @escaping (ResponseType?, Error?) -> Void) {

        // Hack to fool CloudKit, real solution is tracked in <rdar://problem/49086080>
        self.queue.async {

            let operation = CodeOperation<RequestType, ResponseType>(
                service: self.serviceName,
                functionName: function,
                request: request,
                destinationServer: self.local ? .local : .default)

            // As each UUID finishes, log it.
            let requestCompletion = { (requestInfo: CKRequestInfo?) -> Void in
                if let requestUUID = requestInfo?.requestUUID {
                    os_log("ckoperation request finished: %{public}@ %{public}@", log: tplogDebug, function, requestUUID)
                }
            }
            operation.requestCompletedBlock = requestCompletion

            let loggingCompletion = { (response: ResponseType?, error: Error?) -> Void in
                os_log("%@(%@): %@, error: %@",
                       log: tplogDebug,
                       function,
                       "\(String(describing: request))",
                       "\(String(describing: response))",
                       "\(String(describing: error))")
                completion(response, error)
            }
            operation.codeOperationCompletionBlock = loggingCompletion

            /* Same convenience API properties that we specify in CKContainer / CKDatabase */
            operation.queuePriority = .low

            // One alternative here would be to not set any QoS and trust the
            // QoS propagation to do what's right. But there is also some benefit in
            // just using a lower CPU QoS because it should be hard to measure for the
            // casual adopter.
            operation.qualityOfService = .userInitiated

            operation.configuration.discretionaryNetworkBehavior = .nonDiscretionary
            operation.configuration.automaticallyRetryNetworkFailures = false
            operation.configuration.isCloudKitSupportOperation = true

            operation.configuration.sourceApplicationBundleIdentifier = CuttlefishPushTopicBundleIdentifier

            let database = self.container.database(with: self.databaseScope)

            database.add(operation)
        }
    }
}

protocol ContainerNameToCuttlefishInvocable {
    func client(container: String) -> CloudKitCode.Invocable
}

class CKCodeCuttlefishInvocableCreator: ContainerNameToCuttlefishInvocable {
    func client(container: String) -> CloudKitCode.Invocable {
        // Set up Cuttlefish client connection
        let ckContainer = CKContainer(identifier: container)

        // Cuttlefish is using its own push topic.
        // To register for this push topic, we need to issue CK operations with a specific bundle identifier
        ckContainer.sourceApplicationBundleIdentifier = CuttlefishPushTopicBundleIdentifier

        let ckDatabase = ckContainer.privateCloudDatabase
        return MyCodeConnection(service: "Cuttlefish", container: ckContainer,
                                databaseScope: ckDatabase.databaseScope, local: false)
    }
}

// A collection of Containers.
// When a Container of a given name is requested, it is created if it did not already exist.
// Methods may be invoked concurrently.
class ContainerMap {
    private let queue = DispatchQueue(label: "com.apple.security.TrustedPeersHelper.ContainerMap")

    let invocableCreator: ContainerNameToCuttlefishInvocable

    init(invocableCreator: ContainerNameToCuttlefishInvocable) {
        self.invocableCreator = invocableCreator
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
                let cuttlefishInvocable = self.invocableCreator.client(container: name.container)
                let retryingInvocable = RetryingInvocable(retry: cuttlefishInvocable)
                let cuttlefish = CuttlefishAPIAsyncClient(invocable: retryingInvocable)

                let container = try Container(name: name, persistentStoreDescription: description,
                                              cuttlefish: cuttlefish)
                self.containers[name] = container
                return container
            }
        }
    }

    static func urlForPersistentStore(name: ContainerName) -> URL {
        let filename = name.container + "-" + name.context + ".TrustedPeersHelper.db"
        return SecCopyURLForFileInKeychainDirectory(filename as CFString) as URL
    }
}
