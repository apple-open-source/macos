#if OCTAGON

extension OctagonPairingTests {
    func test2ClientsBothOctagonAndSOS() throws {
        OctagonSetSOSFeatureEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator2ContextID = "initiatorContext2"
        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        let initiator2Context = self.manager.context(forContainerName: OTCKContainerName,
                                                     contextID: initiator2ContextID,
                                                     sosAdapter: self.mockSOSAdapter!,
                                                     accountsAdapter: self.mockAuthKit2,
                                                     authKitAdapter: self.mockAuthKit2,
                                                     tooManyPeersAdapter: self.mockTooManyPeers,
                                                     tapToRadarAdapter: self.mockTapToRadar,
                                                     lockStateTracker: self.lockStateTracker,
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
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
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
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorThirdPacket = packet!
            ThirdAcceptorCallback.fulfill()
        }
        self.wait(for: [ThirdAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorThirdPacket, "acceptor third packet should not be nil")

        /* PAIR-1 INITIATOR FOURTH STEP*/
        let fourthInitiatorCallback = self.expectation(description: "fourthInitiatorCallback callback occurs")

        var initiatorFourthPacket = Data()
        initiator.exchangePacket(acceptorThirdPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorFourthPacket = packet!
            fourthInitiatorCallback.fulfill()
        }

        self.wait(for: [fourthInitiatorCallback], timeout: 10)
        XCTAssertNotNil(initiatorFourthPacket, "initiator fourth packet should not be nil")

        var acceptorFourthPacket = Data()
        let FourthAcceptorCallback = self.expectation(description: "FourthAcceptorCallback callback occurs")

        acceptor.setDSIDForTest("123456")
        acceptor.exchangePacket(initiatorFourthPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorFourthPacket = packet!
            FourthAcceptorCallback.fulfill()
        }
        self.wait(for: [FourthAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorFourthPacket, "acceptor fourth packet should not be nil")

        let fifthInitiatorCallback = self.expectation(description: "fifthInitiatorCallback callback occurs")
        var initiatorFifthPacket = Data()

        initiator.exchangePacket(acceptorFourthPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorFifthPacket = packet!
            fifthInitiatorCallback.fulfill()
        }
        self.wait(for: [fifthInitiatorCallback], timeout: 10)
        XCTAssertNotNil(initiatorFifthPacket, "initiator fifth packet should not be nil")

        let FifthAcceptorCallback = self.expectation(description: "FifthAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorFifthPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should be nil")
            XCTAssertNil(error, "error should be nil")
            FifthAcceptorCallback.fulfill()
        }
        self.wait(for: [FifthAcceptorCallback], timeout: 10)

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        try self.forceFetch(context: self.cuttlefishContextForAcceptor)

        let initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
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
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContextForAcceptor.activeAccount)) { dump, _ in
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
        self.otControl.appleAccountSigned(in: OTControlArguments(containerName: OTCKContainerName, contextID: initiator2ContextID, altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
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
        let pair2ThirdAcceptorCallback = self.expectation(description: "pair2ThirdAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorThirdPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorThirdPacket = packet!
            pair2ThirdAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2ThirdAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorThirdPacket, "acceptor third packet should not be nil")

        /* PAIR-2 INITIATOR FOURTH STEP*/
        let pair2FourthInitiatorCallback = self.expectation(description: "pair2FourthInitiatorCallback callback occurs")
        var pair2InitiatorFourthPacket = Data()

        initiator2.exchangePacket(pair2AcceptorThirdPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorFourthPacket = packet!
            pair2FourthInitiatorCallback.fulfill()
        }

        self.wait(for: [pair2FourthInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2InitiatorFourthPacket, "initiator fourth packet should not be nil")

        var pair2AcceptorFourthPacket = Data()
        let pair2FourthAcceptorCallback = self.expectation(description: "pair2FourthAcceptorCallback callback occurs")

        acceptor2.setDSIDForTest("123456")
        acceptor2.exchangePacket(pair2InitiatorFourthPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorFourthPacket = packet!
            pair2FourthAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2FourthAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorFourthPacket, "acceptor fourth packet should not be nil")

        /* PAIR-2 INITIATOR FOURTH STEP*/
        let pair2FifthInitiatorCallback = self.expectation(description: "pair2FifthInitiatorCallback callback occurs")
        var pair2InitiatorFifthPacket = Data()

        initiator2.exchangePacket(pair2AcceptorFourthPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorFifthPacket = packet!
            pair2FifthInitiatorCallback.fulfill()
        }

        self.wait(for: [pair2FifthInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2InitiatorFifthPacket, "initiator fifth packet should not be nil")

        let pair2AcceptorFifthCallback = self.expectation(description: "pair2AcceptorFifthCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorFifthPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorFifthCallback.fulfill()
        }
        self.wait(for: [pair2AcceptorFifthCallback], timeout: 10)


        try self.forceFetch(context: self.cuttlefishContextForAcceptor)

        XCTAssertNil(initiator2Context.pairingUUID, "pairingUUID should be nil")
        XCTAssertNil(initiator1Context.pairingUUID, "pairingUUID should be nil")

        let pair2InitiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
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
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContextForAcceptor.activeAccount)) { dump, _ in
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

    func test2ClientsOctagonOnly() throws {
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator2ContextID = "initiatorContext2"
        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        let initiator2Context = self.manager.context(forContainerName: OTCKContainerName,
                                                     contextID: initiator2ContextID,
                                                     sosAdapter: self.mockSOSAdapter!,
                                                     accountsAdapter: self.mockAuthKit2,
                                                     authKitAdapter: self.mockAuthKit2,
                                                     tooManyPeersAdapter: self.mockTooManyPeers,
                                                     tapToRadarAdapter: self.mockTapToRadar,
                                                     lockStateTracker: self.lockStateTracker,
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
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
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
            XCTAssertNotNil(packet, "packet should be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorSecondPacket = packet!
            SecondAcceptorCallback.fulfill()
        }
        self.wait(for: [SecondAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorSecondPacket, "acceptor second packet should not be nil")

        /* PAIR-1 INITIATOR THIRD STEP*/
        let thirdInitiatorCallback = self.expectation(description: "thirdInitiatorCallback callback occurs")
        var initiatorThirdPacket = Data()

        initiator.exchangePacket(acceptorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorThirdPacket = packet!
            thirdInitiatorCallback.fulfill()
        }
        self.wait(for: [thirdInitiatorCallback], timeout: 10)

        var acceptorThirdPacket = Data()
        let ThirdAcceptorCallback = self.expectation(description: "ThirdAcceptorCallback callback occurs")

        acceptor.setDSIDForTest("123456")
        acceptor.exchangePacket(initiatorThirdPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorThirdPacket = packet!
            ThirdAcceptorCallback.fulfill()
        }
        self.wait(for: [ThirdAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorThirdPacket, "acceptor third packet should not be nil")

        let fourthInitiatorCallback = self.expectation(description: "fourthInitiatorCallback callback occurs")
        var initiatorFourthPacket = Data()

        initiator.exchangePacket(acceptorThirdPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorFourthPacket = packet!
            fourthInitiatorCallback.fulfill()
        }
        self.wait(for: [fourthInitiatorCallback], timeout: 10)
        XCTAssertNotNil(initiatorFourthPacket, "acceptor fourth packet should not be nil")

        let FourthAcceptorCallback = self.expectation(description: "FourthAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorFourthPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should be nil")
            XCTAssertNil(error, "error should be nil")
            FourthAcceptorCallback.fulfill()
        }
        self.wait(for: [FourthAcceptorCallback], timeout: 10)


        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        try self.forceFetch(context: self.cuttlefishContextForAcceptor)

        let initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
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
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContextForAcceptor.activeAccount)) { dump, _ in
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
        self.otControl.appleAccountSigned(in: OTControlArguments(containerName: OTCKContainerName, contextID: initiator2ContextID, altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
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
        let pair2ThirdInitiatorCallback = self.expectation(description: "thirdInitiatorCallback callback occurs")
        var pair2InitiatorThirdPacket = Data()

        initiator2.exchangePacket(pair2AcceptorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorThirdPacket = packet!
            pair2ThirdInitiatorCallback.fulfill()
        }
        self.wait(for: [pair2ThirdInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2ThirdInitiatorCallback, "initiator third packet should not be nil")

        var pair2AcceptorThirdPacket = Data()
        let pair2ThirdAcceptorCallback = self.expectation(description: "pair2ThirdAcceptorCallback callback occurs")

        acceptor2.setDSIDForTest("123456")
        acceptor2.exchangePacket(pair2InitiatorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorThirdPacket = packet!
            pair2ThirdAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2ThirdAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorThirdPacket, "acceptor third packet should not be nil")

        let pair2FourthInitiatorCallback = self.expectation(description: "fourthInitiatorCallback callback occurs")
        var pair2InitiatorFourthPacket = Data()

        initiator2.exchangePacket(pair2AcceptorThirdPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorFourthPacket = packet!
            pair2FourthInitiatorCallback.fulfill()
        }
        self.wait(for: [pair2FourthInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2InitiatorFourthPacket, "initiator fourth packet should not be nil")

        let pair2FourthAcceptorCallback = self.expectation(description: "pair2FourthAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorFourthPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should be nil")
            XCTAssertNil(error, "error should be nil")
            pair2FourthAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2FourthAcceptorCallback], timeout: 10)

        XCTAssertNil(initiator2Context.pairingUUID, "pairingUUID should be nil")
        XCTAssertNil(initiator1Context.pairingUUID, "pairingUUID should be nil")

        try self.forceFetch(context: self.cuttlefishContextForAcceptor)

        let pair2InitiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
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
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContextForAcceptor.activeAccount)) { dump, _ in
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

    func test2ClientsInterlacedOctagonAndSOS() throws {
        OctagonSetSOSFeatureEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator2ContextID = "initiatorContext2"
        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        let initiator2Context = self.manager.context(forContainerName: OTCKContainerName,
                                                     contextID: initiator2ContextID,
                                                     sosAdapter: self.mockSOSAdapter!,
                                                     accountsAdapter: self.mockAuthKit2,
                                                     authKitAdapter: self.mockAuthKit2,
                                                     tooManyPeersAdapter: self.mockTooManyPeers,
                                                     tapToRadarAdapter: self.mockTapToRadar,
                                                     lockStateTracker: self.lockStateTracker,
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
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
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
        self.otControl.appleAccountSigned(in: OTControlArguments(containerName: OTCKContainerName, contextID: initiator2ContextID, altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
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

        try self.forceFetch(context: self.cuttlefishContextForAcceptor)

        let initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
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
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContextForAcceptor.activeAccount)) { dump, _ in
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

        try self.forceFetch(context: self.cuttlefishContextForAcceptor)

        XCTAssertNil(initiator2Context.pairingUUID, "pairingUUID should be nil")
        XCTAssertNil(initiator1Context.pairingUUID, "pairingUUID should be nil")

        let pair2InitiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
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
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContextForAcceptor.activeAccount)) { dump, _ in
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
        self.tphClient.trustStatus(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { egoStatus, error in
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
        self.tphClient.trustStatus(with: try XCTUnwrap(self.cuttlefishContextForAcceptor.activeAccount)) {egoStatus, error in
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

    func test2ClientsInterlacedOctagonOnly() throws {
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        SecCKKSSetTestSkipTLKShareHealing(true)

        self.getAcceptorInCircle()

        let initiator2ContextID = "initiatorContext2"
        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        let initiator2Context = self.manager.context(forContainerName: OTCKContainerName,
                                                     contextID: initiator2ContextID,
                                                     sosAdapter: self.mockSOSAdapter!,
                                                     accountsAdapter: self.mockAuthKit2,
                                                     authKitAdapter: self.mockAuthKit2,
                                                     tooManyPeersAdapter: self.mockTooManyPeers,
                                                     tapToRadarAdapter: self.mockTapToRadar,
                                                     lockStateTracker: self.lockStateTracker,
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
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
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
        self.otControl.appleAccountSigned(in: OTControlArguments(containerName: OTCKContainerName, contextID: initiator2ContextID, altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
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


        var acceptorThirdPacket = Data()
        let ThirdAcceptorCallback = self.expectation(description: "ThirdAcceptorCallback callback occurs")

        acceptor.setDSIDForTest("123456")
        acceptor.exchangePacket(initiatorThirdPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            acceptorThirdPacket = packet!
            ThirdAcceptorCallback.fulfill()
        }
        self.wait(for: [ThirdAcceptorCallback], timeout: 10)
        XCTAssertNotNil(acceptorThirdPacket, "third packet should not be nil")

        var initiatorFourthPacket: Data?
        let fourthInitiatorCallback = self.expectation(description: "fourthInitiatorCallback callback occurs")

        initiator.exchangePacket(acceptorThirdPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            initiatorFourthPacket = (packet)
            fourthInitiatorCallback.fulfill()
        }
        self.wait(for: [fourthInitiatorCallback], timeout: 10)
        XCTAssertNotNil(initiatorFourthPacket, "initiator fourth packet should not be nil")

        let FourthAcceptorCallback = self.expectation(description: "FourthAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorFourthPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should be nil")
            XCTAssertNil(error, "error should be nil")
            FourthAcceptorCallback.fulfill()
        }
        self.wait(for: [FourthAcceptorCallback], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContextForAcceptor)
        self.sendContainerChangeWaitForUntrustedFetch(context: initiator2Context)

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

        try self.forceFetch(context: self.cuttlefishContextForAcceptor)

        XCTAssertNil(initiator1Context.pairingUUID, "pairingUUID should be nil")

        let initiator1DumpCallback = self.expectation(description: "initiator1DumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(initiator1Context.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            initiator1DumpCallback.fulfill()
        }
        self.wait(for: [initiator1DumpCallback], timeout: 10)

        let acceptorDumpCallback = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContextForAcceptor.activeAccount)) { dump, _ in
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
        var pair2AcceptorThirdPacket = Data()
        let pair2ThirdAcceptorCallback = self.expectation(description: "pair2ThirdAcceptorCallback callback occurs")

        acceptor2.exchangePacket(pair2InitiatorSecondPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorThirdPacket = packet!
            pair2ThirdAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2ThirdAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorThirdPacket, "acceptor third packet should not be nil")

        /* PAIR-2 INITIATOR THIRD STEP*/
        var pair2InitiatorThirdPacket: Data?
        let pair2ThirdInitiatorCallback = self.expectation(description: "thirdInitiatorCallback callback occurs")

        initiator2.exchangePacket(pair2AcceptorThirdPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorThirdPacket = packet!
            pair2ThirdInitiatorCallback.fulfill()
        }
        self.wait(for: [pair2ThirdInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2InitiatorThirdPacket, "initiator third packet should not be nil")

        var pair2AcceptorFourthPacket = Data()
        let pair2FourthAcceptorCallback = self.expectation(description: "pair2FourthAcceptorCallback callback occurs")

        acceptor2.setDSIDForTest("123456")
        acceptor2.exchangePacket(pair2InitiatorThirdPacket) { complete, packet, error in
            XCTAssertFalse(complete, "should be false")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2AcceptorFourthPacket = packet!
            pair2FourthAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2FourthAcceptorCallback], timeout: 10)
        XCTAssertNotNil(pair2AcceptorFourthPacket, "acceptor fourth packet should not be nil")

        /* PAIR-2 INITIATOR THIRD STEP*/
        var pair2InitiatorFourthPacket: Data?
        let pair2FourthInitiatorCallback = self.expectation(description: "pair2FourthInitiatorCallback callback occurs")

        initiator2.exchangePacket(pair2AcceptorFourthPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNotNil(packet, "packet should not be nil")
            XCTAssertNil(error, "error should be nil")
            pair2InitiatorFourthPacket = packet!
            pair2FourthInitiatorCallback.fulfill()
        }
        self.wait(for: [pair2FourthInitiatorCallback], timeout: 10)
        XCTAssertNotNil(pair2InitiatorFourthPacket, "initiator fourth packet should not be nil")


        let pair2FifthAcceptorCallback = self.expectation(description: "pair2FifthAcceptorCallback callback occurs")
        acceptor2.exchangePacket(pair2InitiatorThirdPacket) { complete, packet, error in
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should be nil")
            XCTAssertNil(error, "error should be nil")
            pair2FifthAcceptorCallback.fulfill()
        }
        self.wait(for: [pair2FifthAcceptorCallback], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContextForAcceptor)
        self.sendContainerChangeWaitForFetch(context: initiator2Context)
        self.sendContainerChangeWaitForFetch(context: initiator1Context)

        let pair2InitiatorDumpCallback = self.expectation(description: "initiator2DumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(initiator2Context.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 3, "should be 3 peer ids")

            pair2InitiatorDumpCallback.fulfill()
        }
        self.wait(for: [pair2InitiatorDumpCallback], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContextForAcceptor)
        self.sendContainerChangeWaitForFetch(context: initiator2Context)
        self.sendContainerChangeWaitForFetch(context: initiator1Context)

        let pair2AcceptorDumpCallback = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContextForAcceptor.activeAccount)) { dump, _ in
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

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContextForAcceptor)
        self.sendContainerChangeWaitForFetch(context: initiator2Context)
        self.sendContainerChangeWaitForFetch(context: initiator1Context)

        let initiator1Status = self.expectation(description: "initiator1 callback occurs")
        self.tphClient.trustStatus(with: try XCTUnwrap(initiator1Context.activeAccount)) {egoStatus, error in
            XCTAssertEqual(egoStatus.egoStatus.rawValue & TPPeerStatus.partiallyReciprocated.rawValue, TPPeerStatus.partiallyReciprocated.rawValue, "initiator should be partially accepted")
            XCTAssertNotNil(egoStatus.egoPeerID, "should have an identity")
            XCTAssertEqual(egoStatus.numberOfPeersInOctagon, 3, "should be 3 peers")
            XCTAssertFalse(egoStatus.isExcluded, "should not be excluded")

            XCTAssertNil(error, "error should be nil")
            initiator1Status.fulfill()
        }
        self.wait(for: [initiator1Status], timeout: 10)

        let acceptorStatus = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.trustStatus(with: try XCTUnwrap(self.cuttlefishContextForAcceptor.activeAccount)) {egoStatus, error in
            XCTAssertEqual(egoStatus.egoStatus.rawValue & TPPeerStatus.partiallyReciprocated.rawValue, TPPeerStatus.partiallyReciprocated.rawValue, "acceptor should be partially accepted")
            XCTAssertNotNil(egoStatus.egoPeerID, "should have an identity")
            XCTAssertEqual(egoStatus.numberOfPeersInOctagon, 3, "should be 3 peers")
            XCTAssertFalse(egoStatus.isExcluded, "should not be excluded")
            XCTAssertNil(error, "error should be nil")
            acceptorStatus.fulfill()
        }
        self.wait(for: [acceptorStatus], timeout: 10)

        self.verifyDatabaseMocks()

        XCTAssertNil(initiator2Context.pairingUUID, "pairingUUID should be nil")

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }
}

#endif /* OCTAGON */
