//
// trustdHelper.swift
//

import ArgumentParserInternal
import Foundation

@main
public struct TrustdHelper: AsyncParsableCommand {
    public static var configuration = CommandConfiguration(
        commandName: "trustdHelper",
        abstract: "Tool for modifying files used by trustd",
        subcommands: [
            trustdReset.self,
        ]
    )
    public init() { }
}
