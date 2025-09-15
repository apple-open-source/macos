import AppleKeyStore
import CoreAnalytics
import Foundation
import System

struct SSHDWrapper {
  // MARK: - Strategy types
  typealias PlanTransformer = (Subprocess.Plan) -> Subprocess.Plan

  // MARK: - Dependencies
  let hostKeyPlanTransformer: PlanTransformer?
  let sshdPlanTransformer: PlanTransformer?
  let systemProperties: SystemPropertiesStrategy

  // MARK: - Instance methods
  func usage() {
    _ = try? FileDescriptor.standardError.writeAll(
      "Usage: sshd-keygen-wrapper\n".utf8)
  }

  /// sshd-keygen-wrapper’s main entry point. First generates host
  /// keys, if not already present. Then builds `sshd` command
  /// line arguments appropriate for the platform and system
  /// configuration, and finally spawns `sshd`.
  func run(_ arguments: [String] = CommandLine.arguments) async throws {
    guard arguments.count <= 1 else {
      throw Error.unexpectedArgument(arguments[1])
    }

    let volumeGroupUUID: String? = {
      do {
        return try systemProperties.volumeGroupUUID
      } catch {
        logger.error("Could not determine volume group UUID: \(error)")
        return nil
      }
    }()
    let prebootDirectory: FilePath? =
      if let volumeGroupUUID {
        systemProperties.prebootVolumePrefix.appending(
          [volumeGroupUUID, "var", "db", "sshd"].map { FilePath.Component($0)! }
        )
      } else {
        nil
      }

    let hostKeysDirectory =
      if !systemProperties.isBaseSystem {
        systemProperties.sshDirectory
      } else if let prebootDirectory {
        prebootDirectory
      } else {
        systemProperties.temporaryDirectory
      }
    let hostKeysCopyDirectory: FilePath? =
      if !systemProperties.isBaseSystem {
        prebootDirectory
      } else {
        nil
      }

    logger.debug(
      "hostKeysDirectory=\(hostKeysDirectory.string) hostKeysCopyDirectory=\(hostKeysCopyDirectory?.string ?? "nil")"
    )
    let hostKeyManager = HostKeyManager(
      keygen: systemProperties.pathPrefix.appending("bin/ssh-keygen"),
      hostKeysDirectory: hostKeysDirectory,
      hostKeysCopyDirectory: hostKeysCopyDirectory,
      plan: hostKeyPlanTransformer)
    for algorithm in HostKeyManager.Algorithm.allCases {
      do {
        if try await hostKeyManager.generate(algorithm: algorithm) {
          logger.info("Generated \(algorithm.rawValue) host key")
        }
      } catch {
        logger.error("Failed to generate \(algorithm.rawValue) host key: \(error)")
      }
      do {
        if try hostKeyManager.copy(algorithm: algorithm) {
          logger.info("Copied \(algorithm.rawValue) host key to basesystem")
        }
      } catch {
        logger.error("Could not copy \(algorithm.rawValue) host key: \(error)")
      }
    }

    /// If Remote Login has never been toggled after an update
    /// from an earlier version of macOS, then the
    /// sshd-fvunlock.plist will not exist in the preboot directory.
    /// Create it with the setting Enabled = true.
    let sshdFVUnlockPlist = hostKeysCopyDirectory?.appending("sshd-fvunlock.plist")
    if let hostKeysCopyDirectory,
      let sshdFVUnlockPlist,
      !systemProperties.isBaseSystem,
      !sshdFVUnlockPlist.exists()
    {
      try? hostKeysCopyDirectory.mkdir()
      do {
        try sshdFVUnlockPlist.spew(
           """
          <?xml version="1.0" encoding="UTF-8"?>
          <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
          <plist version="1.0">
          <dict>
                  <key>Enabled</key>
                  <true/>
          </dict>
          </plist>
          """)
      } catch {
        logger.error("Could not create sshd-fvunlock.plist: \(error)")
      }
    }

    var sshdArguments = ["-i"]

    /// Create a banner to explain the behavior of SSH in base system
    let banner = FilePath("/var/run/banner")
    if systemProperties.isBaseSystem, !banner.exists() {
      do {
        try banner.spew(
          """
          This system is locked. To unlock it, use a local
          account name and password. Once successfully
          unlocked, you will be able to connect normally.

          """)
      } catch {
        logger.error("Could not create \(banner): \(error)")
      }
      sshdArguments.append("-oBanner=\(banner)")
    }

    /// If we are running on the base system, we will use the host keys
    /// that were previously sync'd from the data volume.
    if systemProperties.isBaseSystem {
      for algorithm in HostKeyManager.Algorithm.allCases {
        let privateKeyFilePath = hostKeysDirectory.appending(algorithm.privateKeyFilename)
        if privateKeyFilePath.exists() {
          sshdArguments.append("-oHostKey=\(hostKeysDirectory)/\(algorithm.privateKeyFilename)")
        } else {
          // we have ad-hoc encrypted host key, decrypt it
          let encryptedPrivateKeyFilePath = hostKeysDirectory.appending(algorithm.privateKeyFilename + ".enc")
          let refKeyFilePath = hostKeysDirectory.appending(algorithm.privateKeyFilename + ".refkey")
          
          let refKeyBytes = try refKeyFilePath.slurpBytes()
          let encryptedPrivateKeyBytes = try encryptedPrivateKeyFilePath.slurpBytes()
          
          let refKey = try AKSRefKey(handle: .bad, blob: refKeyBytes)
          let clearText = try refKey.decrypt(data: encryptedPrivateKeyBytes)
          
          let tmpDir = FilePath("/tmp/ssh")
          let decryptedPrivateKeyDest = tmpDir.appending(algorithm.privateKeyFilename)
          // write clearText to
          try tmpDir.mkdir(mode: .ownerReadWrite)
          try decryptedPrivateKeyDest.spew(bytes: clearText)
          
          sshdArguments.append("-oHostKey=\(decryptedPrivateKeyDest.string)")
        }
      }
      sshdArguments.append("-oUsePAM=yes")
      sshdArguments.append("-oPamServiceName=sshd-basesystem")
      sshdArguments.append("-oAppleBaseSystem=yes")
    }
    let sshd = systemProperties.pathPrefix.appending("sbin/sshd")
    var plan = Subprocess.Plan(path: sshd, arguments: sshdArguments)
    plan.flags = [.setExec]
    if let sshdPlanTransformer {
      plan = sshdPlanTransformer(plan)
    }
    let process = Subprocess(plan)
    let result = try await process.run()
    // only reachable during testing
    if !result.success {
      let errorString = try? process.errorString
      throw Error.sshdFailed(process.command, errorString ?? "unknown error", result)
    }
  }

