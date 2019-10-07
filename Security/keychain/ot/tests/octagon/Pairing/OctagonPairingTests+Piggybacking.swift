#if OCTAGON

let version1_txt: [UInt8] = [
    0x30, 0x82, 0x01, 0x9c, 0x02, 0x01, 0x00, 0x04, 0x82, 0x01, 0x80, 0x36,
    0xda, 0xda, 0xbf, 0x19, 0xdc, 0xe8, 0xb9, 0x8b, 0x45, 0xcd, 0xeb, 0xf2,
    0x14, 0xb3, 0x47, 0xfa, 0x65, 0x4c, 0x81, 0xdb, 0x7a, 0xf7, 0x99, 0x06,
    0x8d, 0xc8, 0x82, 0x2b, 0xae, 0x9d, 0x1f, 0x8e, 0x4f, 0xe4, 0x05, 0x59,
    0xf0, 0x46, 0xd6, 0x80, 0xb2, 0x9f, 0x7a, 0x9e, 0x9b, 0x66, 0x1e, 0x16,
    0x7a, 0x81, 0x19, 0x44, 0xc1, 0xa7, 0xaf, 0xaf, 0x56, 0xa9, 0x8a, 0x86,
    0x89, 0x42, 0xd9, 0xac, 0xf5, 0x32, 0xf6, 0x14, 0xd9, 0xc4, 0x41, 0x2b,
    0x8f, 0x6f, 0x48, 0x7c, 0xe4, 0x3b, 0xea, 0x97, 0xa6, 0x86, 0x93, 0x0f,
    0xf2, 0x6c, 0x77, 0x10, 0x2c, 0xc7, 0xe8, 0x84, 0xb9, 0x21, 0xda, 0xf1,
    0xe4, 0x62, 0x0d, 0x10, 0x09, 0x4c, 0x83, 0xb0, 0x13, 0x66, 0xe8, 0xcf,
    0xbf, 0x60, 0x49, 0xe3, 0x68, 0xa9, 0x27, 0x50, 0x94, 0xfb, 0x4f, 0x05,
    0x31, 0x7d, 0x13, 0xa7, 0x6a, 0x68, 0x5c, 0x14, 0x25, 0x7e, 0xc4, 0xb3,
    0x10, 0xe3, 0x45, 0x0d, 0xa1, 0x8f, 0x10, 0x98, 0x69, 0x1c, 0x8c, 0x0d,
    0x21, 0x3f, 0x64, 0xd5, 0x31, 0x90, 0x41, 0x90, 0x61, 0xd4, 0x60, 0x94,
    0xc3, 0x62, 0xd7, 0xfa, 0x40, 0xc7, 0xc5, 0x41, 0x95, 0x4b, 0xa2, 0xe6,
    0xfe, 0xda, 0xf3, 0xb9, 0x24, 0x3e, 0x9e, 0xe0, 0xa0, 0xfc, 0x38, 0xdb,
    0x21, 0x35, 0xe1, 0x2e, 0x39, 0x2a, 0x5a, 0xf4, 0xa1, 0x24, 0x89, 0xfd,
    0x4e, 0x2d, 0x9c, 0x44, 0xde, 0xda, 0x9b, 0xb6, 0xa6, 0x00, 0xf3, 0x3f,
    0xd6, 0x61, 0x4f, 0xe0, 0xf5, 0x01, 0x69, 0xa5, 0xfa, 0x91, 0xed, 0xfb,
    0xa9, 0xc2, 0x9d, 0x28, 0x94, 0x95, 0x7f, 0xaa, 0x21, 0x53, 0xff, 0x2a,
    0xfb, 0xe5, 0xcf, 0x4a, 0x8a, 0xc5, 0xff, 0x8e, 0xf4, 0x69, 0xbe, 0x4d,
    0x3c, 0xfd, 0x84, 0x9d, 0xd2, 0xf7, 0x06, 0x86, 0x50, 0x23, 0x01, 0x19,
    0x85, 0x02, 0x08, 0xbb, 0x1c, 0x7b, 0x38, 0x72, 0x49, 0xa6, 0xf5, 0x5f,
    0x33, 0x44, 0xc7, 0x68, 0xa2, 0x43, 0x51, 0xae, 0x49, 0xe8, 0x31, 0x3a,
    0x8d, 0x54, 0x09, 0x12, 0x5f, 0x22, 0xd0, 0xe8, 0x53, 0x5a, 0xd8, 0x62,
    0x71, 0x65, 0x51, 0xdc, 0xdc, 0x0f, 0x25, 0x3a, 0x90, 0xf0, 0x46, 0x85,
    0x83, 0xbc, 0xb2, 0xf3, 0xdb, 0x5a, 0xc8, 0x37, 0x12, 0x99, 0x47, 0x47,
    0x6f, 0x9a, 0x15, 0xec, 0x27, 0xfb, 0xe4, 0x38, 0x2e, 0x00, 0x96, 0x92,
    0x97, 0x55, 0x02, 0x2c, 0xf9, 0x8d, 0x6e, 0x0f, 0x73, 0xac, 0x17, 0xb9,
    0x46, 0xbc, 0x56, 0x06, 0xc9, 0x1e, 0x95, 0xd5, 0xf6, 0x45, 0xc2, 0x16,
    0x36, 0xa8, 0x50, 0xc7, 0xf9, 0xec, 0xb5, 0xe8, 0x8d, 0xec, 0x4a, 0x1d,
    0xe0, 0x8c, 0x86, 0xf1, 0x6e, 0xf0, 0x79, 0x17, 0x33, 0x71, 0xab, 0xe7,
    0x48, 0xc6, 0x1a, 0xd1, 0x09, 0x2b, 0x9c, 0xd3, 0xd8, 0x19, 0xf6, 0x02,
    0x01, 0x01, 0x04, 0x10, 0xf1, 0x3f, 0xd5, 0x7d, 0x58, 0x74, 0x4e, 0xfe,
    0xaa, 0x93, 0x8e, 0x03, 0x89, 0xbb, 0x8f, 0xeb,
]
let version1_txt_len = 416

