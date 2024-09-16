import CoreData
import Foundation

private let logger = Logger(subsystem: "com.apple.security.trustedpeers", category: "escrowrecords")

extension Container {

    func onqueueCachedRecordsContainEgoPeerBottle(cachedRecords: [OTEscrowRecord]) -> Bool {
        guard let egoPeerID = self.containerMO.egoPeerID else {
            logger.info("onqueueCachedRecordsContainEgoPeerBottle: No identity.")
            return false
        }
        guard let bottles: Set<BottleMO> = self.containerMO.bottles as? Set<BottleMO>  else {
            logger.info("onqueueCachedRecordsContainEgoPeerBottle: No Bottles.")
            return false
        }
        var matchesCached: Bool = false
        for bottle in bottles {
            guard let bottleID: String = bottle.bottleID else {
                continue
            }
            if bottle.peerID == egoPeerID && (cachedRecords.compactMap { $0.escrowInformationMetadata.bottleId }).contains(bottleID) {
                matchesCached = true
                break
            }
        }
        return matchesCached
    }

    func escrowRecordMOToEscrowRecords(record: EscrowRecordMO, viability: Viability) -> OTEscrowRecord? {
        let escrowRecord = OTEscrowRecord()
        let escrowRecordMetadata = OTEscrowRecordMetadata()
        let clientMetadata = OTEscrowRecordMetadataClientMetadata()

        if let e = escrowRecord {
            if let creationDate = record.creationDate {
                e.creationDate = UInt64(creationDate.timeIntervalSince1970)
            }
            e.label = record.label ?? ""
            e.remainingAttempts = UInt64(record.remainingAttempts)
            e.silentAttemptAllowed = UInt64(record.silentAttemptAllowed)
            e.recordStatus = record.recordStatus == 0 ? .RECORD_STATUS_VALID : .RECORD_STATUS_INVALID
            e.federationId = record.federationID ?? ""
            e.expectedFederationId = record.expectedFederationID ?? ""

            switch viability {
            case .full:
                e.recordViability = .RECORD_VIABILITY_FULLY_VIABLE
            case .partial:
                e.recordViability = .RECORD_VIABILITY_PARTIALLY_VIABLE
            case .none:
                e.recordViability = .RECORD_VIABILITY_LEGACY
            }

            switch record.sosViability {
            case 0:
                e.viabilityStatus = .SOS_VIABLE_UNKNOWN
            case 1:
                e.viabilityStatus = .SOS_VIABLE
            case 2:
                e.viabilityStatus = .SOS_NOT_VIABLE
            default:
                e.viabilityStatus = .SOS_VIABLE_UNKNOWN
            }

            if let metadata = escrowRecordMetadata {
                if let m = record.escrowMetadata {
                    metadata.backupKeybagDigest = m.backupKeybagDigest ?? Data()
                    if let timestamp = m.secureBackupTimestamp {
                        metadata.secureBackupTimestamp = UInt64(timestamp.timeIntervalSince1970)
                    }
                    metadata.secureBackupUsesMultipleIcscs = UInt64(m.secureBackupUsesMultipleiCSCS)
                    metadata.bottleId = m.bottleID ?? ""
                    metadata.escrowedSpki = m.escrowedSPKI ?? Data()
                    metadata.peerInfo = m.peerInfo ?? Data()
                    metadata.serial = m.serial ?? ""
                    metadata.build = m.build ?? ""
                    if let passcodeGeneration = m.passcodeGen {
                        if let passcodeGen = OTEscrowRecordMetadataPasscodeGeneration() {
                            passcodeGen.value = UInt64(passcodeGeneration.value)
                            metadata.passcodeGeneration = passcodeGen
                        }
                    }
                    if let cmToFill = clientMetadata {
                        if let cm = m.clientMetadata {
                            cmToFill.deviceMid = cm.deviceMid ?? ""
                            cmToFill.deviceColor = cm.deviceColor ?? ""
                            cmToFill.deviceModel = cm.deviceModel ?? ""
                            cmToFill.deviceName = cm.deviceName ?? ""
                            cmToFill.devicePlatform = UInt64(cm.devicePlatform)
                            cmToFill.deviceModelClass = cm.deviceModelClass ?? ""
                            cmToFill.deviceModelVersion = cm.deviceModelVersion ?? ""
                            cmToFill.deviceEnclosureColor = cm.deviceEnclosureColor ?? ""
                            if let timestamp = cm.secureBackupMetadataTimestamp {
                                cmToFill.secureBackupMetadataTimestamp = UInt64(timestamp.timeIntervalSince1970)
                            }
                            cmToFill.secureBackupUsesComplexPassphrase = UInt64(cm.secureBackupUsesComplexPassphrase)
                            cmToFill.secureBackupUsesNumericPassphrase = UInt64(cm.secureBackupUsesNumericPassphrase)
                            cmToFill.secureBackupNumericPassphraseLength = UInt64(cm.secureBackupNumericPassphraseLength)
                        }
                    }
                    metadata.clientMetadata = clientMetadata
                }
                e.escrowInformationMetadata = metadata
            }
        }

        return escrowRecord
    }

