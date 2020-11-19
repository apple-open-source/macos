import CloudKitCode
import CloudKitCodeProtobuf
import CoreData
import Foundation
import os
import Security
import SecurityFoundation

extension Container {
    internal static func removingDuplicates(vouchers: Set<VoucherMO>) -> Set<VoucherMO> {
        var unique: Set<VoucherMO> = Set()

        for voucher in vouchers {
            if !unique.contains(voucher) {
                unique.insert(voucher)
            }
        }
        return unique
    }

    internal static func onqueueRemoveDuplicateVouchersPerPeer(container: ContainerMO, moc: NSManagedObjectContext) {
        var peersWithUniqueSetOfVouchers: Set<PeerMO> = Set()
        let peers = container.peers as? Set<PeerMO> ?? Set()
        for peer in peers {
            let vouchers = peer.vouchers as? Set<VoucherMO> ?? Set()
            let uniqueSet = Container.removingDuplicates(vouchers: vouchers)
            for voucher in vouchers {
                peer.removeFromVouchers(voucher)
            }
            for voucher in uniqueSet {
                peer.addToVouchers(voucher)
            }
            peersWithUniqueSetOfVouchers.insert(peer)
        }
        for peer in peers {
            container.removeFromPeers(peer)
        }
        for peer in peersWithUniqueSetOfVouchers {
            container.addToPeers(peer)
        }
    }
}
