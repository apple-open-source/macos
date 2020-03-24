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

    func testJoin() {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.startOctagonStateMachine()
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let rpcEpochCallbacks = self.expectation(description: "rpcEpoch callback occurs")
        clientStateMachine.rpcEpoch(self.cuttlefishContextForAcceptor) { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertTrue(epoch == 1, "epoch should be 1")
            rpcEpochCallbacks.fulfill()
        }
        self.wait(for: [rpcEpochCallbacks], timeout: 10)

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
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

        clientStateMachine.rpcVoucher(self.cuttlefishContextForAcceptor, peerID: p, permanentInfo: pI, permanentInfoSig: pIS, stableInfo: sI, stableInfoSig: sIS) { voucher, voucherSig, error in
            XCTAssertNotNil(v, "Prepare should have returned a voucher")
            XCTAssertNotNil(vS, "Prepare should have returned a voucherSig")
            XCTAssertNil(error, "error should be nil")

            v = voucher
            vS = voucherSig

            rpcVoucherCallback.fulfill()

            XCTAssertEqual(0, (clientStateMachine.stateConditions[OctagonStateAcceptorDone] as! CKKSCondition).wait(10 * NSEC_PER_SEC), "State machine should enter 'OctagonStateAcceptorDone'")
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

        XCTAssertEqual(0, (clientStateMachine.stateConditions[OctagonStateAcceptorDone] as! CKKSCondition).wait(10 * NSEC_PER_SEC), "State machine should enter 'OctagonStateAcceptorDone'")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContextForAcceptor, sender: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testJoinRetry() {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.startOctagonStateMachine()
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let rpcEpochCallbacks = self.expectation(description: "rpcEpoch callback occurs")
        clientStateMachine.rpcEpoch(self.cuttlefishContextForAcceptor) { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertTrue(epoch == 1, "epoch should be 1")
            rpcEpochCallbacks.fulfill()
        }
        self.wait(for: [rpcEpochCallbacks], timeout: 10)

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
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

        clientStateMachine.rpcVoucher(self.cuttlefishContextForAcceptor, peerID: p, permanentInfo: pI, permanentInfoSig: pIS, stableInfo: sI, stableInfoSig: sIS) { voucher, voucherSig, error in
            XCTAssertNotNil(v, "Prepare should have returned a voucher")
            XCTAssertNotNil(vS, "Prepare should have returned a voucherSig")
            XCTAssertNil(error, "error should be nil")

            v = voucher
            vS = voucherSig

            rpcVoucherCallback.fulfill()

            XCTAssertEqual(0, (clientStateMachine.stateConditions[OctagonStateAcceptorDone] as! CKKSCondition).wait(10 * NSEC_PER_SEC), "State machine should enter 'OctagonStateAcceptorDone'")
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

        self.cuttlefishContext.rpcJoin(v, vouchSig: vS) { error in
            XCTAssertNil(error, "error should be nil")
            rpcJoinCallbackOccurs.fulfill()
        }

        self.wait(for: [rpcJoinCallbackOccurs], timeout: 64)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")

        XCTAssertEqual(0, (clientStateMachine.stateConditions[OctagonStateAcceptorDone] as! CKKSCondition).wait(10 * NSEC_PER_SEC), "State machine should enter 'OctagonStateAcceptorDone'")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContextForAcceptor, sender: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testJoinRetryFail() {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.startOctagonStateMachine()
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let rpcEpochCallbacks = self.expectation(description: "rpcEpoch callback occurs")
        clientStateMachine.rpcEpoch(self.cuttlefishContextForAcceptor) { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertTrue(epoch == 1, "epoch should be 1")
            rpcEpochCallbacks.fulfill()
        }
        self.wait(for: [rpcEpochCallbacks], timeout: 10)

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
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

        clientStateMachine.rpcVoucher(self.cuttlefishContextForAcceptor, peerID: p, permanentInfo: pI, permanentInfoSig: pIS, stableInfo: sI, stableInfoSig: sIS) { voucher, voucherSig, error in
            XCTAssertNotNil(v, "Prepare should have returned a voucher")
            XCTAssertNotNil(vS, "Prepare should have returned a voucherSig")
            XCTAssertNil(error, "error should be nil")

            v = voucher
            vS = voucherSig

            rpcVoucherCallback.fulfill()

            XCTAssertEqual(0, (clientStateMachine.stateConditions[OctagonStateAcceptorDone] as! CKKSCondition).wait(10 * NSEC_PER_SEC), "State machine should enter 'OctagonStateAcceptorDone'")
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
    }

    func testJoinWithCKKSConflict() {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/

        self.getAcceptorInCircle()

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        self.silentFetchesAllowed = false
        self.expectCKFetchAndRun {
            self.putFakeKeyHierarchiesInCloudKit()
            self.putFakeDeviceStatusesInCloudKit()
            self.silentFetchesAllowed = true
        }

        clientStateMachine.startOctagonStateMachine()
        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let rpcEpochCallbacks = self.expectation(description: "rpcEpoch callback occurs")
        clientStateMachine.rpcEpoch(self.cuttlefishContextForAcceptor) { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertTrue(epoch == 1, "epoch should be 1")
            rpcEpochCallbacks.fulfill()
        }
        self.wait(for: [rpcEpochCallbacks], timeout: 10)

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
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

        clientStateMachine.rpcVoucher(self.cuttlefishContextForAcceptor, peerID: p, permanentInfo: pI, permanentInfoSig: pIS, stableInfo: sI, stableInfoSig: sIS) { voucher, voucherSig, error in
            XCTAssertNotNil(v, "Prepare should have returned a voucher")
            XCTAssertNotNil(vS, "Prepare should have returned a voucherSig")
            XCTAssertNil(error, "error should be nil")

            v = voucher
            vS = voucherSig

            rpcVoucherCallback.fulfill()

            XCTAssertEqual(0, (clientStateMachine.stateConditions[OctagonStateAcceptorDone] as! CKKSCondition).wait(10 * NSEC_PER_SEC), "State machine should enter 'OctagonStateAcceptorDone'")
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

        XCTAssertEqual(0, (clientStateMachine.stateConditions[OctagonStateAcceptorDone] as! CKKSCondition).wait(10 * NSEC_PER_SEC), "State machine should enter 'OctagonStateAcceptorDone'")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testNextJoiningMessageInterface() {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/
        self.getAcceptorInCircle()

        let rpcEpochCallbackOccurs = self.expectation(description: "rpcEpoch callback occurs")

        self.manager.rpcEpoch(with: self.acceptorPairingConfig) { epoch, error in
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
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
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

        self.manager.rpcPrepareIdentityAsApplicant(with: self.initiatorPairingConfig) { pID, pI, pISig, sI, sISig, error in
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

        let voucherCallback = self.expectation(description: "creating voucher message callback")
        self.manager.rpcVoucher(with: self.acceptorPairingConfig, peerID: peerID, permanentInfo: permanentInfo, permanentInfoSig: permanentInfoSig, stableInfo: stableInfo, stableInfoSig: stableInfoSig ) { v, vS, error in
            XCTAssertNil(error, "error should be nil")
            voucher = v
            voucherSig = vS
            voucherCallback.fulfill()
        }
        self.wait(for: [voucherCallback], timeout: 10)

        let rpcJoinCallback = self.expectation(description: "joining octagon callback")
        self.manager.rpcJoin(with: self.initiatorPairingConfig, vouchData: voucher, vouchSig: voucherSig) { error in
            XCTAssertNil(error, "error should be nil")
            rpcJoinCallback.fulfill()
        }
        self.wait(for: [rpcJoinCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: self.cuttlefishContextForAcceptor, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContextForAcceptor)
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")
        self.verifyDatabaseMocks()
    }

    func testEpochFetching() {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)
        self.getAcceptorInCircle()

        let rpcEpochCallbacks = self.expectation(description: "rpcEpoch callback occurs")
        clientStateMachine.startOctagonStateMachine()
        clientStateMachine.rpcEpoch(self.cuttlefishContextForAcceptor) { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertTrue(epoch == 1, "epoch should be 1")
            rpcEpochCallbacks.fulfill()
        }

        self.wait(for: [rpcEpochCallbacks], timeout: 10)
        XCTAssertEqual(0, (clientStateMachine.stateConditions[OctagonStateAcceptorAwaitingIdentity] as! CKKSCondition).wait(10 * NSEC_PER_SEC), "State machine should enter 'OctagonStateAcceptorAwaitingIdentity'")
    }

    func testVoucherCreation() {
        self.startCKAccountStatusMock()

        /*Setup acceptor first*/

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)
        self.getAcceptorInCircle()

        let rpcEpochCallbacks = self.expectation(description: "rpcEpoch callback occurs")
        clientStateMachine.startOctagonStateMachine()
        clientStateMachine.rpcEpoch(self.cuttlefishContextForAcceptor) { epoch, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertTrue(epoch == 1, "epoch should be 1")
            rpcEpochCallbacks.fulfill()
        }

        self.wait(for: [rpcEpochCallbacks], timeout: 10)
        XCTAssertEqual(0, (clientStateMachine.stateConditions[OctagonStateAcceptorAwaitingIdentity] as! CKKSCondition).wait(10 * NSEC_PER_SEC), "State machine should enter 'OctagonStateAcceptorAwaitingIdentity'")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateMachineNotStarted, within: 10 * NSEC_PER_SEC)

        /* now initiator's turn*/
        self.manager.startOctagonStateMachine(self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { _ in
        }

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

        clientStateMachine.rpcVoucher(self.cuttlefishContextForAcceptor, peerID: p, permanentInfo: pI, permanentInfoSig: pIS, stableInfo: sI, stableInfoSig: sIS) { voucher, voucherSig, error in
            XCTAssertNotNil(v, "Prepare should have returned a voucher")
            XCTAssertNotNil(vS, "Prepare should have returned a voucherSIg")
            XCTAssertNil(error, "error should be nil")

            v = voucher
            vS = voucherSig

            rpcVoucherCallback.fulfill()
        }

        self.wait(for: [rpcVoucherCallback], timeout: 10)
        XCTAssertEqual(0, (clientStateMachine.stateConditions[OctagonStateAcceptorDone] as! CKKSCondition).wait(10 * NSEC_PER_SEC), "State machine should enter 'OctagonStateAcceptorDone'")
    }

    func testPrepareTimeoutIfStateMachineUnstarted() {
        self.startCKAccountStatusMock()

        let rpcCallbackOccurs = self.expectation(description: "rpcPrepare callback occurs")
        self.initiatorPairingConfig.timeout = Int64(2 * NSEC_PER_SEC)

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

    func testProximitySetupUsingCliqueOctagonAndSOS() {
        self.startCKAccountStatusMock()

        OctagonSetPlatformSupportsSOS(true)
        OctagonSetIsEnabled(true)

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.startOctagonStateMachine()
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
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
        let acceptorThirdPacket = self.sendPairingExpectingCompletionAndReply(channel: acceptor, packet: initiatorThirdPacket, reason: "acceptor third packet")
        XCTAssertFalse(try self.tlkInPairingChannel(packet: acceptorThirdPacket), "pairing channel should NOT transport TLKs for SOS+Octagon")

        /* INITIATOR FOURTH STEP*/
        self.sendPairingExpectingCompletion(channel: initiator, packet: acceptorThirdPacket, reason: "final packet receipt")

        // pairing completes here

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        clientStateMachine.notifyContainerChange()

        // Initiator should join!
        self.assertEnters(context: initiator1Context, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: initiator1Context)
        self.verifyDatabaseMocks()

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

        self.assertSOSSuccess()
    }

    func testProximitySetupUsingCliqueOctagonOnly() throws {
        OctagonSetPlatformSupportsSOS(false)
        OctagonSetIsEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.startOctagonStateMachine()
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
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
        let acceptorVoucherPacket = self.sendPairingExpectingCompletionAndReply(channel: acceptor, packet: initiatorPreparedIdentityPacket, reason: "acceptor third packet")

        // the tlks are in the 3rd roundtrip, but lets check here too
        XCTAssertFalse(try self.tlkInPairingChannel(packet: acceptorVoucherPacket), "pairing channel should not transport TLKs for octagon")

        /* INITIATOR THIRD STEP*/
        self.sendPairingExpectingCompletion(channel: initiator, packet: acceptorVoucherPacket, reason: "final packet receipt")

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        clientStateMachine.notifyContainerChange()

        // Initiator should join!
        self.assertEnters(context: initiator1Context, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: initiator1Context)
        self.verifyDatabaseMocks()

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
    }

    func testProximitySetupUsingCliqueSOSOnly() {
        OctagonSetPlatformSupportsSOS(true)
        OctagonSetIsEnabled(false)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.startOctagonStateMachine()
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1",
                                                               initiatorContextID: OTDefaultContext,
                                                               acceptorContextID: self.contextForAcceptor,
                                                               initiatorUniqueID: self.initiatorName,
                                                               acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
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

        // the tlks are in the 3rd roundtrip, but lets check here too
        XCTAssertFalse(try self.tlkInPairingChannel(packet: acceptorVoucherPacket), "pairing channel should transport TLKs for SOS not 2nd step though")

        /* INITIATOR THIRD STEP*/
        let initiatorThirdPacket = self.sendPairingExpectingReply(channel: initiator, packet: acceptorVoucherPacket, reason: "intitiator third packet")

        /* ACCEPTOR THIRD STEP */
        let acceptorThirdPacket = self.sendPairingExpectingCompletionAndReply(channel: acceptor, packet: initiatorThirdPacket, reason: "acceptor third packet")

        XCTAssertTrue(try self.tlkInPairingChannel(packet: acceptorThirdPacket), "pairing channel should transport TLKs for SOS")

        /* INITIATOR FORTH STEP*/
        self.sendPairingExpectingCompletion(channel: initiator, packet: acceptorThirdPacket, reason: "final packet receipt")

        self.assertSOSSuccess()
    }

    func testProximitySetupOctagonAndSOSWithSOSFailure() {
        //ensure Octagon protocol continues even if SOS fails in some way.
        OctagonSetPlatformSupportsSOS(true)
        OctagonSetIsEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.startOctagonStateMachine()
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        initiator.setSOSMessageFailForTesting(true)
        acceptor.setSOSMessageFailForTesting(true)

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
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

        clientStateMachine.notifyContainerChange()

        // Initiator should join!
        self.assertEnters(context: initiator1Context, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: initiator1Context)
        self.verifyDatabaseMocks()

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
    }

    func testProximitySetupOctagonAndSOSWithOcatagonInitiatorMessage1Failure() {
        //ensure Octagon protocol halts if enabled and encounters a failure
        OctagonSetPlatformSupportsSOS(true)
        OctagonSetIsEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.startOctagonStateMachine()
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        initiator.setOctagonMessageFailForTesting(true)

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
            XCTAssertNil(error, "error should be nil")
            signInCallback.fulfill()
        }
        self.wait(for: [signInCallback], timeout: 10)

        /* INITIATOR FIRST RTT JOINING MESSAGE*/
        self.sendPairingExpectingCompletion(channel: initiator, packet: nil, reason: "error on first message")
    }

    func testProximitySetupOctagonAndSOSWithOctagonAcceptorMessage1Failure() {
        //ensure Octagon protocol continues even if SOS fails in some way.
        OctagonSetPlatformSupportsSOS(true)
        OctagonSetIsEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.startOctagonStateMachine()
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
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

    func testProximitySetupOctagonAndSOSWithOctagonInitiatorMessage2Failure() {
        OctagonSetPlatformSupportsSOS(true)
        OctagonSetIsEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.startOctagonStateMachine()
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
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

        //set up initiator's message 2 to fail
        initiator.setOctagonMessageFailForTesting(true)

        initiator.exchangePacket(acceptorEpochPacket) { complete, packet, error in
            XCTAssertNil(error, "should be no error")
            XCTAssertTrue(complete, "should be true")
            XCTAssertNil(packet, "packet should not be nil")
            secondInitiatorCallback.fulfill()
        }

        self.wait(for: [secondInitiatorCallback], timeout: 10)
    }

    func testProximitySetupOctagonAndSOSWithOctagonAcceptorMessage2Failure() {
        OctagonSetPlatformSupportsSOS(true)
        OctagonSetIsEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.startOctagonStateMachine()
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
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

    func testProximitySetupOctagonAndSOSWithOctagonInitiatorMessage3Failure() {
        OctagonSetPlatformSupportsSOS(true)
        OctagonSetIsEnabled(true)
        self.startCKAccountStatusMock()

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.startOctagonStateMachine()
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
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

        self.mockSOSAdapter.trustedPeers.add(AcceptorSOSPeer)
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)

        let acceptor = self.manager.context(forContainerName: OTCKContainerName,
                                            contextID: self.contextForAcceptor,
                                            sosAdapter: self.mockSOSAdapter,
                                            authKitAdapter: self.mockAuthKit2,
                                            lockStateTracker: self.lockStateTracker,
                                            accountStateTracker: self.accountStateTracker,
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

    func testProximitySetupUsingCliqueAcceptorResolvesVersionToSOSOnly() {
        self.startCKAccountStatusMock()

        OctagonSetPlatformSupportsSOS(true)
        OctagonSetIsEnabled(true)

        let newSOSPeer = createSOSPeer(peerID: "sos-peer-id")
        self.mockSOSAdapter.selfPeer = newSOSPeer
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.mockSOSAdapter.trustedPeers.add(newSOSPeer)

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.startOctagonStateMachine()
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        acceptor.setSessionSupportsOctagonForTesting(false)

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
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

    func testProximitySetupUsingCliqueInitiatorResolvesVersionToSOSOnly() {
        self.startCKAccountStatusMock()

        OctagonSetPlatformSupportsSOS(true)
        OctagonSetIsEnabled(true)

        self.getAcceptorInCircle()

        let initiator1Context = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        let clientStateMachine = self.manager.clientStateMachine(forContainerName: OTCKContainerName, contextID: self.contextForAcceptor, clientName: self.initiatorName)

        clientStateMachine.startOctagonStateMachine()
        initiator1Context.startOctagonStateMachine()

        self.assertEnters(context: initiator1Context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let (acceptor, initiator) = self.setupPairingEndpoints(withPairNumber: "1", initiatorContextID: OTDefaultContext, acceptorContextID: self.contextForAcceptor, initiatorUniqueID: self.initiatorName, acceptorUniqueID: "acceptor-2")

        initiator.setSessionSupportsOctagonForTesting(false)

        XCTAssertNotNil(acceptor, "acceptor should not be nil")
        XCTAssertNotNil(initiator, "initiator should not be nil")

        let signInCallback = self.expectation(description: "trigger sign in")
        self.otControl.sign(in: "348576349857", container: OTCKContainerName, context: OTDefaultContext) { error in
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

}

#endif
