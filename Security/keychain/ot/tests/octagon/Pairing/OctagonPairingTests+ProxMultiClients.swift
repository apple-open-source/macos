#if OCTAGON

extension OctagonPairingTests {
    func test2ClientsBothOctagonAndSOS() {
        OctagonSetPlatformSupportsSOS(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator2ContextID = "initiatorContext2"
        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        let initiator2Context = self.manager.context(forContainerName: OTCKContainerName,
                                                     contextID: initiator2ContextID,
                                                     sosAdapter: self.mockSOSAdapter,
                                                     authKitAdapter: self.mockAuthKit2,
                                                     lockStateTracker: self.lockStateTracker,
                                                     accountStateTracker: self.accountStateTracker,
                                                     deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-initiator-2", serialNumber: "456", osVersion: "iOS (fake version)"))

        initiator1Context.startOctagonStateMachine()
        initiator2Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: initiator2Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-1")
        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let clientStateMachine2 = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: "initiator-2")

        let (acceptor2, initiator2) = self.setupPairingEndpoints(withPairNumber: "2", initiatorContextID: initiator2ContextID, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: "initiator-2", acceptorUniqueID: "acceptor-1")

        XCTAssertNotNil(acceptor2, "acceptor should not be nil")
        XCTAssertNotNil(initiator2, "initiator should not be nil")

        var signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* PAIR-1 INITIATOR FIRST RTT JOINING MESSAGE*/
        var initiatorFirstPacket = Data()
        let firstInitiatorCallback = self.expectation(description: "firstInitiatorCallback callback occurs")

        initiator.exchangePacket(nil) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorFirstPacket = packet!
            firstInitiatorCallback.fulfill()
        }

        self.wait(for: [firstInitiatorCallback], timeout: 10)

        /* PAIR-1 ACCEPTOR FIRST RTT EPOCH*/
        var acceptorFirstPacket = Data()
        let firstAcceptorCallback = self.expectation(description: "firstAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorFirstPacket = packet!
            firstAcceptorCallback.fulfill()
        }
        self.wait(for: [firstAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorFirstPacket, "first packet should not be nil")

        /* PAIR-1 INITIATOR SECOND RTT PREPARE*/
        var initiatorSecondPacket = Data()
        let secondInitiatorCallback = self.expectation(description: "secondInitiatorCallback callback occurs")

        initiator.exchangePacket(acceptorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorSecondPacket = packet!
            secondInitiatorCallback.fulfill()
        }

        self.wait(for: [secondInitiatorCallback], timeout: 10)
        XCTAssertNotNil(initiatorSecondPacket, "initiator second packet should not be nil")

        /* PAIR-1 ACCEPTOR SECOND RTT */
        var acceptorSecondPacket = Data()
        let SecondAcceptorCallback = self.expectation(description: "SecondAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorSecondPacket = packet!
            SecondAcceptorCallback.fulfill()
        }
        self.wait(for: [SecondAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorSecondPacket, "acceptor second packet should not be nil")

        /* PAIR-1 INITIATOR THIRD STEP*/
        var initiatorThirdPacket: Data?
        let thirdInitiatorCallback = self.expectation(description: "thirdInitiatorCallback callback occurs")

        initiator.exchangePacket(acceptorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorThirdPacket = (packet)
            thirdInitiatorCallback.fulfill()
        }
        self.wait(for: [thirdInitiatorCallback], timeout: 10)
        XCTAssertNotNil(initiatorThirdPacket, "acceptor second packet should not be nil")

        var acceptorThirdPacket = Data()
        let ThirdAcceptorCallback = self.expectation(description: "ThirdAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorThirdPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorThirdPacket = packet!
            ThirdAcceptorCallback.fulfill()
        }
        self.wait(for: [ThirdAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorThirdPacket, "acceptor third packet should not be nil")

        /* PAIR-1 INITIATOR FOURTH STEP*/
        let fourthInitiatorCallback = self.expectation(description: "fourthInitiatorCallback callback occurs")

        initiator.exchangePacket(acceptorThirdPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should be nil")
            XCTAssertNil(error, "error should be nil")
            fourthInitiatorCallback.fulfill()
        }

        self.wait(for: [fourthInitiatorCallback], timeout: 10)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.notifyContainerChange()

        let initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            initiatorDumpCallback.fulfill()
        }
        self.wait(for: [initiatorDumpCallback], timeout: 10)

        let acceptorDumpCallback = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.contextForAcceptor) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            acceptorDumpCallback.fulfill()
        }
        self.wait(for: [acceptorDumpCallback], timeout: 10)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")

        /*
         SECOND PAIRING
         */
        signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: initiator2ContextID) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* PAIR-2 INITIATOR FIRST RTT JOINING MESSAGE*/
        var pair2InitiatorFirstPacket = Data()
        let pair2FirstInitiatorCallback = self.expectation(description: "firstInitiatorCallback callback occurs")