  // MARK: - Initialization

  /// Creates object encapsulating the main logic for launching `sshd`.
  /// The parameters permit specifying optional dependencies for
  /// testing.
  /// - Parameters:
  ///   - hostKeyPlanTransformer: This function is given the
  ///     `Subprocess.Plan` for invocations of `ssh-keygen`, and
  ///     returns a modified plan that will be used.
  ///   - sshdPlanTransformer: As previous, but for invocations
  ///     of `sshd`.
  ///   - systemProperties: Provides properties representing
  ///     the system’s run time environment and configuration.
  init(
    hostKeyPlanTransformer: PlanTransformer? = nil,
    sshdPlanTransformer: PlanTransformer? = nil,
    systemProperties: SystemPropertiesStrategy = SystemProperties()
  ) {
    self.hostKeyPlanTransformer = hostKeyPlanTransformer
    self.sshdPlanTransformer = sshdPlanTransformer
    self.systemProperties = systemProperties
  }

  // MARK: - Errors
  enum Error: Swift.Error, CustomStringConvertible {
    case sshdFailed(String, String, Subprocess.Result)
    case unexpectedArgument(String)

    var description: String {
      return
        switch self
      {
      case .sshdFailed(let command, let error, let result):
        "\(command): \(result): \(error)"
      case .unexpectedArgument(let arg):
        "Unexpected argument: “\(arg)”."
      }
    }
  }
}
