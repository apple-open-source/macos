import Darwin
import Dispatch
import System

public final class Subprocess {
  public let plan: Plan

  private var pid_: pid_t = -1
  private var status: Int32 = 0

  private var outputBytes_: [UInt8] = []
  private var errorBytes_: [UInt8] = []

  public init(_ plan: Plan) {
    self.plan = plan
  }

  public convenience init(
    path: FilePath,
    arguments: [String],
    environment: [String: String]? = nil,
    inputDisposition: InputDisposition? = nil,
    outputDisposition: OutputDisposition? = nil,
    errorDisposition: OutputDisposition? = nil
  ) {
    let plan = Plan(
      path: path,
      arguments: arguments,
      environment: environment,
      inputDisposition: inputDisposition,
      outputDisposition: outputDisposition,
      errorDisposition: errorDisposition)
    self.init(plan)
  }

  public var pid: pid_t {
    get throws {
      guard pid_ != -1 else {
        throw Error.hasNotRun(plan.command)
      }
      return pid_
    }
  }

  public var command: String { plan.command }

  public var outputBytes: [UInt8] {
    get throws {
      guard pid_ != -1 else {
        throw Error.hasNotRun(plan.command)
      }
      guard plan.outputDisposition == .bytes else {
        throw Error.noOutputRequested(plan.command)
      }
      return outputBytes_
    }
  }

  public var outputString: String {
    get throws {
      let bytes = try outputBytes
      return bytes.trimmedString()
    }
  }

  public var errorBytes: [UInt8] {
    get throws {
      guard pid_ != -1 else {
        throw Error.hasNotRun(plan.command)
      }
      guard plan.errorDisposition == .bytes else {
        throw Error.noOutputRequested(plan.command)
      }
      return errorBytes_
    }
  }

  public var errorString: String {
    get throws {
      let bytes = try errorBytes
      return bytes.trimmedString()
    }
  }

  public func run() async throws -> Result {
    if pid_ > 0 {
      status = 0
      outputBytes_ = []
      errorBytes_ = []
    }
    pid_ = 0

    // Create any pipes required according to I/O dispositions
    let (inputPipe, inputActions) = try await prepareInput()
    let (outputPipe, outputActions) = try await prepareOutput(for: .standardOutput)
    let (errorPipe, errorActions) = try await prepareOutput(for: .standardError)

    // Prepare file actions, flags, and attributes
    var actions = try await prepareFileActions(inputActions + outputActions + errorActions)
    defer { posix_spawn_file_actions_destroy(&actions) }
    var attr = try await prepareFlagsAndAttributes()
    defer { posix_spawnattr_destroy(&attr) }

    // Invoke `posix_spawn`
    let argv = [plan.path.string] + plan.arguments
    let env = plan.environ.map { $0.key + "=" + $0.value }
    let rc = plan.path.withPlatformString { path in
      let argv = argv.map { $0.withCString(strdup) }
      let env = env.map { $0.withCString(strdup) }
      defer {
        argv.forEach { free($0) }
        env.forEach { free($0) }
      }
      return DispatchQueue.main.sync {
        return posix_spawn(&pid_, path, &actions, &attr, argv + [nil], env + [nil])
      }
    }
    guard rc == 0 else {
      throw Error.osError(Errno(rawValue: rc), "posix_spawn: “\(plan.command)”")
    }
    // The subprocess has been successfully started after this point

    // Close subprocess-side descriptors to avoid hangs
    await inputPipe?.closeReadEnd()
    await outputPipe?.closeWriteEnd()
    await errorPipe?.closeWriteEnd()

    // Read/write data according to I/O dispositions, and await exit of subprocess
    return try await withThrowingTaskGroup(of: Void.self, returning: Result.self) { group in
      let pid = pid_
      if let inputPipe {
        group.addTask { [self] in
          try await self.handleInput(inputPipe)
        }
      }
      if let outputPipe {
        group.addTask { [self] in
          try await self.handleOutput(for: .standardOutput, pipe: outputPipe)
        }
      }
      if let errorPipe {
        group.addTask { [self] in
          try await self.handleOutput(for: .standardError, pipe: errorPipe)
        }
      }
      group.addTask { [self] in
        try await self.waitpid(pid)
      }
      for try await _ in group {}
      return Result(status)
    }
  }

