import CloudKitCode
import CoreData
import Foundation
import InternalSwiftProtobuf
import os
import Security
import SecurityFoundation

private let logger = Logger(subsystem: "com.apple.security.trustedpeers", category: "container")

extension Container {
    internal static func onqueueMoveAndDedupVouchers(for peerMO: PeerMO, container: ContainerMO, moc: NSManagedObjectContext) throws -> Bool {
        logger.notice("cleaning up vouchers for peer \(peerMO.peerID ?? "nil", privacy: .public)")

        var uniqueSet: Set<TPVoucher> = Set()
        var vouchersToDelete = [VoucherMO]()
        // The dict key is the beneficiaryID to which the VoucherMOs for that key should be added, satisfying the invariant that the semantic beneficiary in the PB data matches the beneficiary releationship in the DB
        var vouchersToMove = [String: [VoucherMO]]()

        let fetch = VoucherMO.fetchRequest()
        fetch.predicate = NSPredicate(format: "beneficiary == %@", peerMO)
        fetch.propertiesToFetch = ["voucherInfo", "voucherInfoSig"]
        fetch.fetchBatchSize = 10

        try moc.executeBatchedFetchAndEnumerateChunkwise(
            fetchRequest: fetch,
            itemBlock: { voucher, _ in
                defer { moc.refaultUnchanged(voucher) }
                let tpv = TPVoucher(infoWith: voucher.voucherInfo ?? Data(), sig: voucher.voucherInfoSig ?? Data())
                guard let tpv else {
                    logger.error("voucher dedup unable to construct TPVoucher for peerID \(peerMO.peerID ?? "nil", privacy: .public), leaving in DB")
                    return
                }
                if tpv.beneficiaryID != peerMO.peerID {
                    logger.notice("voucher inconsistency for peerID \(peerMO.peerID ?? "nil", privacy: .public) sponsorID \(tpv.sponsorID, privacy: .public) beneficiaryID \(tpv.beneficiaryID, privacy: .public) reason \(tpv.reason.rawValue, privacy: .public)")

                    let fetch = PeerMO.fetchRequest()
                    fetch.predicate = NSPredicate(format: "container == %@ && peerID == %@", container, tpv.beneficiaryID)
                    fetch.fetchLimit = 1
                    if try moc.count(for: fetch) == 0 {
                        logger.error("deleting voucher due to peerID not found: \(tpv.beneficiaryID)")
                        vouchersToDelete.append(voucher)
                    } else {
                        logger.notice("moving voucher")
                        var current = vouchersToMove[tpv.beneficiaryID, default: []]
                        current.append(voucher)
                        vouchersToMove[tpv.beneficiaryID] = current
                    }
                } else if uniqueSet.contains(tpv) {
                    logger.notice("duplicate voucher for peerID \(peerMO.peerID ?? "nil", privacy: .public) sponsorID \(tpv.sponsorID, privacy: .public) beneficiaryID \(tpv.beneficiaryID, privacy: .public) reason \(tpv.reason.rawValue, privacy: .public)")
                    vouchersToDelete.append(voucher)
                } else {
                    uniqueSet.insert(tpv)
                    logger.debug("voucher kept for peerID \(peerMO.peerID ?? "nil", privacy: .public) sponsorID \(tpv.sponsorID, privacy: .public) beneficiaryID \(tpv.beneficiaryID, privacy: .public) reason \(tpv.reason.rawValue, privacy: .public)")
                }
            },
            chunkBlock: { _ in }
        )

        vouchersToDelete.forEach(moc.delete)
        do {
            try moc.save()
        } catch {
            logger.error("voucher cleanup unable to save \(error, privacy: .public)")
            throw error
        }

        vouchersToMove.forEach { beneficiaryID, vouchers in
            autoreleasepool {
                do {
                    guard let beneficiary = try Container.DBAdapter.fetchPeerMO(moc: moc, containerMO: container, peerID: beneficiaryID) else {
                        logger.error("Could not find peerMO for beneficiary \(beneficiaryID, privacy: .public)")
                        vouchers.forEach(moc.delete)
                        return
                    }
                    // CoreData will automatically remove this from oldPeer, as each voucherMO can only have one beneficiary relation to a PeerMO
                    vouchers.forEach(beneficiary.addToVouchers)
                    do {
                        try moc.save()
                    } catch {
                        logger.error("voucher cleanup unable to save \(error, privacy: .public)")
                        throw error
                    }
                    moc.refaultUnchanged(vouchers)
                    moc.refaultUnchanged(beneficiary)
                } catch {
                    logger.error("Failed to move vouchers for beneficiary \(beneficiaryID, privacy: .public)): \(error)")
                }
            }
        }

        return !vouchersToMove.isEmpty
    }

    internal static func onqueueRemoveDuplicateVouchers(container: ContainerMO, moc: NSManagedObjectContext) throws {
        logger.debug("onqueueRemoveDuplicateVouchers start")

        try autoreleasepool {
            let fetch = PeerMO.fetchRequest()
            fetch.predicate = NSPredicate(format: "container == %@", container)
            // limit to 10 peerMOs, and only fetch the peerIDs (& faulting for vouchers), to reduce memory footprint
            fetch.propertiesToFetch = ["peerID"]
            fetch.fetchBatchSize = 10

            var someVouchersMoved = false

            try moc.executeBatchedFetchAndEnumerateChunkwise(
                fetchRequest: fetch,
                itemBlock: { peerMO, _ in
                    someVouchersMoved = try onqueueMoveAndDedupVouchers(for: peerMO, container: container, moc: moc) || someVouchersMoved
                },
                chunkBlock: moc.refaultUnchanged
            )

            // might have to go through all peers again to deduplicate moved vouchers
            // but there should be no move moves to do
            if someVouchersMoved {
                logger.notice("voucher cleanup: have to iterate again")
                someVouchersMoved = false
                try moc.executeBatchedFetchAndEnumerateChunkwise(
                    fetchRequest: fetch,
                    itemBlock: { peerMO, _ in
                        someVouchersMoved = try onqueueMoveAndDedupVouchers(for: peerMO, container: container, moc: moc) || someVouchersMoved
                    },
                    chunkBlock: moc.refaultUnchanged
                )

                if someVouchersMoved {
                    logger.error("voucher cleanup: Unexpectedly have to iterate again??")
                }
            }

            moc.refreshAllObjects()
        }
    }
}
