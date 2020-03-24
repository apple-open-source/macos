#if OCTAGON

func GenerateFullECKey(keySize: Int) -> (SecKey) {

    let keyPair = _SFECKeyPair.init(randomKeyPairWith: _SFECKeySpecifier.init(curve: SFEllipticCurve.nistp384))!

    var keyAttributes: [String: String] = [:]
    keyAttributes[kSecAttrKeyClass as String] = kSecAttrKeyClassPrivate as String
    keyAttributes[kSecAttrKeyType as String] = kSecAttrKeyTypeEC as String

    let key = SecKeyCreateWithData(keyPair.keyData as CFData, keyAttributes as CFDictionary, nil)!

    return key
}

class KCJoiningRequestTestDelegate: NSObject, KCJoiningRequestSecretDelegate, KCJoiningRequestCircleDelegate {
    var sharedSecret: String = ""
    var accountCode: String = ""
    var circleJoinData = Data()
    var peerInfo: SOSPeerInfoRef?
    var incorrectSecret: String = ""
    var incorrectTries: Int = 0

    class func requestDelegate(withSecret secret: String) -> KCJoiningRequestTestDelegate {
        return self.requestDelegateWithSecret(secret: secret, wrongSecret: "", retries: 0)
    }
    class func requestDelegateWithSecret(secret: String, wrongSecret: String, retries: Int) -> KCJoiningRequestTestDelegate {
        return KCJoiningRequestTestDelegate(withSecret: secret, incorrectSecret: wrongSecret, retries: retries)
    }
    init(withSecret secret: String, incorrectSecret: String, retries: Int) {
        let signingKey = GenerateFullECKey(keySize: 256)
        let octagonSigningKey = GenerateFullECKey(keySize: 384)
        let octagonEncryptionKey = GenerateFullECKey(keySize: 384)

        XCTAssertNotNil(octagonSigningKey, "signing key should not be nil")
        XCTAssertNotNil(octagonEncryptionKey, "encryption key should not be nil")

        var gestalt: [String: String] = [:]
        gestalt[kPIUserDefinedDeviceNameKey as String] = "Fakey"

        let newPeerInfo = SOSPeerInfoCreate(nil, gestalt as CFDictionary, nil, signingKey, octagonSigningKey, octagonEncryptionKey, nil)

        self.peerInfo = newPeerInfo
        self.sharedSecret = secret
        self.incorrectSecret = incorrectSecret
        self.incorrectTries = retries
    }

    func nextSecret() -> String {
        if self.incorrectTries > 0 {
            self.incorrectTries -= 1
            return self.incorrectSecret
        }
        return self.sharedSecret
    }

    func secret() -> String {
        return self.nextSecret()
    }

    func verificationFailed(_ codeChanged: Bool) -> String {
        return self.nextSecret()
    }

    func processAccountCode(_ accountCode: String, error: NSErrorPointer) -> Bool {
        self.accountCode = accountCode
        return true
    }

    func copyPeerInfoError(_ error: NSErrorPointer) -> SOSPeerInfoRef {
        return self.peerInfo!
    }

    func processCircleJoin(_ circleJoinData: Data, version: PiggyBackProtocolVersion, error: NSErrorPointer) -> Bool {
        self.circleJoinData = circleJoinData
        return true
    }
}

class KCJoiningAcceptTestDelegate: NSObject, KCJoiningAcceptSecretDelegate, KCJoiningAcceptCircleDelegate {

    var secrets: [String] = []
    var currentSecret: Int = 0
    var retriesLeft: Int = 0
    var retriesPerSecret: Int = 0
    var codeToUse: String = ""
    var circleJoinData = Data()
    var peerInfo: SOSPeerInfoRef?

    class func acceptDelegateWithSecrets(secrets: [String], retries: Int, code: String) -> KCJoiningAcceptTestDelegate {
        return KCJoiningAcceptTestDelegate(withSecrets: secrets, retries: retries, code: code)
    }

    class func acceptDelegateWithSecret(secret: String, code: String) -> KCJoiningAcceptTestDelegate {
        return KCJoiningAcceptTestDelegate.initWithSecret(secret: secret, code: code)
    }

