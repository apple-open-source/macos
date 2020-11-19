/*
* Copyright (c) 2019 Apple Inc. All Rights Reserved.
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

#if OCTAGON

class OctagonAccountTests: OctagonTestsBase {
    func testAccountSave() throws {
        let contextName = OTDefaultContext
        let containerName = OTCKContainerName

        self.startCKAccountStatusMock()

        // Before resetAndEstablish, there shouldn't be any stored account state
        XCTAssertThrowsError(try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName), "Before doing anything, loading a non-existent account state should fail")

        let resetAndEstablishExpectation = self.expectation(description: "resetAndEstablish callback occurs")
        self.manager.resetAndEstablish(containerName,
                                       context: contextName,
                                       altDSID: "new altDSID",
                                       resetReason: .testGenerated) { resetError in
                                        XCTAssertNil(resetError, "Should be no error calling resetAndEstablish")
                                        resetAndEstablishExpectation.fulfill()
        }

        self.wait(for: [resetAndEstablishExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let selfPeerID = try self.cuttlefishContext.accountMetadataStore.loadOrCreateAccountMetadata().peerID

        // After resetAndEstablish, you should be able to see the persisted account state
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName)
            XCTAssertEqual(selfPeerID, accountState.peerID, "Saved account state should have the same peer ID that prepare returned")
            XCTAssertEqual(accountState.cdpState, .ENABLED, "Saved CDP status should be 'enabled' after a resetAndEstablish")
        } catch {
            XCTFail("error loading account state: \(error)")
        }
    }

    func testLoadToNoAccount() throws {
        // No CloudKit account, either
        self.accountStatus = .noAccount
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // With no identity and AuthKit reporting no iCloud account, Octagon should go directly into 'no account'
        self.mockAuthKit.altDSID = nil

        let asyncExpectation = self.expectation(description: "dispatch works")
        let quiescentExpectation = self.expectation(description: "quiescence has been determined")
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            asyncExpectation.fulfill()

            let c = self!.cuttlefishContext.stateMachine.paused
            XCTAssertEqual(0, c.wait(10 * NSEC_PER_SEC), "State machine should become quiescent")
            quiescentExpectation.fulfill()
        }
        // Wait for the block above to fire before continuing
        self.wait(for: [asyncExpectation], timeout: 10)

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        XCTAssertTrue(self.cuttlefishContext.stateMachine.isPaused(), "State machine should be stopped")
        self.assertNoAccount(context: self.cuttlefishContext)

        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "State machine should be quiescent")

        self.wait(for: [quiescentExpectation], timeout: 10)

        // Since there's no acocunt, CKKS shouldn't even have any views loaded
        XCTAssertEqual(self.injectedManager!.views.count, 0, "Should have 0 CKKS views loaded")
    }

    func testNoAccountLeadsToInitialize() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // With no identity and AuthKit reporting no iCloud account, Octagon should go directly into 'no account'
        self.mockAuthKit.altDSID = nil

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        self.mockAuthKit.altDSID = "1234"
        let signinExpectation = self.expectation(description: "sign in returns")
        self.otControl.sign(in: "1234", container: nil, context: OTDefaultContext) { error in
            XCTAssertNil(error, "error should be nil")
            signinExpectation.fulfill()
        }
        self.wait(for: [signinExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)

        // And now the CDP bit is set...
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testSignIn() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // Device is signed out
        self.mockAuthKit.altDSID = nil
        self.mockAuthKit.hsa2 = false

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Sign in occurs
        let newAltDSID = UUID().uuidString
        self.mockAuthKit.altDSID = newAltDSID
        self.mockAuthKit.hsa2 = true
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'waitforcdp'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfWaitingForCDP(context: self.cuttlefishContext)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .disabled, "CDP status should be 'disabled'")
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Octagon should have not posted a repair CFU while waiting for CDP")

        // And CDP is enabled:
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .enabled, "CDP status should be 'enabled'")

        #if !os(tvOS)
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Octagon should have posted a repair CFU")
        #else
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "posted should be false on tvOS; there aren't any devices around to repair it")
        #endif

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // On sign-out, octagon should go back to 'no account'
        self.mockAuthKit.altDSID = nil
        XCTAssertNoThrow(try self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .unknown, "CDP status should be 'unknown'")

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testSignInWithDelayedHSA2Status() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // Device is signed out
        self.mockAuthKit.altDSID = nil
        self.mockAuthKit.hsa2 = false

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Sign in occurs, but HSA2 status isn't here yet
        let newAltDSID = UUID().uuidString
        self.mockAuthKit.altDSID = newAltDSID
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'waitforhsa2'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForHSA2, within: 10 * NSEC_PER_SEC)

        self.mockAuthKit.hsa2 = true
        XCTAssertNoThrow(try self.cuttlefishContext.idmsTrustLevelChanged(), "Notification of IDMS trust level shouldn't error")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfWaitingForCDP(context: self.cuttlefishContext)

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // On sign-out, octagon should go back to 'no account'
        self.mockAuthKit.altDSID = nil
        XCTAssertNoThrow(try self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testSignInWithCDPStateBeforeDelayedHSA2Status() throws {
        self.startCKAccountStatusMock()

        // Device is signed out
        self.mockAuthKit.altDSID = nil
        self.mockAuthKit.hsa2 = false

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // CDP state is set. Cool?
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())

        // Sign in occurs, but HSA2 status isn't here yet
        let newAltDSID = UUID().uuidString
        self.mockAuthKit.altDSID = newAltDSID
        self.mockAuthKit.hsa2 = true
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'untrusted', as everything is in place
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // On sign-out, octagon should go back to 'no account'
        self.mockAuthKit.altDSID = nil
        XCTAssertNoThrow(try self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testSetCDPStateWithUnconfiguredArguments() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // Device is signed out
        self.mockAuthKit.altDSID = nil
        self.mockAuthKit.hsa2 = false

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Now set the CDP state, but using the OTClique API (and not configuring the context)
        let unconfigured = OTConfigurationContext()
        unconfigured.otControl = self.otControl
        XCTAssertNoThrow(try OTClique.setCDPEnabled(unconfigured))

        // Sign in occurs, but HSA2 status isn't here yet
        let newAltDSID = UUID().uuidString
        self.mockAuthKit.altDSID = newAltDSID
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'waitforhsa2'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForHSA2, within: 10 * NSEC_PER_SEC)

        self.mockAuthKit.hsa2 = true
        XCTAssertNoThrow(try self.cuttlefishContext.idmsTrustLevelChanged(), "Notification of IDMS trust level shouldn't error")

        // and we should skip waiting for CDP, as it's already set
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // On sign-out, octagon should go back to 'no account'
        self.mockAuthKit.altDSID = nil
        XCTAssertNoThrow(try self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testSignInWithExistingCuttlefishRecordsSetsCDPStatus() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // signing in to an account with pre-existing Octagon data should turn on the CDP bit by default:
        // no setting needed.

        let remote = self.makeInitiatorContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

        // when this context boots up, it should go straight into untrusted, and set its CDP bit
        // since there's already CDP data in the account
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .enabled, "CDP status should be 'enabled'")

        #if !os(tvOS)
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Octagon should have posted a repair CFU after the CDP bit was set")
        #else
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Octagon should not have posted on tvOS; there aren't any iphones around to repair it")
        #endif
    }

    func testEnableCDPStatusIfNotificationArrives() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // default context comes up, but CDP is not enabled
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfWaitingForCDP(context: self.cuttlefishContext)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .disabled, "CDP status should be 'disabled'")
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Octagon should have not posted a repair CFU while waiting for CDP")

        // If a cuttlefish push occurs without any data existing, the CDP bit should stay off
        self.sendContainerChangeWaitForFetchForStates(context: self.cuttlefishContext, states: [OctagonStateWaitForCDPUpdated, OctagonStateDetermineCDPState])
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfWaitingForCDP(context: self.cuttlefishContext)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .disabled, "CDP status should be 'disabled'")
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Octagon should have not posted a repair CFU while waiting for CDP")

        // Another peer comes along and performs Octagon operations
        let remote = self.makeInitiatorContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

        // And some SOS operations. SOS now returns "error" when asked its state
        self.mockSOSAdapter.circleStatusError = NSError(domain: kSOSErrorDomain as String, code: kSOSErrorPublicKeyAbsent, userInfo: nil)
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCError)

        // And after the update, the context should go into 'untrusted'
        self.sendContainerChangeWaitForFetchForStates(context: self.cuttlefishContext, states: [OctagonStateWaitForCDPUpdated, OctagonStateDetermineCDPState])
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .enabled, "CDP status should be 'enabled'")

        #if !os(tvOS)
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Octagon should have posted a repair CFU")
        #else
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Octagon should not have posted on tvOS; there aren't any iphones around to repair it")
        #endif
    }

    func testEnableCDPStatusIfNotificationArrivesWithoutCreatingSOSCircle() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // default context comes up, but CDP is not enabled
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfWaitingForCDP(context: self.cuttlefishContext)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .disabled, "CDP status should be 'disabled'")
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Octagon should have not posted a repair CFU while waiting for CDP")

        // If a cuttlefish push occurs without any data existing, the CDP bit should stay off
        self.sendContainerChangeWaitForFetchForStates(context: self.cuttlefishContext, states: [OctagonStateWaitForCDPUpdated, OctagonStateDetermineCDPState])
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfWaitingForCDP(context: self.cuttlefishContext)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .disabled, "CDP status should be 'disabled'")
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Octagon should have not posted a repair CFU while waiting for CDP")

        // Another peer comes along and performs Octagon operations
        let remote = self.makeInitiatorContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

        // Unlike testEnableCDPStatusIfNotificationArrives, SOS remains in 'absent', simulating a non-sos platform creating Octagon

        // And after the update, the context should go into 'untrusted'
        self.sendContainerChangeWaitForFetchForStates(context: self.cuttlefishContext, states: [OctagonStateWaitForCDPUpdated, OctagonStateDetermineCDPState])
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .enabled, "CDP status should be 'enabled'")

        #if !os(tvOS)
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Octagon should have posted a repair CFU")
        #else
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Octagon should not have posted on tvOS; there aren't any iphones around to repair it")
        #endif
    }

    func testCDPEnableAPIRaceWithCDPStateDetermination() throws {
        // The API call to enable CDP might occur while Octagon is figuring out that it should be disabled
        // Octagon should respect the API call

        // SOS is absent
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.startCKAccountStatusMock()

        // The initial CDP-enable API call happens while Octagon is fetching the world's state
        let fetchExpectation = self.expectation(description: "fetchChanges called")
        self.fakeCuttlefishServer.fetchChangesListener = { [unowned self] _ in
            do {
                try self.cuttlefishContext.setCDPEnabled()
            } catch {
                XCTFail("Expected to be able to set CDP status without error: \(error)")
            }
            fetchExpectation.fulfill()
            return nil
        }

        self.cuttlefishContext.startOctagonStateMachine()

        // Octagon should go into 'untrusted', as the API call said that CDP was there
        self.wait(for: [fetchExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testDetermineCDPStateFromSOSError() throws {
        // If SOS reports that it doesn't have the user key, a circle might exist and it might not
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCError)
        self.mockSOSAdapter.circleStatusError = NSError(domain: kSOSErrorDomain as String, code: kSOSErrorPublicKeyAbsent, userInfo: nil)

        self.startCKAccountStatusMock()

        // Octagon should discover the right CDP state, and end up in untrusted
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .unknown, "CDP status should be 'unknown'")
    }

    func testSignOut() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        do {
            let clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
        }

        // Now, we should be in 'ready'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // And 'dump' should show some information
        let dumpExpectation = self.expectation(description: "dump callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, error in
            XCTAssertNil(error, "Should be no error dumping data")
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let peerID = egoSelf!["peerID"] as? String
            XCTAssertNotNil(peerID, "peerID should not be nil")

            dumpExpectation.fulfill()
        }
        self.wait(for: [dumpExpectation], timeout: 10)

        // Turn off the CK account too
        self.accountStatus = .noAccount
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        XCTAssertNoThrow(try self.cuttlefishContext.accountNoLongerAvailable(), "Should be no issue signing out")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        // And 'dump' should show nothing
        let signedOutDumpExpectation = self.expectation(description: "dump callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, error in
            XCTAssertNil(error, "Should be no error dumping data")
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            XCTAssertEqual(egoSelf!.count, 0, "egoSelf should have zero elements")

            signedOutDumpExpectation.fulfill()
        }
        self.wait(for: [signedOutDumpExpectation], timeout: 10)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)

        //check trust status
        let checkTrustExpectation = self.expectation(description: "checkTrustExpectation callback occurs")
        let configuration = OTOperationConfiguration()
        self.cuttlefishContext.rpcTrustStatus(configuration) { _, _, _, _, _, _ in
            checkTrustExpectation.fulfill()
        }
        self.wait(for: [checkTrustExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // And 'dump' should show nothing
        let signedOutDumpExpectationAfterCheckTrustStatus = self.expectation(description: "dump callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, error in
            XCTAssertNil(error, "Should be no error dumping data")
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            XCTAssertEqual(egoSelf!.count, 0, "egoSelf should have zero elements")

            signedOutDumpExpectationAfterCheckTrustStatus.fulfill()
        }
        self.wait(for: [signedOutDumpExpectationAfterCheckTrustStatus], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)
    }

    func testSignOutFromWaitForCDP() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)

        // Turn off the CK account too
        self.accountStatus = .noAccount
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        XCTAssertNoThrow(try self.cuttlefishContext.accountNoLongerAvailable(), "Should be no issue signing out")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .unknown, "CDP status should be 'unknown'")
    }

    func testNoAccountTimeoutTransitionWatcher() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // With no identity and AuthKit reporting no iCloud account, Octagon should go directly into 'no account'
        self.mockAuthKit.altDSID = nil

        self.cuttlefishContext.startOctagonStateMachine()
        self.cuttlefishContext.stateMachine.setWatcherTimeout(2 * NSEC_PER_SEC)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        XCTAssertTrue(self.cuttlefishContext.stateMachine.isPaused(), "State machine should be stopped")
        self.assertNoAccount(context: self.cuttlefishContext)
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "State machine should be quiescent")

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        self.cuttlefishContext.join(withBottle: "bottleID", entropy: Data(), bottleSalt: "peer2AltDSID") { error in
            XCTAssertNotNil(error, "error should not be nil")
            joinWithBottleExpectation.fulfill()
        }
        self.wait(for: [joinWithBottleExpectation], timeout: 3)
    }
}

#endif
