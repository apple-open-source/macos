import CoreData
import Foundation
import CryptoKit

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

/* 
 Devices that are on the allowed TDL list and trusted in Octagon, become distrusted in Octagon once
 a device's MID becomes disallowed.
 */

// If a MID is allowed but then falls off the TDL entirely, 48 hours grace to get back on the TDL before being disallowed
let ghostedCutoffHours = 48

// If on the evicted list and not on the allowed list - 48 hours grace to get back on the TDL before being disallowed
let evictedCutoffHours = 48

// If on the unknown reason list and not on the allowed list - 48 hours grace to get back on the TDL before being disallowed
let unknownReasonCutoffHours = 48

// If on the unknown list - 48 hours grace to get back on the TDL before being disallowed
let unknownCutoffHours = 48

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

    // return whether or not there are list differences
    func handleEvicted(machine: MachineMO, listDifferences: inout Bool) {
        if machine.modifiedInPast(hours: evictedCutoffHours) {
            logger.info("Evicted machine ID last modified \(String(describing: machine.modifiedDate()), privacy: .public); leaving evicted: \(String(describing: machine.machineID), privacy: .public)")
        } else {
            logger.notice("Evicted machine ID last modified \(String(describing: machine.modifiedDate()), privacy: .public); distrusting: \(String(describing: machine.machineID), privacy: .public)")
            machine.status = Int64(TPMachineIDStatus.disallowed.rawValue)
            machine.modified = Date()
            listDifferences = true
        }
    }

    // return whether or not there are list differences
    func handleUnknownReasons(machine: MachineMO, listDifferences: inout Bool) {
        if machine.modifiedInPast(hours: unknownReasonCutoffHours) {
            logger.info("Unknown reason machine ID last modified \(String(describing: machine.modifiedDate()), privacy: .public); leaving unknown reason: \(String(describing: machine.machineID), privacy: .public)")
        } else {
            logger.notice("Unknown reason machine ID last modified \(String(describing: machine.modifiedDate()), privacy: .public); distrusting: \(String(describing: machine.machineID), privacy: .public)")
            machine.status = Int64(TPMachineIDStatus.disallowed.rawValue)
            machine.modified = Date()
            listDifferences = true
        }
    }

    func checkForDuplicateEntriesAndSendMetric(_ allowedMachineIDs: Set<String>,
                                               userInitiatedRemovals: Set<String>? = nil,
                                               evictedRemovals: Set<String>? = nil,
                                               unknownReasonRemovals: Set<String>? = nil,
                                               flowID: String?,
                                               deviceSessionID: String?,
                                               altDSID: String?,
                                               testsAreEnabled: Bool) -> Bool {

        var duplicateDetected = false
        for allowedMachineId in allowedMachineIDs {
            if let userInitiatedRemovals, userInitiatedRemovals.contains(allowedMachineId) {
                duplicateDetected = true
                break
            }

            if let evictedRemovals, evictedRemovals.contains(allowedMachineIDs) {
                duplicateDetected = true
                break
            }

            if let unknownReasonRemovals, unknownReasonRemovals.contains(allowedMachineId) {
                duplicateDetected = true
                break
            }
        }
        
        if duplicateDetected {
            let eventS = AAFAnalyticsEventSecurity(keychainCircleMetrics: nil,
                                                   altDSID: altDSID,
                                                   flowID: flowID,
                                                   deviceSessionID: deviceSessionID,
                                                   eventName: kSecurityRTCEventNameDuplicateMachineID,
                                                   testsAreEnabled: testsAreEnabled,
                                                   canSendMetrics: true,
                                                   category: kSecurityRTCEventCategoryAccountDataAccessRecovery)

            SecurityAnalyticsReporterRTC.sendMetric(withEvent: eventS, success: false, error: ContainerError.duplicateMachineID)

            return true
        }

        return false
    }

    func sendVanishedMetric(egoMachineIDVanished: Bool,
                            altDSID: String?,
                            flowID: String?,
                            deviceSessionID: String?) -> Void {

        let eventS = AAFAnalyticsEventSecurity(keychainCircleMetrics: [kSecurityRTCFieldEgoMachineIDVanishedFromTDL : egoMachineIDVanished],
                                               altDSID: altDSID,
                                               flowID: flowID,
                                               deviceSessionID: deviceSessionID,
                                               eventName: kSecurityRTCEventNameMIDVanishedFromTDL,
                                               testsAreEnabled: false,
                                               canSendMetrics: true,
                                               category: kSecurityRTCEventCategoryAccountDataAccessRecovery)

        SecurityAnalyticsReporterRTC.sendMetric(withEvent: eventS, success: false, error: ContainerError.machineIDVanishedFromTDL)
    }

    func computeHash(machineIDs: Set<String>) -> String? {
        var machineIDSConcatenated: String = ""

        let sorted = machineIDs.sorted()
        for machineId in sorted {
            machineIDSConcatenated += machineId
        }

        if let data = machineIDSConcatenated.data(using: .utf8) {
            let digest = SHA256.hash(data: data)
            let hashString = digest
                .compactMap { String(format: "%02x", $0) }
                .joined()
            return hashString
        }

        return nil
    }

    func setAllowedMachineIDs(_ allowedMachineIDs: Set<String>,
                              userInitiatedRemovals: Set<String>? = nil,
                              evictedRemovals: Set<String>? = nil,
                              unknownReasonRemovals: Set<String>? = nil,
                              honorIDMSListChanges: Bool,
                              version: String?,
                              flowID: String?,
                              deviceSessionID: String?,
                              canSendMetrics: Bool,
                              altDSID: String?,
                              trustedDeviceHash: String?,
                              deletedDeviceHash: String?,
                              trustedDevicesUpdateTimestamp: NSNumber?,
                              reply: @escaping (Bool, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Bool, Error?) -> Void = {
            logger.info("setAllowedMachineIDs complete: \(traceError($1), privacy: .public)")
            sem.release()
            reply($0, $1)
        }

        logger.info("Setting allowed machine IDs: \(allowedMachineIDs, privacy: .public), version \(String(describing: version), privacy: .public)")

        // Note: we currently ignore any machineIDs that are set in the model, but never appeared on the
        // Trusted Devices list. We should give them a grace period (1wk?) then kick them out.

        self.moc.performAndWait {
            do {

                var egoMachineID: String? = nil
                var egoIsTrusted: Bool = false

                if let egoPeerID = self.containerMO.egoPeerID {
                    let egoPeer: TPPeer?
                    do {
                        egoPeer = try self.model.peer(withID: egoPeerID)
                        if egoPeer == nil {
                            logger.warning("Couldn't find ego peer in model")
                        }
                    } catch {
                        logger.warning("Error getting ego peer from model: \(String(describing: error), privacy: .public)")
                        egoPeer = nil
                    }
                    if let egoPeer {
                        egoMachineID = egoPeer.permanentInfo.machineID
                        
                        do {
                            let status = try self.model.statusOfPeer(withID: egoPeerID)
                            egoIsTrusted = (status != .excluded && status != .unknown && status != .ignored)
                        } catch {
                            logger.warning("error calling statusOfPeer: \(error, privacy: .public)")
                        }
                    }
                }

                var deletedList: Set<String> = Set()
                if let userInitiatedRemovals {
                    deletedList.formUnion(userInitiatedRemovals)
                }
                if let evictedRemovals {
                    deletedList.formUnion(evictedRemovals)
                }
                if let unknownReasonRemovals {
                    deletedList.formUnion(unknownReasonRemovals)
                }
                var hashOfDeleted: String? = ""
                if !deletedList.isEmpty {
                    hashOfDeleted = self.computeHash(machineIDs: deletedList)
                    logger.info("sha256 of deleted list: \(String(describing: hashOfDeleted), privacy: .public)")
                }
                var hashOfAllowed: String? = ""
                if !allowedMachineIDs.isEmpty {
                    hashOfAllowed = self.computeHash(machineIDs: allowedMachineIDs)
                    logger.info("sha256 of allowed list: \(String(describing: hashOfAllowed), privacy: .public)")
                }

                self.testHashMismatchDetected = false
                if trustedDeviceHash != hashOfAllowed {
                    let eventS = AAFAnalyticsEventSecurity(keychainCircleMetrics: nil,
                                                           altDSID: altDSID,
                                                           flowID: flowID,
                                                           deviceSessionID: deviceSessionID,
                                                           eventName: kSecurityRTCEventNameAllowedMIDHashMismatch,
                                                           testsAreEnabled: false,
                                                           canSendMetrics: true,
                                                           category: kSecurityRTCEventCategoryAccountDataAccessRecovery)
                    SecurityAnalyticsReporterRTC.sendMetric(withEvent: eventS, success: false, error: ContainerError.allowedMIDHashMismatch)
                    self.testHashMismatchDetected = true
                }
                if deletedDeviceHash != hashOfDeleted {
                    let eventS = AAFAnalyticsEventSecurity(keychainCircleMetrics: nil,
                                                           altDSID: altDSID,
                                                           flowID: flowID,
                                                           deviceSessionID: deviceSessionID,
                                                           eventName: kSecurityRTCEventNameDeletedMIDHashMismatch,
                                                           testsAreEnabled: false,
                                                           canSendMetrics: true,
                                                           category: kSecurityRTCEventCategoryAccountDataAccessRecovery)
                    SecurityAnalyticsReporterRTC.sendMetric(withEvent: eventS, success: false, error: ContainerError.deletedMIDHashMismatch)
                    self.testHashMismatchDetected = true
                }

                let detectedDuplicate = self.checkForDuplicateEntriesAndSendMetric(allowedMachineIDs, userInitiatedRemovals: userInitiatedRemovals, evictedRemovals: evictedRemovals, unknownReasonRemovals: unknownReasonRemovals, flowID: flowID, deviceSessionID: deviceSessionID, altDSID: altDSID, testsAreEnabled: false)

                var midVanishedFromTDL = false
                var differences = false
                self.containerMO.honorIDMSListChanges = honorIDMSListChanges ? "YES" : "NO"

                var knownMachines = self.containerMO.machines as? Set<MachineMO> ?? Set()
                var knownMachineIDs = Set(knownMachines.compactMap { $0.machineID })

                knownMachines.forEach { machine in
                    guard let mid = machine.machineID else {
                        logger.info("Machine has no ID: \(String(describing: machine), privacy: .public)")
                        return
                    }

                    if allowedMachineIDs.contains(mid) {
                        if machine.status == TPMachineIDStatus.allowed.rawValue {
                            logger.info("Machine ID still trusted: \(String(describing: machine.machineID), privacy: .public)")
                        } else {
                            logger.notice("Machine ID newly retrusted: \(String(describing: machine.machineID), privacy: .public)")
                            differences = true
                        }
                        machine.status = Int64(TPMachineIDStatus.allowed.rawValue)
                        machine.seenOnFullList = true
                        machine.modified = Date()
                        return
                    }

                    if let removeNow = userInitiatedRemovals {
                        if removeNow.contains(mid) {
                            logger.notice("User initiated removal! machine ID last modified \(String(describing: machine.modifiedDate()), privacy: .public); distrusting: \(String(describing: machine.machineID), privacy: .public)")
                            if machine.status != TPMachineIDStatus.disallowed.rawValue {
                                machine.status = Int64(TPMachineIDStatus.disallowed.rawValue)
                                machine.modified = Date()
                                differences = true
                            }
                            return
                        }
                    }

                    if let evicted = evictedRemovals {
                        if evicted.contains(mid) {
                            logger.notice("Evicted removal! machine ID last modified \(String(describing: machine.modifiedDate()), privacy: .public); tagging as evicted: \(String(describing: machine.machineID), privacy: .public)")
                            if machine.status == TPMachineIDStatus.evicted.rawValue {
                                self.handleEvicted(machine: machine, listDifferences: &differences)
                            } else {
                                machine.status = Int64(TPMachineIDStatus.evicted.rawValue)
                                machine.modified = Date()
                                differences = true
                            }
                            return
                        }
                    }

                    if let unknowns = unknownReasonRemovals {
                        if unknowns.contains(mid) {
                            logger.notice("Unknown reason removal! machine ID last modified \(String(describing: machine.modifiedDate()), privacy: .public); tagging as unknown reason: \(String(describing: machine.machineID), privacy: .public)")
                            if machine.status == TPMachineIDStatus.unknownReason.rawValue {
                                self.handleUnknownReasons(machine: machine, listDifferences: &differences)
                            } else {
                                machine.status = Int64(TPMachineIDStatus.unknownReason.rawValue)
                                machine.modified = Date()
                                differences = true
                            }
                            return
                        }
                    }

                    // This machine ID is not on the allow list, user initiated list, evicted list, or unknown reason list. What, if anything, should be done?

                    var egoMachineIDVanished = false
                    self.testEgoMachineIDVanished = false

                    if egoIsTrusted, let egoMachineID, let machineID = machine.machineID, egoMachineID == machineID {
                        egoMachineIDVanished = true
                        self.testEgoMachineIDVanished = true
                    }

                    self.sendVanishedMetric(egoMachineIDVanished: egoMachineIDVanished, altDSID: altDSID, flowID: flowID, deviceSessionID: deviceSessionID)

                    midVanishedFromTDL = true

                    if machine.status == TPMachineIDStatus.evicted.rawValue {
                        self.handleEvicted(machine: machine, listDifferences: &differences)
                    } else if machine.status == TPMachineIDStatus.unknownReason.rawValue {
                        self.handleUnknownReasons(machine: machine, listDifferences: &differences)
                    } else if machine.status == TPMachineIDStatus.ghostedFromTDL.rawValue {
                        if machine.modifiedInPast(hours: ghostedCutoffHours) {
                            if machine.seenOnFullList {
                                logger.info("Seen on full list machine ID isn't on full list, last modified \(String(describing: machine.modifiedDate()), privacy: .public), ignoring: \(String(describing: machine.machineID), privacy: .public)")
                            } else {
                                logger.info("Allowed-but-unseen machine ID isn't on full list, last modified \(String(describing: machine.modifiedDate()), privacy: .public), ignoring: \(String(describing: machine.machineID), privacy: .public)")
                            }
                        } else {
                            if machine.seenOnFullList {
                                logger.notice("Seen on full list machine ID isn't on full list, last modified \(String(describing: machine.modifiedDate()), privacy: .public), distrusting: \(String(describing: machine.machineID), privacy: .public)")
                            } else {
                                logger.notice("Allowed-but-unseen machine ID isn't on full list, last modified \(String(describing: machine.modifiedDate()), privacy: .public), distrusting: \(String(describing: machine.machineID), privacy: .public)")
                            }
                            machine.status = Int64(TPMachineIDStatus.disallowed.rawValue)
                            machine.modified = Date()
                            differences = true
                        }
                    } else if machine.status == TPMachineIDStatus.allowed.rawValue {
                        // IDMS sometimes has list consistency issues. So, if we see a device 'disappear' from the list, it may or may not
                        // actually have disappered.  Devices that were allowed get a 48 hour grace period.
                        logger.info("MachineID was allowed but no longer on the TDL, last modified \(String(describing: machine.modifiedDate()), privacy: .public), tagging as ghosted fromt TDL: \(String(describing: machine.machineID), privacy: .public)")

                        machine.status = Int64(TPMachineIDStatus.ghostedFromTDL.rawValue)
                        machine.modified = Date()
                        differences = true
                } else if machine.status == TPMachineIDStatus.unknown.rawValue {
                    if machine.modifiedInPast(hours: unknownCutoffHours) {
                            logger.info("Unknown machine ID last modified \(String(describing: machine.modifiedDate()), privacy: .public); leaving unknown: \(String(describing: machine.machineID), privacy: .public)")
                        } else {
                            logger.notice("Unknown machine ID last modified \(String(describing: machine.modifiedDate()), privacy: .public); distrusting: \(String(describing: machine.machineID), privacy: .public)")
                            machine.status = Int64(TPMachineIDStatus.disallowed.rawValue)
                            machine.modified = Date()
                            differences = true
                        }
                    }
                }

                // reload knownMachineIDs in case peerIDs end up in multiple sets
                knownMachines = self.containerMO.machines as? Set<MachineMO> ?? Set()
                knownMachineIDs = Set(knownMachines.compactMap { $0.machineID })

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
                        logger.notice("Newly trusted machine ID: \(String(describing: machine.machineID), privacy: .public)")
                        differences = true

                        self.containerMO.addToMachines(machine)
                        knownMachines.insert(machine)
                    }
                }

                // reload knownMachineIDs in case peerIDs end up in multiple sets
                knownMachines = self.containerMO.machines as? Set<MachineMO> ?? Set()
                knownMachineIDs = Set(knownMachines.compactMap { $0.machineID })

                if let removeNow = userInitiatedRemovals {
                    removeNow.forEach { machineID in
                        if !knownMachineIDs.contains(machineID) {
                            // We didn't know about this machine before; it's newly untrusted!
                            let machine = MachineMO(context: self.moc)
                            machine.machineID = machineID
                            machine.container = containerMO
                            machine.seenOnFullList = true
                            machine.modified = Date()
                            machine.status = Int64(TPMachineIDStatus.disallowed.rawValue)
                            logger.notice("Newly distrusted machine ID: \(String(describing: machine.machineID), privacy: .public)")
                            differences = true

                            self.containerMO.addToMachines(machine)
                            knownMachines.insert(machine)
                        }
                    }
                }

                // reload knownMachineIDs in case peerIDs end up in multiple sets
                knownMachines = self.containerMO.machines as? Set<MachineMO> ?? Set()
                knownMachineIDs = Set(knownMachines.compactMap { $0.machineID })

                if let evicted = evictedRemovals {
                    evicted.forEach { machineID in
                        if !knownMachineIDs.contains(machineID) {
                            // We didn't know about this machine before; it's newly evicted!
                            let machine = MachineMO(context: self.moc)
                            machine.machineID = machineID
                            machine.container = containerMO
                            machine.seenOnFullList = true
                            machine.modified = Date()
                            machine.status = Int64(TPMachineIDStatus.evicted.rawValue)
                            logger.notice("Newly evicted machine ID: \(String(describing: machine.machineID), privacy: .public)")
                            differences = true

                            self.containerMO.addToMachines(machine)
                            knownMachines.insert(machine)
                        }
                    }
                }

                // reload knownMachineIDs in case peerIDs end up in multiple sets
                knownMachines = self.containerMO.machines as? Set<MachineMO> ?? Set()
                knownMachineIDs = Set(knownMachines.compactMap { $0.machineID })

                if let unknownReasonRemovals = unknownReasonRemovals {
                    unknownReasonRemovals.forEach { machineID in
                        if !knownMachineIDs.contains(machineID) {
                            // We didn't know about this machine before; it's newly removed with an unknown reason!
                            let machine = MachineMO(context: self.moc)
                            machine.machineID = machineID
                            machine.container = containerMO
                            machine.seenOnFullList = true
                            machine.modified = Date()
                            machine.status = Int64(TPMachineIDStatus.unknownReason.rawValue)
                            logger.notice("Newly removed with unknown reason machine ID: \(String(describing: machine.machineID), privacy: .public)")
                            differences = true

                            self.containerMO.addToMachines(machine)
                            knownMachines.insert(machine)
                        }
                    }
                }

                // reload knownMachineIDs in case peerIDs end up in multiple sets
                knownMachines = self.containerMO.machines as? Set<MachineMO> ?? Set()
                knownMachineIDs = Set(knownMachines.compactMap { $0.machineID })

                if self.enforceIDMSListChanges(knownMachines: knownMachines) {
                    // Are there any machine IDs in the model that aren't in the list? If so, add them as "unknown"
                    let modelMachineIDs = try self.model.allMachineIDs()
                    modelMachineIDs.forEach { peerMachineID in
                        if !knownMachineIDs.contains(peerMachineID) && !allowedMachineIDs.contains(peerMachineID) && 
                            !(evictedRemovals?.contains(peerMachineID) ?? false) &&
                            !(unknownReasonRemovals?.contains(peerMachineID) ?? false) &&
                            !(userInitiatedRemovals?.contains(peerMachineID) ?? false) {
                            logger.notice("Peer machineID is unknown, beginning grace period: \(String(describing: peerMachineID), privacy: .public)")
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
                    logger.notice("Believe we're in a demo account, not enforcing IDMS list")
                }

                self.containerMO.idmsTrustedDevicesVersion = version
                self.containerMO.idmsTrustedDeviceListFetchDate = Date()

                // We no longer use allowed machine IDs.
                self.containerMO.allowedMachineIDs = NSSet()

                let eventS = AAFAnalyticsEventSecurity(keychainCircleMetrics: nil,
                                                       altDSID: altDSID,
                                                       flowID: flowID,
                                                       deviceSessionID: deviceSessionID,
                                                       eventName: kSecurityRTCEventNameTDLProcessingSuccess,
                                                       testsAreEnabled: false,
                                                       canSendMetrics: true,
                                                       category: kSecurityRTCEventCategoryAccountDataAccessRecovery)
                if midVanishedFromTDL == false && detectedDuplicate == false && self.testHashMismatchDetected == false {
                    SecurityAnalyticsReporterRTC.sendMetric(withEvent: eventS, success: true, error: nil)
                } else {
                    SecurityAnalyticsReporterRTC.sendMetric(withEvent: eventS, success: false, error: nil)
                }

                Container.onqueueRemoveDuplicateMachineIDs(containerMO: self.containerMO, moc: self.moc)

                try self.moc.save()

                reply(differences, nil)
            } catch {
                logger.error("Error setting machine ID list: \(String(describing: error), privacy: .public)")
                reply(false, error)
            }
        }
    }

    func fetchAllowedMachineIDs(reply: @escaping (Set<String>?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Set<String>?, Error?) -> Void = {
            logger.info("fetchAllowedMachineIDs complete: \(traceError($1), privacy: .public)")
            sem.release()
            reply($0, $1)
        }

        logger.info("Fetching allowed machine IDs")

        self.moc.performAndWait {
            let knownMachines = containerMO.machines as? Set<MachineMO> ?? Set()
            let allowedMachineIDs = knownMachines.filter { $0.status == Int64(TPMachineIDStatus.allowed.rawValue) }.compactMap { $0.machineID }

            reply(Set(allowedMachineIDs), nil)
        }
    }

    func onqueueMachineIDAllowedByIDMS(machineID: String) -> Bool {

        // For Demo accounts, if the list is entirely empty, then everything is allowed
        let knownMachines = self.containerMO.machines as? Set<MachineMO> ?? Set()

        if !self.enforceIDMSListChanges(knownMachines: knownMachines) {
            logger.info("not enforcing idms list changes; allowing \(String(describing: machineID), privacy: .public)")
            return true
        }

        // Note: this function rejects grey devices: machineIDs that are neither allowed nor disallowed
        for mo in knownMachines where mo.machineID == machineID {
            if mo.status == TPMachineIDStatus.allowed.rawValue {
                return true
            } else {
                logger.info("machineID \(String(describing: machineID), privacy: .public) not explicitly allowed: \(String(describing: mo), privacy: .public)")
                return false
            }
        }

        // Didn't find it? reject.
        logger.notice("machineID \(String(describing: machineID), privacy: .public) not found on list")
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
        let trustedMachineIDs = Set(dynamicInfo.includedPeerIDs.compactMap {
            do {
                return try self.model.peer(withID: $0)?.permanentInfo.machineID
            } catch {
                logger.warning("Error getting peer with machineID \($0, privacy: .public): \(String(describing: error), privacy: .public)")
                return nil
            }
        })

        // if this account is not a demo account...
        if self.enforceIDMSListChanges(knownMachines: machines) {
            for peerMachineID in trustedMachineIDs.subtracting(knownMachineIDs) {
                logger.info("Peer machineID is unknown, beginning grace period: \(String(describing: peerMachineID), privacy: .public)")
                let machine = MachineMO(context: self.moc)
                machine.machineID = peerMachineID
                machine.container = self.containerMO
                machine.seenOnFullList = false
                machine.modified = Date()
                machine.status = Int64(TPMachineIDStatus.unknown.rawValue)

                self.containerMO.addToMachines(machine)
            }
        } else {
            logger.info("Not enforcing IDMS list changes")
        }

        // Remove all disallowed MIDs, unless we continue to trust the peer for some other reason
        for mo in (machines) where mo.status == TPMachineIDStatus.disallowed.rawValue && !trustedMachineIDs.contains(mo.machineID ?? "") {
            logger.notice("Dropping knowledge of machineID \(String(describing: mo.machineID), privacy: .public)")
            self.containerMO.removeFromMachines(mo)
        }
    }

    // Computes if a full list fetch would be 'useful'
    // Useful means that there's an unknown MID or evicted MID or unknownReasonRemoval whose modification date is before the cutoff
    // A full list fetch would either confirm it as 'untrusted' or make it trusted again
    func onqueueFullIDMSListWouldBeHelpful() -> Bool {

        if self.containerMO.honorIDMSListChanges == "UNKNOWN" {
            return true
        }

        let outdatedGracePeriodMachineIDMOs = (containerMO.machines as? Set<MachineMO> ?? Set()).filter {
            $0.status == TPMachineIDStatus.unknown.rawValue && !$0.modifiedInPast(hours: unknownCutoffHours) ||
            $0.status == TPMachineIDStatus.evicted.rawValue && !$0.modifiedInPast(hours: evictedCutoffHours) ||
            $0.status == TPMachineIDStatus.unknownReason.rawValue && !$0.modifiedInPast(hours: unknownReasonCutoffHours) ||
            $0.status == TPMachineIDStatus.ghostedFromTDL.rawValue && !$0.modifiedInPast(hours: ghostedCutoffHours)
        }

        return !outdatedGracePeriodMachineIDMOs.isEmpty
    }

    func fullIDMSListWouldBeHelpful() -> Bool {
        self.moc.performAndWait {
            self.onqueueFullIDMSListWouldBeHelpful()
        }
    }
}