    class func initWithSecret(secret: String, code: String) -> KCJoiningAcceptTestDelegate {
        var secretArray: [String] = Array()
        secretArray.append(secret)
        return KCJoiningAcceptTestDelegate(withSecrets: secretArray, retries: 3, code: code)
    }

    init(withSecrets secrets: [String], retries: Int, code: String) {

        self.secrets = secrets
        self.currentSecret = 0
        self.retriesPerSecret = retries
        self.retriesLeft = self.retriesPerSecret

        self.codeToUse = code

        let joinDataBuffer = [10, 9, 8, 7, 6, 5, 4, 3, 2, 1]
        do {
            self.circleJoinData = try NSKeyedArchiver.archivedData(withRootObject: joinDataBuffer, requiringSecureCoding: false)
        } catch {
            XCTFail("error loading account state: \(error)")
        }
    }

    func advanceSecret() -> KCRetryOrNot {
        if self.retriesLeft == 0 {
            self.currentSecret += 1
            if self.currentSecret >= self.secrets.count {
                self.currentSecret = self.secrets.count - 1
            }
            self.retriesLeft = self.retriesPerSecret
            return kKCRetryWithNewChallenge
        } else {
            self.retriesLeft -= 1
            return kKCRetryWithSameChallenge
        }
    }

    func secret() -> String {
        return self.secrets[self.currentSecret]
    }
    func accountCode() -> String {
        return self.codeToUse
    }

    func circleGetInitialSyncViews(error: Error) -> Data {
        return Data()
    }

    func verificationFailed(_ error: NSErrorPointer) -> KCRetryOrNot {
        return self.advanceSecret()
    }

    func circleJoinData(for peer: SOSPeerInfoRef, error: NSErrorPointer) -> Data {
        let joinDataBuffer = [ 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 ]
        self.peerInfo = peer
        do {
            return try NSKeyedArchiver.archivedData(withRootObject: joinDataBuffer, requiringSecureCoding: false)
        } catch {
            XCTFail("error loading account state: \(error)")
            return Data()
        }
    }

    func circleGetInitialSyncViews(_ flags: SOSInitialSyncFlags, error: NSErrorPointer) -> Data {
        return Data()
    }
}

// Similar to the helpers below, but more accessible
extension OctagonTestsBase {
    func joiningConfigurations(initiator: OTCuttlefishContext, initiatorDeviceID: String, sponsor: OTCuttlefishContext, sponsorDeviceID: String) -> (OTJoiningConfiguration, OTJoiningConfiguration) {
        let acceptorConfig = OTJoiningConfiguration(protocolType: OTProtocolPairing,
                                                    uniqueDeviceID: sponsorDeviceID,
                                                    uniqueClientID: initiatorDeviceID,
                                                    pairingUUID: UUID().uuidString,
                                                    containerName: OTCKContainerName,
                                                    contextID: sponsor.contextID,
                                                    epoch: 1,
                                                    isInitiator: false)

        let initiatorConfig = OTJoiningConfiguration(protocolType: OTProtocolPairing,
                                                     uniqueDeviceID: initiatorDeviceID,
                                                     uniqueClientID: initiatorDeviceID,
                                                     pairingUUID: UUID().uuidString,
                                                     containerName: OTCKContainerName,
                                                     contextID: initiator.contextID,
                                                     epoch: 1,
                                                     isInitiator: true)

        return (acceptorConfig, initiatorConfig)
    }

