import System

protocol SystemPropertiesStrategy {
  var pathPrefix: FilePath { get }
  var sshDirectory: FilePath { get }
}

final class SystemProperties: SystemPropertiesStrategy {
  var pathPrefix = FilePath("/usr")
  var sshDirectory = FilePath("/etc/ssh")
}
