import Darwin
import System
import AppleKeyStore

struct HostKeyManager {
  // MARK: - Properties
  let keygen: FilePath
  let hostKeysDirectory: FilePath
  let hostKeysCopyDirectory: FilePath?
  let planTransformer: SSHDWrapper.PlanTransformer?

  // MARK: - Instance methods
  func generate(algorithm: Algorithm) async throws -> Bool {
    let path = hostKeysDirectory.appending(algorithm.privateKeyFilename)
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

  func copy(algorithm: Algorithm) throws -> Bool {
    guard let hostKeysCopyDirectory else { return false }
    
    let destinationPub = hostKeysCopyDirectory.appending(algorithm.publicKeyFilename)
    let unencryptedDestinationPriv = hostKeysCopyDirectory.appending(algorithm.privateKeyFilename)
    let destinationPriv = hostKeysCopyDirectory.appending(algorithm.privateKeyFilename + ".enc")
    let destinationRefKey = hostKeysCopyDirectory.appending(algorithm.privateKeyFilename + ".refkey")
    let sourcePub = hostKeysDirectory.appending(algorithm.publicKeyFilename)
    let sourcePriv = hostKeysDirectory.appending(algorithm.privateKeyFilename)
    
    // We can only reliably check if we have already copied host keys by checking that the public key
    // is identical.
    // Also check if we are migrating from the old setup where we had unencrypted private keys on disk,
    // and proceed with a recopy if so.
    if sourcePub.contentsEqual(destinationPub), !unencryptedDestinationPriv.exists() {
      return false
    }
    
    // always try and delete any plaintext private key previously stored
    try unencryptedDestinationPriv.unlink()
    try hostKeysCopyDirectory.mkdir()
    
    // first copy the public key, then write an encrypted version of the private key
    let state: copyfile_state_t? = nil
    let flags = copyfile_flags_t(COPYFILE_STAT | COPYFILE_DATA)
    let rc = copyfile(sourcePub.string, destinationPub.string, state, flags)
    guard rc == 0 else {
      throw Error.copyfileFailed(Errno(rawValue: Darwin.errno), sourcePub, destinationPub)
    }
    
    // now slurp the private key and encrypt it with a new ref key
    let privateKeyFileBytes: [UInt8] = try sourcePriv.slurpBytes()
    let refKey = try AKSRefKey(handle: .bad, key_class: .wku, key_type: .sym)
    
    // symmetric ref key will GCM encrypt with its own randomly generated IV.
    let cipherText = try refKey.encrypt(data: privateKeyFileBytes)
    
    // write out the cipherText and refKey to preboot
    try destinationPriv.spew(bytes: cipherText)
    try destinationRefKey.spew(bytes: refKey.serialize())

    return true
  }

  // MARK: - Initialization
  init(keygen: FilePath, hostKeysDirectory: FilePath, hostKeysCopyDirectory: FilePath? = nil, plan: SSHDWrapper.PlanTransformer? = nil) {
    self.keygen = keygen
    self.hostKeysDirectory = hostKeysDirectory
    self.hostKeysCopyDirectory = hostKeysCopyDirectory
    self.planTransformer = plan
  }

  // MARK: - Supporting types
  enum Algorithm: String, CaseIterable {
    case ecdsa, ed25519, rsa

    var privateKeyFilename: String {
      "ssh_host_\(self)_key"
    }
    var publicKeyFilename: String {
      "ssh_host_\(self)_key.pub"
    }
  }

  enum Error: Swift.Error, CustomStringConvertible {
    case commandFailed(String, String, Subprocess.Result)
    case copyfileFailed(Errno, FilePath, FilePath)

    var description: String {
      return
        switch self
      {
      case .commandFailed(let command, let error, let result):
        "\(command): \(result): \(error)"
      case .copyfileFailed(let errno, let source, let destination):
        "Failed copying from “\(source)” to “\(destination)”: [\(errno.rawValue): \(errno)]"
      }
    }
  }
}