    func setupPairingChannels(initiator: OTCuttlefishContext, sponsor: OTCuttlefishContext) -> (KCPairingChannel, KCPairingChannel) {
        let sponsorChannelContext = KCPairingChannelContext()
        sponsorChannelContext.model = sponsor.deviceAdapter.modelID()
        sponsorChannelContext.osVersion = sponsor.deviceAdapter.osVersion()
        sponsorChannelContext.modelClass = "AcceptorModelClass"

        sponsorChannelContext.uniqueDeviceID = UUID().uuidString
        sponsorChannelContext.uniqueClientID = UUID().uuidString

        let initiatorChannelContext = KCPairingChannelContext()
        initiatorChannelContext.model = initiator.deviceAdapter.modelID()
        initiatorChannelContext.osVersion = initiator.deviceAdapter.osVersion()
        initiatorChannelContext.modelClass = "InitiatorModelClass"

        // The initiator's client ID is equivalent to the sponsor's client ID
        initiatorChannelContext.uniqueDeviceID = sponsorChannelContext.uniqueClientID

        let sponsorPairingChannel = KCPairingChannel(acceptor: sponsorChannelContext)
        let initiatorPairingChannel = KCPairingChannel(initiator: initiatorChannelContext)

        XCTAssertNotNil(sponsorPairingChannel, "Should have a sponsor pairing channel")
        XCTAssertNotNil(initiatorPairingChannel, "Should have a initiator pairing channel")

        sponsorPairingChannel?.setControlObject(self.otControl)
        initiatorPairingChannel?.setControlObject(self.otControl)

        let (acceptorPairingConfig, initiatorPairingConfig) = self.joiningConfigurations(initiator: initiator,
                                                                                         initiatorDeviceID: initiatorChannelContext.uniqueDeviceID,
                                                                                         sponsor: sponsor,
                                                                                         sponsorDeviceID: sponsorChannelContext.uniqueDeviceID)

        sponsorPairingChannel?.setConfiguration(acceptorPairingConfig)
        initiatorPairingChannel?.setConfiguration(initiatorPairingConfig)

        let fakeCircle = SOSCircleCreate(kCFAllocatorDefault, "TEST DOMAIN" as CFString, nil) as SOSCircleRef

        sponsorPairingChannel?.setXPCConnectionObject(FakeNSXPCConnectionSOS(withSOSControl: FCPairingFakeSOSControl(randomAccountKey: true, circle: fakeCircle)))
        initiatorPairingChannel?.setXPCConnectionObject(FakeNSXPCConnectionSOS(withSOSControl: FCPairingFakeSOSControl(randomAccountKey: true, circle: fakeCircle)))

        return (sponsorPairingChannel!, initiatorPairingChannel!)
    }

    func setupPiggybackingSessions(initiator: OTCuttlefishContext,
                                   sponsor: OTCuttlefishContext) throws -> (KCJoiningRequestTestDelegate, KCJoiningAcceptTestDelegate, KCJoiningAcceptSession, KCJoiningRequestSecretSession) {
        let (acceptorJoiningConfig, _) = self.joiningConfigurations(initiator: initiator,
                                                                    initiatorDeviceID: UUID().uuidString,
                                                                    sponsor: sponsor,
                                                                    sponsorDeviceID: UUID().uuidString)

        return try self.setupKCJoiningSessionObjects(dsid: 0x123456,
                                                     sponsorConfiguration: acceptorJoiningConfig)
    }

    func setupKCJoiningSessionObjects(dsid: UInt64,
                                      sponsorConfiguration: OTJoiningConfiguration) throws -> (KCJoiningRequestTestDelegate, KCJoiningAcceptTestDelegate, KCJoiningAcceptSession, KCJoiningRequestSecretSession) {
        let secret = "123456"
        let code = "987654"

        let requestDelegate = KCJoiningRequestTestDelegate.requestDelegate(withSecret: secret)
        let acceptDelegate = KCJoiningAcceptTestDelegate.acceptDelegateWithSecret(secret: secret, code: code)

        let requestSession = try KCJoiningRequestSecretSession(secretDelegate: requestDelegate as KCJoiningRequestSecretDelegate, dsid: dsid, rng: ccDRBGGetRngState())

        let acceptSession = try KCJoiningAcceptSession(secretDelegate: acceptDelegate as KCJoiningAcceptSecretDelegate,
                                                       circleDelegate: acceptDelegate as KCJoiningAcceptCircleDelegate,
                                                       dsid: dsid,
                                                       rng: ccDRBGGetRngState())
        requestSession.setControlObject(self.otControl)
        acceptSession.setControlObject(self.otControl)

        // requestSessions don't need control objects
        acceptSession.setConfiguration(sponsorConfiguration)

        return (requestDelegate, acceptDelegate, acceptSession, requestSession)
    }

    func octagonPiggypackingMessage(in message: KCJoiningMessage) throws -> OTPairingMessage {
        guard let octagonData = message.secondData else {
            throw NSError(domain: "missing octagon data" as String, code: -1, userInfo: nil)
        }

        return OTPairingMessage(data: octagonData)
    }