let version0_txt: [UInt8] = [
    0x30, 0x82, 0x01, 0x87, 0x02, 0x01, 0x00, 0x04, 0x82, 0x01, 0x80, 0x81,
    0x13, 0x06, 0x96, 0x1f, 0x38, 0x75, 0x10, 0x4f, 0xcf, 0x0d, 0x50, 0x43,
    0x33, 0xd7, 0xe3, 0xc6, 0x6a, 0xc3, 0x21, 0xb6, 0x8d, 0x51, 0x76, 0x61,
    0xdd, 0xa0, 0xb9, 0xed, 0xbd, 0xc4, 0xd9, 0x63, 0x0d, 0xa8, 0xa5, 0x6b,
    0x7f, 0x6e, 0x77, 0xf2, 0xfb, 0x48, 0xaa, 0xd7, 0x8f, 0x5f, 0xb7, 0x97,
    0x7e, 0xee, 0xf8, 0xa0, 0xa1, 0xdc, 0x15, 0xfb, 0xb7, 0x36, 0x9c, 0x43,
    0xbf, 0xc5, 0x1a, 0x73, 0xae, 0xa4, 0xcd, 0xa6, 0xa8, 0xcc, 0x16, 0xcb,
    0x42, 0xfa, 0x3f, 0x22, 0x0c, 0xd4, 0xd8, 0xdc, 0x8d, 0xdb, 0xe2, 0x19,
    0xbc, 0x1d, 0x22, 0xb5, 0xe1, 0x3f, 0x4f, 0xca, 0x3a, 0xbd, 0xb6, 0xb3,
    0xfd, 0x5e, 0x61, 0x9e, 0x40, 0x67, 0xfc, 0x9e, 0x61, 0x44, 0x70, 0x97,
    0xb9, 0x86, 0x50, 0xa9, 0xe2, 0xd3, 0x6f, 0x46, 0x6a, 0x73, 0xc4, 0xf3,
    0xb3, 0xdb, 0x1a, 0x4e, 0xd1, 0x9d, 0xf2, 0x99, 0xb0, 0xe1, 0x81, 0x09,
    0xc4, 0x08, 0xf0, 0x48, 0xff, 0xa2, 0x7a, 0xd2, 0xd0, 0x73, 0xe5, 0x7c,
    0x2c, 0x16, 0xae, 0x59, 0x17, 0x4d, 0xa9, 0x8e, 0xab, 0x01, 0xe8, 0xab,
    0xf8, 0x3b, 0x7a, 0xad, 0x38, 0x12, 0x50, 0x73, 0xc7, 0x5e, 0xa5, 0x02,
    0x34, 0x97, 0x4a, 0x97, 0x5e, 0x0f, 0x78, 0x9d, 0xed, 0x1b, 0x69, 0xc9,
    0xd3, 0x2c, 0x07, 0xe0, 0x36, 0x40, 0x5d, 0x39, 0x14, 0x62, 0x29, 0xbd,
    0x11, 0x1b, 0xb9, 0x66, 0xd6, 0xcd, 0x17, 0xdd, 0x83, 0xc3, 0x95, 0xe1,
    0xac, 0x90, 0xab, 0x57, 0x10, 0x0b, 0xc5, 0x18, 0xe2, 0xdb, 0xeb, 0x22,
    0x81, 0x91, 0x1e, 0xa0, 0xa7, 0x91, 0xe2, 0xc9, 0xdc, 0x07, 0x07, 0xf6,
    0xd3, 0x04, 0x58, 0xd8, 0x68, 0xed, 0xeb, 0x9e, 0x0f, 0xf5, 0xa2, 0xdd,
    0x87, 0x94, 0x5e, 0x71, 0x1f, 0xeb, 0x6c, 0x9c, 0xad, 0x38, 0x58, 0x2f,
    0x48, 0x6c, 0xc1, 0x40, 0xde, 0x4f, 0x00, 0x08, 0x20, 0x9c, 0x84, 0x75,
    0xa5, 0x60, 0x55, 0x3e, 0x33, 0xc8, 0x74, 0x65, 0x4f, 0xe5, 0x2f, 0xd2,
    0x8a, 0xee, 0x21, 0x9f, 0xe5, 0x8d, 0xda, 0x5c, 0xc6, 0x2e, 0xcc, 0x7b,
    0x14, 0x2a, 0xfa, 0x7c, 0x37, 0x34, 0x1e, 0x07, 0x57, 0xd8, 0xf5, 0xc6,
    0x09, 0xa3, 0x28, 0xa0, 0x28, 0x13, 0x60, 0xe3, 0xd1, 0xec, 0xbb, 0x86,
    0xce, 0x06, 0x1d, 0xa5, 0xfd, 0x3a, 0x92, 0xe8, 0xa4, 0x9a, 0x28, 0x93,
    0x6e, 0x16, 0xe8, 0xd6, 0x86, 0x8e, 0xef, 0x50, 0x42, 0x46, 0xd5, 0x5d,
    0x9a, 0x66, 0xae, 0x60, 0x31, 0x6c, 0x38, 0x18, 0x3a, 0x1b, 0x98, 0xcc,
    0x55, 0xe5, 0x92, 0xac, 0xca, 0xfb, 0xae, 0xcc, 0xdc, 0x31, 0x24, 0x9d,
    0x01, 0x1e, 0xa7, 0x3b, 0x11, 0xca, 0xc1, 0x92, 0x60, 0x73, 0x7e, 0x70,
    0x63, 0x54, 0x1e, 0x7d, 0xa5, 0x3c, 0xb9, 0x87, 0x2a, 0x9e, 0xa3,
]
let version0_txt_len = 395

