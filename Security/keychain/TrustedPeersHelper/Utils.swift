/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

extension SignedPeerPermanentInfo {
    init(_ permanentInfo: TPPeerPermanentInfo) {
        self.peerPermanentInfo = permanentInfo.data
        self.sig = permanentInfo.sig
    }

    func toPermanentInfo(peerID: String) -> TPPeerPermanentInfo? {
        return TPPeerPermanentInfo(peerID: peerID,
                                   data: self.peerPermanentInfo,
                                   sig: self.sig,
                                   keyFactory: TPECPublicKeyFactory())
    }
}

extension SignedPeerStableInfo {
    init(_ stableInfo: TPPeerStableInfo) {
        self.peerStableInfo = stableInfo.data
        self.sig = stableInfo.sig
    }

    func toStableInfo() -> TPPeerStableInfo? {
        return TPPeerStableInfo(data: self.peerStableInfo, sig: self.sig)
    }
}

extension SignedPeerDynamicInfo {
    init(_ dynamicInfo: TPPeerDynamicInfo) {
        self.peerDynamicInfo = dynamicInfo.data
        self.sig = dynamicInfo.sig
    }

    func toDynamicInfo() -> TPPeerDynamicInfo? {
        return TPPeerDynamicInfo(data: self.peerDynamicInfo, sig: self.sig)
    }
}

extension TPPolicyVersion: Comparable {
    public static func < (lhs: TPPolicyVersion, rhs: TPPolicyVersion) -> Bool {
        if lhs.versionNumber != rhs.versionNumber {
            return lhs.versionNumber < rhs.versionNumber
        } else {
            return lhs.policyHash < rhs.policyHash
        }
    }
}