        initiator2.exchangePacket(nil) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorFirstPacket = packet!
            pair2FirstInitiatorCallback.fulfill()
        }

        self.wait(for: [pair2FirstInitiatorCallback], timeout: 10)

        /* PAIR-2 ACCEPTOR FIRST RTT EPOCH*/
        var pair2AcceptorFirstPacket = Data()
        let pair2FirstAcceptorCallback = self.expectation(description: "pair2FirstAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorFirstPacket = packet!
            pair2FirstAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2FirstAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorFirstPacket, "first packet should not be nil")

        /* PAIR-2 INITIATOR SECOND RTT PREPARE*/
        var pair2InitiatorSecondPacket = Data()
        let pair2SecondInitiatorCallback = self.expectation(description: "pair2SecondInitiatorCallback callback occurs")

        initiator2.exchangePacket(pair2AcceptorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorSecondPacket = packet!
            pair2SecondInitiatorCallback.fulfill()
        }

        self.wait(for: [pair2SecondInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2InitiatorSecondPacket, "pair2InitiatorSecondPacket should not be nil")

        /* PAIR-2 ACCEPTOR SECOND RTT */
        var pair2AcceptorSecondPacket = Data()
        let pair2SecondAcceptorCallback = self.expectation(description: "pair2SecondAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorSecondPacket = packet!
            pair2SecondAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2SecondAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorSecondPacket, "acceptor second packet should not be nil")

        /* PAIR-2 INITIATOR THIRD STEP*/
        var pair2InitiatorThirdPacket: Data?
        let pair2ThirdInitiatorCallback = self.expectation(description: "pair2ThirdInitiatorCallback callback occurs")

        initiator2.exchangePacket(pair2AcceptorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorThirdPacket = (packet)
            pair2ThirdInitiatorCallback.fulfill()
        }
        self.wait(for: [pair2ThirdInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2InitiatorThirdPacket, "acceptor second packet should not be nil")

        var pair2AcceptorThirdPacket = Data()
        let pair2FourthAcceptorCallback = self.expectation(description: "pair2FourthAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorThirdPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorThirdPacket = packet!
            pair2FourthAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2FourthAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorThirdPacket, "acceptor third packet should not be nil")

        /* PAIR-2 INITIATOR FOURTH STEP*/
        let pair2FourthInitiatorCallback = self.expectation(description: "pair2FourthInitiatorCallback callback occurs")

        initiator2.exchangePacket(pair2AcceptorThirdPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should be nil")
            XCTAssertNil(error, "error should be nil")
            pair2FourthInitiatorCallback.fulfill()
        }

        self.wait(for: [pair2FourthInitiatorCallback], timeout: 10)

        clientStateMachine2.notifyContainerChange()

        let pair2InitiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            pair2InitiatorDumpCallback.fulfill()
        }
        self.wait(for: [pair2InitiatorDumpCallback], timeout: 10)

        let pair2AcceptorDumpCallback = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.contextForAcceptor) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 3, "should be 3 peer ids")
            pair2AcceptorDumpCallback.fulfill()
        }
        self.wait(for: [pair2AcceptorDumpCallback], timeout: 10)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 3, "should be 3 bottles")

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.verifyDatabaseMocks()
    }

    func test2ClientsOctagonOnly() {
        OctagonSetIsEnabled(true)
        OctagonSetPlatformSupportsSOS(false)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator2ContextID = "initiatorContext2"
        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        let initiator2Context = self.manager.context(forContainerName: OTCKContainerName,
                                                     contextID: initiator2ContextID,
                                                     sosAdapter: self.mockSOSAdapter,
                                                     authKitAdapter: self.mockAuthKit2,
                                                     lockStateTracker: self.lockStateTracker,
                                                     accountStateTracker: self.accountStateTracker,
                                                     deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-initiator-2", serialNumber: "456", osVersion: "iOS (fake version)"))

        initiator1Context.startOctagonStateMachine()
        initiator2Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: initiator2Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-1")
        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let clientStateMachine2 = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: "initiator-2")

        let (acceptor2, initiator2) = self.setupPairingEndpoints(withPairNumber: "2", initiatorContextID: initiator2ContextID, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: "initiator-2", acceptorUniqueID: "acceptor-1")

        XCTAssertNotNil(acceptor2, "acceptor should not be nil")
        XCTAssertNotNil(initiator2, "initiator should not be nil")

        var signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* PAIR-1 INITIATOR FIRST RTT JOINING MESSAGE*/
        var initiatorFirstPacket = Data()
        let firstInitiatorCallback = self.expectation(description: "firstInitiatorCallback callback occurs")

        initiator.exchangePacket(nil) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorFirstPacket = packet!
            firstInitiatorCallback.fulfill()
        }

        self.wait(for: [firstInitiatorCallback], timeout: 10)

        /* PAIR-1 ACCEPTOR FIRST RTT EPOCH*/
        var acceptorFirstPacket = Data()
        let firstAcceptorCallback = self.expectation(description: "firstAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorFirstPacket = packet!
            firstAcceptorCallback.fulfill()
        }
        self.wait(for: [firstAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorFirstPacket, "first packet should not be nil")

        /* PAIR-1 INITIATOR SECOND RTT PREPARE*/
        var initiatorSecondPacket = Data()
        let secondInitiatorCallback = self.expectation(description: "secondInitiatorCallback callback occurs")

        initiator.exchangePacket(acceptorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorSecondPacket = packet!
            secondInitiatorCallback.fulfill()
        }

        self.wait(for: [secondInitiatorCallback], timeout: 10)
        XCTAssertNotNil(initiatorSecondPacket, "initiator second packet should not be nil")

        /* PAIR-1 ACCEPTOR SECOND RTT */
        var acceptorSecondPacket = Data()
        let SecondAcceptorCallback = self.expectation(description: "SecondAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorSecondPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNotNil(packet, "packet should be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorSecondPacket = packet!
            SecondAcceptorCallback.fulfill()
        }
        self.wait(for: [SecondAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorSecondPacket, "acceptor second packet should not be nil")

        /* PAIR-1 INITIATOR THIRD STEP*/
        let thirdInitiatorCallback = self.expectation(description: "thirdInitiatorCallback callback occurs")

        initiator.exchangePacket(acceptorSecondPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should be nil")
            XCTAssertNil(error, "error should be nil")
            thirdInitiatorCallback.fulfill()
        }
        self.wait(for: [thirdInitiatorCallback], timeout: 10)

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.notifyContainerChange()

        let initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            initiatorDumpCallback.fulfill()
        }
        self.wait(for: [initiatorDumpCallback], timeout: 10)

        let acceptorDumpCallback = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.contextForAcceptor) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            acceptorDumpCallback.fulfill()
        }
        self.wait(for: [acceptorDumpCallback], timeout: 10)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")

        /*
         SECOND PAIRING
         */
        signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: initiator2ContextID) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* PAIR-2 INITIATOR FIRST RTT JOINING MESSAGE*/
        var pair2InitiatorFirstPacket = Data()
        let pair2FirstInitiatorCallback = self.expectation(description: "firstInitiatorCallback callback occurs")

        initiator2.exchangePacket(nil) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorFirstPacket = packet!
            pair2FirstInitiatorCallback.fulfill()
        }

        self.wait(for: [pair2FirstInitiatorCallback], timeout: 10)

        /* PAIR-2 ACCEPTOR FIRST RTT EPOCH*/
        var pair2AcceptorFirstPacket = Data()
        let pair2FirstAcceptorCallback = self.expectation(description: "pair2FirstAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorFirstPacket = packet!
            pair2FirstAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2FirstAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorFirstPacket, "first packet should not be nil")

        /* PAIR-2 INITIATOR SECOND RTT PREPARE*/
        var pair2InitiatorSecondPacket = Data()
        let pair2SecondInitiatorCallback = self.expectation(description: "pair2SecondInitiatorCallback callback occurs")

        initiator2.exchangePacket(pair2AcceptorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorSecondPacket = packet!
            pair2SecondInitiatorCallback.fulfill()
        }

        self.wait(for: [pair2SecondInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2InitiatorSecondPacket, "pair2InitiatorSecondPacket should not be nil")

        /* PAIR-2 ACCEPTOR SECOND RTT */
        var pair2AcceptorSecondPacket = Data()
        let pair2SecondAcceptorCallback = self.expectation(description: "pair2SecondAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorSecondPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorSecondPacket = packet!
            pair2SecondAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2SecondAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorSecondPacket, "acceptor second packet should not be nil")

        /* PAIR-2 INITIATOR THIRD STEP*/
        let pair2ThirdInitiatorCallback = self.expectation(description: "thirdInitiatorCallback callback occurs")

        initiator2.exchangePacket(pair2AcceptorSecondPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should be nil")
            XCTAssertNil(error, "error should be nil")
            pair2ThirdInitiatorCallback.fulfill()
        }
        self.wait(for: [pair2ThirdInitiatorCallback], timeout: 10)

        clientStateMachine2.notifyContainerChange()

        let pair2InitiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            pair2InitiatorDumpCallback.fulfill()
        }
        self.wait(for: [pair2InitiatorDumpCallback], timeout: 10)

        let pair2AcceptorDumpCallback = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.contextForAcceptor) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 3, "should be 3 peer ids")
            pair2AcceptorDumpCallback.fulfill()
        }
        self.wait(for: [pair2AcceptorDumpCallback], timeout: 10)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 3, "should be 3 bottles")

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.verifyDatabaseMocks()
    }

    func test2ClientsSOSOnly() {
        OctagonSetPlatformSupportsSOS(true)
        OctagonSetIsEnabled(false)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator2ContextID = "initiatorContext2"
        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        let initiator2Context = self.manager.context(forContainerName: OTCKContainerName,
                                                     contextID: initiator2ContextID,
                                                     sosAdapter: self.mockSOSAdapter,
                                                     authKitAdapter: self.mockAuthKit2,
                                                     lockStateTracker: self.lockStateTracker,
                                                     accountStateTracker: self.accountStateTracker,
                                                     deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-initiator-2", serialNumber: "456", osVersion: "iOS (fake version)"))

        initiator1Context.startOctagonStateMachine()
        initiator2Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: initiator2Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-1")
        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let (acceptor2, initiator2) = self.setupPairingEndpoints(withPairNumber: "2", initiatorContextID: initiator2ContextID, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: "initiator-2", acceptorUniqueID: "acceptor-1")

        XCTAssertNotNil(acceptor2, "acceptor should not be nil")
        XCTAssertNotNil(initiator2, "initiator should not be nil")

        var signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* PAIR-1 INITIATOR FIRST RTT JOINING MESSAGE*/
        var initiatorFirstPacket = Data()
        let firstInitiatorCallback = self.expectation(description: "firstInitiatorCallback callback occurs")

        initiator.exchangePacket(nil) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorFirstPacket = packet!
            firstInitiatorCallback.fulfill()
        }

        self.wait(for: [firstInitiatorCallback], timeout: 10)

        /* PAIR-1 ACCEPTOR FIRST RTT EPOCH*/
        var acceptorFirstPacket = Data()
        let firstAcceptorCallback = self.expectation(description: "firstAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorFirstPacket = packet!
            firstAcceptorCallback.fulfill()
        }
        self.wait(for: [firstAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorFirstPacket, "first packet should not be nil")

        /* PAIR-1 INITIATOR SECOND RTT PREPARE*/
        var initiatorSecondPacket = Data()
        let secondInitiatorCallback = self.expectation(description: "secondInitiatorCallback callback occurs")

        initiator.exchangePacket(acceptorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorSecondPacket = packet!
            secondInitiatorCallback.fulfill()
        }

        self.wait(for: [secondInitiatorCallback], timeout: 10)
        XCTAssertNotNil(initiatorSecondPacket, "initiator second packet should not be nil")

        /* PAIR-1 ACCEPTOR SECOND RTT */
        var acceptorSecondPacket = Data()
        let SecondAcceptorCallback = self.expectation(description: "SecondAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorSecondPacket = packet!
            SecondAcceptorCallback.fulfill()
        }
        self.wait(for: [SecondAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorSecondPacket, "acceptor second packet should not be nil")

        /* PAIR-1 INITIATOR THIRD STEP*/
        var initiatorThirdPacket: Data?
        let thirdInitiatorCallback = self.expectation(description: "thirdInitiatorCallback callback occurs")

        initiator.exchangePacket(acceptorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorThirdPacket = (packet)
            thirdInitiatorCallback.fulfill()
        }
        self.wait(for: [thirdInitiatorCallback], timeout: 10)
        XCTAssertNotNil(initiatorThirdPacket, "acceptor second packet should not be nil")

        var acceptorThirdPacket = Data()
        let ThirdAcceptorCallback = self.expectation(description: "ThirdAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorThirdPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorThirdPacket = packet!
            ThirdAcceptorCallback.fulfill()
        }
        self.wait(for: [ThirdAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorThirdPacket, "acceptor third packet should not be nil")

        /* PAIR-1 INITIATOR FOURTH STEP*/
        let fourthInitiatorCallback = self.expectation(description: "fourthInitiatorCallback callback occurs")

        initiator.exchangePacket(acceptorThirdPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should be nil")
            XCTAssertNil(error, "error should be nil")
            fourthInitiatorCallback.fulfill()
        }

        self.wait(for: [fourthInitiatorCallback], timeout: 10)

        /*
         SECOND PAIRING
         */
        signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: initiator2ContextID) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* PAIR-2 INITIATOR FIRST RTT JOINING MESSAGE*/
        var pair2InitiatorFirstPacket = Data()
        let pair2FirstInitiatorCallback = self.expectation(description: "firstInitiatorCallback callback occurs")

        initiator2.exchangePacket(nil) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorFirstPacket = packet!
            pair2FirstInitiatorCallback.fulfill()
        }

        self.wait(for: [pair2FirstInitiatorCallback], timeout: 10)

        /* PAIR-2 ACCEPTOR FIRST RTT EPOCH*/
        var pair2AcceptorFirstPacket = Data()
        let pair2FirstAcceptorCallback = self.expectation(description: "pair2FirstAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorFirstPacket = packet!
            pair2FirstAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2FirstAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorFirstPacket, "first packet should not be nil")

        /* PAIR-2 INITIATOR SECOND RTT PREPARE*/
        var pair2InitiatorSecondPacket = Data()
        let pair2SecondInitiatorCallback = self.expectation(description: "pair2SecondInitiatorCallback callback occurs")

        initiator2.exchangePacket(pair2AcceptorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorSecondPacket = packet!
            pair2SecondInitiatorCallback.fulfill()
        }

        self.wait(for: [pair2SecondInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2InitiatorSecondPacket, "pair2InitiatorSecondPacket should not be nil")

        /* PAIR-2 ACCEPTOR SECOND RTT */
        var pair2AcceptorSecondPacket = Data()
        let pair2SecondAcceptorCallback = self.expectation(description: "pair2SecondAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorSecondPacket = packet!
            pair2SecondAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2SecondAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorSecondPacket, "acceptor second packet should not be nil")

        /* PAIR-2 INITIATOR THIRD STEP*/
        var pair2InitiatorThirdPacket: Data?
        let pair2ThirdInitiatorCallback = self.expectation(description: "thirdInitiatorCallback callback occurs")

        initiator2.exchangePacket(pair2AcceptorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorThirdPacket = (packet)
            pair2ThirdInitiatorCallback.fulfill()
        }
        self.wait(for: [pair2ThirdInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2InitiatorThirdPacket, "acceptor second packet should not be nil")

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func test2ClientsInterlacedOctagonAndSOS() {
        OctagonSetPlatformSupportsSOS(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator2ContextID = "initiatorContext2"
        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        let initiator2Context = self.manager.context(forContainerName: OTCKContainerName,
                                                     contextID: initiator2ContextID,
                                                     sosAdapter: self.mockSOSAdapter,
                                                     authKitAdapter: self.mockAuthKit2,
                                                     lockStateTracker: self.lockStateTracker,
                                                     accountStateTracker: self.accountStateTracker,
                                                     deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-initiator-2", serialNumber: "456", osVersion: "iOS (fake version)"))

        initiator1Context.startOctagonStateMachine()
        initiator2Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: initiator2Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-1")
        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let clientStateMachine2 = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: "initiator-2")

        let (acceptor2, initiator2) = self.setupPairingEndpoints(withPairNumber: "2", initiatorContextID: initiator2ContextID, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: "initiator-2", acceptorUniqueID: "acceptor-1")

        XCTAssertNotNil(acceptor2, "acceptor should not be nil")
        XCTAssertNotNil(initiator2, "initiator should not be nil")

        var signInCallback = self.expectation(description: "trigger sign in for initiator one")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* PAIR-1 INITIATOR FIRST RTT JOINING MESSAGE*/
        var initiatorFirstPacket = Data()
        let firstInitiatorCallback = self.expectation(description: "firstInitiatorCallback callback occurs")

        initiator.exchangePacket(nil) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorFirstPacket = packet!
            firstInitiatorCallback.fulfill()
        }

        self.wait(for: [firstInitiatorCallback], timeout: 10)

        /* PAIR-1 ACCEPTOR FIRST RTT EPOCH*/
        var acceptorFirstPacket = Data()
        let firstAcceptorCallback = self.expectation(description: "firstAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorFirstPacket = packet!
            firstAcceptorCallback.fulfill()
        }
        self.wait(for: [firstAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorFirstPacket, "first packet should not be nil")

        signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: initiator2ContextID) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* PAIR-2 INITIATOR FIRST RTT JOINING MESSAGE*/
        var pair2InitiatorFirstPacket = Data()
        let pair2FirstInitiatorCallback = self.expectation(description: "firstInitiatorCallback callback occurs")

        initiator2.exchangePacket(nil) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorFirstPacket = packet!
            pair2FirstInitiatorCallback.fulfill()
        }

        self.wait(for: [pair2FirstInitiatorCallback], timeout: 10)

        /* PAIR-1 INITIATOR SECOND RTT PREPARE*/
        var initiatorSecondPacket = Data()
        let secondInitiatorCallback = self.expectation(description: "secondInitiatorCallback callback occurs")

        initiator.exchangePacket(acceptorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorSecondPacket = packet!
            secondInitiatorCallback.fulfill()
        }

        self.wait(for: [secondInitiatorCallback], timeout: 10)
        XCTAssertNotNil(initiatorSecondPacket, "initiator second packet should not be nil")

        /* PAIR-1 ACCEPTOR SECOND RTT */
        var acceptorSecondPacket = Data()
        let SecondAcceptorCallback = self.expectation(description: "SecondAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorSecondPacket = packet!
            SecondAcceptorCallback.fulfill()
        }
        self.wait(for: [SecondAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorSecondPacket, "acceptor second packet should not be nil")

        /* PAIR-2 ACCEPTOR FIRST RTT EPOCH*/
        var pair2AcceptorFirstPacket = Data()
        let pair2FirstAcceptorCallback = self.expectation(description: "pair2FirstAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorFirstPacket = packet!
            pair2FirstAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2FirstAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorFirstPacket, "first packet should not be nil")

        /* PAIR-1 INITIATOR THIRD STEP*/
        var initiatorThirdPacket: Data?
        let thirdInitiatorCallback = self.expectation(description: "thirdInitiatorCallback callback occurs")

        initiator.exchangePacket(acceptorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorThirdPacket = (packet)
            thirdInitiatorCallback.fulfill()
        }
        self.wait(for: [thirdInitiatorCallback], timeout: 10)
        XCTAssertNotNil(initiatorThirdPacket, "acceptor second packet should not be nil")

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        /* PAIR-2 INITIATOR SECOND RTT PREPARE*/
        var pair2InitiatorSecondPacket = Data()
        let pair2SecondInitiatorCallback = self.expectation(description: "pair2SecondInitiatorCallback callback occurs")

        initiator2.exchangePacket(pair2AcceptorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorSecondPacket = packet!
            pair2SecondInitiatorCallback.fulfill()
        }

        self.wait(for: [pair2SecondInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2InitiatorSecondPacket, "pair2InitiatorSecondPacket should not be nil")

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.notifyContainerChange()

        let initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            initiatorDumpCallback.fulfill()
        }
        self.wait(for: [initiatorDumpCallback], timeout: 10)

        let acceptorDumpCallback = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.contextForAcceptor) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            acceptorDumpCallback.fulfill()
        }
        self.wait(for: [acceptorDumpCallback], timeout: 10)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")

        /* PAIR-2 ACCEPTOR SECOND RTT */
        var pair2AcceptorSecondPacket = Data()
        let pair2SecondAcceptorCallback = self.expectation(description: "pair2SecondAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorSecondPacket = packet!
            pair2SecondAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2SecondAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorSecondPacket, "acceptor second packet should not be nil")

        /* PAIR-2 INITIATOR THIRD STEP*/
        var pair2InitiatorThirdPacket: Data?
        let pair2ThirdInitiatorCallback = self.expectation(description: "thirdInitiatorCallback callback occurs")

        initiator2.exchangePacket(pair2AcceptorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorThirdPacket = (packet)
            pair2ThirdInitiatorCallback.fulfill()
        }
        self.wait(for: [pair2ThirdInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2InitiatorThirdPacket, "acceptor second packet should not be nil")

        clientStateMachine2.notifyContainerChange()
        clientStateMachine.notifyContainerChange()

        let pair2InitiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            pair2InitiatorDumpCallback.fulfill()
        }
        self.wait(for: [pair2InitiatorDumpCallback], timeout: 10)

        let pair2AcceptorDumpCallback = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.contextForAcceptor) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 3, "should be 3 peer ids")
            pair2AcceptorDumpCallback.fulfill()
        }
        self.wait(for: [pair2AcceptorDumpCallback], timeout: 10)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 3, "should be 3 bottles")

        let initiatorStatus = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.trustStatus(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { egoStatus, error in
            XCTAssertEqual(egoStatus.egoStatus.rawValue & TPPeerStatus.partiallyReciprocated.rawValue, TPPeerStatus.partiallyReciprocated.rawValue, "initiator should be partially accepted")
            XCTAssertNotNil(egoStatus.egoPeerID, "should have an identity")
            XCTAssertEqual(egoStatus.numberOfPeersInOctagon, 2, "should have 2 peers")
            XCTAssertFalse(egoStatus.isExcluded, "should not be excluded")
            XCTAssertFalse(egoStatus.isLocked, "should not be locked")
            XCTAssertNil(error, "error should be nil")
            initiatorStatus.fulfill()
        }
        self.wait(for: [initiatorStatus], timeout: 10)

        let acceptorStatus = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.trustStatus(withContainer: self.cuttlefishContext.containerName, context: self.contextForAcceptor) {egoStatus, error in
            XCTAssertEqual(egoStatus.egoStatus.rawValue & TPPeerStatus.partiallyReciprocated.rawValue, TPPeerStatus.partiallyReciprocated.rawValue, "acceptor should be partially accepted")
            XCTAssertNotNil(egoStatus.egoPeerID, "should have an identity")
            XCTAssertEqual(egoStatus.numberOfPeersInOctagon, 3, "should have 2 peers")
            XCTAssertFalse(egoStatus.isExcluded, "should not be excluded")
            XCTAssertFalse(egoStatus.isLocked, "should not be locked")
            XCTAssertNil(error, "error should be nil")
            acceptorStatus.fulfill()
        }
        self.wait(for: [acceptorStatus], timeout: 10)

        self.verifyDatabaseMocks()

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func test2ClientsInterlacedOctagonOnly() {
        OctagonSetPlatformSupportsSOS(false)
        OctagonSetIsEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator2ContextID = "initiatorContext2"
        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        let initiator2Context = self.manager.context(forContainerName: OTCKContainerName,
                                                     contextID: initiator2ContextID,
                                                     sosAdapter: self.mockSOSAdapter,
                                                     authKitAdapter: self.mockAuthKit2,
                                                     lockStateTracker: self.lockStateTracker,
                                                     accountStateTracker: self.accountStateTracker,
                                                     deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-initiator-2", serialNumber: "456", osVersion: "iOS (fake version)"))

        initiator1Context.startOctagonStateMachine()
        initiator2Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: initiator2Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-1")
        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let clientStateMachine2 = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: "initiator-2")

        let (acceptor2, initiator2) = self.setupPairingEndpoints(withPairNumber: "2", initiatorContextID: initiator2ContextID, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: "initiator-2", acceptorUniqueID: "acceptor-1")

        XCTAssertNotNil(acceptor2, "acceptor should not be nil")
        XCTAssertNotNil(initiator2, "initiator should not be nil")

        var signInCallback = self.expectation(description: "trigger sign in for initiator one")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* PAIR-1 INITIATOR FIRST RTT JOINING MESSAGE*/
        var initiatorFirstPacket = Data()
        let firstInitiatorCallback = self.expectation(description: "firstInitiatorCallback callback occurs")

        initiator.exchangePacket(nil) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorFirstPacket = packet!
            firstInitiatorCallback.fulfill()
        }

        self.wait(for: [firstInitiatorCallback], timeout: 10)

        /* PAIR-1 ACCEPTOR FIRST RTT EPOCH*/
        var acceptorFirstPacket = Data()
        let firstAcceptorCallback = self.expectation(description: "firstAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorFirstPacket = packet!
            firstAcceptorCallback.fulfill()
        }
        self.wait(for: [firstAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorFirstPacket, "first packet should not be nil")

        signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: initiator2ContextID) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* PAIR-2 INITIATOR FIRST RTT JOINING MESSAGE*/
        var pair2InitiatorFirstPacket = Data()
        let pair2FirstInitiatorCallback = self.expectation(description: "firstInitiatorCallback callback occurs")

        initiator2.exchangePacket(nil) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorFirstPacket = packet!
            pair2FirstInitiatorCallback.fulfill()
        }

        self.wait(for: [pair2FirstInitiatorCallback], timeout: 10)

        /* PAIR-1 INITIATOR SECOND RTT PREPARE*/
        var initiatorSecondPacket = Data()
        let secondInitiatorCallback = self.expectation(description: "secondInitiatorCallback callback occurs")

        initiator.exchangePacket(acceptorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorSecondPacket = packet!
            secondInitiatorCallback.fulfill()
        }

        self.wait(for: [secondInitiatorCallback], timeout: 10)
        XCTAssertNotNil(initiatorSecondPacket, "initiator second packet should not be nil")

        /* PAIR-1 ACCEPTOR SECOND RTT */
        var acceptorSecondPacket = Data()
        let SecondAcceptorCallback = self.expectation(description: "SecondAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorSecondPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorSecondPacket = packet!
            SecondAcceptorCallback.fulfill()
        }
        self.wait(for: [SecondAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorSecondPacket, "acceptor second packet should not be nil")

        /* PAIR-2 ACCEPTOR FIRST RTT EPOCH*/
        var pair2AcceptorFirstPacket = Data()
        let pair2FirstAcceptorCallback = self.expectation(description: "pair2FirstAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorFirstPacket = packet!
            pair2FirstAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2FirstAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorFirstPacket, "first packet should not be nil")

        /* PAIR-1 INITIATOR THIRD STEP*/
        var initiatorThirdPacket: Data?
        let thirdInitiatorCallback = self.expectation(description: "thirdInitiatorCallback callback occurs")

        initiator.exchangePacket(acceptorSecondPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorThirdPacket = (packet)
            thirdInitiatorCallback.fulfill()
        }
        self.wait(for: [thirdInitiatorCallback], timeout: 10)
        XCTAssertNil(initiatorThirdPacket, "acceptor second packet should be nil")

        /* PAIR-2 INITIATOR SECOND RTT PREPARE*/
        var pair2InitiatorSecondPacket = Data()
        let pair2SecondInitiatorCallback = self.expectation(description: "pair2SecondInitiatorCallback callback occurs")

        initiator2.exchangePacket(pair2AcceptorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorSecondPacket = packet!
            pair2SecondInitiatorCallback.fulfill()
        }

        self.wait(for: [pair2SecondInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2InitiatorSecondPacket, "pair2InitiatorSecondPacket should not be nil")

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.notifyContainerChange()

        let initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            initiatorDumpCallback.fulfill()
        }
        self.wait(for: [initiatorDumpCallback], timeout: 10)

        let acceptorDumpCallback = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.contextForAcceptor) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            acceptorDumpCallback.fulfill()
        }
        self.wait(for: [acceptorDumpCallback], timeout: 10)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")

        /* PAIR-2 ACCEPTOR SECOND RTT */
        var pair2AcceptorSecondPacket = Data()
        let pair2SecondAcceptorCallback = self.expectation(description: "pair2SecondAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorSecondPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorSecondPacket = packet!
            pair2SecondAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2SecondAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorSecondPacket, "acceptor second packet should not be nil")

        /* PAIR-2 INITIATOR THIRD STEP*/
        var pair2InitiatorThirdPacket: Data?
        let pair2ThirdInitiatorCallback = self.expectation(description: "thirdInitiatorCallback callback occurs")

        initiator2.exchangePacket(pair2AcceptorSecondPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorThirdPacket = (packet)
            pair2ThirdInitiatorCallback.fulfill()
        }
        self.wait(for: [pair2ThirdInitiatorCallback], timeout: 10)
        XCTAssertNil(pair2InitiatorThirdPacket, "acceptor second packet should be nil")

        clientStateMachine2.notifyContainerChange()

        clientStateMachine.notifyContainerChange()

        let pair2InitiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            pair2InitiatorDumpCallback.fulfill()
        }
        self.wait(for: [pair2InitiatorDumpCallback], timeout: 10)

        let pair2AcceptorDumpCallback = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.contextForAcceptor) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 3, "should be 3 peer ids")
            pair2AcceptorDumpCallback.fulfill()
        }
        self.wait(for: [pair2AcceptorDumpCallback], timeout: 10)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 3, "should be 3 bottles")

        let initiatorStatus = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.trustStatus(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) {egoStatus, error in
            XCTAssertEqual(egoStatus.egoStatus.rawValue & TPPeerStatus.partiallyReciprocated.rawValue, TPPeerStatus.partiallyReciprocated.rawValue, "initiator should be partially accepted")
            XCTAssertNotNil(egoStatus.egoPeerID, "should have an identity")
            XCTAssertEqual(egoStatus.numberOfPeersInOctagon, 2, "should be 2 peers")
            XCTAssertFalse(egoStatus.isExcluded, "should not be excluded")

            XCTAssertNil(error, "error should be nil")
            initiatorStatus.fulfill()
        }
        self.wait(for: [initiatorStatus], timeout: 10)

        let acceptorStatus = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.trustStatus(withContainer: self.cuttlefishContext.containerName, context: self.contextForAcceptor) {egoStatus, error in
            XCTAssertEqual(egoStatus.egoStatus.rawValue & TPPeerStatus.partiallyReciprocated.rawValue, TPPeerStatus.partiallyReciprocated.rawValue, "acceptor should be partially accepted")
            XCTAssertNotNil(egoStatus.egoPeerID, "should have an identity")
            XCTAssertEqual(egoStatus.numberOfPeersInOctagon, 3, "should be 3 peers")
            XCTAssertFalse(egoStatus.isExcluded, "should not be excluded")
            XCTAssertNil(error, "error should be nil")
            acceptorStatus.fulfill()
        }
        self.wait(for: [acceptorStatus], timeout: 10)

        self.verifyDatabaseMocks()

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func test2ClientsInterlacedSOSOnly() {
        OctagonSetPlatformSupportsSOS(true)
        OctagonSetIsEnabled(false)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator2ContextID = "initiatorContext2"
        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        let initiator2Context = self.manager.context(forContainerName: OTCKContainerName,
                                                     contextID: initiator2ContextID,
                                                     sosAdapter: self.mockSOSAdapter,
                                                     authKitAdapter: self.mockAuthKit2,
                                                     lockStateTracker: self.lockStateTracker,
                                                     accountStateTracker: self.accountStateTracker,
                                                     deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-initiator-2", serialNumber: "456", osVersion: "iOS (fake version)"))

        initiator1Context.startOctagonStateMachine()
        initiator2Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: initiator2Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-1")
        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let (acceptor2, initiator2) = self.setupPairingEndpoints(withPairNumber: "2", initiatorContextID: initiator2ContextID, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: "initiator-2", acceptorUniqueID: "acceptor-1")

        XCTAssertNotNil(acceptor2, "acceptor should not be nil")
        XCTAssertNotNil(initiator2, "initiator should not be nil")

        var signInCallback = self.expectation(description: "trigger sign in for initiator one")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* PAIR-1 INITIATOR FIRST RTT JOINING MESSAGE*/
        var initiatorFirstPacket = Data()
        let firstInitiatorCallback = self.expectation(description: "firstInitiatorCallback callback occurs")

        initiator.exchangePacket(nil) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorFirstPacket = packet!
            firstInitiatorCallback.fulfill()
        }

        self.wait(for: [firstInitiatorCallback], timeout: 10)

        /* PAIR-1 ACCEPTOR FIRST RTT EPOCH*/
        var acceptorFirstPacket = Data()
        let firstAcceptorCallback = self.expectation(description: "firstAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorFirstPacket = packet!
            firstAcceptorCallback.fulfill()
        }
        self.wait(for: [firstAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorFirstPacket, "first packet should not be nil")

        signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: initiator2ContextID) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* PAIR-2 INITIATOR FIRST RTT JOINING MESSAGE*/
        var pair2InitiatorFirstPacket = Data()
        let pair2FirstInitiatorCallback = self.expectation(description: "firstInitiatorCallback callback occurs")

        initiator2.exchangePacket(nil) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorFirstPacket = packet!
            pair2FirstInitiatorCallback.fulfill()
        }

        self.wait(for: [pair2FirstInitiatorCallback], timeout: 10)

        /* PAIR-1 INITIATOR SECOND RTT PREPARE*/
        var initiatorSecondPacket = Data()
        let secondInitiatorCallback = self.expectation(description: "secondInitiatorCallback callback occurs")

        initiator.exchangePacket(acceptorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorSecondPacket = packet!
            secondInitiatorCallback.fulfill()
        }

        self.wait(for: [secondInitiatorCallback], timeout: 10)
        XCTAssertNotNil(initiatorSecondPacket, "initiator second packet should not be nil")

        /* PAIR-1 ACCEPTOR SECOND RTT */
        var acceptorSecondPacket = Data()
        let SecondAcceptorCallback = self.expectation(description: "SecondAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorSecondPacket = packet!
            SecondAcceptorCallback.fulfill()
        }
        self.wait(for: [SecondAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorSecondPacket, "acceptor second packet should not be nil")

        /* PAIR-2 ACCEPTOR FIRST RTT EPOCH*/
        var pair2AcceptorFirstPacket = Data()
        let pair2FirstAcceptorCallback = self.expectation(description: "pair2FirstAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorFirstPacket = packet!
            pair2FirstAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2FirstAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorFirstPacket, "first packet should not be nil")

        /* PAIR-1 INITIATOR THIRD STEP*/
        var initiatorThirdPacket: Data?
        let thirdInitiatorCallback = self.expectation(description: "thirdInitiatorCallback callback occurs")

        initiator.exchangePacket(acceptorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorThirdPacket = (packet)
            thirdInitiatorCallback.fulfill()
        }
        self.wait(for: [thirdInitiatorCallback], timeout: 10)
        XCTAssertNotNil(initiatorThirdPacket, "acceptor second packet should not be nil")

        /* PAIR-2 INITIATOR SECOND RTT PREPARE*/
        var pair2InitiatorSecondPacket = Data()
        let pair2SecondInitiatorCallback = self.expectation(description: "pair2SecondInitiatorCallback callback occurs")

        initiator2.exchangePacket(pair2AcceptorFirstPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorSecondPacket = packet!
            pair2SecondInitiatorCallback.fulfill()
        }

        self.wait(for: [pair2SecondInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2InitiatorSecondPacket, "pair2InitiatorSecondPacket should not be nil")

        /* PAIR-2 ACCEPTOR SECOND RTT */
        var pair2AcceptorSecondPacket = Data()
        let pair2SecondAcceptorCallback = self.expectation(description: "pair2SecondAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorSecondPacket = packet!
            pair2SecondAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2SecondAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorSecondPacket, "acceptor second packet should not be nil")

        /* PAIR-2 INITIATOR THIRD STEP*/
        var pair2InitiatorThirdPacket: Data?
        let pair2ThirdInitiatorCallback = self.expectation(description: "thirdInitiatorCallback callback occurs")

        initiator2.exchangePacket(pair2AcceptorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorThirdPacket = (packet)
            pair2ThirdInitiatorCallback.fulfill()
        }
        self.wait(for: [pair2ThirdInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2InitiatorThirdPacket, "acceptor second packet should not be nil")

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }
}

#endif /* OCTAGON */
