import Foundation

// Apple TVs and watches have no UI to enable or disable this status.
// So, help them out by ignoring all efforts.
extension TPPBPeerStableInfo_UserControllableViewStatus {
    func sanitizeForPlatform(permanentInfo: TPPeerPermanentInfo) -> TPPBPeerStableInfo_UserControllableViewStatus {
        // Unknown is the unknown for any platform
        if self == .UNKNOWN {
            return .UNKNOWN
        }

        if permanentInfo.modelID.hasPrefix("AppleTV") ||
            permanentInfo.modelID.hasPrefix("AudioAccessory") {
            // Apple TVs, and HomePods don't have UI to set this bit. So, they should always sync the
            // user-controlled views to which they have access.
            //
            // Some watches don't have UI to set the bit, but some do.
            //
            // Note that we want this sanitization behavior to be baked into the local OS, which is what owns
            // the UI software, and not in the Policy, which can change.
            return .FOLLOWING
        } else {
            // All other platforms can choose their own fate
            return self
        }
    }
}

extension StableChanges {
    static func change(viewStatus: TPPBPeerStableInfo_UserControllableViewStatus?) -> StableChanges? {
        if viewStatus == nil {
            return nil
        }
        return StableChanges(deviceName: nil,
                             serialNumber: nil,
                             osVersion: nil,
                             policyVersion: nil,
                             policySecrets: nil,
                             setSyncUserControllableViews: viewStatus)
    }
}
