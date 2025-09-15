import ACMLib
import APFS
import AppleKeyStore
import DiskManagement
import LocalAuthentication
import Security_Private.AuthorizationTagsPriv
import aks_fv
import os

public struct DiskManager {
    private let manager: DMManager
    private let apfs: DMAPFS

    public init?() {
        self.manager = DMManager()
        self.manager.setAuthorizationInteraction(false)
        self.manager.setDefaultDASession(DASessionCreate(kCFAllocatorDefault))
        guard let apfs = DMAPFS(manager: manager) else {
            Logger.fvunlock.error("failed to initialize DMManager")
            return nil
        }
        self.apfs = apfs
    }

    public func volumeGroupUUID(path: String) throws -> UUID {
        var err: DMDiskErrorType = .diskErrorNoError
        guard let disk = manager.copyDisk(forPath: path, error: &err) else {
            throw Error.diskError(err)
        }

        var volumeGroupUUID: NSString? = nil
        err = apfs.volumeGroup(forVolume: disk, id: &volumeGroupUUID)
        guard let volumeGroupUUID = volumeGroupUUID as? String, err == .diskErrorNoError else {
            throw Error.diskError(err)
        }

        guard let volumeGroupUUID = UUID(uuidString: volumeGroupUUID) else {
            throw Error.invalidUUID(volumeGroupUUID)
        }

        return volumeGroupUUID
    }

    public func unlockVolume(_ path: String, username: String, password: Data) throws(Error) {
        try setACMEnvironment(username: username, password: password)

        var err: DMDiskErrorType = .diskErrorNoError
        guard let disk = manager.copyDisk(forPath: path, error: &err) else {
            Logger.fvunlock.error("failed to copy disk for path \(path): \(err.rawValue)")
            throw .diskError(err)
        }
        guard let volumeUUID = manager.volumeUUID(for: disk, error: &err) else {
            Logger.fvunlock.error("failed to get volumeUUID for disk at \(path): \(err.rawValue)")
            throw .diskError(err)
        }

        guard let validVolumeUUID = UUID(uuidString: volumeUUID) else {
            Logger.fvunlock.error("invalid volume UUID: \(volumeUUID)")
            throw .invalidUUID(volumeUUID)
        }

        var volumeGroupUUID: NSString? = nil
        err = apfs.volumeGroup(forVolume: disk, id: &volumeGroupUUID)
        guard let volumeGroupUUID = volumeGroupUUID as? String, err == .diskErrorNoError else {
            Logger.fvunlock.error(
                "failed to get volumeGroupUUID for disk at \(path): \(err.rawValue)")
            throw .diskError(err)
        }

        guard let validVolumeGroupUUID = UUID(uuidString: volumeGroupUUID) else {
            Logger.fvunlock.error("invalid volume group UUID: \(volumeGroupUUID)")
            throw .invalidUUID(volumeGroupUUID)
        }

        var dataVolumes: NSArray?
        err = apfs.disks(
            forVolumeGroup: volumeGroupUUID as String, volumeDisks: nil, systemVolumeDisks: nil,
            dataVolumeDisks: &dataVolumes, userVolumeDisks: nil, container: nil)
        guard err == .diskErrorNoError, let dataVolumes = dataVolumes as? [DADisk] else {
            Logger.fvunlock.error(
                "failed to resolve data volumes for \(path) [uuid: \(volumeUUID), group: \(volumeGroupUUID)]"
            )
            throw .diskError(err)
        }

        let userRecord: [String: Any]?
        do {
            userRecord = try Authorization.airUserRecord(
                for: username, volume: validVolumeGroupUUID)
        } catch {
            Logger.fvunlock.error(
                "error getting air user record for \(username), volume: \(validVolumeUUID)")
            throw Error.authorizationError(error)
        }

        guard let userRecord else {
            Logger.fvunlock.error(
                "no air user record available for \(username), volume: \(validVolumeUUID)")
            throw .missingUserRecord(username: username)
        }

        guard let kek = userRecord[PLUDB_KEK] as? Data else {
            Logger.fvunlock.error("missing kek")
            throw .missingKEK
        }

        guard let vek = userRecord[PLUDB_VEK] as? Data else {
            Logger.fvunlock.error("missing vek")
            throw .missingVEK
        }

        Logger.fvunlock.debug("unlocking filevault via aks")
        try unlockFileVaultAKS(kek: kek, kekPassword: password, vek: vek)

        for dataVolume in dataVolumes {
            let bsdName = DADiskGetBSDName(dataVolume)
            let nameStr = String(cString: bsdName!)
            Logger.fvunlock.debug("unlocking data volume: \(nameStr)")
            var kekUUID: uuid_t = UUID_NULL
            let err = APFSVolumeUnlockAnyUnlockRecordWithOptions(
                bsdName, password as CFData, &kekUUID, UInt64(apfs_fv_options_none))
            guard err == 0 || err == EALREADY else {
                Logger.fvunlock.error(
                    "failed to unlock data volume \(nameStr): \(String(cString:strerror(err)))")
                throw .apfsUnlockError(err)
            }
        }
    }

