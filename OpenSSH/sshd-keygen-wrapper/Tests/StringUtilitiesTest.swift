import XCTest

final class StringUtilitiesTest: XCTestCase {
  func testEnvironmentVariableNotPresent() throws {
    XCTAssertNil(
      String(fromEnvironmentVariable: "No such variable"),
      "Expected nil for invalid environment variable name.")
  }

  func testEnvironmentalVariablePresent() throws {
    setenv("SSHDWRAPPERTESTVAR", "forty-two", 1)
    addTeardownBlock {
      unsetenv("SSHDWRAPPERTESTVAR")
    }
    XCTAssertEqual(String(fromEnvironmentVariable: "SSHDWRAPPERTESTVAR"), "forty-two")
  }

  func testQuoteNothingSpecial() throws {
    let s = "No-unsafe_characters@here,dudes+%:./42"
    XCTAssertEqual(s, s.shellQuoted)
  }

  func testQuoteSpaces() throws {
    let s = "This has spaces in it."
    XCTAssertEqual(s.shellQuoted, "'\(s)'")
  }

  func testQuoteSpecial() throws {
    let s = "This has a dollar sign in it: $HOME."
    XCTAssertEqual(s.shellQuoted, "'\(s)'")
  }

  func testEmbeddedQuotes() throws {
    let s = "This contains 'single quotes' as well as \"double quotes\"."
    XCTAssertEqual(
      s.shellQuoted, "'This contains '\\''single quotes'\\'' as well as \"double quotes\".'")
  }

  func testQuoteEmptyString() throws {
    let s = ""
    XCTAssertEqual(s.shellQuoted, "''")
  }
}
