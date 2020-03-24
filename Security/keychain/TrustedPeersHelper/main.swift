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
import os.log

let containerMap = ContainerMap(invocableCreator: CKCodeCuttlefishInvocableCreator())

class ServiceDelegate: NSObject, NSXPCListenerDelegate {
    func listener(_ listener: NSXPCListener, shouldAcceptNewConnection newConnection: NSXPCConnection) -> Bool {
        let tphEntitlement = "com.apple.private.trustedpeershelper.client"

        os_log("Received a new client: %{public}@", log: tplogDebug, type: .default, newConnection)
        switch newConnection.value(forEntitlement: tphEntitlement) {
        case 1 as Int:
            os_log("client has entitlement '%{public}@'", log: tplogDebug, type: .default, tphEntitlement)
        case true as Bool:
            os_log("client has entitlement '%{public}@'", log: tplogDebug, type: .default, tphEntitlement)

        case let someInt as Int:
            os_log("client(%{public}@) has wrong integer value for '%{public}@' (%d), rejecting", log: tplogDebug, type: .default, newConnection, tphEntitlement, someInt)
            return false

        case let someBool as Bool:
            os_log("client(%{public}@) has wrong boolean value for '%{public}@' (%d), rejecting", log: tplogDebug, type: .default, newConnection, tphEntitlement, someBool)
            return false

        default:
            os_log("client(%{public}@) is missing entitlement '%{public}@', rejecting", log: tplogDebug, type: .default, newConnection, tphEntitlement)
            return false
        }

        newConnection.exportedInterface = TrustedPeersHelperSetupProtocol(NSXPCInterface(with: TrustedPeersHelperProtocol.self))
        let exportedObject = Client(endpoint: newConnection.endpoint, containerMap: containerMap)
        newConnection.exportedObject = exportedObject
        newConnection.resume()

        return true
    }
}

os_log("Starting up", log: tplogDebug, type: .default)

ValueTransformer.setValueTransformer(SetValueTransformer(), forName: SetValueTransformer.name)

let delegate = ServiceDelegate()
let listener = NSXPCListener.service()
listener.delegate = delegate
listener.resume()