  private func prepareInput() async throws -> (Pipe?, [FileAction]) {
    var actions: [FileAction] = []
    switch plan.inputDisposition {
    case .standardIO:
      break
    case .closedDescriptor:
      actions += [.close(FileDescriptor.standardInput)]
    case .null:
      let path = FilePath(stringLiteral: "/dev/null")
      actions += [.open(FileDescriptor.standardInput, path, O_RDONLY, 0)]
    case .bytes:
      let pipe = try Pipe()
      actions += [
        .close(await pipe.writeEnd),
        .dup2(await pipe.readEnd, FileDescriptor.standardInput),
        .close(await pipe.readEnd),
      ]
      return (pipe, actions)
    }
    return (nil, actions)
  }

  private func prepareOutput(for rfd: RelatedFileDescriptor) async throws -> (Pipe?, [FileAction]) {
    let (disposition, standardFd) =
      switch rfd {
      case .standardOutput: (plan.outputDisposition, FileDescriptor.standardOutput)
      case .standardError: (plan.errorDisposition, FileDescriptor.standardError)
      }
    var actions: [FileAction] = []
    switch disposition {
    case .standardIO:
      break
    case .closedDescriptor:
      actions += [.close(standardFd)]
    case .null:
      let path = FilePath(stringLiteral: "/dev/null")
      actions += [.open(standardFd, path, O_WRONLY, 0)]
    case .bytes:
      let pipe = try Pipe()
      actions += [
        .close(await pipe.readEnd),
        .dup2(await pipe.writeEnd, standardFd),
        .close(await pipe.writeEnd),
      ]
      return (pipe, actions)
    case .standardOutput:
      guard disposition != .standardOutput else {
        throw Error.badDisposition
      }
      actions += [.dup2(FileDescriptor.standardOutput, standardFd)]
    }
    return (nil, actions)
  }

  private func prepareFileActions(_ extra: [FileAction]) async throws
    -> posix_spawn_file_actions_t?
  {
    var actions: posix_spawn_file_actions_t? = nil
    let rc = posix_spawn_file_actions_init(&actions)
    guard rc == 0 else {
      throw Error.osError(Errno(rawValue: rc), "posix_spawn_file_actions_init")
    }
    for action in plan.fileActions + extra {
      let rc =
        switch action {
        case .close(let fd): posix_spawn_file_actions_addclose(&actions, fd.rawValue)
        case .open(let fd, let path, let oflag, let mode):
          path.withPlatformString { path in
            posix_spawn_file_actions_addopen(&actions, fd.rawValue, path, oflag, mode)
          }
        case .dup2(let fd, let newfd):
          posix_spawn_file_actions_adddup2(&actions, fd.rawValue, newfd.rawValue)
        case .inherit(let fd): posix_spawn_file_actions_addinherit_np(&actions, fd.rawValue)
        case .chdir(let path):
          path.withPlatformString { path in
            posix_spawn_file_actions_addchdir_np(&actions, path)
          }
        case .fchdir(let fd): posix_spawn_file_actions_addfchdir_np(&actions, fd.rawValue)
        }
      guard rc == 0 else {
        throw Error.osError(Errno(rawValue: rc), "\(action)")
      }
    }
    return actions
  }

  private func prepareFlagsAndAttributes(
    flags extraFlags: [Flag] = [],
    attributes extraAttrs: [Attribute] = []
  ) async throws -> posix_spawnattr_t? {
    var attr: posix_spawnattr_t? = nil
    var rc = posix_spawnattr_init(&attr)
    guard rc == 0 else {
      throw Error.osError(Errno(rawValue: rc), "posix_spawnattr_init")
    }
    for attribute in extraAttrs + plan.attributes {
      let rc = {
        switch attribute {
        case .setProcessGroup(let pgroup):
          return posix_spawnattr_setpgroup(&attr, pgroup)
        case .setSignalDefault(let sigdefault):
          var sigdefault = sigdefault
          return posix_spawnattr_setsigdefault(&attr, &sigdefault)
        case .setSignalMask(let mask):
          var mask = mask
          return posix_spawnattr_setsigmask(&attr, &mask)
        }
      }()
      guard rc == 0 else {
        throw Error.osError(Errno(rawValue: rc), "\(attribute)")
      }
    }
    var value: Int16 = 0
    for flag in extraFlags + plan.flags {
      value |= flag.rawValue
    }
    rc = posix_spawnattr_setflags(&attr, value)
    guard rc == 0 else {
      throw Error.osError(Errno(rawValue: rc), "posix_spawnattr_setflags \(value)")
    }
    return attr
  }

