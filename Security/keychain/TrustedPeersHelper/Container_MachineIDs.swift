import CoreData
import Foundation

private let logger = Logger(subsystem: "com.apple.security.trustedpeers", category: "machineids")

extension MachineMO {
    func modifiedInPast(hours: Int) -> Bool {
        guard let modifiedDate = self.modified else {
            return false
        }

        let dateLimit = Date(timeIntervalSinceNow: -60 * 60 * TimeInterval(hours))
        return modifiedDate.compare(dateLimit) == ComparisonResult.orderedDescending
    }

    func modifiedDate() -> String {
        guard let modifiedDate = self.modified else {
            return "unknown"
        }

        let dateFormatter = ISO8601DateFormatter()
        return dateFormatter.string(from: modifiedDate)
    }

    func asTPMachineID() -> TPMachineID {
        return TPMachineID(machineID: self.machineID ?? "unknown",
                           status: TPMachineIDStatus(rawValue: UInt(self.status)) ?? .unknown,
                           modified: self.modified ?? Date())
    }
}

// You get two days of grace before you're removed
let cutoffHours = 48

extension Container {
    // CoreData suggests not using heavyweight migrations, so we have two locations to store the machine ID list.
    // Perform our own migration from the no-longer-used field.
    internal static func onqueueUpgradeMachineIDSetToModel(container: ContainerMO, moc: NSManagedObjectContext) {
        let knownMachineMOs = container.machines as? Set<MachineMO> ?? Set()
        let knownMachineIDs = Set(knownMachineMOs.compactMap { $0.machineID })

        let allowedMachineIDs = container.allowedMachineIDs as? Set<String> ?? Set()
        let missingIDs = allowedMachineIDs.filter { !knownMachineIDs.contains($0) }

        missingIDs.forEach { id in
            let mid = MachineMO(context: moc)
            mid.machineID = id
            mid.seenOnFullList = true
            mid.status = Int64(TPMachineIDStatus.allowed.rawValue)
            mid.modified = Date()
            container.addToMachines(mid)
        }

        container.allowedMachineIDs = Set<String>() as NSSet
    }

    internal static func onqueueUpgradeMachineIDSetToUseStatus(container: ContainerMO, moc: NSManagedObjectContext) {
        let knownMachineMOs = container.machines as? Set<MachineMO> ?? Set()

        // Once we run this upgrade, we will set the allowed bool to false, since it's unused.
        // Therefore, if we have a single record with "allowed" set, we haven't run the upgrade.
        let runUpgrade = knownMachineMOs.contains { $0.allowed }
        if runUpgrade {
            knownMachineMOs.forEach { mo in
                if mo.allowed {
                    mo.status = Int64(TPMachineIDStatus.allowed.rawValue)
                } else {
                    mo.status = Int64(TPMachineIDStatus.disallowed.rawValue)
                }
                mo.allowed = false
            }
        }
    }

    func enforceIDMSListChanges(knownMachines: Set<MachineMO>) -> Bool {
        if self.containerMO.honorIDMSListChanges == "YES"{
            return true
        } else if self.containerMO.honorIDMSListChanges == "NO" {
            return false
        } else if self.containerMO.honorIDMSListChanges == "UNKNOWN" && knownMachines.isEmpty {
            return false
        } else if self.containerMO.honorIDMSListChanges == "UNKNOWN" && !knownMachines.isEmpty {
            return true
        } else {
            return true
        }
    }