    func unpackPiggybackingInitialMessage(identityMessage: Data, session: KCAESGCMDuplexSession) throws -> OTPairingMessage {
        let encryptedJoiningIdentityMessage = try KCJoiningMessage(der: identityMessage)
        let joiningIdentityMessage = try session.decryptAndVerify(encryptedJoiningIdentityMessage.firstData)

        let initialMessageProtobuf = KCInitialMessageData(data: joiningIdentityMessage)
        XCTAssertNotNil(initialMessageProtobuf, "should have an initial message container")
        let initiatorIdentityMessageOpt = OTPairingMessage(data: initialMessageProtobuf!.prepareMessage)
        XCTAssertNotNil(initiatorIdentityMessageOpt, "should have an Octagon message container")
        return initiatorIdentityMessageOpt!
    }

    func makePiggybackingPacket(combining currentMessage: KCJoiningMessage, octagonMessage: OTPairingMessage) throws -> Data {
        let newMessage = try KCJoiningMessage(type: currentMessage.type,
                                              data: currentMessage.firstData,
                                              payload: octagonMessage.data)
        return newMessage.der
    }

    func pairingPacketToPlist(packet: Data) throws -> [String: Any] {
        let binaryPlist = KCPairingChannel.pairingChannelDecompressData(packet)!
        let plist = (try PropertyListSerialization.propertyList(from: binaryPlist, options: [], format: nil)) as! [String: Any]
        return plist
    }

    func octagonPairingMessage(in packet: Data) throws -> OTPairingMessage {
        let plist = try self.pairingPacketToPlist(packet: packet)

        guard let octagonData = plist["o"] as? Data else {
            throw NSError(domain: "missing octagon data" as String, code: -1, userInfo: nil)
        }

        return OTPairingMessage(data: octagonData)
    }

    func makePairingPacket(combining currentPacket: Data, octagonMessage: OTPairingMessage) throws -> Data {
        var plist = try self.pairingPacketToPlist(packet: currentPacket)
        plist["o"] = octagonMessage.data

        return KCPairingChannel.pairingChannelCompressData(try PropertyListSerialization.data(fromPropertyList: plist, format: .binary, options: 0))
    }

    func sendPairingExpectingReply(channel: KCPairingChannel, packet: Data?, reason: String) -> Data {
        var packetResponse = Data()
        let callback = self.expectation(description: "callback occurs (\(reason))")

        channel.exchangePacket(packet) { complete, response, error in
            XCTAssertNil(error, "error should be nil (\(reason))")

            XCTAssertFalse(complete, "Expected no completion (\(reason))")
            XCTAssertNotNil(response, "packet should not be nil (\(reason))")
            packetResponse = response!
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)
        return packetResponse
    }

    func sendPairingExpectingCompletionAndReply(channel: KCPairingChannel, packet: Data?, reason: String) -> Data {
        var packetResponse = Data()
        let callback = self.expectation(description: "callback occurs (\(reason))")

        channel.exchangePacket(packet) { complete, response, error in
            XCTAssertNil(error, "error should be nil (\(reason))")

            XCTAssertTrue(complete, "Expected channel completion (\(reason))")
            XCTAssertNotNil(response, "response should be present (\(reason))")
            packetResponse = response!
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)
        return packetResponse
    }

    func sendPairingExpectingCompletion(channel: KCPairingChannel, packet: Data?, reason: String) {
        let callback = self.expectation(description: "callback occurs (\(reason))")

        channel.exchangePacket(packet) { complete, response, error in
            XCTAssertNil(error, "error should be nil (\(reason))")

            XCTAssertTrue(complete, "Expected channel completion (\(reason))")
            XCTAssertNil(response, "response should be nil (\(reason))")
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)
    }
}

@objcMembers
class OctagonPairingTests: OctagonTestsBase {

    var sosAdapterForAcceptor: CKKSMockSOSPresentAdapter!
    var cuttlefishContextForAcceptor: OTCuttlefishContext!
    var contextForAcceptor = "defaultContextForAcceptor"

    var initiatorPiggybackingConfig: OTJoiningConfiguration!
    var acceptorPiggybackingConfig: OTJoiningConfiguration!

    var initiatorPairingConfig: OTJoiningConfiguration!
    var acceptorPairingConfig: OTJoiningConfiguration!
    var circle: SOSCircleRef!
    var sosControl: NSXPCConnection!
    var initiatorName = "uniqueInitiatorID"
    var fcInitiator: FCPairingFakeSOSControl!
    var fcAcceptor: FCPairingFakeSOSControl!