  private func handleInput(_ pipe: Pipe) async throws {
    switch plan.inputDisposition {
    case .bytes(let data):
      try await pipe.writeAll(data)
      await pipe.closeWriteEnd()
    default:
      break
    }
  }

  private func handleOutput(for rfd: RelatedFileDescriptor, pipe: Pipe) async throws {
    let disposition =
      switch rfd {
      case .standardOutput: plan.outputDisposition
      case .standardError: plan.errorDisposition
      }

    switch disposition {
    case .bytes:
      while true {
        let buf = try await pipe.read()
        guard buf.count > 0 else {
          break
        }
        self.received(for: rfd, bytes: buf)
      }
      await pipe.closeReadEnd()
    default:
      break
    }
  }

  private func waitpid(_ pid: pid_t) async throws {
    var status: Int32 = -1
    guard Darwin.waitpid(pid, &status, 0) >= 0 else {
      throw Error.osError(Errno(rawValue: Darwin.errno), "waitpid: \(pid)")
    }
    finalize(status)
  }

  private func received(for rfd: RelatedFileDescriptor, bytes: [UInt8]) {
    switch rfd {
    case .standardOutput: outputBytes_ += bytes
    case .standardError: errorBytes_ += bytes
    }
  }

  private func finalize(_ status: Int32) {
    self.status = status
  }

  public struct Plan {
    public var path: FilePath
    public var arguments: [String]
    public var environ: [String: String]

    public var inputDisposition: InputDisposition = .standardIO
    public var outputDisposition: OutputDisposition = .standardIO
    public var errorDisposition: OutputDisposition = .standardIO

    public var fileActions: [FileAction] = []
    public var flags: [Flag] = []
    public var attributes: [Attribute] = []

    public var command: String {
      let argv = [path.string] + arguments
      return argv.map { $0.shellQuoted }.joined(separator: " ")
    }

    public init(
      path: FilePath,
      arguments: [String],
      environment: [String: String]? = nil,
      inputDisposition: InputDisposition? = nil,
      outputDisposition: OutputDisposition? = nil,
      errorDisposition: OutputDisposition? = nil
    ) {
      self.path = path
      self.arguments = arguments

      if let environment {
        self.environ = environment
      } else {
        var environment: [String: String] = [:]
        var p = Darwin.environ
        while let envp = p.pointee {
          let env = String(cString: envp)
          let pair = env.split(separator: "=", maxSplits: 1).map { String($0) }
          environment[pair[0]] = if pair.count == 1 { "" } else { pair[1] }
          p = p.successor()
        }
        self.environ = environment
      }

      if let d = inputDisposition { self.inputDisposition = d }
      if let d = outputDisposition { self.outputDisposition = d }
      if let d = errorDisposition { self.errorDisposition = d }
    }
  }

  public enum InputDisposition {
    case standardIO
    case closedDescriptor
    case null
    case bytes([UInt8])
  }

  public enum OutputDisposition {
    case standardIO
    case closedDescriptor
    case null
    case bytes
    case standardOutput
  }

  public enum FileAction {
    case close(FileDescriptor)
    case open(FileDescriptor, FilePath, Int32, mode_t)
    case dup2(FileDescriptor, FileDescriptor)
    case inherit(FileDescriptor)
    case chdir(FilePath)
    case fchdir(FileDescriptor)
  }

  public enum Attribute {
    case setProcessGroup(pid_t)
    case setSignalDefault(sigset_t)
    case setSignalMask(sigset_t)
  }

  public enum Flag: Int16 {
    case resetIDs
    case setProcessGroup
    case setSignalDefault
    case setSignalMask
    case setExec
    case startSuspended
    case closeOnExecByDefault

    public var rawValue: Int16 {
      let flag =
        switch self {
        case .resetIDs: POSIX_SPAWN_RESETIDS
        case .setProcessGroup: POSIX_SPAWN_SETPGROUP
        case .setSignalDefault: POSIX_SPAWN_SETSIGDEF
        case .setSignalMask: POSIX_SPAWN_SETSIGMASK
        case .setExec: POSIX_SPAWN_SETEXEC
        case .startSuspended: POSIX_SPAWN_START_SUSPENDED
        case .closeOnExecByDefault: POSIX_SPAWN_CLOEXEC_DEFAULT
        }
      return Int16(flag)
    }
  }

  public enum Result: CustomStringConvertible {
    case exited(_ status: Int32)
    case signaled(_ signal: Int32)
    case stopped(_ signal: Int32)