    func setAllowedMachineIDs(_ allowedMachineIDs: Set<String>, honorIDMSListChanges: Bool, reply: @escaping (Bool, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Bool, Error?) -> Void = {
            logger.info("setAllowedMachineIDs complete: \(traceError($1), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1)
        }

        logger.debug("Setting allowed machine IDs: \(allowedMachineIDs, privacy: .public)")

        // Note: we currently ignore any machineIDs that are set in the model, but never appeared on the
        // Trusted Devices list. We should give them a grace period (1wk?) then kick them out.

        self.moc.performAndWait {
            do {
                var differences = false
                self.containerMO.honorIDMSListChanges = honorIDMSListChanges ? "YES" : "NO"

                var knownMachines = containerMO.machines as? Set<MachineMO> ?? Set()
                let knownMachineIDs = Set(knownMachines.compactMap { $0.machineID })

                knownMachines.forEach { machine in
                    guard let mid = machine.machineID else {
                        logger.debug("Machine has no ID: \(String(describing: machine), privacy: .public)")
                        return
                    }
                    if allowedMachineIDs.contains(mid) {
                        if machine.status == TPMachineIDStatus.allowed.rawValue {
                            logger.debug("Machine ID still trusted: \(String(describing: machine.machineID), privacy: .public)")
                        } else {
                            logger.debug("Machine ID newly retrusted: \(String(describing: machine.machineID), privacy: .public)")
                            differences = true
                        }
                        machine.status = Int64(TPMachineIDStatus.allowed.rawValue)
                        machine.seenOnFullList = true
                        machine.modified = Date()
                    } else {
                        // This machine ID is not on the list. What, if anything, should be done?
                        if machine.status == TPMachineIDStatus.allowed.rawValue {
                            // IDMS sometimes has list consistency issues. So, if we see a device 'disappear' from the list, it may or may not
                            // actually have disappered: we may have received an 'add' push and then fetched the list too quickly.
                            // To hack around this, we track whether we've seen the machine on the full list yet. If we haven't, this was likely
                            // the result of an 'add' push, and will be given 48 hours of grace before being removed.
                            if machine.seenOnFullList {
                                machine.status = Int64(TPMachineIDStatus.disallowed.rawValue)
                                machine.modified = Date()
                                logger.debug("Newly distrusted machine ID: \(String(describing: machine.machineID), privacy: .public)")
                                differences = true
                            } else {
                                if machine.modifiedInPast(hours: cutoffHours) {
                                    logger.debug("Allowed-but-unseen machine ID isn't on full list, last modified \(String(describing: machine.modifiedDate()), privacy: .public), ignoring: \(String(describing: machine.machineID), privacy: .public)")
                                } else {
                                    logger.debug("Allowed-but-unseen machine ID isn't on full list, last modified \(String(describing: machine.modifiedDate()), privacy: .public), distrusting: \(String(describing: machine.machineID), privacy: .public)")
                                    machine.status = Int64(TPMachineIDStatus.disallowed.rawValue)
                                    machine.modified = Date()
                                    differences = true
                                }
                            }
                        } else if machine.status == TPMachineIDStatus.unknown.rawValue {
                            if machine.modifiedInPast(hours: cutoffHours) {
                                logger.debug("Unknown machine ID last modified \(String(describing: machine.modifiedDate()), privacy: .public); leaving unknown: \(String(describing: machine.machineID), privacy: .public)")
                            } else {
                                logger.debug("Unknown machine ID last modified \(String(describing: machine.modifiedDate()), privacy: .public); distrusting: \(String(describing: machine.machineID), privacy: .public)")
                                machine.status = Int64(TPMachineIDStatus.disallowed.rawValue)
                                machine.modified = Date()
                                differences = true
                            }
                        }
                    }
                }

                // Do we need to create any further objects?
                allowedMachineIDs.forEach { machineID in
                    if !knownMachineIDs.contains(machineID) {
                        // We didn't know about this machine before; it's newly trusted!
                        let machine = MachineMO(context: self.moc)
                        machine.machineID = machineID
                        machine.container = containerMO
                        machine.seenOnFullList = true
                        machine.modified = Date()
                        machine.status = Int64(TPMachineIDStatus.allowed.rawValue)
                        logger.debug("Newly trusted machine ID: \(String(describing: machine.machineID), privacy: .public)")
                        differences = true

                        self.containerMO.addToMachines(machine)
                        knownMachines.insert(machine)
                    }
                }

                if self.enforceIDMSListChanges(knownMachines: knownMachines) {
                    // Are there any machine IDs in the model that aren't in the list? If so, add them as "unknown"
                    let modelMachineIDs = self.model.allMachineIDs()
                    modelMachineIDs.forEach { peerMachineID in
                        if !knownMachineIDs.contains(peerMachineID) && !allowedMachineIDs.contains(peerMachineID) {
                            logger.debug("Peer machineID is unknown, beginning grace period: \(String(describing: peerMachineID), privacy: .public)")
                            let machine = MachineMO(context: self.moc)
                            machine.machineID = peerMachineID
                            machine.container = containerMO
                            machine.seenOnFullList = false
                            machine.modified = Date()
                            machine.status = Int64(TPMachineIDStatus.unknown.rawValue)
                            differences = true

                            self.containerMO.addToMachines(machine)
                        }
                    }
                } else {
                    logger.debug("Believe we're in a demo account, not enforcing IDMS list")
                }

                // We no longer use allowed machine IDs.
                self.containerMO.allowedMachineIDs = NSSet()

                try self.moc.save()

                reply(differences, nil)
            } catch {
                logger.debug("Error setting machine ID list: \(String(describing: error), privacy: .public)")
                reply(false, error)
            }
        }
    }

    func addAllow(_ machineIDs: [String], reply: @escaping (Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Error?) -> Void = {
            logger.info("addAllow complete: \(traceError($0), privacy: .public)")
            self.semaphore.signal()
            reply($0)
        }

        logger.debug("Adding allowed machine IDs: \(String(describing: machineIDs), privacy: .public)")

        self.moc.performAndWait {
            do {
                var knownMachines = containerMO.machines as? Set<MachineMO> ?? Set()
                let knownMachineIDs = Set(knownMachines.compactMap { $0.machineID })

                // We treat an add push as authoritative (even though we should really confirm it with a full list fetch).
                // We can get away with this as we're using this list as a deny-list, and if we accidentally don't deny someone fast enough, that's okay.
                machineIDs.forEach { machineID in
                    if knownMachineIDs.contains(machineID) {
                        knownMachines.forEach { machine in
                            if machine.machineID == machineID {
                                machine.status = Int64(TPMachineIDStatus.allowed.rawValue)
                                machine.modified = Date()
                                logger.debug("Continue to trust machine ID: \(String(describing: machine.machineID), privacy: .public)")
                            }
                        }
                    } else {
                        let machine = MachineMO(context: self.moc)
                        machine.machineID = machineID
                        machine.container = containerMO
                        machine.seenOnFullList = false
                        machine.modified = Date()
                        machine.status = Int64(TPMachineIDStatus.allowed.rawValue)
                        logger.debug("Newly trusted machine ID: \(String(describing: machine.machineID), privacy: .public)")
                        self.containerMO.addToMachines(machine)

                        knownMachines.insert(machine)
                    }
                }

                try self.moc.save()
                reply(nil)
            } catch {
                reply(error)
            }
        }
    }

    func removeAllow(_ machineIDs: [String], reply: @escaping (Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Error?) -> Void = {
            logger.info("removeAllow complete: \(traceError($0), privacy: .public)")
            self.semaphore.signal()
            reply($0)
        }

        logger.debug("Removing allowed machine IDs: \(String(describing: machineIDs), privacy: .public)")

        self.moc.performAndWait {
            do {
                var knownMachines = containerMO.machines as? Set<MachineMO> ?? Set()
                let knownMachineIDs = Set(knownMachines.compactMap { $0.machineID })

                // This is an odd approach: we'd like to confirm that this MID was actually removed (and not just a delayed push).
                // So, let's set the status to "unknown", and its modification date to the distant past.
                // The next time we fetch the full list, we'll confirm the removal (or, if the removal push was spurious, re-add the MID as trusted).
                machineIDs.forEach { machineID in
                    if knownMachineIDs.contains(machineID) {
                        knownMachines.forEach { machine in
                            if machine.machineID == machineID {
                                machine.status = Int64(TPMachineIDStatus.unknown.rawValue)
                                machine.modified = Date.distantPast
                                logger.debug("Now suspicious of machine ID: \(String(describing: machine.machineID), privacy: .public)")
                            }
                        }
                    } else {
                        let machine = MachineMO(context: self.moc)
                        machine.machineID = machineID
                        machine.container = containerMO
                        machine.status = Int64(TPMachineIDStatus.unknown.rawValue)
                        machine.modified = Date.distantPast
                        logger.debug("Suspicious of new machine ID: \(String(describing: machine.machineID), privacy: .public)")
                        self.containerMO.addToMachines(machine)

                        knownMachines.insert(machine)
                    }
                }

                try self.moc.save()
                reply(nil)
            } catch {
                reply(error)
            }
        }
    }

    func fetchAllowedMachineIDs(reply: @escaping (Set<String>?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Set<String>?, Error?) -> Void = {
            logger.info("fetchAllowedMachineIDs complete: \(traceError($1), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1)
        }

        logger.debug("Fetching allowed machine IDs")

        self.moc.performAndWait {
            let knownMachines = containerMO.machines as? Set<MachineMO> ?? Set()
            let allowedMachineIDs = knownMachines.filter { $0.status == Int64(TPMachineIDStatus.allowed.rawValue) }.compactMap { $0.machineID }

            reply(Set(allowedMachineIDs), nil)
        }
    }

    func onqueueMachineIDAllowedByIDMS(machineID: String) -> Bool {

        // For Demo accounts, if the list is entirely empty, then everything is allowed
        let knownMachines = containerMO.machines as? Set<MachineMO> ?? Set()

        if !self.enforceIDMSListChanges(knownMachines: knownMachines) {
            logger.debug("not enforcing idms list changes; allowing \(String(describing: machineID), privacy: .public)")
            return true
        }

        // Note: this function rejects grey devices: machineIDs that are neither allowed nor disallowed
        for mo in knownMachines where mo.machineID == machineID {
            if mo.status == TPMachineIDStatus.allowed.rawValue {
                return true
            } else {
                logger.debug("machineID \(String(describing: machineID), privacy: .public) not explicitly allowed: \(String(describing: mo), privacy: .public)")
                return false
            }
        }

        // Didn't find it? reject.
        logger.debug("machineID \(String(describing: machineID), privacy: .public) not found on list")
        return false
    }

    func onqueueCurrentMIDList() -> TPMachineIDList {
        let machines = containerMO.machines as? Set<MachineMO> ?? Set()
        return TPMachineIDList(entries: machines.map { $0.asTPMachineID() })
    }

    func onqueueUpdateMachineIDListFromModel(dynamicInfo: TPPeerDynamicInfo) {
        // This function is intended to be called once the model is in a steady state of adds and deletes.
        //
        // First, we should ensure that we've written down the MIDs of all trusted peers. That way, if they
        // aren't on the MID list now, we'll start the timer for them to be removed if they never make it.
        // (But! don't do this if we think this is a Demo account. Those don't have a list, and we shouldn't make one.)

        // Second, we should remove all disallowed MIDs, as those values have been used.
        // We don't want to automatically kick out new peers if they rejoin with the same MID.

        let machines = containerMO.machines as? Set<MachineMO> ?? Set()
        let knownMachineIDs = Set(machines.compactMap { $0.machineID })

        // Peers trust themselves. So if the ego peer is in Octagon, its machineID will be in this set
        let trustedMachineIDs = Set(dynamicInfo.includedPeerIDs.compactMap { self.model.peer(withID: $0)?.permanentInfo.machineID })

        // if this account is not a demo account...
        if self.enforceIDMSListChanges(knownMachines: machines) {
            for peerMachineID in trustedMachineIDs.subtracting(knownMachineIDs) {
                logger.debug("Peer machineID is unknown, beginning grace period: \(String(describing: peerMachineID), privacy: .public)")
                let machine = MachineMO(context: self.moc)
                machine.machineID = peerMachineID
                machine.container = self.containerMO
                machine.seenOnFullList = false
                machine.modified = Date()
                machine.status = Int64(TPMachineIDStatus.unknown.rawValue)

                self.containerMO.addToMachines(machine)
            }
        } else {
            logger.debug("Not enforcing IDMS list changes")
        }

        // Remove all disallowed MIDs, unless we continue to trust the peer for some other reason
        for mo in (machines) where mo.status == TPMachineIDStatus.disallowed.rawValue && !trustedMachineIDs.contains(mo.machineID ?? "") {
            logger.debug("Dropping knowledge of machineID \(String(describing: mo.machineID), privacy: .public)")
            self.containerMO.removeFromMachines(mo)
        }
    }

    // Computes if a full list fetch would be 'useful'
    // Useful means that there's an unknown MID whose modification date is before the cutoff
    // A full list fetch would either confirm it as 'untrusted' or make it trusted again
    func onqueueFullIDMSListWouldBeHelpful() -> Bool {

        if self.containerMO.honorIDMSListChanges == "UNKNOWN" {
            return true
        }

        let unknownMOs = (containerMO.machines as? Set<MachineMO> ?? Set()).filter { $0.status == TPMachineIDStatus.unknown.rawValue }
        let outdatedMOs = unknownMOs.filter { !$0.modifiedInPast(hours: cutoffHours) }

        return !outdatedMOs.isEmpty
    }

    func fullIDMSListWouldBeHelpful() -> Bool {
        var ret: Bool = false
        self.moc.performAndWait {
            ret = self.onqueueFullIDMSListWouldBeHelpful()
        }

        return ret
    }
}
