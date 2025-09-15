import IOKit
import System
import DarwinPrivate.os.variant

protocol SystemPropertiesStrategy {
  var pathPrefix: FilePath { get }
  var prebootVolumePrefix: FilePath { get }
  var sshDirectory: FilePath { get }
  var temporaryDirectory: FilePath { get }
  var isBaseSystem: Bool { get }
  var volumeGroupUUID: String { get throws }
}

extension SystemPropertiesStrategy {
  var volumeGroupUUID: String {
    get throws {
      let bootuuidBytes = try sysctl(name: "kern.bootuuid")
      guard let bootuuid = String(bytes: bootuuidBytes, encoding: .utf8) else {
        throw SystemPropertiesError.invalidBootUUID(bootuuidBytes)
      }
      return bootuuid
    }
  }
}

final class SystemProperties: SystemPropertiesStrategy {
  var pathPrefix = FilePath("/usr")
  var sshDirectory = FilePath("/etc/ssh")
  var temporaryDirectory = FilePath("/tmp")
  var isBaseSystem: Bool

  var prebootVolumePrefix: FilePath {
    FilePath("/System/Volumes/Preboot")
  }

  init() {
    self.isBaseSystem = os_variant_is_basesystem("com.apple.sshd-keygen-wrapper")
  }
}

private func cString<T>(_ member: T) -> String {
  withUnsafePointer(to: member) {
    $0.withMemoryRebound(to: UInt8.self, capacity: MemoryLayout<T>.size) {
      return String(cString: $0)
    }
  }
}

enum SystemPropertiesError: Swift.Error, CustomStringConvertible {
  case invalidBootUUID([UInt8])
  case sysctlFailed(Errno, String)

  var description: String {
    return
      switch self
    {
      case .invalidBootUUID(let bytes):
        "Invalid boot uuid: \(bytes)"
      case .sysctlFailed(let errno, let name):
        "sysctlbyname “\(name)”: [\(errno.rawValue): \(errno)]"
    }
  }
}

fileprivate func sysctl(name: String) throws(SystemPropertiesError) -> [UInt8] {
  var size: Int = 0
  // sysctlbyname returns:
  //  0 on success, or an error code that indicates a problem occurred. Possible error codes include EFAULT, EINVAL, ENOMEM, ENOTDIR, EISDIR, ENOENT, and EPERM.
  var rc = sysctlbyname(name, nil, &size, nil, 0)
  guard rc == 0 else {
    throw SystemPropertiesError.sysctlFailed(Errno(rawValue: rc), name)
  }

  var buf = [UInt8](repeating: 0, count: size)
  rc = sysctlbyname(name, &buf, &size, nil, 0)
  guard rc == 0 else {
    throw SystemPropertiesError.sysctlFailed(Errno(rawValue: rc), name)
  }
  return buf
}