    public var success: Bool {
      if case let .exited(status) = self {
        return status == 0
      }
      return false
    }

    public var description: String {
      switch self {
      case .exited(let status):
        if status == 0 {
          return "exited successfully"
        }
        return "exited with status \(status)"
      case .signaled(let signal): return "terminated on signal \(signal)"
      case .stopped(let signal): return "stopped on signal \(signal)"
      }
    }

    public init(_ value: Int32) {
      self = .exited(0)
      if exited(value) {
        self = .exited(exitStatus(value))
      } else if signaled(value) {
        self = .signaled(terminatedSignal(value))
      } else if stopped(value) {
        self = .stopped(stoppedSignal(value))
      } else {
        preconditionFailure("unhandled wait status case: \(value)")
      }
    }

    private static let stoppedValue = 0177

    private func masked(_ value: Int32) -> Int32 {
      value & 0177
    }
    private func exitStatus(_ value: Int32) -> Int32 {
      (value >> 8) & 0xff
    }
    private func stoppedSignal(_ value: Int32) -> Int32 {
      value >> 8
    }
    private func stopped(_ value: Int32) -> Bool {
      masked(value) == Self.stoppedValue && stoppedSignal(value) != 0x13
    }
    private func exited(_ value: Int32) -> Bool {
      masked(value) == 0
    }
    private func signaled(_ value: Int32) -> Bool {
      masked(value) != Self.stoppedValue && masked(value) != 0
    }
    private func terminatedSignal(_ value: Int32) -> Int32 {
      masked(value)
    }
  }

  private enum RelatedFileDescriptor {
    case standardOutput, standardError
  }

  private actor Pipe {
    public var readEnd: FileDescriptor
    public var writeEnd: FileDescriptor

    init() throws {
      (readEnd, writeEnd) = try FileDescriptor.pipe()
    }

    func closeReadEnd() {
      guard readEnd.rawValue >= 0 else {
        return
      }
      let _ = try? readEnd.close()
    }

    func closeWriteEnd() {
      guard writeEnd.rawValue >= 0 else {
        return
      }
      let _ = try? writeEnd.close()
    }

    func read() throws -> [UInt8] {
      guard readEnd.rawValue >= 0 else {
        throw Error.pipeClosed
      }
      var buf = [UInt8](repeating: 0, count: 4096)
      var n = 0
      try buf.withUnsafeMutableBytes { n = try readEnd.read(into: $0) }
      return Array(buf[0..<n])
    }

    @discardableResult
    func writeAll<S>(_ sequence: S) throws -> Int where S: Sequence, S.Element == UInt8 {
      guard writeEnd.rawValue >= 0 else {
        throw Error.pipeClosed
      }
      return try writeEnd.writeAll(sequence)
    }

    deinit {
      for d in [readEnd, writeEnd].filter({ $0.rawValue >= 0 }) {
        let _ = try? d.close()
      }
    }
  }

  public enum Error: Swift.Error, CustomStringConvertible {
    case badDisposition
    case decode([UInt8], Bool)
    case hasNotRun(String)
    case noOutputRequested(String)
    case osError(Errno, String)
    case pipeClosed

    public var description: String {
      switch self {
      case .badDisposition: return "Cannot redirect standard output to itself."
      case .decode(let bytes, let truncated):
        var msg = ""
        for b in bytes {
          if msg.count > 0 {
            msg += " "
          }
          if (b & 0xF0) != 0 {
            msg += "0"
          }
          msg += String(b, radix: 16, uppercase: true)
        }
        msg = "Could not decode process output as UTF-8: [" + msg
        if truncated {
          msg += "…"
        }
        msg += "]"
        return msg
      case .noOutputRequested(let cmd): return "No capturing of output was requested: “\(cmd)”"
      case .hasNotRun(let cmd): return "The command has not yet been run: “\(cmd)”"
      case .osError(let errno, let message): return "\(message): [\(errno.rawValue): \(errno)]"
      case .pipeClosed: return "Attempt to read or write from closed pipe."
      }
    }
  }
}

extension Array where Element == UInt8 {
  func trimmedString() -> String {
    let whitespace = " \t\r\n"
    var s = String(decoding: self, as: UTF8.self)
    s = String(s.drop { whitespace.contains($0) })
    s = String(s.reversed().drop { whitespace.contains($0) }.reversed())
    return s
  }
}
