#if OCTAGON
import Foundation

extension SignedPeerStableInfo {
    func stableInfo() -> TPPeerStableInfo {
        let newStableInfo = TPPeerStableInfo(data: self.peerStableInfo, sig: self.sig)
        XCTAssertNotNil(newStableInfo, "should be able to make a stableInfo from protobuf")
        return newStableInfo!
    }
}

extension SignedPeerDynamicInfo {
    func dynamicInfo() -> TPPeerDynamicInfo {
        let newDynamicInfo = TPPeerDynamicInfo(data: self.peerDynamicInfo, sig: self.sig)
        XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamicInfo from protobuf")
        return newDynamicInfo!
    }
}

extension EstablishRequest {
    func permanentInfo() -> TPPeerPermanentInfo {
        XCTAssertTrue(self.hasPeer, "establish request should have a peer")
        XCTAssertTrue(self.peer.hasPermanentInfoAndSig, "establish request should have a permanentInfo")
        let newPermanentInfo = TPPeerPermanentInfo(peerID: self.peer.peerID,
                                                   data: self.peer.permanentInfoAndSig.peerPermanentInfo,
                                                   sig: self.peer.permanentInfoAndSig.sig,
                                                   keyFactory: TPECPublicKeyFactory())
        XCTAssertNotNil(newPermanentInfo, "should be able to make a permanantInfo from protobuf")
        return newPermanentInfo!
    }
}

#endif
