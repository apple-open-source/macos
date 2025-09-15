import ArgumentParserInternal
import DarwinPrivate.os.variant
import Foundation
import os
import aks_fv

@main
struct FileVaultUnlock: ParsableCommand {
    static let pivotMountPath = "/System/Volumes/macOS"

    static let configuration = CommandConfiguration(
        commandName: "sshd-fvunlock",
        abstract: "Unlock FileVault over SSH in BaseSystem"
    )

    @Flag(inversion: .prefixedNo, help: "perform an userspace reboot after successful unlock")
    var reboot: Bool = true

    @Argument(help: "The username to authenticate as")
    var username: String

    func run() throws {
        do {
            try unlock()
        } catch Error.badPassword {
            print("Authentication failed: bad password")
            throw ExitCode(EX_NOPERM)
        } catch Error.temporarilyLocked {
            print("Authentication failed: account is temporarily locked")
            throw ExitCode(EX_TEMPFAIL)
        } catch Error.authenticationError(let code) {
            print("Authentication failed: code(\(code))")
            throw ExitCode(EX_NOPERM)
        } catch {
            print("Unexpected error: \(error)")
            throw ExitCode(EX_SOFTWARE)
        }
    }

    func unlock() throws {

        guard os_variant_is_basesystem("com.apple.sshd-fvunlock") else {
            throw Error.notInBaseSystem
        }

        Logger.fvunlock.debug(
            "unlocking \(Self.pivotMountPath) with user: \(username, privacy: .public)")

        guard let mgr = DiskManager() else {
            throw Error.instantiatingDiskManager
        }

        var password: Data
        Logger.fvunlock.debug("reading password from stdin")

        if isatty(FileHandle.standardInput.fileDescriptor) == 1 {
            Logger.fvunlock.debug("stdin is a tty")
            var buf = [UInt8](repeating: 0, count: 1024)
            guard readpassphrase("FileVault Password:", &buf, buf.count, 0) != nil else {
                throw Error.noInput
            }
            password = Data(buf)
            buf.resetBytes(in: 0...)
        } else {
            guard var passwordCString = try FileHandle.standardInput.readToEnd() else {
                throw Error.noInput
            }

            // password should be a null terminated C String
            // create a slice from 0..(firstNUL)
            guard let end = passwordCString.firstIndex(of: 0), end == passwordCString.count - 1
            else {
                throw Error.malformedInput
            }

            password = passwordCString[..<end]
            passwordCString.resetBytes(in: 0...)
        }
        Logger.fvunlock.debug("read password")
        defer {
            password.resetBytes(in: 0...)
        }

        do {
            try mgr.unlockVolume(Self.pivotMountPath, username: username, password: password)
        } catch DiskManager.Error.aksError(Int32(kAKSReturnBadPassword)) {
            throw Error.badPassword
        } catch DiskManager.Error.aksError(Int32(kAKSReturnPolicyError)) {
            throw Error.temporarilyLocked
        } catch DiskManager.Error.aksError(let code) {
            throw Error.authenticationError(code)
        } catch {
            throw error
        }

        if reboot {
            Logger.fvunlock.debug("pivoting root to \(Self.pivotMountPath, privacy: .public)")
            let pivotStatus = DiskManager.pivotRoot(root: Self.pivotMountPath)
            guard pivotStatus == 0 else {
                throw Error.pivotError(pivotStatus)
            }
        }
    }

    enum Error: Swift.Error {
        case notInBaseSystem
        case invalidArguments
        case instantiatingDiskManager
        case noInput
        case malformedInput
        case pivotError(Int32)
        case authenticationError(Int32)
        case badPassword
        case temporarilyLocked
    }
}

extension Data {
    var hexString: String {
        self.compactMap { String(format: "%02x", $0) }.joined()
    }
}