extension OctagonPairingTests {

    func testPiggybacking() {
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let rpcEpochCallbackOccurs = self.expectation(description: "rpcEpoch callback occurs")

        self.manager.rpcEpoch(with: self.acceptorPiggybackingConfig) { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertEqual(epoch, 1, "epoch should be nil")
            rpcEpochCallbackOccurs.fulfill()
        }
        self.wait(for: [rpcEpochCallbackOccurs], timeout: 10)

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        var peerID: String = ""
        var permanentInfo = Data(count: 0)
        var permanentInfoSig = Data(count: 0)
        var stableInfo = Data(count: 0)
        var stableInfoSig = Data(count: 0)

        /*begin message passing*/
        let rpcFirstInitiatorJoiningMessageCallBack = self.expectation(description: "Creating prepare message callback")
        self.manager.rpcPrepareIdentityAsApplicant(with: self.initiatorPiggybackingConfig) { pID, pI, pIS, sI, sIS, error in
            XCTAssertNotNil(peerID, "peerID should not be nil")
            XCTAssertNotNil(permanentInfo, "permanentInfo should not be nil")
            XCTAssertNotNil(permanentInfoSig, "permanentInfoSig should not be nil")
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfoSig, "stableInfoSig should not be nil")

            peerID = pID!
            permanentInfo = pI!
            permanentInfoSig = pIS!
            stableInfo = sI!
            stableInfoSig = sIS!

            XCTAssertNil(error, "error should be nil")
            rpcFirstInitiatorJoiningMessageCallBack.fulfill()
        }
        self.wait(for: [rpcFirstInitiatorJoiningMessageCallBack], timeout: 10)

