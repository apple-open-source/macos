import Darwin

extension String {
  static let onlySafeShellCharacters = #/[-@%_+:,./A-Za-z0-9]*/#

  /// Return a quoted version of the string for copy-and-paste
  /// to a shell.
  var shellQuoted: String {
    if self == "" {
      return "''"
    }
    if let _ = try? Self.onlySafeShellCharacters.wholeMatch(in: self) {
      return self
    }
    var quoted = "'"
    for ch in self {
      switch ch {
      case "'":
        quoted.append("'\\''")
      default:
        quoted.append(ch)
      }
    }
    quoted.append("'")
    return quoted
  }

  /// Create a new string with the value initialized from
  /// an environmental variable.
  init?(fromEnvironmentVariable name: String) {
    guard let envp = getenv(name) else {
      return nil
    }
    self.init(cString: envp)
  }
}