    func setEscrowRecord(record: EscrowInformation, viability: Viability) {
        let escrowRecordMO = EscrowRecordMO(context: self.moc)
        escrowRecordMO.label = record.label
        escrowRecordMO.creationDate = record.creationDate.date
        escrowRecordMO.remainingAttempts = Int64(record.remainingAttempts)
        escrowRecordMO.silentAttemptAllowed = Int64(record.silentAttemptAllowed)
        escrowRecordMO.recordStatus = Int64(record.recordStatus.rawValue)
        escrowRecordMO.sosViability = Int64(record.viabilityStatus.rawValue)
        escrowRecordMO.federationID = record.federationID
        escrowRecordMO.expectedFederationID = record.expectedFederationID

        let escrowRecordMetadataMO = EscrowMetadataMO(context: self.moc)
        escrowRecordMetadataMO.backupKeybagDigest = record.escrowInformationMetadata.backupKeybagDigest
        escrowRecordMetadataMO.secureBackupUsesMultipleiCSCS = Int64(record.escrowInformationMetadata.secureBackupUsesMultipleIcscs)
        escrowRecordMetadataMO.bottleID = record.escrowInformationMetadata.bottleID
        escrowRecordMetadataMO.secureBackupTimestamp = record.escrowInformationMetadata.secureBackupTimestamp.date
        escrowRecordMetadataMO.escrowedSPKI = record.escrowInformationMetadata.escrowedSpki
        escrowRecordMetadataMO.peerInfo = record.escrowInformationMetadata.peerInfo
        escrowRecordMetadataMO.serial = record.escrowInformationMetadata.serial
        escrowRecordMetadataMO.build = record.escrowInformationMetadata.build
        if record.escrowInformationMetadata.hasPasscodeGeneration {
            let passcodeGen = PasscodeGen(context: self.moc)
            passcodeGen.value = Int64(record.escrowInformationMetadata.passcodeGeneration.value)
            escrowRecordMetadataMO.passcodeGen = passcodeGen
        }
        escrowRecordMO.escrowMetadata = escrowRecordMetadataMO

        let escrowRecordClientMetadataMO = EscrowClientMetadataMO(context: self.moc)
        escrowRecordClientMetadataMO.secureBackupMetadataTimestamp = record.escrowInformationMetadata.clientMetadata.secureBackupMetadataTimestamp.date
        escrowRecordClientMetadataMO.secureBackupNumericPassphraseLength = Int64(record.escrowInformationMetadata.clientMetadata.secureBackupNumericPassphraseLength)
        escrowRecordClientMetadataMO.secureBackupUsesComplexPassphrase = Int64(record.escrowInformationMetadata.clientMetadata.secureBackupUsesComplexPassphrase)
        escrowRecordClientMetadataMO.secureBackupUsesNumericPassphrase = Int64(record.escrowInformationMetadata.clientMetadata.secureBackupUsesNumericPassphrase)
        escrowRecordClientMetadataMO.deviceColor = record.escrowInformationMetadata.clientMetadata.deviceColor
        escrowRecordClientMetadataMO.deviceEnclosureColor = record.escrowInformationMetadata.clientMetadata.deviceEnclosureColor
        escrowRecordClientMetadataMO.deviceMid = record.escrowInformationMetadata.clientMetadata.deviceMid
        escrowRecordClientMetadataMO.deviceModel = record.escrowInformationMetadata.clientMetadata.deviceModel
        escrowRecordClientMetadataMO.deviceModelClass = record.escrowInformationMetadata.clientMetadata.deviceModelClass
        escrowRecordClientMetadataMO.deviceModelVersion = record.escrowInformationMetadata.clientMetadata.deviceModelVersion
        escrowRecordClientMetadataMO.deviceName = record.escrowInformationMetadata.clientMetadata.deviceName
        escrowRecordClientMetadataMO.devicePlatform = Int64(record.escrowInformationMetadata.clientMetadata.devicePlatform)

        escrowRecordMetadataMO.clientMetadata = escrowRecordClientMetadataMO

        logger.info("setEscrowRecord saving new escrow record: \(escrowRecordMO, privacy: .public)")
        switch viability {
        case .full:
            self.containerMO.addToFullyViableEscrowRecords(escrowRecordMO)
        case .partial:
            self.containerMO.addToPartiallyViableEscrowRecords(escrowRecordMO)
        case .none:
            self.containerMO.addToLegacyEscrowRecords(escrowRecordMO)
        }
    }

