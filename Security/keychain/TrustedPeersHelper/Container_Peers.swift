import CloudKitCode
import CoreData
import Foundation
import InternalSwiftProtobuf
import os
import Security
import SecurityFoundation

private let logger = Logger(subsystem: "com.apple.security.trustedpeers", category: "container")

extension Container {
    internal static func onqueueRemoveDuplicateVouchers(container: ContainerMO, moc: NSManagedObjectContext) {
        logger.debug("onqueueRemoveDuplicateVouchers start")
        var uniqueSet: Set<TPVoucher> = Set()
        var dups = [VoucherMO]()
        // The dict key is the beneficiaryID to which the VoucherMOs for that key should be added, satisfying the invariant that the semantic beneficiary in the PB data matches the beneficiary releationship in the DB
        var vouchersToMove = [String: [VoucherMO]]()
        let peers = container.peers as? Set<PeerMO> ?? Set()
        for peer in peers {
            for voucher in peer.vouchers as? Set<VoucherMO> ?? Set() {
                let tpv = TPVoucher(infoWith: voucher.voucherInfo ?? Data(), sig: voucher.voucherInfoSig ?? Data())
                guard let tpv else {
                    logger.error("voucher dedup unable to construct TPVoucher for peerID \(peer.peerID ?? "nil", privacy: .public), leaving in DB")
                    continue
                }
                if uniqueSet.contains(tpv) {
                    dups.append(voucher)
                    logger.notice("duplicate voucher for peerID \(peer.peerID ?? "nil", privacy: .public) sponsorID \(tpv.sponsorID, privacy: .public) beneficiaryID \(tpv.beneficiaryID, privacy: .public) reason \(tpv.reason.rawValue, privacy: .public)")
                } else {
                    uniqueSet.insert(tpv)
                    if tpv.beneficiaryID != peer.peerID {
                        logger.notice("voucher inconsistency for peerID \(peer.peerID ?? "nil", privacy: .public) sponsorID \(tpv.sponsorID, privacy: .public) beneficiaryID \(tpv.beneficiaryID, privacy: .public) reason \(tpv.reason.rawValue, privacy: .public)")
                        var current = vouchersToMove[tpv.beneficiaryID, default: []]
                        current.append(voucher)
                        vouchersToMove[tpv.beneficiaryID] = current
                    } else {
                        logger.debug("voucher kept for peerID \(peer.peerID ?? "nil", privacy: .public) sponsorID \(tpv.sponsorID, privacy: .public) beneficiaryID \(tpv.beneficiaryID, privacy: .public) reason \(tpv.reason.rawValue, privacy: .public)")
                    }
                }
            }
        }

        dups.forEach { voucher in
            guard let bene = voucher.beneficiary else {
                logger.error("voucher dedup unable to find beneficiary relation \(voucher, privacy: .public)")
                return
            }
            bene.removeFromVouchers(voucher)
        }

        vouchersToMove.forEach { beneficiaryID, vouchers in
            do {
                guard let beneficiary = try Container.DBAdapter.fetchPeerMO(moc: moc, containerMO: container, peerID: beneficiaryID) else {
                    logger.error("Could not find peerMO for beneficiary \(beneficiaryID, privacy: .public)")
                    vouchers.forEach { voucher in
                        guard let bene = voucher.beneficiary else {
                            logger.error("voucher cleanup unable to find beneficiary relation \(voucher, privacy: .public)")
                            return
                        }
                        bene.removeFromVouchers(voucher)
                    }
                    return
                }
                // CoreData will automatically remove this from oldPeer, as each voucherMO can only have one beneficiary relation to a PeerMO
                vouchers.forEach { voucher in beneficiary.addToVouchers(voucher) }
            } catch {
                logger.error("Failed to move vouchers for beneficiary \(beneficiaryID, privacy: .public)): \(error)")
            }
        }

        logger.debug("onqueueRemoveDuplicateVouchers complete")
    }
}
