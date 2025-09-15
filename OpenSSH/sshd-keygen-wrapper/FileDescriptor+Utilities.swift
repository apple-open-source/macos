import Darwin
import System

extension FileDescriptor {
  public func slurp() throws -> String {
    return try String(decoding: self.slurpBytes(), as: UTF8.self)
  }
  
  public func slurpBytes() throws -> [UInt8] {
    var data = [UInt8]()
    var buf = [UInt8](repeating: 0, count: 16384)
    var n = 0
    repeat {
      try buf.withUnsafeMutableBytes {
        n = try read(into: $0)
      }
      guard n > 0 else {
        break
      }
      data.append(contentsOf: buf[0..<n])
    } while n > 0
    return data
  }
}
