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
    dump(arguments, name: "arguments")
    guard arguments.count <= 1 else {
      throw Error.unexpectedArgument(arguments[1])
    }

    let sshdArguments = ["-i"]
    let sshKeygen = systemProperties.pathPrefix.appending("bin/ssh-keygen")
    let generator = HostKeyGenerator(
      keygen: sshKeygen,
      directory: systemProperties.sshDirectory,
      plan: hostKeyPlanTransformer)
    for algorithm in HostKeyGenerator.Algorithm.allCases {
      do {
        if try await generator.generate(algorithm: algorithm) {
          logger.info("Generated \(algorithm.rawValue) host key")
        }
      } catch {
        logger.error("Failed to generate \(algorithm.rawValue) host key: \(error)")
      }
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