        let firstMessageAcceptorCallback = self.expectation(description: "creating voucher message callback")
        var voucher = Data(count: 0)
        var voucherSig = Data(count: 0)

        self.manager.rpcVoucher(with: self.acceptorPiggybackingConfig, peerID: peerID, permanentInfo: permanentInfo, permanentInfoSig: permanentInfoSig, stableInfo: stableInfo, stableInfoSig: stableInfoSig) { v, vS, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(v, "voucher should not be nil")
            XCTAssertNotNil(vS, "voucher signature should not be nil")
            voucher = v
            voucherSig = vS
            firstMessageAcceptorCallback.fulfill()
        }
        self.wait(for: [firstMessageAcceptorCallback], timeout: 10)

        let rpcJoinCallback = self.expectation(description: "joining callback")
        self.manager.rpcJoin(with: self.initiatorPiggybackingConfig, vouchData: voucher, vouchSig: voucherSig, preapprovedKeys: nil) { error in
            XCTAssertNil(error, "error should be nil")
            rpcJoinCallback.fulfill()
        }
        self.wait(for: [rpcJoinCallback], timeout: 10)

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: self.cuttlefishContextForAcceptor, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContextForAcceptor)

        clientStateMachine.notifyContainerChange()

        let initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) {
            dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? Array<String>
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            initiatorDumpCallback.fulfill()
        }
        self.wait(for: [initiatorDumpCallback], timeout: 10)

        let acceptorDumpCallback = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.contextForAcceptor) {
            dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? Array<String>
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            acceptorDumpCallback.fulfill()
        }
        self.wait(for: [acceptorDumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
    }

    func testVersion2ofPiggybacking() {
        KCSetJoiningOctagonPiggybackingEnabled(true)
        OctagonSetIsEnabled(true)
        self.startCKAccountStatusMock()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        let acceptorContext = self.manager.context(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor)

        self.getAcceptorInCircle()
        self.assertEnters(context: acceptorContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        var clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (requestDelegate, acceptDelegate, acceptSession, requestSession) = self.setupKCJoiningSessionObjects()
        var initialMessageContainingOctagonVersion: Data?
        var challengeContainingEpoch: Data?
        var response: Data?
        var verification: Data?
        var doneMessage: Data?

        XCTAssertNotNil(acceptSession, "acceptSession should not be nil")
        XCTAssertNotNil(requestSession, "requestSession should not be nil")
        XCTAssertNotNil(requestDelegate, "requestDelegate should not be nil")
        XCTAssertNotNil(acceptDelegate, "acceptDelegate should not be nil")

        do {
            initialMessageContainingOctagonVersion = try requestSession!.initialMessage()
        } catch {
            XCTAssertNil(error, "error retrieving initialMessageContainingOctagonVersion message")
        }

        XCTAssertNotNil(initialMessageContainingOctagonVersion, "initial message should not be nil")

        do {
            challengeContainingEpoch = try acceptSession!.processMessage(initialMessageContainingOctagonVersion!)
            XCTAssertNotNil(challengeContainingEpoch, "challengeContainingEpoch should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving challengeContainingEpoch message")
        }

        do {
            response = try requestSession!.processMessage(challengeContainingEpoch!)
            XCTAssertNotNil(response, "response message should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving response message")
        }

        do {
            verification = try acceptSession!.processMessage(response!)
            XCTAssertNotNil(verification, "verification should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving verification message")
        }

        do {
            doneMessage = try requestSession!.processMessage(verification!)
            XCTAssertNotNil(doneMessage, "doneMessage should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving response message")
        }

        XCTAssertTrue(requestSession!.isDone(), "SecretSession done")
        XCTAssertFalse(acceptSession!.isDone(), "Unexpected accept session done")

        let aesSession = requestSession!.session

        let requestCircleSession = KCJoiningRequestCircleSession(circleDelegate: requestDelegate!,
                                                                 session: aesSession!,
                                                                 otcontrol: self.otControl,
                                                                 error: nil)
        XCTAssertNotNil(requestCircleSession, "No request secret session")

        requestCircleSession.setJoiningConfigurationObject(self.initiatorPiggybackingConfig)
        requestCircleSession.setControlObject(self.otControl)

        var identityMessage: Data?
        do {
            identityMessage = try requestCircleSession.initialMessage()
            XCTAssertNotNil(identityMessage, "No identity message")

        } catch {
            XCTAssertNil(error, "error retrieving identityMessage message")
        }

        var voucherMessage: Data?
        do {
            voucherMessage = try acceptSession!.processMessage(identityMessage!)
            XCTAssertNotNil(voucherMessage, "No voucherMessage message")
        } catch {
            XCTAssertNil(error, "error retrieving voucherMessage message")

        }

        var nothing: Data?
        do {
            nothing = try requestCircleSession.processMessage(voucherMessage!)
            XCTAssertNotNil(nothing, "No nothing message")
        } catch {
            XCTAssertNil(error, "error retrieving nothing message")

        }

        XCTAssertTrue(requestSession!.isDone(), "requestor should be done")
        XCTAssertTrue(acceptSession!.isDone(), "acceptor should be done")

        clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.notifyContainerChange()

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.sendContainerChange(context: initiator1Context)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: self.cuttlefishContextForAcceptor, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContextForAcceptor)
        self.verifyDatabaseMocks()

        let initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) {
            dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? Array<String>
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            initiatorDumpCallback.fulfill()
        }
        self.wait(for: [initiatorDumpCallback], timeout: 10)

        let acceptorDumpCallback = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.contextForAcceptor) {
            dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? Array<String>
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            acceptorDumpCallback.fulfill()
        }
        self.wait(for: [acceptorDumpCallback], timeout: 10)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")
    }

    func testVersion0TestVectorofPiggybacking() {
        let (_, _, accept, request) = self.setupKCJoiningSessionObjects()
        XCTAssertNotNil(accept, "acceptor should not be nil")
        XCTAssertNotNil(request, "requester should not be nil")

        let version0Data = NSData(bytes: version0_txt, length: version0_txt_len)
        XCTAssertNotNil(version0Data, "version0Data should not be nil")
        do {
            let challenge = try accept!.processMessage(version0Data as Data)
            XCTAssertNotNil(challenge, "challenge should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving initial message")
        }
    }

    func testVersion1TestVectorofPiggybacking() {
        let (_, _, accept, request) = self.setupKCJoiningSessionObjects()
        XCTAssertNotNil(accept, "acceptor should not be nil")
        XCTAssertNotNil(request, "requester should not be nil")
        let version1Data = NSData(bytes: version1_txt, length: version1_txt_len)
        XCTAssertNotNil(version1Data, "version1Data should not be nil")
        do {
            let challenge = try accept!.processMessage(version1Data as Data)
            XCTAssertNotNil(challenge, "challenge should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving initial message")
        }
    }
    /* FIX ME, This isn't testing version 1.
    func testV1() {
        let (requestDelegate, acceptDelegate, acceptSession, requestSession) = self.setupKCJoiningSessionObjects()
        var initialMessageContainingOctagonVersion: Data?
        var challengeContainingEpoch: Data?
        var response: Data?
        var verification: Data?
        var doneMessage: Data?

        XCTAssertNotNil(acceptSession, "acceptSession should not be nil")
        XCTAssertNotNil(requestSession, "requestSession should not be nil")
        XCTAssertNotNil(requestDelegate, "requestDelegate should not be nil")
        XCTAssertNotNil(acceptDelegate, "acceptDelegate should not be nil")

        do {
            initialMessageContainingOctagonVersion = try requestSession!.initialMessage()
        } catch {
            XCTAssertNil(error, "error retrieving initialMessageContainingOctagonVersion message")
        }

        XCTAssertNotNil(initialMessageContainingOctagonVersion, "initial message should not be nil")

        do {
            challengeContainingEpoch = try acceptSession!.processMessage(initialMessageContainingOctagonVersion!)
            XCTAssertNotNil(challengeContainingEpoch, "challengeContainingEpoch should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving challengeContainingEpoch message")
        }

        do {
            response = try requestSession!.processMessage(challengeContainingEpoch!)
            XCTAssertNotNil(response, "response message should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving response message")
        }

        do {
            verification = try acceptSession!.processMessage(response!)
            XCTAssertNotNil(verification, "verification should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving verification message")
        }

        do {
            doneMessage = try requestSession!.processMessage(verification!)
            XCTAssertNotNil(doneMessage, "doneMessage should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving response message")
        }

        XCTAssertTrue(requestSession!.isDone(), "SecretSession done")
        XCTAssertFalse(acceptSession!.isDone(), "Unexpected accept session done")

        let aesSession = requestSession!.session

        let requestCircleSession = KCJoiningRequestCircleSession(circleDelegate: requestDelegate!, session: aesSession!, error: nil)
        XCTAssertNotNil(requestCircleSession, "No request secret session")

        requestCircleSession.setJoiningConfigurationObject(self.initiatorPiggybackingConfig)
        requestCircleSession.setControlObject(self.otControl)

        var identityMessage: Data?
        do {
            identityMessage = try requestCircleSession.initialMessage()
            XCTAssertNotNil(identityMessage, "No identity message")

        } catch {
            XCTAssertNil(error, "error retrieving identityMessage message")
        }

        var voucherMessage: Data?
        do {
            voucherMessage = try acceptSession!.processMessage(identityMessage!)
            XCTAssertNotNil(voucherMessage, "No voucherMessage message")
        } catch {
            XCTAssertNil(error, "error retrieving voucherMessage message")

        }

        var nothing: Data?
        do {
            nothing = try requestCircleSession.processMessage(voucherMessage!)
            XCTAssertNotNil(nothing, "No nothing message")
        } catch {
            XCTAssertNil(error, "error retrieving nothing message")

        }

        XCTAssertTrue(requestSession!.isDone(), "requestor should be done")
        XCTAssertTrue(acceptSession!.isDone(), "acceptor should be done")
    }
*/
    func testPairingReset() {
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()
        let rpcEpochCallbackOccurs = self.expectation(description: "rpcEpoch callback occurs")

        self.manager.rpcEpoch(with: self.acceptorPiggybackingConfig) { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertEqual(epoch, 1, "epoch should be nil")
            rpcEpochCallbackOccurs.fulfill()
        }
        self.wait(for: [rpcEpochCallbackOccurs], timeout: 10)

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.startOctagonStateMachine()
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        var peerID: String = ""
        var permanentInfo = Data(count: 0)
        var permanentInfoSig = Data(count: 0)
        var stableInfo = Data(count: 0)
        var stableInfoSig = Data(count: 0)

        /*begin message passing*/
        let rpcFirstInitiatorJoiningCallBack = self.expectation(description: "Creating prepare callback")
        self.manager.rpcPrepareIdentityAsApplicant(with: self.initiatorPiggybackingConfig) { pID, pI, pISig, sI, sISig, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(pID, "peer id should not be nil")
            XCTAssertNotNil(pI, "permanentInfo should not be nil")
            XCTAssertNotNil(sI, "sI should not be nil")
            XCTAssertNotNil(sISig, "sISig should not be nil")

            peerID = pID!
            permanentInfo = pI!
            permanentInfoSig = pISig!
            stableInfo = sI!
            stableInfoSig = sISig!

            rpcFirstInitiatorJoiningCallBack.fulfill()
        }
        self.wait(for: [rpcFirstInitiatorJoiningCallBack], timeout: 10)

        let firstAcceptorCallback = self.expectation(description: "creating vouch callback")

        self.manager.rpcVoucher(with: self.acceptorPiggybackingConfig, peerID: peerID, permanentInfo: permanentInfo, permanentInfoSig: permanentInfoSig, stableInfo: stableInfo, stableInfoSig: stableInfoSig) { voucher, voucherSig, error in
            XCTAssertNotNil(voucher, "voucher should not be nil")
            XCTAssertNotNil(voucherSig, "voucherSig should not be nil")
            XCTAssertNil(error, "error should be nil")
            firstAcceptorCallback.fulfill()
        }
        self.wait(for: [firstAcceptorCallback], timeout: 10)

        //change pairing UUID

        initiator1Context.pairingUUID = "1234"

        let firstMessageWithNewJoiningConfigCallback = self.expectation(description: "first message with different configuration")
        self.manager.rpcPrepareIdentityAsApplicant(with: self.initiatorPiggybackingConfig) { pID, pI, pISig, sI, sISig, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(pID, "peer id should not be nil")
            XCTAssertNotNil(pI, "permanentInfo should not be nil")
            XCTAssertNotNil(sI, "sI should not be nil")
            XCTAssertNotNil(sISig, "sISig should not be nil")

            peerID = pID!
            permanentInfo = pI!
            permanentInfoSig = pISig!
            stableInfo = sI!
            stableInfoSig = sISig!
            firstMessageWithNewJoiningConfigCallback.fulfill()
        }
        self.wait(for: [firstMessageWithNewJoiningConfigCallback], timeout: 10)
    }

    func testVersion2ofPiggybackingWithSOS() {
        KCSetJoiningOctagonPiggybackingEnabled(true)
        OctagonSetPlatformSupportsSOS(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        var clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // Note that in this strange situation, the join should create the CKKS TLKs
        let (requestDelegate, acceptDelegate, acceptSession, requestSession) = self.setupKCJoiningSessionObjects()
        var initialMessageContainingOctagonVersion: Data?
        var challengeContainingEpoch: Data?
        var response: Data?
        var verification: Data?
        var doneMessage: Data?

        XCTAssertNotNil(acceptSession, "acceptSession should not be nil")
        XCTAssertNotNil(requestSession, "requestSession should not be nil")
        XCTAssertNotNil(requestDelegate, "requestDelegate should not be nil")
        XCTAssertNotNil(acceptDelegate, "acceptDelegate should not be nil")

        do {
            initialMessageContainingOctagonVersion = try requestSession!.initialMessage()
        } catch {
            XCTAssertNil(error, "error retrieving initialMessageContainingOctagonVersion message")
        }

        XCTAssertNotNil(initialMessageContainingOctagonVersion, "initial message should not be nil")

        do {
            challengeContainingEpoch = try acceptSession!.processMessage(initialMessageContainingOctagonVersion!)
            XCTAssertNotNil(challengeContainingEpoch, "challengeContainingEpoch should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving challengeContainingEpoch message")
        }

        do {
            response = try requestSession!.processMessage(challengeContainingEpoch!)
            XCTAssertNotNil(response, "response message should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving response message")
        }

        do {
            verification = try acceptSession!.processMessage(response!)
            XCTAssertNotNil(verification, "verification should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving verification message")
        }

        do {
            doneMessage = try requestSession!.processMessage(verification!)
            XCTAssertNotNil(doneMessage, "doneMessage should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving response message")
        }

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        XCTAssertTrue(requestSession!.isDone(), "SecretSession done")
        XCTAssertFalse(acceptSession!.isDone(), "Unexpected accept session done")

        let aesSession = requestSession!.session

        let requestCircleSession = KCJoiningRequestCircleSession(circleDelegate: requestDelegate!,
                                                                 session: aesSession!,
                                                                 otcontrol: self.otControl,
                                                                 error: nil)
        XCTAssertNotNil(requestCircleSession, "No request secret session")

        requestCircleSession.setJoiningConfigurationObject(self.initiatorPiggybackingConfig)
        requestCircleSession.setControlObject(self.otControl)

        var identityMessage: Data?
        do {
            identityMessage = try requestCircleSession.initialMessage()
            let parsedMessage = try KCJoiningMessage(der: identityMessage!)
            XCTAssertNotNil(parsedMessage.firstData, "No octagon message")
            XCTAssertNotNil(parsedMessage.secondData, "No sos message")
            XCTAssertNotNil(identityMessage, "No identity message")

        } catch {
            XCTAssertNil(error, "error retrieving identityMessage message")
        }

        var voucherMessage: Data?
        do {
            voucherMessage = try acceptSession!.processMessage(identityMessage!)
            let parsedMessage = try KCJoiningMessage(der: identityMessage!)
            XCTAssertNotNil(parsedMessage.firstData, "No octagon message")
            XCTAssertNotNil(parsedMessage.secondData, "No sos message")
            XCTAssertNotNil(voucherMessage, "No voucherMessage message")
        } catch {
            XCTAssertNil(error, "error retrieving voucherMessage message")

        }

        var nothing: Data?
        do {
            nothing = try requestCircleSession.processMessage(voucherMessage!)
            XCTAssertNotNil(nothing, "No nothing message")
        } catch {
            XCTAssertNil(error, "error retrieving nothing message")
        }

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(requestSession!.isDone(), "requestor should be done")
        XCTAssertTrue(acceptSession!.isDone(), "acceptor should be done")

        clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.notifyContainerChange()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: self.cuttlefishContextForAcceptor, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContextForAcceptor)
        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContextForAcceptor, sender: self.cuttlefishContext)

        let initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) {
            dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? Array<String>
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            initiatorDumpCallback.fulfill()
        }
        self.wait(for: [initiatorDumpCallback], timeout: 10)

        let acceptorDumpCallback = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.contextForAcceptor) {
            dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? Array<String>
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            acceptorDumpCallback.fulfill()
        }
        self.wait(for: [acceptorDumpCallback], timeout: 10)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")
    }

    func testOctagonCapableButAcceptorHasNoIdentity() {
        KCSetJoiningOctagonPiggybackingEnabled(true)
        OctagonSetPlatformSupportsSOS(true)
        OctagonSetSOSUpgrade(true)
        self.startCKAccountStatusMock()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        _ = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (requestDelegate, acceptDelegate, acceptSession, requestSession) = self.setupKCJoiningSessionObjects()
        var initialMessageContainingOctagonVersion: Data?
        var challengeContainingEpoch: Data?
        var response: Data?
        var verification: Data?
        var doneMessage: Data?

        XCTAssertNotNil(acceptSession, "acceptSession should not be nil")
        XCTAssertNotNil(requestSession, "requestSession should not be nil")
        XCTAssertNotNil(requestDelegate, "requestDelegate should not be nil")
        XCTAssertNotNil(acceptDelegate, "acceptDelegate should not be nil")

        do {
            initialMessageContainingOctagonVersion = try requestSession!.initialMessage()
        } catch {
            XCTAssertNil(error, "error retrieving initialMessageContainingOctagonVersion message")
        }

        XCTAssertNotNil(initialMessageContainingOctagonVersion, "initial message should not be nil")

        do {
            challengeContainingEpoch = try acceptSession!.processMessage(initialMessageContainingOctagonVersion!)
            XCTAssertNotNil(challengeContainingEpoch, "challengeContainingEpoch should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving challengeContainingEpoch message")
        }

        do {
            response = try requestSession!.processMessage(challengeContainingEpoch!)
            XCTAssertNotNil(response, "response message should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving response message")
        }

        do {
            verification = try acceptSession!.processMessage(response!)
            XCTAssertNotNil(verification, "verification should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving verification message")
        }

        do {
            doneMessage = try requestSession!.processMessage(verification!)
            XCTAssertNotNil(doneMessage, "doneMessage should not be nil")
        } catch {
            XCTAssertNil(error, "error retrieving response message")
        }

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        XCTAssertTrue(requestSession!.isDone(), "SecretSession done")
        XCTAssertFalse(acceptSession!.isDone(), "Unexpected accept session done")

        let aesSession = requestSession!.session

        let requestCircleSession = KCJoiningRequestCircleSession(circleDelegate: requestDelegate!,
                                                                 session: aesSession!,
                                                                 otcontrol: self.otControl,
                                                                 error: nil)
        XCTAssertNotNil(requestCircleSession, "No request secret session")

        requestCircleSession.setJoiningConfigurationObject(self.initiatorPiggybackingConfig)
        requestCircleSession.setControlObject(self.otControl)

        var identityMessage: Data?
        do {
            identityMessage = try requestCircleSession.initialMessage()
            XCTAssertNotNil(identityMessage, "No identity message")

        } catch {
            XCTAssertNil(error, "error retrieving identityMessage message")
        }

        var voucherMessage: Data?
        do {
            voucherMessage = try acceptSession!.processMessage(identityMessage!)
            XCTAssertNotNil(voucherMessage, "No voucherMessage message")
        } catch {
            XCTAssertNil(error, "error retrieving voucherMessage message")

        }

        var nothing: Data?
        do {
            nothing = try requestCircleSession.processMessage(voucherMessage!)
            XCTAssertNotNil(nothing, "No nothing message")
        } catch {
            XCTAssertNil(error, "error retrieving nothing message")

        }

        XCTAssertTrue(requestSession!.isDone(), "requestor should be done")
        XCTAssertTrue(acceptSession!.isDone(), "acceptor should be done")
    }

}

#endif
