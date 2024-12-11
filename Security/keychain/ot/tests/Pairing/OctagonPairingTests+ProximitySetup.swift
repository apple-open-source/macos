#if OCTAGON

extension OctagonPairingTests {
    func assertSOSSuccess() {
        XCTAssertNotNil(self.fcInitiator?.accountPrivateKey, "no accountPrivateKey in fcInitiator")
        XCTAssertNotNil(self.fcAcceptor?.accountPrivateKey, "no accountPrivateKey in fcAcceptor")
        XCTAssert(CFEqualSafe(self.fcInitiator.accountPrivateKey, self.fcAcceptor.accountPrivateKey), "no accountPrivateKey not same in both")

        XCTAssert(SOSCircleHasPeer(self.circle, self.fcInitiator.peerInfo(), nil), "HasPeer 1")
//        XCTAssert(SOSCircleHasPeer(self.circle, self.fcAcceptor.peerInfo(), nil), "HasPeer 2") <rdar://problem/54040068>
    }

    func tlkInPairingChannel(packet: Data) throws -> Bool {
        let plist = try self.pairingPacketToPlist(packet: packet)

        guard let arrayOfItems = (plist["d"] as? [[String: Any]]) else {
            return false
        }

        var foundTLK = false
        arrayOfItems.forEach { item in
            guard let agrp = (item["agrp"] as? String) else {
                return
            }
            guard let cls = (item["class"] as? String) else {
                return
            }
            if cls == "inet" && agrp == "com.apple.security.ckks" {
                foundTLK = true
            }
        }
        return foundTLK
    }

    func testJoin() throws {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let rpcEpochCallbacks = self.expectation(description: "rpcEpoch callback occurs")
        self.cuttlefishContextForAcceptor.rpcEpoch { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertEqual(epoch, 1, "epoch should be 1")
            rpcEpochCallbacks.fulfill()
        }

        self.wait(for: [rpcEpochCallbacks], timeout: 10)

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* now initiator's turn*/
        /* calling prepare identity*/
        let rpcInitiatorPrepareCallback = self.expectation(description: "rpcPrepare callback occurs")

        var p = String()
        var pI = Data()
        var pIS = Data()
        var sI = Data()
        var sIS = Data()

        initiator1Context.rpcPrepareIdentityAsApplicant(with: self.initiatorPairingConfig, epoch: 1) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error in
            XCTAssertNil(error, "Should be no error calling 'prepare'")
            XCTAssertNotNil(peerID, "Prepare should have returned a peerID")
            XCTAssertNotNil(permanentInfo, "Prepare should have returned a permanentInfo")
            XCTAssertNotNil(permanentInfoSig, "Prepare should have returned a permanentInfoSig")
            XCTAssertNotNil(stableInfo, "Prepare should have returned a stableInfo")
            XCTAssertNotNil(stableInfoSig, "Prepare should have returned a stableInfoSig")

            p = peerID!
            pI = permanentInfo!
            pIS = permanentInfoSig!
            sI = stableInfo!
            sIS = stableInfoSig!

            rpcInitiatorPrepareCallback.fulfill()
        }

        self.wait(for: [rpcInitiatorPrepareCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInitiatorAwaitingVoucher, within: 10 * NSEC_PER_SEC)

        /* calling voucher */
        let rpcVoucherCallback = self.expectation(description: "rpcVoucher callback occurs")

        var v = Data(count: 0)
        var vS = Data(count: 0)

        self.cuttlefishContextForAcceptor.rpcVoucher(withConfiguration: p, permanentInfo: pI, permanentInfoSig: pIS, stableInfo: sI, stableInfoSig: sIS) { voucher, voucherSig, error in
            XCTAssertNotNil(v, "Prepare should have returned a voucher")
            XCTAssertNotNil(vS, "Prepare should have returned a voucherSig")
            XCTAssertNil(error, "error should be nil")

            v = voucher!
            vS = voucherSig!

            rpcVoucherCallback.fulfill()
        }

        self.wait(for: [rpcVoucherCallback], timeout: 10)

        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        /* calling Join */
        let rpcJoinCallbackOccurs = self.expectation(description: "rpcJoin callback occurs")

        self.cuttlefishContext.rpcJoin(v, vouchSig: vS) { error in
            XCTAssertNil(error, "error should be nil")
            rpcJoinCallbackOccurs.fulfill()
        }

        self.wait(for: [rpcJoinCallbackOccurs], timeout: 10)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")

        XCTAssertEqual(0, self.cuttlefishContext.ckks!.stateMachine.stateConditions[CKKSStateFetchComplete]!.wait(20 * NSEC_PER_SEC), "State machine should enter CKKSStateFetchComplete")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContextForAcceptor, sender: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testJoinRetry() throws {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let rpcEpochCallbacks = self.expectation(description: "rpcEpoch callback occurs")

        self.cuttlefishContextForAcceptor.rpcEpoch { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertEqual(epoch, 1, "epoch should be 1")
            rpcEpochCallbacks.fulfill()
        }
        self.wait(for: [rpcEpochCallbacks], timeout: 10)

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* now initiator's turn*/
        /* calling prepare identity*/
        let rpcInitiatorPrepareCallback = self.expectation(description: "rpcPrepare callback occurs")

        var p = String()
        var pI = Data()
        var pIS = Data()
        var sI = Data()
        var sIS = Data()

        initiator1Context.rpcPrepareIdentityAsApplicant(with: self.initiatorPairingConfig, epoch: 1) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error in
            XCTAssertNil(error, "Should be no error calling 'prepare'")
            XCTAssertNotNil(peerID, "Prepare should have returned a peerID")
            XCTAssertNotNil(permanentInfo, "Prepare should have returned a permanentInfo")
            XCTAssertNotNil(permanentInfoSig, "Prepare should have returned a permanentInfoSig")
            XCTAssertNotNil(stableInfo, "Prepare should have returned a stableInfo")
            XCTAssertNotNil(stableInfoSig, "Prepare should have returned a stableInfoSig")

            p = peerID!
            pI = permanentInfo!
            pIS = permanentInfoSig!
            sI = stableInfo!
            sIS = stableInfoSig!

            rpcInitiatorPrepareCallback.fulfill()
        }

        self.wait(for: [rpcInitiatorPrepareCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInitiatorAwaitingVoucher, within: 10 * NSEC_PER_SEC)

        /* calling voucher */
        let rpcVoucherCallback = self.expectation(description: "rpcVoucher callback occurs")

        var v = Data(count: 0)
        var vS = Data(count: 0)

        self.cuttlefishContextForAcceptor.rpcVoucher(withConfiguration: p, permanentInfo: pI, permanentInfoSig: pIS, stableInfo: sI, stableInfoSig: sIS) { voucher, voucherSig, error in
            XCTAssertNotNil(v, "Prepare should have returned a voucher")
            XCTAssertNotNil(vS, "Prepare should have returned a voucherSig")
            XCTAssertNil(error, "error should be nil")

            v = voucher!
            vS = voucherSig!

            rpcVoucherCallback.fulfill()
        }

        self.wait(for: [rpcVoucherCallback], timeout: 10)

        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        /* calling Join */
        let ckError = FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .transactionalFailure)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)

        let rpcJoinCallbackOccurs = self.expectation(description: "rpcJoin callback occurs")

        self.cuttlefishContext.sessionMetrics = OTMetricsSessionData(flowID: "OctagonTests-flowID", deviceSessionID: "OctagonTests-deviceSessionID")

        self.cuttlefishContext.rpcJoin(v, vouchSig: vS) { error in
            XCTAssertNil(error, "error should be nil")
            rpcJoinCallbackOccurs.fulfill()
        }

        self.wait(for: [rpcJoinCallbackOccurs], timeout: 64)