    public static func pivotRoot(root: String) -> Int32 {
        root.withCString { cstr in
            reboot3_1_str(RB3_PIVOTROOT, cstr)
        }
    }

    private func unlockFileVaultAKS(kek: Data, kekPassword: Data, vek: Data) throws(Error) {
        let context = LAContext()

        var externalizedContext = context.externalizedContext!

        let rc: Int32 = kekPassword.withUnsafeBytes { passwdPtr in
            var aksSecret = aks_fv_data_s(
                data: .init(mutating: passwdPtr.baseAddress!), len: passwdPtr.count)

            return kek.withUnsafeBytes { kekPtr in
                var aksKEK = aks_fv_data_s(
                    data: .init(mutating: kekPtr.baseAddress!), len: kekPtr.count)

                return vek.withUnsafeBytes { vekPtr in
                    var aksVEK = aks_fv_data_s(
                        data: .init(mutating: vekPtr.baseAddress!), len: vekPtr.count)

                    return externalizedContext.withUnsafeMutableBytes { acmPtr in
                        var acm = aks_fv_data_s(data: acmPtr.baseAddress!, len: acmPtr.count)
                        return aks_fv_verify_user_opts(
                            &aksSecret, &aksKEK, &aksVEK, &acm, UInt32(aks_fv_unwrap_vek_opts_none))
                    }
                }
            }
        }

        guard rc == kAKSReturnSuccess else {
            Logger.fvunlock.error("aks failed to unlock: \(rc, privacy: .public)")
            throw .aksError(rc)
        }

        let acmContext = externalizedContext.withUnsafeBytes { acmPtr in
            ACMContextCreateWithExternalForm(acmPtr.baseAddress!, acmPtr.count)
        }

        guard let acmContext else {
            Logger.fvunlock.error("failed to initialize acm context")
            throw .invalidACM
        }
        defer { ACMContextDelete(acmContext, false) }

        ACMContextVerifyPolicyEx(
            acmContext, kACMPolicyUserAuthenticationWithPasscodeRecovery, false, nil, 0, 0
        ) { status, satisifed, req in
            if status == kACMErrorSuccess && satisifed {
                // log success
                Logger.fvunlock.info("acm policy passed")
            } else {
                // log acm failure
                Logger.fvunlock.error("acm policy failed")
            }
        }
    }

    private func setACMEnvironment(username: String, password: Data) throws(Error) {
        let usernameStatus = username.withCString { usernamePtr in
            ACMSetEnvironmentVariable(
                UInt32(kACMEnvironmentVariableLoginUserName), .init(usernamePtr), username.count)
        }
        let passwordStatus = password.withUnsafeBytes { passwordPtr in
            ACMSetEnvironmentVariable(
                UInt32(kACMEnvironmentVariableLoginUserName), passwordPtr.baseAddress!,
                username.count)
        }

        guard usernameStatus == kACMErrorSuccess else {
            Logger.fvunlock.error("failed to set acm username: \(usernameStatus, privacy: .public)")
            throw .acmError(usernameStatus)
        }
        guard passwordStatus == kACMErrorSuccess else {
            Logger.fvunlock.error("failed to set acm password: \(passwordStatus, privacy: .public)")
            throw .acmError(passwordStatus)
        }
    }
}

extension DiskManager {
    public enum Error: Swift.Error {
        case diskError(DMDiskErrorType)
        case apfsUnlockError(errno_t)
        case invalidUUID(String)
        case authorizationError(Authorization.Error)
        case missingUserRecord(username: String)
        case missingKEK
        case missingVEK
        case aksError(Int32)
        case invalidACM
        case acmError(ACMStatus)
    }
}
