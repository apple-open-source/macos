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
        XCTAssertThrowsError(try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil), "Before doing anything, loading a non-existent account state should fail")

        let resetAndEstablishExpectation = self.expectation(description: "resetAndEstablish callback occurs")

        self.manager.resetAndEstablish(OTControlArguments(configuration: self.otcliqueContext),
                                       resetReason: .testGenerated) { resetError in
                                        XCTAssertNil(resetError, "Should be no error calling resetAndEstablish")
                                        resetAndEstablishExpectation.fulfill()
        }

        self.wait(for: [resetAndEstablishExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let selfPeerID = try self.cuttlefishContext.accountMetadataStore.loadOrCreateAccountMetadata().peerID

        // After resetAndEstablish, you should be able to see the persisted account state
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(selfPeerID, accountState.peerID, "Saved account state should have the same peer ID that prepare returned")
            XCTAssertEqual(accountState.cdpState, .ENABLED, "Saved CDP status should be 'enabled' after a resetAndEstablish")
        } catch {
            XCTFail("error loading account state: \(error)")
        }
    }

    func testLoadToNoAccount() throws {
        // With no identity and Accounts reporting no iCloud account, Octagon should go directly into 'no account'
        self.mockAuthKit.removePrimaryAccount()

        // No CloudKit account, either
        self.accountStatus = .noAccount
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

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
        XCTAssertEqual(self.defaultCKKS.viewList.count, 0, "Should have 0 CKKS views loaded")
    }

    func testNoAccountLeadsToInitialize() throws {
        // With no identity and Accounts reporting no iCloud account, Octagon should go directly into 'no account'
        self.mockAuthKit.removePrimaryAccount()

        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        let account = CloudKitAccount(altDSID: "1234", persona: nil, hsa2: true, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)

        let signinExpectation = self.expectation(description: "sign in returns")
        self.otControl.appleAccountSigned(in: OTControlArguments(altDSID: "1234")) { error in
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
        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // Device is signed out
        self.mockAuthKit.removePrimaryAccount()

        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Sign in occurs
        let newAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)
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

        XCTAssertEqual(self.mockTooManyPeers.timesPopped, 0, "too many peers dialog should not have popped")
        XCTAssertNotEqual(self.mockTooManyPeers.shouldPopCount, 0, "too many peers dialog shouldPopCount should not be zero")

        // On sign-out, octagon should go back to 'no account'
        self.mockAuthKit.removePrimaryAccount()
        XCTAssertNoThrow(self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .unknown, "CDP status should be 'unknown'")

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testSignInWithSlowCKAccount() throws {
        // There is an account configured in the mockAuthKit/mockAccount
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitingForCloudKitAccount, within: 10 * NSEC_PER_SEC)

        self.startCKAccountStatusMock()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testSignInWithPoppedDialog() throws {
        self.startCKAccountStatusMock()

        // Test that we pop the dialog for the third peer, but not the first two
        self.mockTooManyPeers.limit = 2

        // Octagon only examines the JoinState if SOS is enabled
        self.mockSOSAdapter!.setSOSEnabled(false)

        // First peer
        let peer1 = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: "peer1",
                                         sosAdapter: self.mockSOSAdapter!,
                                         accountsAdapter: self.mockAuthKit,
                                         authKitAdapter: self.mockAuthKit,
                                         tooManyPeersAdapter: self.mockTooManyPeers,
                                         tapToRadarAdapter: self.mockTapToRadar,
                                         lockStateTracker: self.lockStateTracker,
                                         deviceInformationAdapter: self.mockDeviceInfo)
        self.assertResetAndBecomeTrusted(context: peer1)

        // TooManyPeers dialog should not have popped, since we set the limit to 2
        XCTAssertEqual(self.mockTooManyPeers.timesPopped, 0, "too many peers dialog should not have popped")
        XCTAssertNotEqual(self.mockTooManyPeers.shouldPopCount, 0, "too many peers dialog shouldPopCount should not be zero")

        // Second peer
        let peer2 = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: "peer2",
                                         sosAdapter: self.mockSOSAdapter!,
                                         accountsAdapter: self.mockAuthKit2,
                                         authKitAdapter: self.mockAuthKit2,
                                         tooManyPeersAdapter: self.mockTooManyPeers,
                                         tapToRadarAdapter: self.mockTapToRadar,
                                         lockStateTracker: self.lockStateTracker,
                                         deviceInformationAdapter: self.mockDeviceInfo)
        self.assertJoinViaProximitySetup(joiningContext: peer2, sponsor: peer1)

        XCTAssertEqual(self.mockTooManyPeers.timesPopped, 0, "too many peers dialog should not have popped")

        // Third peer
        let peer3 = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: "peer3",
                                         sosAdapter: self.mockSOSAdapter!,
                                         accountsAdapter: self.mockAuthKit3,
                                         authKitAdapter: self.mockAuthKit3,
                                         tooManyPeersAdapter: self.mockTooManyPeers,
                                         tapToRadarAdapter: self.mockTapToRadar,
                                         lockStateTracker: self.lockStateTracker,
                                         deviceInformationAdapter: self.mockDeviceInfo)
        self.assertJoinViaProximitySetup(joiningContext: peer3, sponsor: peer1)

        XCTAssertNotEqual(self.mockTooManyPeers.shouldPopCount, 0, "too many peers dialog shouldPopCount should not be zero")
        XCTAssertEqual(self.mockTooManyPeers.timesPopped, 1, "too many peers dialog should have popped")
        XCTAssertEqual(self.mockTooManyPeers.lastPopCount, 2, "should have two peers")
        XCTAssertEqual(self.mockTooManyPeers.lastPopLimit, 2, "should have limit of two")

        // Now test sign out & back into same account, after resetting the mockTooManyPeers adapter
        self.mockTooManyPeers.shouldPopCount = 0
        self.mockTooManyPeers.timesPopped = 0
        self.mockTooManyPeers.lastPopCount = 0
        self.mockTooManyPeers.lastPopLimit = 0

        // On sign-out, octagon should go back to 'no account'
        let peer3AltDSID = try XCTUnwrap(self.mockAuthKit3.primaryAltDSID())
        self.mockAuthKit3.removePrimaryAccount()

        XCTAssertNoThrow(peer3.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: peer3, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: peer3)

        // Now sign back in with same altDSID to ensure the dialog is popped again
        let account = CloudKitAccount(altDSID: peer3AltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available)
        self.mockAuthKit3.add(account)

        XCTAssertNoThrow(try peer3.accountAvailable(try XCTUnwrap(self.mockAuthKit3.primaryAltDSID())!), "Sign-in again shouldn't error")
        self.assertEnters(context: peer3, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // TooManyPeers dialog should have popped again, since there are still 3 peers in this account
        XCTAssertNotEqual(self.mockTooManyPeers.shouldPopCount, 0, "too many peers dialog shouldPopCount should not be zero")
        XCTAssertEqual(self.mockTooManyPeers.timesPopped, 1, "too many peers dialog should have popped")
        XCTAssertEqual(self.mockTooManyPeers.lastPopCount, 3, "should have three peers")
        XCTAssertEqual(self.mockTooManyPeers.lastPopLimit, 2, "should have limit of two")
    }

    // Test behavior when [OTTooManyPeersAdapter shouldPopDialog:] returns false
    // e.g. for external builds, or not iOS, or feature flag disabled
    func testSignInWithoutPoppedDialog() throws {
        self.startCKAccountStatusMock()

        // Pop dialog for any number of peers
        self.mockTooManyPeers.limit = 0
        // But say we're an external device (or feature flag is off)
        self.mockTooManyPeers.shouldPop = false

        // Octagon only examines the JoinState if SOS is enabled
        self.mockSOSAdapter!.setSOSEnabled(false)

        // Sign in the peer
        self.assertResetAndBecomeTrusted(context: self.cuttlefishContext)

        // TooManyPeers dialog should not have popped, since we set the limit to 0 but internal to false
        // But shouldPop should have been called
        XCTAssertNotEqual(self.mockTooManyPeers.shouldPopCount, 0, "too many peers dialog shouldPopCount should not be zero")
        XCTAssertEqual(self.mockTooManyPeers.timesPopped, 0, "too many peers dialog should not have popped")
    }

    func testSignInWithDelayedHSA2Status() throws {
        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // Device is signed out
        self.mockAuthKit.removePrimaryAccount()

        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Sign in occurs, but HSA2 status isn't here yet
        let newAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: false, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)

        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'wait for cdp capable level'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDPCapableSecurityLevel, within: 10 * NSEC_PER_SEC)

        let account2 = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account2)

        XCTAssertNoThrow(try self.cuttlefishContext.idmsTrustLevelChanged(), "Notification of IDMS trust level shouldn't error")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfWaitingForCDP(context: self.cuttlefishContext)

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // On sign-out, octagon should go back to 'no account'
        self.mockAuthKit.removePrimaryAccount()
        XCTAssertNoThrow(self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testSignInWithCDPStateBeforeDelayedHSA2Status() throws {
        // Device is signed out
        self.mockAuthKit.removePrimaryAccount()

        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // CDP state is set. Cool?
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())

        // Sign in occurs, after CDP enablement
        let newAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)
        let account2 = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account2)

        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'untrusted', as everything is in place
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // On sign-out, octagon should go back to 'no account'
        self.mockAuthKit.removePrimaryAccount()
        XCTAssertNoThrow(self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testSetCDPStateWithUnconfiguredArguments() throws {
        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // Device is signed out
        self.mockAuthKit.removePrimaryAccount()

        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Now set the CDP state, but using the OTClique API (and not configuring the context)
        let unconfigured = OTConfigurationContext()
        unconfigured.otControl = self.otControl
        unconfigured.testsEnabled = true
        XCTAssertNoThrow(try OTClique.setCDPEnabled(unconfigured))

        // Sign in occurs, but HSA2 status isn't here yet
        let newAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: false, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'cdp capable security level'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDPCapableSecurityLevel, within: 10 * NSEC_PER_SEC)

        let account2 = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account2)
        XCTAssertNoThrow(try self.cuttlefishContext.idmsTrustLevelChanged(), "Notification of IDMS trust level shouldn't error")

        // and we should skip waiting for CDP, as it's already set
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // On sign-out, octagon should go back to 'no account'
        self.mockAuthKit.removePrimaryAccount()
        XCTAssertNoThrow(self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testSignInWithExistingCuttlefishRecordsSetsCDPStatus() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

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
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

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
        self.mockSOSAdapter!.circleStatusError = NSError(domain: kSOSErrorDomain as String, code: kSOSErrorPublicKeyAbsent, userInfo: nil)
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCError)

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
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

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
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

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
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCError)
        self.mockSOSAdapter!.circleStatusError = NSError(domain: kSOSErrorDomain as String, code: kSOSErrorPublicKeyAbsent, userInfo: nil)

        self.startCKAccountStatusMock()

        // Octagon should discover the right CDP state, and end up in untrusted
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .unknown, "CDP status should be 'unknown'")
    }

    func testDetermineCDPStateFromNetworkFailure() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // default context comes up, but CDP should end up unknown if it can't fetch changes
        self.cuttlefishContext.startOctagonStateMachine()

        self.pauseOctagonStateMachine(context: self.cuttlefishContext, entering: OctagonStateDetermineCDPState)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateDetermineCDPState, within: 10 * NSEC_PER_SEC)

        let container = try! self.tphClient.getContainer(with: try XCTUnwrap(self.cuttlefishContext.activeAccount))
        container.moc.performAndWait {
            container.containerMO.changeToken = nil
        }

        let ckError = NSError(domain: NSURLErrorDomain,
                              code: -1009,
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

        self.reachabilityTracker.setNetworkReachability(false)

        self.releaseOctagonStateMachine(context: self.cuttlefishContext, from: OctagonStateDetermineCDPState)
        self.wait(for: [fetchChangesExpectation], timeout: 20)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .unknown, "CDP status should be 'unknown'")
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Octagon should have not posted a repair CFU while waiting for CDP")

        var error: NSError?
        let cdpstatus = self.cuttlefishContext.getCDPStatus(&error)
        XCTAssertEqual(cdpstatus, .unknown, "cdpstatus should be unknown")
        XCTAssertNil(error, "Should have no error fetching CDP status")

        self.fakeCuttlefishServer.nextFetchErrors.removeAll()
        self.reachabilityTracker.setNetworkReachability(true)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateDetermineCDPState, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)

        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .disabled, "CDP status should be 'disabled'")
        self.assertConsidersSelfWaitingForCDP(context: self.cuttlefishContext)
    }

    func testSignOut() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

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
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, error in
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

        // Accounts deletes the account from its database before issuing the command. Simulate that.
        let altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        self.mockAuthKit.removePrimaryAccount()

        let signedOutExpectation = self.expectation(description: "signout callback occurs")
        self.manager.appleAccountSignedOut(OTControlArguments(altDSID: altDSID)) { error in
            XCTAssertNil(error, "Should be no error signing out")
            signedOutExpectation.fulfill()
        }
        self.wait(for: [signedOutExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        // And 'dump' should show nothing
        let signedOutDumpExpectation = self.expectation(description: "dump callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, error in
            XCTAssertNil(error, "Should be no error dumping data")
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            XCTAssertEqual(egoSelf!.count, 0, "egoSelf should have zero elements")

            signedOutDumpExpectation.fulfill()
        }
        self.wait(for: [signedOutDumpExpectation], timeout: 10)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)

        // check trust status
        let checkTrustExpectation = self.expectation(description: "checkTrustExpectation callback occurs")
        let configuration = OTOperationConfiguration()
        self.cuttlefishContext.rpcTrustStatus(configuration) { _, _, _, _, _, _ in
            checkTrustExpectation.fulfill()
        }
        self.wait(for: [checkTrustExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // And 'dump' should show nothing
        let signedOutDumpExpectationAfterCheckTrustStatus = self.expectation(description: "dump callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, error in
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
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)

        // Turn off the CK account too
        self.accountStatus = .noAccount
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        XCTAssertNoThrow(self.cuttlefishContext.accountNoLongerAvailable(), "Should be no issue signing out")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .unknown, "CDP status should be 'unknown'")
    }

    func testNoAccountTimeoutTransitionWatcher() throws {
        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // With no identity and Accounts reporting no iCloud account, Octagon should go directly into 'no account'
        self.mockAuthKit.removePrimaryAccount()

        self.startCKAccountStatusMock()

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
