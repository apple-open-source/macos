import os

let logger = Logger(subsystem: "com.apple.sshd-keygen-wrapper", category: "default")

@main
struct Main {
  static func main() async throws {
    do {
      try await SSHDWrapper().run()
    } catch let error as SSHDWrapper.Error {
      print("Error: \(error)")
      exit(EX_SOFTWARE)
    } catch let error {
      logger.error("Unexpected error: \(error)")
      exit(EX_SOFTWARE)
    }
  }
}
