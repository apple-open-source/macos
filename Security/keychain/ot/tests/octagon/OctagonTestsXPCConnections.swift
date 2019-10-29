#if OCTAGON

import Foundation

class ProxyXPCConnection: NSObject, NSXPCListenerDelegate {
    let obj: Any
    let serverInterface: NSXPCInterface
    let listener: NSXPCListener

    init(_ obj: Any, interface: NSXPCInterface) {
        self.obj = obj
        self.serverInterface = interface
        self.listener = NSXPCListener.anonymous()

        super.init()
        self.listener.delegate = self
        self.listener.resume()
    }

    public func listener(_ listener: NSXPCListener, shouldAcceptNewConnection newConnection: NSXPCConnection) -> Bool {
        newConnection.exportedInterface = self.serverInterface
        newConnection.exportedObject = self.obj
        newConnection.resume()
        return true
    }

    public func connection() -> NSXPCConnection {
        let connection = NSXPCConnection(listenerEndpoint: self.listener.endpoint)
        connection.remoteObjectInterface = self.serverInterface
        connection.resume()
        return connection
    }
}

class FakeNSXPCConnectionSOS: NSXPCConnection {
    var sosControl: SOSControlProtocol

    init(withSOSControl: SOSControlProtocol) {
        self.sosControl = withSOSControl
        super.init()
    }

    override func remoteObjectProxyWithErrorHandler(_ handler: @escaping (Error) -> Void) -> Any {
        return self.sosControl
    }

    override func synchronousRemoteObjectProxyWithErrorHandler(_ handler: @escaping (Error) -> Void) -> Any {
        return FakeNSXPCConnection(control: self.sosControl)
    }
}

class FakeOTControlEntitlementBearer: OctagonEntitlementBearerProtocol {
    var entitlements: [String: Any]

    init() {
        // By default, this client has all octagon entitlements
        self.entitlements = [kSecEntitlementPrivateOctagonEscrow: true]
    }
    func value(forEntitlement entitlement: String) -> Any? {
        return self.entitlements[entitlement]
    }
}

#endif // OCTAGON