    func onqueueCachedBottlesFromEscrowRecords() -> (TPCachedViableBottles) {
        var viableRecords: [String] = []
        var partiallyViableRecords: [String] = []

        if let fullyViableEscrowRecords = self.containerMO.fullyViableEscrowRecords as? Set<EscrowRecordMO> {
            viableRecords = fullyViableEscrowRecords.compactMap { $0.escrowMetadata?.bottleID }
        }
        if let partiallyViableEscrowRecords = self.containerMO.partiallyViableEscrowRecords as? Set<EscrowRecordMO> {
            partiallyViableRecords = partiallyViableEscrowRecords.compactMap { $0.escrowMetadata?.bottleID }
        }
        return TPCachedViableBottles(viableBottles: viableRecords, partialBottles: partiallyViableRecords)
    }

    func onqueueCachedEscrowRecords() -> ([OTEscrowRecord]) {
        var escrowRecords: [OTEscrowRecord] = []

        if let fullyViableEscrowRecords = self.containerMO.fullyViableEscrowRecords as? Set<EscrowRecordMO> {
            for record in fullyViableEscrowRecords {
                let convertedRecord = escrowRecordMOToEscrowRecords(record: record, viability: .full)
                if let r = convertedRecord {
                    escrowRecords.append(r)
                }
            }
        }
        if let partiallyViableEscrowRecords = self.containerMO.partiallyViableEscrowRecords as? Set<EscrowRecordMO> {
            for record in partiallyViableEscrowRecords {
                let convertedRecord = escrowRecordMOToEscrowRecords(record: record, viability: .partial)
                if let r = convertedRecord {
                    escrowRecords.append(r)
                }
            }
        }
        if let legacyEscrowRecords = self.containerMO.legacyEscrowRecords as? Set<EscrowRecordMO> {
            for record in legacyEscrowRecords {
                let convertedRecord = escrowRecordMOToEscrowRecords(record: record, viability: .none)
                if let r = convertedRecord {
                    escrowRecords.append(r)
                }
            }
        }
        return escrowRecords
    }

    func fetchEscrowRecords(from source: OTEscrowRecordFetchSource, with reply: @escaping ([Data]?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        self.fetchEscrowRecordsWithSemaphore(from: source) { result in
            sem.release()

            switch result {
            case let .success(data):
                logger.info("fetchEscrowRecords succeeded with \(data.count, privacy: .public) records")
                reply(data, nil)
            case let .failure(error):
                logger.error("fetchEscrowRecords failed with \(traceError(error), privacy: .public)")
                reply(nil, error)
            }
        }
    }
}
