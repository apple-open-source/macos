import Foundation
import System
import XCTest

final class SSHDWrapperTest: XCTestCase {
  static func temporaryDirectory() -> FilePath {
    FilePath(NSTemporaryDirectory()).appending("SSHDWarpperTest")
  }

  var recorder = SubprocessFlightRecorder()

  var tmpdir: FilePath {
    Self.temporaryDirectory()
  }

  override func setUp() {
    _ = try? FileManager.default.removeItem(atPath: tmpdir.string)
    _ = tmpdir.withPlatformString {
      mkdir($0, 0o0700)
    }
    recorder.clear()
  }

  override func tearDown() {
    _ = try? FileManager.default.removeItem(atPath: tmpdir.string)
  }

  func testSystemProperties() {
    let sp = SystemProperties()
    XCTAssertEqual(sp.pathPrefix.string, "/usr")
    XCTAssertEqual(sp.sshDirectory.string, "/etc/ssh")
  }

  func testRunNoArguments() async throws {
    let sp = SystemPropertiesMock()
    let w = SSHDWrapper(
      hostKeyPlanTransformer: recorder.ensureSuccess,
      sshdPlanTransformer: recorder.ensureSuccess,
      systemProperties: sp
    )
    try await w.run([])
    guard let lastCommand = recorder.commands.last else {
      XCTFail("no commands run")
      return
    }
    recorder.clear()
    let expected = "\(sp.pathPrefix)/sbin/sshd -i"
    XCTAssertEqual(lastCommand, expected)

    try await w.run(["sshd-wrapper"])
    guard let lastCommand = recorder.commands.last else {
      XCTFail("no commands run")
      return
    }
    XCTAssertEqual(lastCommand, expected)
  }

  func testRunSshdFailure() async throws {
    let sp = SystemPropertiesMock()
    let w = SSHDWrapper(
      hostKeyPlanTransformer: recorder.ensureSuccess,
      sshdPlanTransformer: recorder.injectFailure,
      systemProperties: sp)
    do {
      try await w.run([])
      XCTFail("expected error")
    } catch {
      XCTAssertEqual("\(error)", "/usr/bin/false /usr/sbin/sshd -i: exited with status 1: unknown error")
    }
  }

  func testRunUnexpectedArgument() async throws {
    let sp = SystemPropertiesMock()
    let w = SSHDWrapper(
      hostKeyPlanTransformer: recorder.ensureSuccess,
      sshdPlanTransformer: recorder.ensureSuccess,
      systemProperties: sp)
    do {
      _ = try await w.run(["sshd-wrapper", "-x"])
      XCTFail("Error expected")
    } catch {
      XCTAssertEqual("\(error)", "Unexpected argument: “-x”.")
    }
  }


  func testHostKeyGeneration() async throws {
    let sp = SystemPropertiesMock()
    let w = SSHDWrapper(
      hostKeyPlanTransformer: recorder.ensureSuccess,
      sshdPlanTransformer: recorder.ensureSuccess,
      systemProperties: sp)
    try await w.run(["sshd-wrapper"])
    XCTAssertEqual(recorder.commands.count, HostKeyGenerator.Algorithm.allCases.count + 1)
    guard recorder.commands.count == 3 else {
      return
    }
    for keyType in ["dsa", "ecda", "ed25519", "rsa"] {
      let found = recorder.commands.first { $0.contains(keyType) }
      XCTAssertNotNil(found)
    }
    XCTAssertTrue(recorder.commands[2].contains("sshd"))
  }

  func testHostKeyGenerationFailure1() async throws {
    let sp = SystemPropertiesMock()
    let w = SSHDWrapper(
      hostKeyPlanTransformer: recorder.injectFailure,
      sshdPlanTransformer: recorder.ensureSuccess,
      systemProperties: sp)
    try await w.run(["sshd-wrapper"])
    guard let lastCommand = recorder.commands.last else {
      XCTFail("no commands run")
      return
    }
    XCTAssertEqual(lastCommand, "\(sp.pathPrefix)/sbin/sshd -i")
  }

  func testHostKeyGenerationFailure2() async throws {
    let sp = SystemPropertiesMock()
    let w = SSHDWrapper(
      hostKeyPlanTransformer: { plan in
        var newPlan = plan
        newPlan.flags.removeAll { $0 == .setExec }
        newPlan.arguments.insert(plan.path.string, at: 0)
        newPlan.path = FilePath("/no/such/file")
        return newPlan
      },
      sshdPlanTransformer: recorder.ensureSuccess,
      systemProperties: sp)
    try await w.run(["sshd-wrapper"])
    guard let lastCommand = recorder.commands.last else {
      XCTFail("no commands run")
      return
    }
    XCTAssertEqual(lastCommand, "\(sp.pathPrefix)/sbin/sshd -i")
  }

  struct SystemPropertiesMock: SystemPropertiesStrategy {
    let pathPrefix = FilePath("/usr")
    let sshDirectory: FilePath

    init(sshDirectory: FilePath = SSHDWrapperTest.temporaryDirectory()) {
      self.sshDirectory = sshDirectory
    }
  }

  class SubprocessFlightRecorder {
    var commands: [String] = []

    func ensureSuccess(plan: Subprocess.Plan) -> Subprocess.Plan {
      commands.append(plan.command)
      var newPlan = plan
      newPlan.flags.removeAll { $0 == .setExec }
      newPlan.arguments.insert(plan.path.string, at: 0)
      newPlan.path = FilePath("/usr/bin/true")
      return newPlan
    }

    func injectFailure(plan: Subprocess.Plan) -> Subprocess.Plan {
      commands.append(plan.command)
      var newPlan = plan
      newPlan.flags.removeAll { $0 == .setExec }
      newPlan.arguments.insert(plan.path.string, at: 0)
      newPlan.path = FilePath("/usr/bin/false")
      return newPlan
    }

    func clear() {
      commands.removeAll()
    }
  }
}
