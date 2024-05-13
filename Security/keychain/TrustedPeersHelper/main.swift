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

import Foundation
import Foundation_Private.NSXPCConnection
import os.log

let containerMap = ContainerMap(ckCodeOperationRunnerCreator: CuttlefishCKOperationRunnerCreator(),
                                darwinNotifier: CKKSNotifyPostNotifier.self,
                                personaAdapter: OTPersonaActualAdapter(),
                                managedConfigurationAdapter: OTManagedConfigurationActualAdapter())

private let logger = Logger(subsystem: "com.apple.security.trustedpeers", category: "main")

class ServiceDelegate: NSObject, NSXPCListenerDelegate {
    func listener(_ listener: NSXPCListener, shouldAcceptNewConnection newConnection: NSXPCConnection) -> Bool {
        let tphEntitlement = "com.apple.private.trustedpeershelper.client"

        logger.info("Received a new client: \(newConnection, privacy: .public)")

#if os(macOS)
        // The recommended way to do this check is: SAUserSetupState.getForUser(getuid()) != .setupUser
        // That call is expensive in the non-setup case; use constant from MacBuddyX/SharedConstants.h
        let kMBBuddyUserID = 248
        guard getuid() != kMBBuddyUserID else {
            logger.info("client(\(newConnection, privacy: .public)) is running as setup user")
            return false
        }
#endif

        switch newConnection.value(forEntitlement: tphEntitlement) {
        case 1 as Int:
            logger.info("client has entitlement '\(tphEntitlement, privacy: .public)'")
        case true as Bool:
            logger.info("client has entitlement '\(tphEntitlement, privacy: .public)'")

        case let someInt as Int:
            logger.info("client(\(newConnection, privacy: .public) has wrong integer value for '\(tphEntitlement, privacy: .public)' (\(someInt)), rejecting")
            return false

        case let someBool as Bool:
            logger.info("client(\(newConnection, privacy: .public) has wrong boolean value for '\(tphEntitlement, privacy: .public)' (\(someBool)), rejecting")
            return false

        default:
            logger.info("client(\(newConnection, privacy: .public) is missing entitlement '\(tphEntitlement, privacy: .public)'")
            return false
        }

        newConnection.exportedInterface = TrustedPeersHelperSetupProtocol(NSXPCInterface(with: TrustedPeersHelperProtocol.self))
        let exportedObject = Client(endpoint: newConnection.endpoint, containerMap: containerMap)
        newConnection.exportedObject = exportedObject
        newConnection.resume()

        return true
    }
}

let sandboxIdentifier = "com.apple.TrustedPeersHelper"

#if os(macOS)
    Sandbox.enterSandbox(identifier: sandboxIdentifier, profile: sandboxIdentifier)
#else
    Sandbox.enterSandbox(identifier: sandboxIdentifier)
#endif

logger.info("Starting up")

ValueTransformer.setValueTransformer(SetValueTransformer(), forName: SetValueTransformer.name)

let delegate = ServiceDelegate()
let listener = NSXPCListener.service()

listener.delegate = delegate
listener.resume()