    override func setUp() {
        super.setUp()

        // The acceptor should have its own SOS state
        self.sosAdapterForAcceptor = CKKSMockSOSPresentAdapter(selfPeer: self.createSOSPeer(peerID: "sos-acceptor"),
                                                               trustedPeers: Set(),
                                                               essential: false)
        self.sosAdapterForAcceptor.circleStatus = SOSCCStatus(kSOSCCInCircle)

        self.cuttlefishContextForAcceptor = self.manager.context(forContainerName: OTCKContainerName,
                                                contextID: self.contextForAcceptor,
                                                sosAdapter: self.sosAdapterForAcceptor,
                                                authKitAdapter: self.mockAuthKit3,
                                                lockStateTracker: self.lockStateTracker,
                                                accountStateTracker: self.accountStateTracker,
                                                deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-SOS-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

        self.acceptorPiggybackingConfig = OTJoiningConfiguration(protocolType: OTProtocolPiggybacking,
                                                                 uniqueDeviceID: "acceptor",
                                                                 uniqueClientID: self.initiatorName,
                                                                 pairingUUID: UUID().uuidString,
                                                                 containerName: OTCKContainerName,
                                                                 contextID: self.contextForAcceptor,
                                                                 epoch: 1,
                                                                 isInitiator: false)
        self.initiatorPiggybackingConfig = OTJoiningConfiguration(protocolType: OTProtocolPiggybacking,
                                                                  uniqueDeviceID: "initiator",
                                                                  uniqueClientID: "acceptor",
                                                                  pairingUUID: UUID().uuidString,
                                                                  containerName: OTCKContainerName,
                                                                  contextID: OTDefaultContext,
                                                                  epoch: 1,
                                                                  isInitiator: true)

        self.acceptorPairingConfig = OTJoiningConfiguration(protocolType: OTProtocolPairing,
                                                            uniqueDeviceID: "acceptor",
                                                            uniqueClientID: self.initiatorName,
                                                            pairingUUID: UUID().uuidString,
                                                            containerName: OTCKContainerName,
                                                            contextID: self.contextForAcceptor,
                                                            epoch: 1,
                                                            isInitiator: false)
        self.initiatorPairingConfig = OTJoiningConfiguration(protocolType: OTProtocolPairing,
                                                             uniqueDeviceID: "initiator",
                                                             uniqueClientID: "acceptor",
                                                             pairingUUID: UUID().uuidString,
                                                             containerName: OTCKContainerName,
                                                             contextID: OTDefaultContext,
                                                             epoch: 1,
                                                             isInitiator: true)
    }

    func getAcceptorInCircle() {
        let resetAndEstablishExpectation = self.expectation(description: "resetAndEstablish callback occurs")
        self.cuttlefishContextForAcceptor.startOctagonStateMachine()
        self.cuttlefishContextForAcceptor.rpcResetAndEstablish(.testGenerated) { resetError in
            XCTAssertNil(resetError, "Should be no error calling resetAndEstablish")
            resetAndEstablishExpectation.fulfill()
        }

        self.wait(for: [resetAndEstablishExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContextForAcceptor, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContextForAcceptor)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 1, "should be 1 bottles")
    }

    func setupPairingEndpoints(withPairNumber pairNumber: String, initiatorContextID: String, acceptorContextID: String, initiatorUniqueID: String, acceptorUniqueID: String) -> (KCPairingChannel, KCPairingChannel) {

        let (acceptorClique, initiatorClique) = self.setupOTCliquePair(withNumber: pairNumber)
        XCTAssertNotNil(acceptorClique, "acceptorClique should not be nil")
        XCTAssertNotNil(initiatorClique, "initiatorClique should not be nil")

        let acceptorContext = KCPairingChannelContext()
        acceptorContext.model = "AcceptorModel"
        acceptorContext.osVersion = "AcceptorOsVersion"
        acceptorContext.modelClass = "AcceptorModelClass"
        acceptorContext.uniqueDeviceID = acceptorUniqueID
        acceptorContext.uniqueClientID = initiatorUniqueID

        let initiatorContext = KCPairingChannelContext()
        initiatorContext.model = "InitiatorModel"
        initiatorContext.osVersion = "InitiatorOsVersion"
        initiatorContext.modelClass = "InitiatorModelClass"
        initiatorContext.uniqueDeviceID = initiatorUniqueID

        let acceptor = acceptorClique!.setupPairingChannel(asAcceptor: acceptorContext)
        let initiator = initiatorClique!.setupPairingChannel(asInitiator: initiatorContext)

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        acceptor.setControlObject(self.otControl)
        initiator.setControlObject(self.otControl)

        let acceptorPairingConfig = OTJoiningConfiguration(protocolType: OTProtocolPairing,
                                                           uniqueDeviceID: acceptorUniqueID,
                                                           uniqueClientID: initiatorUniqueID,
                                                           pairingUUID: UUID().uuidString,
                                                           containerName: OTCKContainerName,
                                                           contextID: acceptorContextID,
                                                           epoch: 1,
                                                           isInitiator: false)
        let initiatorPairingConfig = OTJoiningConfiguration(protocolType: OTProtocolPairing,
                                                            uniqueDeviceID: initiatorUniqueID,
                                                            uniqueClientID: initiatorUniqueID,
                                                            pairingUUID: UUID().uuidString,
                                                            containerName: OTCKContainerName,
                                                            contextID: initiatorContextID,
                                                            epoch: 1,
                                                            isInitiator: true)

        acceptor.setConfiguration(acceptorPairingConfig)
        initiator.setConfiguration(initiatorPairingConfig)

        self.circle = SOSCircleCreate(kCFAllocatorDefault, "TEST DOMAIN" as CFString, nil) as SOSCircleRef
        self.fcInitiator = FCPairingFakeSOSControl(randomAccountKey: true, circle: circle)
        self.fcAcceptor = FCPairingFakeSOSControl(randomAccountKey: true, circle: circle)

        let sosConnectionInitiator = FakeNSXPCConnectionSOS(withSOSControl: self.fcInitiator)
        let sosConnectionAcceptor = FakeNSXPCConnectionSOS(withSOSControl: self.fcAcceptor)

        acceptor.setXPCConnectionObject(sosConnectionAcceptor)
        initiator.setXPCConnectionObject(sosConnectionInitiator)

        return (acceptor, initiator)
    }

    func setupOTCliquePair(withNumber count: String) -> (OTClique?, OTClique?) {

        let secondAcceptorData = OTConfigurationContext()
        secondAcceptorData.context = "secondAcceptor"
        secondAcceptorData.dsid = "a-"+count
        secondAcceptorData.altDSID = "alt-a-"+count

        let acceptorAnalytics = SFSignInAnalytics(signInUUID: "uuid", category: "com.apple.cdp", eventName: "signed in")
        XCTAssertNotNil(acceptorAnalytics, "acceptorAnalytics should not be nil")
        secondAcceptorData.analytics = acceptorAnalytics

        let acceptor = OTClique(contextData: secondAcceptorData)
        XCTAssertNotNil(acceptor, "Clique should not be nil")
        acceptor.setPairingDefault(true)

        let secondInitiatorData = OTConfigurationContext()
        secondInitiatorData.context = "secondInitiator"
        secondInitiatorData.dsid = "i-"+count
        secondInitiatorData.altDSID = "alt-i-"+count

        let initiatorAnalytics = SFSignInAnalytics(signInUUID: "uuid", category: "com.apple.cdp", eventName: "signed in")
        XCTAssertNotNil(initiatorAnalytics, "initiatorAnalytics should not be nil")
        secondInitiatorData.analytics = initiatorAnalytics
        let initiator = OTClique(contextData: secondInitiatorData)
        XCTAssertNotNil(initiator, "Clique should not be nil")
        initiator.setPairingDefault(true)

        return (acceptor, initiator)
    }

    func setupKCJoiningSessionObjects() -> (KCJoiningRequestTestDelegate?, KCJoiningAcceptTestDelegate?, KCJoiningAcceptSession?, KCJoiningRequestSecretSession?) {
        let dsid: UInt64 = 0x1234567887654321

        do {
            return try self.setupKCJoiningSessionObjects(dsid: dsid, sponsorConfiguration: self.acceptorPiggybackingConfig)
        } catch {
            XCTFail("error creating test clique: \(error)")
            return (nil, nil, nil, nil)
        }
    }
}

#endif
