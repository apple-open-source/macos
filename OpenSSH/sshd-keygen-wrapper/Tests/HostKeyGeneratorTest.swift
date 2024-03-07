import System
import XCTest

final class HostKeyGeneratorTest: XCTestCase {
  var tmpdir: FilePath {
    FilePath(NSTemporaryDirectory()).appending("HostKeyGeneratorTest")
  }

  var pathPrefix: FilePath {
#if os(macOS)
  FilePath("/usr")
#else
  FilePath("/usr/local")
#endif
  }

  override func setUp() {
    _ = tmpdir.withPlatformString {
      mkdir($0, 0o0700)
    }
  }

  override func tearDown() {
    let fm = FileManager.default
    do {
      try fm.removeItem(atPath: tmpdir.string)
    } catch {
      preconditionFailure("could not remove temporary directory \(tmpdir)")
    }
  }

  func testHostKeyGeneration() async throws {
    var created: Bool
    let keygen = pathPrefix.appending("bin/ssh-keygen")
    let generator = HostKeyGenerator(keygen: keygen, directory: tmpdir)
    XCTAssertFalse(tmpdir.appending("ssh_host_ecdsa_key").exists())
    created = try await generator.generate(algorithm: .ecdsa)
    XCTAssertTrue(created)
    XCTAssertTrue(tmpdir.appending("ssh_host_ecdsa_key").exists())
    XCTAssertFalse(tmpdir.appending("ssh_host_ed25519_key").exists())
    created = try await generator.generate(algorithm: .ed25519)
    XCTAssertTrue(created)
    XCTAssertTrue(tmpdir.appending("ssh_host_ed25519_key").exists())
    created = try await generator.generate(algorithm: .ecdsa)
    XCTAssertFalse(created)
    XCTAssertTrue(tmpdir.appending("ssh_host_ecdsa_key").exists())
  }

  func testHostKeyGenerationFailure1() async throws {
    let keygen = FilePath("/usr/bin/ssh-keygen-nonexistent")
    let generator = HostKeyGenerator(keygen: keygen, directory: tmpdir)
    do {
      _ = try await generator.generate(algorithm: .ecdsa)
      XCTFail("expected an error to be thrown")
      return
    } catch {
      XCTAssertEqual(
        "\(error)",
        "posix_spawn: “/usr/bin/ssh-keygen-nonexistent -q -t ecdsa -f" +
        " \(tmpdir)/ssh_host_ecdsa_key -N '' -C ''”: [2: No such file or directory]")
    }
  }
  func testHostKeyGenerationFailure2() async throws {
    let keygen = pathPrefix.appending("bin/ssh-keygen")
    let generator = HostKeyGenerator(keygen: keygen, directory: FilePath("/no/such/directory"))
    do {
      _ = try await generator.generate(algorithm: .ecdsa)
      XCTFail("expected an error to be thrown")
      return
    } catch {
      XCTAssertEqual(
        "\(error)",
        "\(keygen) -q -t ecdsa -f /no/such/directory/ssh_host_ecdsa_key -N '' -C '':" +
        " exited with status 1: Saving key \"/no/such/directory/ssh_host_ecdsa_key\" failed:" +
        " No such file or directory")
    }
  }
}
