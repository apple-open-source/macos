import Darwin
import System

extension FilePath {
  /// Check for the existance of a file at this path.
  /// - Returns: true if the path exists and it is a regular file,
  ///   else false.
  func exists() -> Bool {
    var st = stat()
    let rc = stat(self.string, &st)
    let e = Errno(rawValue: Darwin.errno)
    if rc == 0 && (st.st_mode & S_IFMT) == S_IFREG {
      return true
    } else if rc == 0 {
      let mode = String(st.st_mode, radix: 16, uppercase: false)
      logger.error("\(self): Not a regular file, mode=\(mode)")
    } else if e != .noSuchFileOrDirectory {
      logger.error("\(self): [\(e.rawValue): \(e)]")
    }
    return false
  }
}