        XCTAssertEqual(0, self.cuttlefishContext.ckks!.stateMachine.stateConditions[CKKSStateFetchComplete]!.wait(20 * NSEC_PER_SEC), "State machine should enter CKKSStateFetchComplete")

        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContextForAcceptor, sender: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testJoinRetryFail() throws {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let rpcEpochCallbacks = self.expectation(description: "rpcEpoch callback occurs")
        self.cuttlefishContextForAcceptor.rpcEpoch { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertEqual(epoch, 1, "epoch should be 1")
            rpcEpochCallbacks.fulfill()
        }
        self.wait(for: [rpcEpochCallbacks], timeout: 10)

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* now initiator's turn*/
        /* calling prepare identity*/
        let rpcInitiatorPrepareCallback = self.expectation(description: "rpcPrepare callback occurs")

        var p = String()
        var pI = Data()
        var pIS = Data()
        var sI = Data()
        var sIS = Data()

        initiator1Context.handlePairingRestart(self.initiatorPairingConfig)

        initiator1Context.rpcPrepareIdentityAsApplicant(with: self.initiatorPairingConfig, epoch: 1) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error in
            XCTAssertNil(error, "Should be no error calling 'prepare'")
            XCTAssertNotNil(peerID, "Prepare should have returned a peerID")
            XCTAssertNotNil(permanentInfo, "Prepare should have returned a permanentInfo")
            XCTAssertNotNil(permanentInfoSig, "Prepare should have returned a permanentInfoSig")
            XCTAssertNotNil(stableInfo, "Prepare should have returned a stableInfo")
            XCTAssertNotNil(stableInfoSig, "Prepare should have returned a stableInfoSig")

            p = peerID!
            pI = permanentInfo!
            pIS = permanentInfoSig!
            sI = stableInfo!
            sIS = stableInfoSig!

            rpcInitiatorPrepareCallback.fulfill()
        }

