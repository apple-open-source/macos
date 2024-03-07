import System

struct HostKeyGenerator {
  // MARK: - Properties
  let keygen: FilePath
  let directory: FilePath
  let planTransformer: SSHDWrapper.PlanTransformer?

  // MARK: - Instance methods
  func generate(algorithm: Algorithm) async throws -> Bool {
    let path = directory.appending("ssh_host_\(algorithm)_key")
    if path.exists() {
      return false
    }
    var plan = Subprocess.Plan(
      path: keygen,
      arguments: ["-q", "-t", algorithm.rawValue, "-f", path.string, "-N", "", "-C", ""],
      inputDisposition: .null,
      outputDisposition: .bytes,
      errorDisposition: .bytes
    )
    if let planTransformer {
      plan = planTransformer(plan)
    }
    let process = Subprocess(plan)
    let result = try await process.run()
    if !result.success {
      let errorString = try? process.errorString
      throw Error.commandFailed(process.command, errorString ?? "unknown error", result)
    }
    return true
  }

  // MARK: - Initilization
  init(keygen: FilePath, directory: FilePath, plan: SSHDWrapper.PlanTransformer? = nil) {
    self.keygen = keygen
    self.directory = directory
    self.planTransformer = plan
  }

  // MARK: - Supporting types
  enum Algorithm: String, CaseIterable {
    case dsa, ecdsa, ed25519, rsa
  }

  enum Error: Swift.Error, CustomStringConvertible {
    case commandFailed(String, String, Subprocess.Result)

    var description: String {
      return
        switch self
      {
      case .commandFailed(let command, let error, let result):
        "\(command): \(result): \(error)"
      }
    }
  }
}

