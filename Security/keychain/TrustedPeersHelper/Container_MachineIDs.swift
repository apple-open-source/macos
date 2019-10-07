
import CoreData
import Foundation

extension MachineMO {
    func modifiedInPast(hours: Int) -> Bool {
        guard let modifiedDate = self.modified else {
            return false
        }

        let dateLimit = Date(timeIntervalSinceNow: -60*60*TimeInterval(hours))
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
        let runUpgrade = knownMachineMOs.filter { $0.allowed }.count > 0
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

    func setAllowedMachineIDs(_ allowedMachineIDs: Set<String>, reply: @escaping (Bool, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Bool, Error?) -> Void = {
            os_log("setAllowedMachineIDs complete: %@", log: tplogTrace, type: .info, traceError($1))
            self.semaphore.signal()
            reply($0, $1)
        }

        os_log("Setting allowed machine IDs: %@", log: tplogDebug, type: .default, allowedMachineIDs)

        // Note: we currently ignore any machineIDs that are set in the model, but never appeared on the
        // Trusted Devices list. We should give them a grace period (1wk?) then kick them out.

        self.moc.performAndWait {
            do {
                var differences = false

                var knownMachines = containerMO.machines as? Set<MachineMO> ?? Set()
                let knownMachineIDs = Set(knownMachines.compactMap { $0.machineID } )


                knownMachines.forEach { machine in
                    guard let mid = machine.machineID else {
                        os_log("Machine has no ID: %@", log: tplogDebug, type: .default, machine)
                        return
                    }
                    if allowedMachineIDs.contains(mid) {
                        if machine.status == TPMachineIDStatus.allowed.rawValue {
                            os_log("Machine ID still trusted: %@", log: tplogDebug, type: .default, String(describing: machine.machineID))
                        } else {
                            os_log("Machine ID newly retrusted: %@", log: tplogDebug, type: .default, String(describing: machine.machineID))
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
                                os_log("Newly distrusted machine ID: %@", log: tplogDebug, type: .default, String(describing: machine.machineID))
                                differences = true

                            } else {
                                if machine.modifiedInPast(hours: cutoffHours) {
                                    os_log("Allowed-but-unseen machine ID isn't on full list, last modified %@, ignoring: %@", log: tplogDebug, type: .default, machine.modifiedDate(), String(describing: machine.machineID))
                                } else {
                                    os_log("Allowed-but-unseen machine ID isn't on full list, last modified %@, distrusting: %@", log: tplogDebug, type: .default, machine.modifiedDate(), String(describing: machine.machineID))
                                    machine.status = Int64(TPMachineIDStatus.disallowed.rawValue)
                                    machine.modified = Date()
                                    differences = true
                                }
                            }

                        } else if machine.status == TPMachineIDStatus.unknown.rawValue {
                            if machine.modifiedInPast(hours: cutoffHours) {
                                os_log("Unknown machine ID last modified %@; leaving unknown: %@", log: tplogDebug, type: .default, machine.modifiedDate(), String(describing: machine.machineID))
                            } else {
                                os_log("Unknown machine ID last modified %@; distrusting: %@", log: tplogDebug, type: .default, machine.modifiedDate(), String(describing: machine.machineID))
                                machine.status = Int64(TPMachineIDStatus.disallowed.rawValue)
                                machine.modified = Date()
                                differences = true
                            }
                        }
                    }
                }

                // Do we need to create any further objects?
                allowedMachineIDs.forEach { machineID in
                    if(!knownMachineIDs.contains(machineID)) {
                        // We didn't know about this machine before; it's newly trusted!
                        let machine = MachineMO(context: self.moc)
                        machine.machineID = machineID
                        machine.container = containerMO
                        machine.seenOnFullList = true
                        machine.modified = Date()
                        machine.status = Int64(TPMachineIDStatus.allowed.rawValue)
                        os_log("Newly trusted machine ID: %@", log: tplogDebug, type: .default, String(describing: machine.machineID))
                        differences = true

                        self.containerMO.addToMachines(machine)
                        knownMachines.insert(machine)
                    }
                }

                // Now, are there any machine IDs in the model that aren't in the list? If so, add them as "unknown"
                let modelMachineIDs = self.model.allMachineIDs()
                modelMachineIDs.forEach { peerMachineID in
                    if !knownMachineIDs.contains(peerMachineID) && !allowedMachineIDs.contains(peerMachineID) {
                        os_log("Peer machineID is unknown, beginning grace period: %@", log: tplogDebug, type: .default, peerMachineID)
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

                // We no longer use allowed machine IDs.
                self.containerMO.allowedMachineIDs = NSSet()

                try self.moc.save()

                reply(differences, nil)
            } catch {
                os_log("Error setting machine ID list: %@", log: tplogDebug, type: .default, (error as CVarArg?) ?? "no error")
                reply(false, error)
            }
        }
    }

    func addAllow(_ machineIDs: [String], reply: @escaping (Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Error?) -> Void = {
            os_log("addAllow complete: %@", log: tplogTrace, type: .info, traceError($0))
            self.semaphore.signal()
            reply($0)
        }

        os_log("Adding allowed machine IDs: %@", log: tplogDebug, type: .default, machineIDs)

        self.moc.performAndWait {
            do {
                var knownMachines = containerMO.machines as? Set<MachineMO> ?? Set()
                let knownMachineIDs = Set(knownMachines.compactMap { $0.machineID } )

                // We treat an add push as authoritative (even though we should really confirm it with a full list fetch).
                // We can get away with this as we're using this list as a deny-list, and if we accidentally don't deny someone fast enough, that's okay.
                machineIDs.forEach { machineID in
                    if knownMachineIDs.contains(machineID) {
                        knownMachines.forEach { machine in
                            if machine.machineID == machineID {
                                machine.status = Int64(TPMachineIDStatus.allowed.rawValue)
                                machine.modified = Date()
                                os_log("Continue to trust machine ID: %@", log: tplogDebug, type: .default, String(describing: machine.machineID))
                            }
                        }

                    } else {
                        let machine = MachineMO(context: self.moc)
                        machine.machineID = machineID
                        machine.container = containerMO
                        machine.seenOnFullList = false
                        machine.modified = Date()
                        machine.status = Int64(TPMachineIDStatus.allowed.rawValue)
                        os_log("Newly trusted machine ID: %@", log: tplogDebug, type: .default, String(describing: machine.machineID))
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
            os_log("removeAllow complete: %@", log: tplogTrace, type: .info, traceError($0))
            self.semaphore.signal()
            reply($0)
        }

        os_log("Removing allowed machine IDs: %@", log: tplogDebug, type: .default, machineIDs)

        self.moc.performAndWait {
            do {
                var knownMachines = containerMO.machines as? Set<MachineMO> ?? Set()
                let knownMachineIDs = Set(knownMachines.compactMap { $0.machineID } )

                // This is an odd approach: we'd like to confirm that this MID was actually removed (and not just a delayed push).
                // So, let's set the status to "unknown", and its modification date to the distant past.
                // The next time we fetch the full list, we'll confirm the removal (or, if the removal push was spurious, re-add the MID as trusted).
                machineIDs.forEach { machineID in
                    if knownMachineIDs.contains(machineID) {
                        knownMachines.forEach { machine in
                            if machine.machineID == machineID {
                                machine.status = Int64(TPMachineIDStatus.unknown.rawValue)
                                machine.modified = Date.distantPast
                                os_log("Now suspicious of machine ID: %@", log: tplogDebug, type: .default, String(describing: machine.machineID))
                            }
                        }

                    } else {
                        let machine = MachineMO(context: self.moc)
                        machine.machineID = machineID
                        machine.container = containerMO
                        machine.status = Int64(TPMachineIDStatus.unknown.rawValue)
                        machine.modified = Date.distantPast
                        os_log("Suspicious of new machine ID: %@", log: tplogDebug, type: .default, String(describing: machine.machineID))
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

    func onqueueMachineIDAllowedByIDMS(machineID: String) -> Bool {
        // For Demo accounts, if the list is entirely empty, then everything is allowed
        let machines = containerMO.machines as? Set<MachineMO> ?? Set()
        if machines.count == 0 {
            os_log("machineID list is empty; allowing %@", log: tplogDebug, type: .debug, machineID)
            return true
        }

        // Note: this function rejects grey devices: machineIDs that are neither allowed nor disallowed
        for mo in machines {
            if mo.machineID == machineID {
                if mo.status == TPMachineIDStatus.allowed.rawValue {
                    return true
                } else {
                    os_log("machineID %@ not explicitly allowed: %@", log: tplogDebug, type: .debug, machineID, mo)
                    return false
                }
            }
        }

        // Didn't find it? reject.
        os_log("machineID %@ not found on list", log: tplogDebug, type: .debug, machineID)
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
        let knownMachineIDs = Set(machines.compactMap { $0.machineID } )

        // Peers trust themselves. So if the ego peer is in Octagon, its machineID will be in this set
        let trustedMachineIDs = Set(dynamicInfo.includedPeerIDs.compactMap { self.model.peer(withID: $0)?.permanentInfo.machineID })

        // if this account is not a demo account...
        if machines.count > 0 {
            for peerMachineID in trustedMachineIDs.subtracting(knownMachineIDs) {
                os_log("Peer machineID is unknown, beginning grace period: %@", log: tplogDebug, type: .default, peerMachineID)
                let machine = MachineMO(context: self.moc)
                machine.machineID = peerMachineID
                machine.container = self.containerMO
                machine.seenOnFullList = false
                machine.modified = Date()
                machine.status = Int64(TPMachineIDStatus.unknown.rawValue)

                self.containerMO.addToMachines(machine)
            }
        } else {
            os_log("Believe we're in a demo account; not starting an unknown machine ID timer", log: tplogDebug, type: .default)
        }

        for mo in (machines) {
            if mo.status == TPMachineIDStatus.disallowed.rawValue {
                os_log("Dropping knowledge of machineID %@", log: tplogDebug, type: .debug, String(describing: mo.machineID))
                self.containerMO.removeFromMachines(mo)
            }
        }
    }

    // Computes if a full list fetch would be 'useful'
    // Useful means that there's an unknown MID whose modification date is before the cutoff
    // A full list fetch would either confirm it as 'untrusted' or make it trusted again
    func onqueueFullIDMSListWouldBeHelpful() -> Bool {
        let unknownMOs = (containerMO.machines as? Set<MachineMO> ?? Set()).filter { $0.status == TPMachineIDStatus.unknown.rawValue }
        let outdatedMOs = unknownMOs.filter { !$0.modifiedInPast(hours: cutoffHours) }

        return outdatedMOs.count > 0
    }
}