        self.wait(for: [rpcInitiatorPrepareCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInitiatorAwaitingVoucher, within: 10 * NSEC_PER_SEC)

        /* calling voucher */
        let rpcVoucherCallback = self.expectation(description: "rpcVoucher callback occurs")

        var v = Data(count: 0)
        var vS = Data(count: 0)

        self.cuttlefishContextForAcceptor.rpcVoucher(withConfiguration: p, permanentInfo: pI, permanentInfoSig: pIS, stableInfo: sI, stableInfoSig: sIS) { voucher, voucherSig, error in
            XCTAssertNotNil(v, "Prepare should have returned a voucher")
            XCTAssertNotNil(vS, "Prepare should have returned a voucherSig")
            XCTAssertNil(error, "error should be nil")

            v = voucher!
            vS = voucherSig!

            rpcVoucherCallback.fulfill()
        }

        self.wait(for: [rpcVoucherCallback], timeout: 10)

        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        /* calling Join */
        let ckError = FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .transactionalFailure)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)

        let rpcJoinCallbackOccurs = self.expectation(description: "rpcJoin callback occurs")

        self.cuttlefishContext.rpcJoin(v, vouchSig: vS) { error in
            XCTAssertNotNil(error, "error should be set")
            rpcJoinCallbackOccurs.fulfill()
        }
        self.wait(for: [rpcJoinCallbackOccurs], timeout: 35)
        XCTAssertNotNil(self.cuttlefishContext.pairingUUID, "pairingUUID should NOT be nil")
    }

    func testJoinWithCKKSConflict() throws {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/

        self.getAcceptorInCircle()

        self.silentFetchesAllowed = false
        self.expectCKFetchAndRun {
            self.putFakeKeyHierarchiesInCloudKit()
            self.putFakeDeviceStatusesInCloudKit()
            self.silentFetchesAllowed = true
        }

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let rpcEpochCallbacks = self.expectation(description: "rpcEpoch callback occurs")
        self.cuttlefishContextForAcceptor.rpcEpoch { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertEqual(epoch, 1, "epoch should be 1")
            rpcEpochCallbacks.fulfill()
        }
        self.wait(for: [rpcEpochCallbacks], timeout: 10)

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* now initiator's turn*/
        /* calling prepare identity*/
        let rpcInitiatorPrepareCallback = self.expectation(description: "rpcPrepare callback occurs")

        var p = String()
        var pI = Data()
        var pIS = Data()
        var sI = Data()
        var sIS = Data()

        self.cuttlefishContext.rpcPrepareIdentityAsApplicant(with: self.initiatorPairingConfig, epoch: 1) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error in
            XCTAssertNil(error, "Should be no error calling 'prepare'")
            XCTAssertNotNil(peerID, "Prepare should have returned a peerID")
            XCTAssertNotNil(permanentInfo, "Prepare should have returned a permanentInfo")
            XCTAssertNotNil(permanentInfoSig, "Prepare should have returned a permanentInfoSig")
            XCTAssertNotNil(stableInfo, "Prepare should have returned a stableInfo")
            XCTAssertNotNil(stableInfoSig, "Prepare should have returned a stableInfoSig")

            p = peerID!
            pI = permanentInfo!
            pIS = permanentInfoSig!
            sI = stableInfo!
            sIS = stableInfoSig!

            rpcInitiatorPrepareCallback.fulfill()
        }

        self.wait(for: [rpcInitiatorPrepareCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInitiatorAwaitingVoucher, within: 10 * NSEC_PER_SEC)

        /* calling voucher */
        let rpcVoucherCallback = self.expectation(description: "rpcVoucher callback occurs")

        var v = Data(count: 0)
        var vS = Data(count: 0)

        self.cuttlefishContextForAcceptor.rpcVoucher(withConfiguration: p, permanentInfo: pI, permanentInfoSig: pIS, stableInfo: sI, stableInfoSig: sIS) { voucher, voucherSig, error in
            XCTAssertNotNil(v, "Prepare should have returned a voucher")
            XCTAssertNotNil(vS, "Prepare should have returned a voucherSig")
            XCTAssertNil(error, "error should be nil")

            v = voucher!
            vS = voucherSig!

            rpcVoucherCallback.fulfill()
        }

        self.wait(for: [rpcVoucherCallback], timeout: 10)

        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        /* calling Join */
        let rpcJoinCallbackOccurs = self.expectation(description: "rpcJoin callback occurs")

        self.cuttlefishContext.rpcJoin(v, vouchSig: vS) { error in
            XCTAssertNil(error, "error should be nil")
            rpcJoinCallbackOccurs.fulfill()
        }

        self.wait(for: [rpcJoinCallbackOccurs], timeout: 10)

        XCTAssertEqual(0, self.cuttlefishContext.ckks!.stateMachine.stateConditions[CKKSStateFetchComplete]!.wait(20 * NSEC_PER_SEC), "State machine should enter CKKSStateFetchComplete")

        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testNextJoiningMessageInterface() throws {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/
        self.getAcceptorInCircle()

        let rpcEpochCallbackOccurs = self.expectation(description: "rpcEpoch callback occurs")

        self.manager.rpcEpoch(with: self.acceptorArguments, configuration: self.acceptorPairingConfig) { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertEqual(epoch, 1, "epoch should be nil")
            rpcEpochCallbackOccurs.fulfill()
        }
        self.wait(for: [rpcEpochCallbackOccurs], timeout: 10)

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        initiator1Context.startOctagonStateMachine()

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-1")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let rpcSecondInitiatorJoiningMessageCallBack = self.expectation(description: "creating prepare message callback")
        var peerID = ""
        var permanentInfo = Data(count: 0)
        var permanentInfoSig = Data(count: 0)
        var stableInfo = Data(count: 0)
        var stableInfoSig = Data(count: 0)

        self.manager.rpcPrepareIdentityAsApplicant(with: self.initiatorArguments, configuration: initiatorPairingConfig) { pID, pI, pISig, sI, sISig, error in
            XCTAssertNotNil(pID, "peer ID should not be nil")
            XCTAssertNotNil(pI, "permanentInfo should not be nil")
            XCTAssertNotNil(pISig, "permanentInfo Signature should not be nil")
            XCTAssertNotNil(sI, "stable info should not be nil")
            XCTAssertNotNil(sISig, "stable info signature should not be nil")

            peerID = pID!
            permanentInfo = pI!
            permanentInfoSig = pISig!
            stableInfo = sI!
            stableInfoSig = sISig!
            XCTAssertNil(error, "error should be nil")
            rpcSecondInitiatorJoiningMessageCallBack.fulfill()
        }
        self.wait(for: [rpcSecondInitiatorJoiningMessageCallBack], timeout: 10)

        var voucher = Data(count: 0)
        var voucherSig = Data(count: 0)
        let maxCap = KCPairingIntent_Capability._FullPeer

        let voucherCallback = self.expectation(description: "creating voucher message callback")
        self.manager.rpcVoucher(with: self.acceptorArguments, configuration: self.acceptorPairingConfig, peerID: peerID, permanentInfo: permanentInfo, permanentInfoSig: permanentInfoSig, stableInfo: stableInfo, stableInfoSig: stableInfoSig, maxCapability: maxCap.rawValue) { v, vS, error in
            XCTAssertNil(error, "error should be nil")
            voucher = v
            voucherSig = vS
            voucherCallback.fulfill()
        }
        self.wait(for: [voucherCallback], timeout: 10)

        let rpcJoinCallback = self.expectation(description: "joining octagon callback")
        self.manager.rpcJoin(with: self.initiatorArguments, configuration: initiatorPairingConfig, vouchData: voucher, vouchSig: voucherSig) { error in
            XCTAssertNil(error, "error should be nil")
            rpcJoinCallback.fulfill()
        }
        self.wait(for: [rpcJoinCallback], timeout: 10)

        XCTAssertEqual(0, self.cuttlefishContext.ckks!.stateMachine.stateConditions[CKKSStateFetchComplete]!.wait(20 * NSEC_PER_SEC), "State machine should enter CKKSStateFetchComplete")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: self.cuttlefishContextForAcceptor, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContextForAcceptor)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")
        self.verifyDatabaseMocks()
        XCTAssertNil(self.cuttlefishContext.pairingUUID, "pairingUUID should be nil")
    }

    func testEpochFetching() {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/

        self.getAcceptorInCircle()

        let rpcEpochCallbacks = self.expectation(description: "rpcEpoch callback occurs")
        self.cuttlefishContextForAcceptor.rpcEpoch { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertEqual(epoch, 1, "epoch should be 1")
            rpcEpochCallbacks.fulfill()
        }

        self.wait(for: [rpcEpochCallbacks], timeout: 10)
    }

    func testJoinMismatchCapability() throws {

        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()
        let rpcEpochCallbackOccurs = self.expectation(description: "rpcEpoch callback occurs")

        self.manager.rpcEpoch(with: self.acceptorArguments, configuration: self.acceptorPiggybackingConfig) { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertEqual(epoch, 1, "epoch should be nil")
            rpcEpochCallbackOccurs.fulfill()
        }
        self.wait(for: [rpcEpochCallbackOccurs], timeout: 10)

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

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
        self.manager.rpcPrepareIdentityAsApplicant(with: self.initiatorArguments, configuration: self.initiatorPiggybackingConfig) { pID, pI, pISig, sI, sISig, error in
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

        let fullPeerModelId = "iPhone12,3"
        XCTAssertTrue(OTDeviceInformation.isFullPeer(fullPeerModelId), "test peer is not a full peer!")

        // TEST1: Full peer joining in LimitedPeer max capability context == failure
        let prevPeer = TPPeerPermanentInfo(peerID: peerID, data: permanentInfo, sig: permanentInfoSig, keyFactory: TPECPublicKeyFactory())!
        let keys = try EscrowKeys(secret: "toomany".data(using: .utf8)!, bottleSalt: "123456789")
        let newPeer = try TPPeerPermanentInfo(machineID: prevPeer.machineID,
                                              modelID: fullPeerModelId,
                                              epoch: 1,
                                              signing: keys.signingKey,
                                              encryptionKeyPair: keys.encryptionKey,
                                              creationTime: UInt64(Date().timeIntervalSince1970 * 1000),
                                              peerIDHashAlgo: TPHashAlgo.SHA256)
        peerID = newPeer.peerID
        permanentInfo = newPeer.data
        permanentInfoSig = newPeer.sig

        self.manager.rpcVoucher(with: self.acceptorArguments, configuration: self.acceptorPiggybackingConfig, peerID: peerID, permanentInfo: permanentInfo, permanentInfoSig: permanentInfoSig, stableInfo: stableInfo, stableInfoSig: stableInfoSig, maxCapability: KCPairingIntent_Capability._LimitedPeer.rawValue) { voucher, voucherSig, error in
            XCTAssertNotNil(voucher, "voucher should not be nil")
            XCTAssertNotNil(voucherSig, "voucherSig should not be nil")
            XCTAssertTrue(voucher.isEmpty, "voucher should be empty")
            XCTAssertTrue(voucherSig.isEmpty, "voucherSig should be empty")
            XCTAssertNotNil(error, "error should be populated")
            XCTAssertTrue(error!.localizedDescription.contains("full peer attempting to join limited capability pairing context"))
            firstAcceptorCallback.fulfill()
        }
        self.wait(for: [firstAcceptorCallback], timeout: 10)

        // TEST2: Limited peer joining in LimitedPeer max capability context == success
        let secondAcceptorCallback = self.expectation(description: "creating vouch callback")

        let limitedPeerModelId = "AppleTV12,2"
        XCTAssertFalse(OTDeviceInformation.isFullPeer(limitedPeerModelId), "test peer is not a limited peer!")

        let newPeerLimited = try TPPeerPermanentInfo(machineID: prevPeer.machineID,
                                                     modelID: limitedPeerModelId,
                                                     epoch: 1,
                                                     signing: keys.signingKey,
                                                     encryptionKeyPair: keys.encryptionKey,
                                                     creationTime: UInt64(Date().timeIntervalSince1970 * 1000),
                                                     peerIDHashAlgo: TPHashAlgo.SHA256)
        peerID = newPeerLimited.peerID
        permanentInfo = newPeerLimited.data
        permanentInfoSig = newPeerLimited.sig

        self.manager.rpcVoucher(with: self.acceptorArguments, configuration: self.acceptorPiggybackingConfig, peerID: peerID, permanentInfo: permanentInfo, permanentInfoSig: permanentInfoSig, stableInfo: stableInfo, stableInfoSig: stableInfoSig, maxCapability: KCPairingIntent_Capability._LimitedPeer.rawValue) { voucher, voucherSig, error in
            XCTAssertNotNil(voucher, "voucher should not be nil")
            XCTAssertNotNil(voucherSig, "voucherSig should not be nil")
            XCTAssertFalse(voucher.isEmpty, "voucher should not be empty")
            XCTAssertFalse(voucherSig.isEmpty, "voucherSig should not be empty")
            XCTAssertNil(error, "error should not be populated")
            secondAcceptorCallback.fulfill()
        }
        self.wait(for: [secondAcceptorCallback], timeout: 10)

        // TEST3: Limited peer joining in FullPeer max capability context == success
        let thirdAcceptorCallback = self.expectation(description: "creating vouch callback")
        self.manager.rpcVoucher(with: self.acceptorArguments, configuration: self.acceptorPiggybackingConfig, peerID: peerID, permanentInfo: permanentInfo, permanentInfoSig: permanentInfoSig, stableInfo: stableInfo, stableInfoSig: stableInfoSig, maxCapability: KCPairingIntent_Capability._FullPeer.rawValue) { voucher, voucherSig, error in
            XCTAssertNotNil(voucher, "voucher should not be nil")
            XCTAssertNotNil(voucherSig, "voucherSig should not be nil")
            XCTAssertFalse(voucher.isEmpty, "voucher should not be empty")
            XCTAssertFalse(voucherSig.isEmpty, "voucherSig should not be empty")
            XCTAssertNil(error, "error should not be populated")
            thirdAcceptorCallback.fulfill()
        }
        self.wait(for: [thirdAcceptorCallback], timeout: 10)

        // TEST4: Full peer joining in FullPeer max capability context == success
        let fourthAcceptorCallback = self.expectation(description: "creating vouch callback")

        peerID = newPeer.peerID
        permanentInfo = newPeer.data
        permanentInfoSig = newPeer.sig

        self.manager.rpcVoucher(with: self.acceptorArguments, configuration: self.acceptorPiggybackingConfig, peerID: peerID, permanentInfo: permanentInfo, permanentInfoSig: permanentInfoSig, stableInfo: stableInfo, stableInfoSig: stableInfoSig, maxCapability: KCPairingIntent_Capability._FullPeer.rawValue) { voucher, voucherSig, error in
            XCTAssertNotNil(voucher, "voucher should not be nil")
            XCTAssertNotNil(voucherSig, "voucherSig should not be nil")
            XCTAssertFalse(voucher.isEmpty, "voucher should not be empty")
            XCTAssertFalse(voucherSig.isEmpty, "voucherSig should not be empty")
            XCTAssertNil(error, "error should not be populated")
            fourthAcceptorCallback.fulfill()
        }
        self.wait(for: [fourthAcceptorCallback], timeout: 10)
    }

    func testVoucherCreation() throws {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/

        self.getAcceptorInCircle()

        let rpcEpochCallbacks = self.expectation(description: "rpcEpoch callback occurs")
        self.cuttlefishContextForAcceptor.rpcEpoch { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertEqual(epoch, 1, "epoch should be 1")
            rpcEpochCallbacks.fulfill()
        }

        self.wait(for: [rpcEpochCallbacks], timeout: 10)

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateMachineNotStarted, within: 10 * NSEC_PER_SEC)

        /* now initiator's turn*/
        self.manager.startOctagonStateMachine(self.otcontrolArgumentsFor(context: self.cuttlefishContext)) { _ in
        }

        /* calling prepare identity*/
        let rpcInitiatorPrepareCallback = self.expectation(description: "rpcPrepare callback occurs")

        var p = String()
        var pI = Data()
        var pIS = Data()
        var sI = Data()
        var sIS = Data()

        self.cuttlefishContext.handlePairingRestart(self.initiatorPairingConfig)

        self.cuttlefishContext.rpcPrepareIdentityAsApplicant(with: self.initiatorPairingConfig, epoch: 1) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error in
            XCTAssertNil(error, "Should be no error calling 'prepare'")
            XCTAssertNotNil(peerID, "Prepare should have returned a peerID")
            XCTAssertNotNil(permanentInfo, "Prepare should have returned a permanentInfo")
            XCTAssertNotNil(permanentInfoSig, "Prepare should have returned a permanentInfoSig")
            XCTAssertNotNil(stableInfo, "Prepare should have returned a stableInfo")
            XCTAssertNotNil(stableInfoSig, "Prepare should have returned a stableInfoSig")

            p = peerID!
            pI = permanentInfo!
            pIS = permanentInfoSig!
            sI = stableInfo!
            sIS = stableInfoSig!

            rpcInitiatorPrepareCallback.fulfill()
        }

        self.wait(for: [rpcInitiatorPrepareCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInitiatorAwaitingVoucher, within: 10 * NSEC_PER_SEC)

        /* calling voucher */
        let rpcVoucherCallback = self.expectation(description: "rpcVoucher callback occurs")

        var v = Data(count: 0)
        var vS = Data(count: 0)

        self.cuttlefishContextForAcceptor.rpcVoucher(withConfiguration: p, permanentInfo: pI, permanentInfoSig: pIS, stableInfo: sI, stableInfoSig: sIS) { voucher, voucherSig, error in
            XCTAssertNotNil(v, "Prepare should have returned a voucher")
            XCTAssertNotNil(vS, "Prepare should have returned a voucherSIg")
            XCTAssertNil(error, "error should be nil")

            v = voucher!
            vS = voucherSig!

            rpcVoucherCallback.fulfill()
        }

        self.wait(for: [rpcVoucherCallback], timeout: 10)
    }

    func testPrepareTimeoutIfStateMachineUnstarted() {
        self.startCKAccountStatusMock()

        let rpcCallbackOccurs = self.expectation(description: "rpcPrepare callback occurs")
        self.initiatorPairingConfig.timeout = Int64(2 * NSEC_PER_SEC)

        self.cuttlefishContext.handlePairingRestart(self.initiatorPairingConfig)

        self.cuttlefishContext.rpcPrepareIdentityAsApplicant(with: self.initiatorPairingConfig, epoch: 1) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error in
            XCTAssertNotNil(error, "Should be an error calling 'prepare'")
            XCTAssertEqual(error?._domain, CKKSResultErrorDomain, "Error domain should be CKKSResultErrorDomain")
            XCTAssertEqual(error?._code ?? -1, CKKSResultTimedOut, "Error result should be CKKSResultTimedOut")

            XCTAssertNil(peerID, "Prepare should not have returned a peerID")
            XCTAssertNil(permanentInfo, "Prepare should not have returned a permanentInfo")
            XCTAssertNil(permanentInfoSig, "Prepare should not have returned a permanentInfoSig")
            XCTAssertNil(stableInfo, "Prepare should not have returned a stableInfo")
            XCTAssertNil(stableInfoSig, "Prepare should not have returned a stableInfoSig")

            rpcCallbackOccurs.fulfill()
        }

        self.wait(for: [rpcCallbackOccurs], timeout: 10)
    }

    func testProximitySetupUsingCliqueOctagonAndSOS() throws {
        self.startCKAccountStatusMock()

        OctagonSetSOSFeatureEnabled(true)

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")

        /* ACCEPTOR SECOND RTT */
        let acceptorVoucherPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorPreparedIdentityPacket, reason: "epoch return")

        /* INITIATOR THIRD STEP*/
        let initiatorThirdPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorVoucherPacket, reason: "intitiator third packet")

        /* ACCEPTOR THIRD STEP */
        let acceptorThirdPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorThirdPacket, reason: "acceptor third packet")
        XCTAssertFalse(try self.tlkInPairingChannel(packet: acceptorThirdPacket), "pairing channel should NOT transport TLKs for SOS+Octagon")

        /* INITIATOR FOURTH STEP*/
        let initiatorFourthPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorThirdPacket, reason: "initiator fourth packet")

        acceptor.setDSIDForTest("123456")
        let acceptorFourthPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFourthPacket, reason: "acceptor fourth packet")

        let initiatorFifthPacket = self.sendPairingExpectingCompletionAndReply(channel: initiator, packet: acceptorFourthPacket, reason: "initiator fifth packet")

        self.sendPairingExpectingCompletion(channel: acceptor, packet: initiatorFifthPacket, reason: "acceptor finishes")

        XCTAssertNil(initiator1Context.pairingUUID, "pairingUUID should be nil")

        // pairing completes here

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        try self.forceFetch(context: self.cuttlefishContextForAcceptor)

        // Initiator should join!
        self.assertEnters(context: initiator1Context, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: initiator1Context)
        self.verifyDatabaseMocks()

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

        self.assertSOSSuccess()
    }

    func testProximitySetupUsingCliqueOctagonOnly() throws {
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")

        /* ACCEPTOR SECOND RTT */
        let acceptorVoucherPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorPreparedIdentityPacket, reason: "acceptor second packet")

        // the tlks are in the 3rd roundtrip, but lets check here too
        XCTAssertFalse(try self.tlkInPairingChannel(packet: acceptorVoucherPacket), "pairing channel should not transport TLKs for octagon")

        /* INITIATOR THIRD STEP*/
        let initiatorThirdPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorVoucherPacket, reason: "initiator third packet")

        acceptor.setDSIDForTest("123456")
        let acceptorThirdPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorThirdPacket, reason: "acceptor third packet")

        let initiatorFourthPacket = self.sendPairingExpectingCompletionAndReply(channel: initiator, packet: acceptorThirdPacket, reason: "initiator fourth packet")

        self.sendPairingExpectingCompletion(channel: acceptor, packet: initiatorFourthPacket, reason: "acceptor finishes")

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        try self.forceFetch(context: self.cuttlefishContextForAcceptor)

        // Initiator should join!
        self.assertEnters(context: initiator1Context, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: initiator1Context)
        self.verifyDatabaseMocks()

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
    }

    func testProximitySetupOctagonAndSOSWithSOSFailure() throws {
        // ensure Octagon protocol continues even if SOS fails in some way.
        OctagonSetSOSFeatureEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        initiator.setSOSMessageFailForTesting(true)
        acceptor.setSOSMessageFailForTesting(true)

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")

        /* ACCEPTOR SECOND RTT */
        let acceptorVoucherPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorPreparedIdentityPacket, reason: "epoch return")

        /* INITIATOR THIRD STEP*/
        _ = self.sendPairingExpectingReply(channel: initiator, packet: acceptorVoucherPacket, reason: "intitiator third packet")

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        try self.forceFetch(context: self.cuttlefishContextForAcceptor)

        // Initiator should join!
        self.assertEnters(context: initiator1Context, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: initiator1Context)
        self.verifyDatabaseMocks()

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
    }

    func testProximitySetupOctagonAndSOSWithOctagonInitiatorMessage1Failure() throws {
        // ensure Octagon protocol halts if enabled and encounters a failure
        OctagonSetSOSFeatureEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        initiator.setOctagonMessageFailForTesting(true)

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        self.sendPairingExpectingCompletion(channel: initiator, packet: nil, reason: "error on first message")

        XCTAssertNil(initiator1Context.pairingUUID, "pairingUUID should be nil")
    }

    func testProximitySetupOctagonAndSOSWithOctagonAcceptorMessage1Failure() throws {
        // ensure Octagon protocol continues even if SOS fails in some way.
        OctagonSetSOSFeatureEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        acceptor.setOctagonMessageFailForTesting(true)

        /* ACCEPTOR FIRST RTT EPOCH*/
        let firstAcceptorCallback = self.expectation(description: "firstAcceptorCallback callback occurs")

        acceptor.exchangePacket(initiatorFirstPacket) { complete, packet, error in
            XCTAssertNil(error, "should be no error")
            XCTAssertTrue(complete, "should be True")
            XCTAssertNil(packet, "packet should be nil")
            firstAcceptorCallback.fulfill()
        }
        self.wait(for: [firstAcceptorCallback], timeout: 10)
    }

    func testProximitySetupOctagonAndSOSWithOctagonInitiatorMessage2Failure() throws {
        OctagonSetSOSFeatureEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let secondInitiatorCallback = self.expectation(description: "secondInitiatorCallback callback occurs")

        // set up initiator's message 2 to fail
        initiator.setOctagonMessageFailForTesting(true)

        initiator.exchangePacket(acceptorEpochPacket) { complete, packet, error in
            XCTAssertNil(error, "should be no error")
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should not be nil")
            secondInitiatorCallback.fulfill()
        }

        self.wait(for: [secondInitiatorCallback], timeout: 10)
    }

    func testProximitySetupOctagonAndSOSWithOctagonAcceptorMessage2Failure() throws {
        OctagonSetSOSFeatureEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")

        /* ACCEPTOR SECOND RTT */
        let SecondAcceptorCallback = self.expectation(description: "SecondAcceptorCallback callback occurs")

        acceptor.setOctagonMessageFailForTesting(true)

        acceptor.exchangePacket(initiatorPreparedIdentityPacket) { complete, packet, error in
            XCTAssertNil(error, "should be no error")
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should be nil")
            SecondAcceptorCallback.fulfill()
        }
        self.wait(for: [SecondAcceptorCallback], timeout: 10)
    }

    func testProximitySetupOctagonAndSOSWithOctagonInitiatorMessage3Failure() throws {
        OctagonSetSOSFeatureEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")

        /* ACCEPTOR SECOND RTT */
        let acceptorVoucherPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorPreparedIdentityPacket, reason: "epoch return")

        /* INITIATOR THIRD STEP*/
        let thirdInitiatorCallback = self.expectation(description: "thirdInitiatorCallback callback occurs")

        initiator.setOctagonMessageFailForTesting(true)

        initiator.exchangePacket(acceptorVoucherPacket) { complete, packet, error in
            XCTAssertNil(error, "should be no error")
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should be nil")
            thirdInitiatorCallback.fulfill()
        }
        self.wait(for: [thirdInitiatorCallback], timeout: 10)
    }

    func circleAndSOS() throws {
        let peerInfoAcceptor: SOSPeerInfoRef = SOSFullPeerInfoGetPeerInfo(self.fcAcceptor.fullPeerInfo)
        let encryptionKeyAcceptor = _SFECKeyPair.init(secKey: self.fcAcceptor.octagonEncryptionKey)
        let signingKeyAcceptor = _SFECKeyPair.init(secKey: self.fcAcceptor.octagonSigningKey)
        let peerIDAcceptor: NSString = (SOSPeerInfoGetPeerID(peerInfoAcceptor) .takeUnretainedValue() as NSString)
        let AcceptorSOSPeer = CKKSSOSSelfPeer(sosPeerID: peerIDAcceptor as String,
                                              encryptionKey: encryptionKeyAcceptor,
                                              signingKey: signingKeyAcceptor,
                                              viewList: self.managedViewList())

        self.mockSOSAdapter!.trustedPeers.add(AcceptorSOSPeer)
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCInCircle)

        let acceptor = self.manager.context(forContainerName: OTCKContainerName,
                                            contextID: self.contextForAcceptor,
                                            sosAdapter: self.mockSOSAdapter!,
                                            accountsAdapter: self.mockAuthKit2,
                                            authKitAdapter: self.mockAuthKit2,
                                            tooManyPeersAdapter: self.mockTooManyPeers,
                                            tapToRadarAdapter: self.mockTapToRadar,
                                            lockStateTracker: self.lockStateTracker,
                                            deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-SOS-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

        acceptor.startOctagonStateMachine()

        let resetAndEstablishExpectation = self.expectation(description: "resetAndEstablish callback occurs")
        acceptor.rpcResetAndEstablish(.testGenerated) { resetError in
            XCTAssertNil(resetError, "Should be no error calling resetAndEstablish")
            resetAndEstablishExpectation.fulfill()
        }
        self.wait(for: [resetAndEstablishExpectation], timeout: 10)
        self.assertEnters(context: acceptor, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertConsidersSelfTrusted(context: acceptor)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 1, "should be 1 bottles")
    }

    func testProximitySetupUsingCliqueAcceptorResolvesVersionToSOSOnly() throws {
        self.startCKAccountStatusMock()

        OctagonSetSOSFeatureEnabled(true)

        let newSOSPeer = createSOSPeer(peerID: "sos-peer-id")
        self.mockSOSAdapter!.selfPeer = newSOSPeer
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.mockSOSAdapter!.trustedPeers.add(newSOSPeer)

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        acceptor.setSessionSupportsOctagonForTesting(false)

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")

        /* ACCEPTOR SECOND RTT */
        let acceptorVoucherPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorPreparedIdentityPacket, reason: "epoch return")

        /* INITIATOR THIRD STEP*/
        _ = self.sendPairingExpectingReply(channel: initiator, packet: acceptorVoucherPacket, reason: "intitiator third packet")
