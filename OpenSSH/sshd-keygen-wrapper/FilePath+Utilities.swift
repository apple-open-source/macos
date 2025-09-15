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

  func mkdir(mode: FilePermissions = [.ownerReadWriteExecute]) throws {
    let rc = Darwin.mkdir(self.string, mode.rawValue)
    guard rc == 0 || Darwin.errno == EEXIST else {
      throw Error.pathError(self, Errno(rawValue: Darwin.errno), "creating directory")
    }
  }
  
  func unlink() throws {
    let rc = Darwin.unlink(self.string)
    guard rc == 0 || Darwin.errno == ENOENT else {
      throw Error.pathError(self, Errno(rawValue: Darwin.errno), "unlinking")
    }
  }

  func slurp() throws -> String {
    return try String(decoding: self.slurpBytes(), as: UTF8.self)
  }
  
  func slurpBytes() throws -> [UInt8] {
    var fd: FileDescriptor
    do {
      fd = try FileDescriptor.open(self, .readOnly)
    } catch let error as Errno {
      if error == Errno.noSuchFileOrDirectory {
        return []
      }
      throw Error.pathError(self, error, "opening for reading")
    }
    defer { _ = try? fd.close() }
    do {
      return try fd.slurpBytes()
    } catch let error as Errno {
      throw Error.pathError(self, error, "while reading from")
    }
  }

  func spew(_ text: String) throws {
    try spew(bytes: text.utf8)
  }
  
  func spew(bytes: some Sequence<UInt8>) throws {
    var fd: FileDescriptor
    do {
      fd = try FileDescriptor.open(
        self, .writeOnly, options: [.create, .truncate], permissions: [.ownerReadWrite])
    } catch let error as Errno {
      throw Error.pathError(self, error, "opening for writing")
    }
    defer { _ = try? fd.close() }
    do {
      try fd.writeAll(bytes)
    } catch let error as Errno {
      throw Error.pathError(self, error, "while writing to")
    }
  }
  
  func contentsEqual(_ other: FilePath) -> Bool {
    var stLhs = stat()
    var stRhs = stat()
    if
      0 == stat(self.string, &stLhs),
      0 == stat(other.string, &stRhs),
      stLhs.st_mtimespec.tv_sec == stRhs.st_mtimespec.tv_sec,
      stLhs.st_mtimespec.tv_nsec == stRhs.st_mtimespec.tv_nsec,
      let contentLhs = try? self.slurp(),
      let contentRhs = try? other.slurp()
    {
      return contentLhs == contentRhs
    }
    return false
  }

  enum Error: Swift.Error, CustomStringConvertible {
    case pathError(FilePath, Errno, String)

    var description: String {
      return
        switch self
      {
      case .pathError(let path, let errno, let action):
        "Error \(action) “\(path)”: [\(errno.rawValue): \(errno)]"
      }
    }
  }
}