/*
        need to fix attempting sos upgrade in the tests when pairing/piggybacking and then kicking off an upgrade
        let initiatorContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.assertEnters(context: initiatorContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: initiatorContext)
 */
    }

    func testProximitySetupUsingCliqueInitiatorResolvesVersionToSOSOnly() throws {
        self.startCKAccountStatusMock()

        OctagonSetSOSFeatureEnabled(true)

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        initiator.setSessionSupportsOctagonForTesting(false)

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")

        /* ACCEPTOR SECOND RTT */
        let acceptorVoucherPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorPreparedIdentityPacket, reason: "epoch return")

        /* INITIATOR THIRD STEP*/
        _ = self.sendPairingExpectingReply(channel: initiator, packet: acceptorVoucherPacket, reason: "intitiator third packet")

        /*
         need to fix attempting sos upgrade in the tests when pairing/piggybacking and then kicking off an upgrade
         let initiatorContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

         self.assertEnters(context: initiatorContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
         self.assertConsidersSelfTrusted(context: initiatorContext)
         */
    }

    func testProximityPairingWithFlowIDAndDeviceID() throws {
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        XCTAssertNotNil(acceptor.peerVersionContext.flowID, "acceptor flowID should not be nil")
        XCTAssertNotNil(acceptor.peerVersionContext.deviceSessionID, "acceptor deviceSessionID should not be nil")
        XCTAssertNotNil(initiator.peerVersionContext.flowID, "requestor flowID should not be nil")
        XCTAssertNotNil(initiator.peerVersionContext.deviceSessionID, "requestor deviceSessionID should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")
        XCTAssertNil(initiator1Context.sessionMetrics, "sendMetrics should be nil")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")
        XCTAssertNil(self.cuttlefishContextForAcceptor.sessionMetrics, "sessionMetrics should be nil")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")
        XCTAssertNil(initiator1Context.sessionMetrics, "sessionMetrics should be nil")

        /* ACCEPTOR SECOND RTT */
        let acceptorVoucherPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorPreparedIdentityPacket, reason: "acceptor second packet")
        XCTAssertNil(self.cuttlefishContextForAcceptor.sessionMetrics, "sessionMetrics should be nil")

        // the tlks are in the 3rd roundtrip, but lets check here too
        XCTAssertFalse(try self.tlkInPairingChannel(packet: acceptorVoucherPacket), "pairing channel should not transport TLKs for octagon")

        /* INITIATOR THIRD STEP*/
        let initiatorThirdPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorVoucherPacket, reason: "initiator third packet")

        acceptor.setDSIDForTest("123456")
        let acceptorThirdPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorThirdPacket, reason: "acceptor third packet")

        let initiatorFourthPacket = self.sendPairingExpectingCompletionAndReply(channel: initiator, packet: acceptorThirdPacket, reason: "initiator fourth packet")

        self.sendPairingExpectingCompletion(channel: acceptor, packet: initiatorFourthPacket, reason: "acceptor finishes")

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        try self.forceFetch(context: self.cuttlefishContextForAcceptor)

        // Initiator should join!
        self.assertEnters(context: initiator1Context, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: initiator1Context)
        self.verifyDatabaseMocks()

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
    }

    func testFetchEpochRetry() throws {
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "pairing object should not be nil")
        XCTAssertNotNil(initiator, "pairing object should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        let otControl = OctagonTrustCliqueBridge.makeMockOTControlObjectWithFailingEpochFetchWithXPCError()
        acceptor.setControlObject(otControl)

        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        let callback = self.expectation(description: "callback occurs")
        acceptor.exchangePacket(initiatorFirstPacket) { complete, response, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).domain, "NSCocoaErrorDomain", "domains should be equal")
            XCTAssertEqual((error! as NSError).code, 4097, "error codes should be equal")
            XCTAssertTrue(complete, "Expected pairing session to halt early")
            XCTAssertNil(response, "packet should be nil")
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)

        XCTAssertEqual(TestsObjectiveC.getInvocationCount(), 3, "fetchEpoch should have been invoked 3 times")
    }

    func testFetchEpochNonRetry() throws {
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "pairing object should not be nil")
        XCTAssertNotNil(initiator, "pairing object should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        let otControl = OctagonTrustCliqueBridge.makeMockOTControlObjectWithFailingEpochFetchWithRandomError()
        acceptor.setControlObject(otControl)

        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        let callback = self.expectation(description: "callback occurs")
        acceptor.exchangePacket(initiatorFirstPacket) { complete, response, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).domain, OctagonErrorDomain, "domains should be equal")
            XCTAssertEqual((error! as NSError).code, 5, "error codes should be equal")
            XCTAssertTrue(complete, "Expected pairing session to halt early")
            XCTAssertNil(response, "packet should be nil")
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)

        XCTAssertEqual(TestsObjectiveC.getInvocationCount(), 1, "fetchEpoch should have been invoked 1 time")
    }

    func testFetchVoucherRetry() throws {
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "pairing object should not be nil")
        XCTAssertNotNil(initiator, "pairing object should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")

        let otControl = OctagonTrustCliqueBridge.makeMockOTControlObjectWithFailingVoucherFetchWithXPCError()
        acceptor.setControlObject(otControl)

        /* ACCEPTOR SECOND RTT */
        let callback = self.expectation(description: "callback occurs")
        acceptor.exchangePacket(initiatorPreparedIdentityPacket) { complete, response, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).domain, "NSCocoaErrorDomain", "domains should be equal")
            XCTAssertEqual((error! as NSError).code, 4097, "error codes should be equal")
            XCTAssertTrue(complete, "Expected pairing session to halt early")
            XCTAssertNil(response, "packet should be nil")
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)

        XCTAssertEqual(TestsObjectiveC.getInvocationCount(), 3, "fetchVoucher should have been invoked 3 times")
    }

    func testFetchVoucherRetryOnNetworkFailure() throws {
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "pairing object should not be nil")
        XCTAssertNotNil(initiator, "pairing object should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")

        let otControl = OctagonTrustCliqueBridge.makeMockOTControlObjectWithFailingVoucherFetchWithNetworkError()
        acceptor.setControlObject(otControl)

        /* ACCEPTOR SECOND RTT */
        let callback = self.expectation(description: "callback occurs")
        acceptor.exchangePacket(initiatorPreparedIdentityPacket) { complete, response, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).domain, NSURLErrorDomain, "domains should be equal")
            XCTAssertEqual((error! as NSError).code, NSURLErrorTimedOut, "error codes should be equal")
            XCTAssertTrue(complete, "Expected pairing session to halt early")
            XCTAssertNil(response, "packet should be nil")
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)

        XCTAssertEqual(TestsObjectiveC.getInvocationCount(), 3, "fetchVoucher should have been invoked 3 times")
    }

    func testFetchVoucherRetryOnNetworkFailureUnderlyingError() throws {
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "pairing object should not be nil")
        XCTAssertNotNil(initiator, "pairing object should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")

        let otControl = OctagonTrustCliqueBridge.makeMockOTControlObjectWithFailingVoucherFetchWithUnderlyingNetworkError()
        acceptor.setControlObject(otControl)

        /* ACCEPTOR SECOND RTT */
        let callback = self.expectation(description: "callback occurs")
        acceptor.exchangePacket(initiatorPreparedIdentityPacket) { complete, response, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).domain, CKErrorDomain, "domains should be equal")
            XCTAssertEqual((error! as NSError).code, CKError.networkFailure.rawValue, "error codes should be equal")

            let underlyingError: NSError = (error! as NSError).userInfo[NSUnderlyingErrorKey] as! NSError
            XCTAssertEqual((underlyingError as NSError).domain, NSURLErrorDomain, "domains should be equal")
            XCTAssertEqual((underlyingError as NSError).code, NSURLErrorTimedOut, "error codes should be equal")

            XCTAssertTrue(complete, "Expected pairing session to halt early")
            XCTAssertNil(response, "packet should be nil")
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)

        XCTAssertEqual(TestsObjectiveC.getInvocationCount(), 3, "fetchVoucher should have been invoked 3 times")
    }

    func testFetchVoucherRetryOnNetworkFailureUnderlyingConnectionLostError() throws {
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "pairing object should not be nil")
        XCTAssertNotNil(initiator, "pairing object should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")

        let otControl = OctagonTrustCliqueBridge.makeMockOTControlObjectWithFailingVoucherFetchWithUnderlyingNetworkErrorConnectionLost()
        acceptor.setControlObject(otControl)

        /* ACCEPTOR SECOND RTT */
        let callback = self.expectation(description: "callback occurs")
        acceptor.exchangePacket(initiatorPreparedIdentityPacket) { complete, response, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).domain, CKErrorDomain, "domains should be equal")
            XCTAssertEqual((error! as NSError).code, CKError.networkFailure.rawValue, "error codes should be equal")

            let underlyingError: NSError = (error! as NSError).userInfo[NSUnderlyingErrorKey] as! NSError
            XCTAssertEqual((underlyingError as NSError).domain, NSURLErrorDomain, "domains should be equal")
            XCTAssertEqual((underlyingError as NSError).code, NSURLErrorNetworkConnectionLost, "error codes should be equal")

            XCTAssertTrue(complete, "Expected pairing session to halt early")
            XCTAssertNil(response, "packet should be nil")
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)

        XCTAssertEqual(TestsObjectiveC.getInvocationCount(), 3, "fetchVoucher should have been invoked 3 times")
    }

    func testFetchVoucherNoRetry() throws {
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "pairing object should not be nil")
        XCTAssertNotNil(initiator, "pairing object should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")

        let otControl = OctagonTrustCliqueBridge.makeMockOTControlObjectWithFailingVoucherFetchWithRandomError()
        acceptor.setControlObject(otControl)

        /* ACCEPTOR SECOND RTT */
        let callback = self.expectation(description: "callback occurs")
        acceptor.exchangePacket(initiatorPreparedIdentityPacket) { complete, response, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).domain, OctagonErrorDomain, "domains should be equal")
            XCTAssertEqual((error! as NSError).code, 25, "error codes should be equal")
            XCTAssertTrue(complete, "Expected pairing session to halt early")
            XCTAssertNil(response, "packet should be nil")
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)

        XCTAssertEqual(TestsObjectiveC.getInvocationCount(), 1, "fetchVoucher should have been invoked 1 time")
    }

    func testFetchPrepareRetry() throws {
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "pairing object should not be nil")
        XCTAssertNotNil(initiator, "pairing object should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        var otControl = OctagonTrustCliqueBridge.makeMockOTControlObjectWithFailingPrepareFetchWithXPCError()
        initiator.setControlObject(otControl)

        var callback = self.expectation(description: "callback occurs")
        initiator.exchangePacket(acceptorEpochPacket) { complete, response, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).domain, "NSCocoaErrorDomain", "domains should be equal")
            XCTAssertEqual((error! as NSError).code, 4097, "error codes should be equal")
            XCTAssertTrue(complete, "Expected pairing session to halt early")
            XCTAssertNil(response, "packet should be nil")
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)

        XCTAssertEqual(TestsObjectiveC.getInvocationCount(), 3, "fetchPrepare should have been invoked 3 times")

        TestsObjectiveC.clearInvocationCount()

        otControl = OctagonTrustCliqueBridge.makeMockOTControlObjectWithFailingPrepareFetchWithOctagonErrorICloudAccountStateUnknown()
        initiator.setControlObject(otControl)

        callback = self.expectation(description: "callback occurs")
        initiator.exchangePacket(acceptorEpochPacket) { complete, response, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).domain, OctagonErrorDomain, "domains should be equal")
            XCTAssertEqual((error! as NSError).code, OctagonError.iCloudAccountStateUnknown.rawValue, "error codes should be equal")
            XCTAssertTrue(complete, "Expected pairing session to halt early")
            XCTAssertNil(response, "packet should be nil")
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)

        XCTAssertEqual(TestsObjectiveC.getInvocationCount(), 3, "fetchPrepare should have been invoked 3 times")
    }

    func testFetchPrepareNoRetry() throws {
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "pairing object should not be nil")
        XCTAssertNotNil(initiator, "pairing object should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        let otControl = OctagonTrustCliqueBridge.makeMockOTControlObjectWithFailingPrepareFetchWithRandomError()
        initiator.setControlObject(otControl)

        let callback = self.expectation(description: "callback occurs")
        initiator.exchangePacket(acceptorEpochPacket) { complete, response, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).domain, OctagonErrorDomain, "domains should be equal")
            XCTAssertEqual((error! as NSError).code, OctagonError.notSignedIn.rawValue, "error codes should be equal")
            XCTAssertTrue(complete, "Expected pairing session to halt early")
            XCTAssertNil(response, "packet should be nil")
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)

        XCTAssertEqual(TestsObjectiveC.getInvocationCount(), 1, "fetchPrepare should have been invoked 1 time")
    }

    func testFetchJoinRetry() throws {
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "pairing object should not be nil")
        XCTAssertNotNil(initiator, "pairing object should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")

        /* ACCEPTOR SECOND RTT */
        let acceptorVoucherPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorPreparedIdentityPacket, reason: "acceptor third packet")

        let otControl = OctagonTrustCliqueBridge.makeMockOTControlObjectWithFailingJoinWithXPCError()
        initiator.setControlObject(otControl)

        let callback = self.expectation(description: "callback occurs")
        initiator.exchangePacket(acceptorVoucherPacket) { complete, response, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).domain, "NSCocoaErrorDomain", "domains should be equal")
            XCTAssertEqual((error! as NSError).code, 4097, "error codes should be equal")
            XCTAssertTrue(complete, "Expected pairing session to halt early")
            XCTAssertNil(response, "packet should be nil")
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)

        XCTAssertEqual(TestsObjectiveC.getInvocationCount(), 3, "join should have been invoked 3 times")
    }

    func testFetchJoinNoRetry() throws {
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "pairing object should not be nil")
        XCTAssertNotNil(initiator, "pairing object should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")

        /* ACCEPTOR SECOND RTT */
        let acceptorVoucherPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorPreparedIdentityPacket, reason: "acceptor third packet")

        let otControl = OctagonTrustCliqueBridge.makeMockOTControlObjectWithFailingJoinWithRandomError()
        initiator.setControlObject(otControl)

        let callback = self.expectation(description: "callback occurs")
        initiator.exchangePacket(acceptorVoucherPacket) { complete, response, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).domain, OctagonErrorDomain, "domains should be equal")
            XCTAssertEqual((error! as NSError).code, OctagonError.notInSOS.rawValue, "error codes should be equal")
            XCTAssertTrue(complete, "Expected pairing session to halt early")
            XCTAssertNil(response, "packet should be nil")
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)

        XCTAssertEqual(TestsObjectiveC.getInvocationCount(), 1, "join should have been invoked 1 time")
    }

    func testFetchJoinWithErrorsFromTPH() throws {
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "pairing object should not be nil")
        XCTAssertNotNil(initiator, "pairing object should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        /* ACCEPTOR FIRST RTT EPOCH*/
        let acceptorEpochPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorFirstPacket, reason: "epoch return")

        /* INITIATOR SECOND RTT PREPARE*/
        let initiatorPreparedIdentityPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorEpochPacket, reason: "prepared identity")

        /* ACCEPTOR SECOND RTT */
        let acceptorVoucherPacket = self.sendPairingExpectingReply(channel: acceptor, packet: initiatorPreparedIdentityPacket, reason: "acceptor third packet")

        let ckError = NSError(domain: AKAppleIDAuthenticationErrorDomain,
                              code: 17,
                              userInfo: [NSLocalizedDescriptionKey: "The Internet connection appears to be offline."])

        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)

        let fetchChangesExpectation = self.expectation(description: "fetchChanges")
        fetchChangesExpectation.expectedFulfillmentCount = 20
        fetchChangesExpectation.isInverted = true
        self.fakeCuttlefishServer.fetchChangesListener = { _ in
            fetchChangesExpectation.fulfill()
            return nil
        }

        let callback = self.expectation(description: "callback occurs")
        initiator.exchangePacket(acceptorVoucherPacket) { complete, response, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).domain, AKAppleIDAuthenticationErrorDomain, "domains should be equal")
            XCTAssertEqual((error! as NSError).code, 17, "error codes should be equal")
            XCTAssertTrue(complete, "Expected pairing session to halt early")
            XCTAssertNil(response, "packet should be nil")
            callback.fulfill()
        }
        self.wait(for: [callback], timeout: 10)
        self.wait(for: [fetchChangesExpectation], timeout: 10)
    }

    func testStressReadingWritingMetricsData() throws {

        let group1 = DispatchGroup()
        let group2 = DispatchGroup()
        let group3 = DispatchGroup()

        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        XCTAssertNotNil(acceptor.peerVersionContext.flowID, "acceptor flowID should not be nil")
        XCTAssertNotNil(acceptor.peerVersionContext.deviceSessionID, "acceptor deviceSessionID should not be nil")
        XCTAssertNotNil(initiator.peerVersionContext.flowID, "requestor flowID should not be nil")
        XCTAssertNotNil(initiator.peerVersionContext.deviceSessionID, "requestor deviceSessionID should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        for _ in 0...500 {
            let queue1 = DispatchQueue(label: "nilTheVariables")
            let queue2 = DispatchQueue(label: "accessProperties")
            let queue3 = DispatchQueue(label: "populateTheVariables")

            queue1.async(group: group1) {
                self.cuttlefishContextForAcceptor.sessionMetrics = nil
            }
            queue2.async(group: group2) {
                _ = self.cuttlefishContextForAcceptor.operationDependencies()
            }
            queue3.async(group: group3) {
                self.cuttlefishContextForAcceptor.sessionMetrics = OTMetricsSessionData(flowID: "testflowID", deviceSessionID: "testDeviceSessionID")
            }
        }

        group1.wait()
        group2.wait()
        group3.wait()
    }

    func testTDLFetchFailsDuringVouchOperation() throws {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: "initiatorContext")
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let rpcEpochCallbacks = self.expectation(description: "rpcEpoch callback occurs")

        self.cuttlefishContextForAcceptor.rpcEpoch { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertEqual(epoch, 1, "epoch should be 1")
            rpcEpochCallbacks.fulfill()
        }
        self.wait(for: [rpcEpochCallbacks], timeout: 10)

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* now initiator's turn*/
        /* calling prepare identity*/
        let rpcInitiatorPrepareCallback = self.expectation(description: "rpcPrepare callback occurs")

        var p = String()
        var pI = Data()
        var pIS = Data()
        var sI = Data()
        var sIS = Data()

        initiator1Context.rpcPrepareIdentityAsApplicant(with: self.initiatorPairingConfig, epoch: 1) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error in
            XCTAssertNil(error, "Should be no error calling 'prepare'")
            XCTAssertNotNil(peerID, "Prepare should have returned a peerID")
            XCTAssertNotNil(permanentInfo, "Prepare should have returned a permanentInfo")
            XCTAssertNotNil(permanentInfoSig, "Prepare should have returned a permanentInfoSig")
            XCTAssertNotNil(stableInfo, "Prepare should have returned a stableInfo")
            XCTAssertNotNil(stableInfoSig, "Prepare should have returned a stableInfoSig")

            p = peerID!
            pI = permanentInfo!
            pIS = permanentInfoSig!
            sI = stableInfo!
            sIS = stableInfoSig!

            rpcInitiatorPrepareCallback.fulfill()
        }

        self.wait(for: [rpcInitiatorPrepareCallback], timeout: 10)

        self.assertEnters(context: initiator1Context, state: OctagonStateInitiatorAwaitingVoucher, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: self.cuttlefishContextForAcceptor, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        /* calling voucher */
        let rpcVoucherCallback = self.expectation(description: "rpcVoucher callback occurs")

        self.mockAuthKit3.injectAuthErrorsAtFetchTime = true
        self.mockAuthKit3.removeAndSendNotification(self.mockAuthKit3.currentMachineID)

        self.cuttlefishContextForAcceptor.rpcVoucher(withConfiguration: p, permanentInfo: pI, permanentInfoSig: pIS, stableInfo: sI, stableInfoSig: sIS) { voucher, voucherSig, error in
            XCTAssertNil(voucher, "should not have returned a voucher")
            XCTAssertNil(voucherSig, "should not have returned a voucher signature")
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertTrue(((error! as NSError).domain == CKKSResultErrorDomain) || ((error! as NSError).domain == AKAppleIDAuthenticationErrorDomain), "error domain should be CKKSResultErrorDomain or AKAuthenticationError")
            XCTAssertTrue(((error! as NSError).code == CKKSResultTimedOut) || ((error! as NSError).code == AKAppleIDAuthenticationError.authenticationErrorNotPermitted.rawValue), "error domain should be CKKSResultTimedOut or AKAuthenticationErrorNotPermitted")
            
            rpcVoucherCallback.fulfill()
        }

        self.wait(for: [rpcVoucherCallback], timeout: 30)

        self.assertEnters(context: self.cuttlefishContextForAcceptor, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testJoinWhenSecurityRestarts() throws {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let rpcEpochCallbacks = self.expectation(description: "rpcEpoch callback occurs")
        self.cuttlefishContextForAcceptor.rpcEpoch { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertEqual(epoch, 1, "epoch should be 1")
            rpcEpochCallbacks.fulfill()
        }

        self.wait(for: [rpcEpochCallbacks], timeout: 10)

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* now initiator's turn*/
        /* calling prepare identity*/
        let rpcInitiatorPrepareCallback = self.expectation(description: "rpcPrepare callback occurs")

        var p = String()
        var pI = Data()
        var pIS = Data()
        var sI = Data()
        var sIS = Data()

        initiator1Context.rpcPrepareIdentityAsApplicant(with: self.initiatorPairingConfig, epoch: 1) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error in
            XCTAssertNil(error, "Should be no error calling 'prepare'")
            XCTAssertNotNil(peerID, "Prepare should have returned a peerID")
            XCTAssertNotNil(permanentInfo, "Prepare should have returned a permanentInfo")
            XCTAssertNotNil(permanentInfoSig, "Prepare should have returned a permanentInfoSig")
            XCTAssertNotNil(stableInfo, "Prepare should have returned a stableInfo")
            XCTAssertNotNil(stableInfoSig, "Prepare should have returned a stableInfoSig")

            p = peerID!
            pI = permanentInfo!
            pIS = permanentInfoSig!
            sI = stableInfo!
            sIS = stableInfoSig!

            rpcInitiatorPrepareCallback.fulfill()
        }

        self.wait(for: [rpcInitiatorPrepareCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInitiatorAwaitingVoucher, within: 10 * NSEC_PER_SEC)

        /* calling voucher */
        let rpcVoucherCallback = self.expectation(description: "rpcVoucher callback occurs")

        var v = Data(count: 0)
        var vS = Data(count: 0)

        self.cuttlefishContextForAcceptor.rpcVoucher(withConfiguration: p, permanentInfo: pI, permanentInfoSig: pIS, stableInfo: sI, stableInfoSig: sIS) { voucher, voucherSig, error in
            XCTAssertNotNil(v, "Prepare should have returned a voucher")
            XCTAssertNotNil(vS, "Prepare should have returned a voucherSig")
            XCTAssertNil(error, "error should be nil")

            v = voucher!
            vS = voucherSig!

            rpcVoucherCallback.fulfill()
        }

        self.wait(for: [rpcVoucherCallback], timeout: 10)

        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInitiatorAwaitingVoucher, within: 10 * NSEC_PER_SEC)

        self.cuttlefishContext = self.simulateRestart(context: self.cuttlefishContext)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        /* calling Join */
        let rpcJoinCallbackOccurs = self.expectation(description: "rpcJoin callback occurs")

        self.cuttlefishContext.rpcJoin(v, vouchSig: vS) { error in
            XCTAssertNil(error, "error should be nil")
            rpcJoinCallbackOccurs.fulfill()
        }

        self.wait(for: [rpcJoinCallbackOccurs], timeout: 10)

        XCTAssertEqual(0, self.cuttlefishContext.ckks!.stateMachine.stateConditions[CKKSStateFetchComplete]!.wait(20 * NSEC_PER_SEC), "State machine should enter CKKSStateFetchComplete")

        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContextForAcceptor, sender: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testPairWithUntrustedAcceptor() throws {

        self.startCKAccountStatusMock()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        let initiatorFirstPacket = self.sendPairingExpectingReply(channel: initiator, packet: nil, reason: "session initiation")

        let firstPacketExpectation = self.expectation(description: "first packet expectation")
        acceptor.exchangePacket(initiatorFirstPacket) { complete, response, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertTrue(complete, "complete should be true")
            XCTAssertNil(response, "response should be nil")
            firstPacketExpectation.fulfill()
        }
        self.wait(for: [firstPacketExpectation], timeout: 10)
    }
}

#endif
