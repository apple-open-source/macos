//
//  trustdReset.swift
//  Security
//
//

import ArgumentParserInternal
import Foundation
import OSLog

public struct trustdReset: ParsableCommand {
    public static var configuration = CommandConfiguration(
        commandName: "reset",
        abstract: "Delete files to reset trustd state",
        subcommands: [
            resetPublic.self,
            resetPrivate.self,
        ]
    )

    public init() { }
}

public struct resetPublic: ParsableCommand {
    public static var configuration = CommandConfiguration(
        commandName: "public",
        abstract: "Reset public trustd files")

    public func run() throws {
        guard os_variant_allows_internal_security_policies("com.apple.security") else {
            print("Cannot reset. Not an internal build.")
            Foundation.exit(1)
        }
        Logger().notice("Deleting /var/protected/trustd/")
        /* change permissions so we can delete the directory */
        if chmod("/private/var/protected/trustd", S_IRWXU | S_IRWXG | S_IRWXO) != 0 {
            let errStr = strerror(errno)!
            print("Failed to change directory permissions: \(errStr)")
            Foundation.exit(1)
        }
        defer {
            /* change permissions back */
            if chmod("/private/var/protected/trustd", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0 {
                let errStr = strerror(errno)!
                print("Failed to change directory permissions back: \(errStr)")
                Foundation.exit(1)
            }
        }
        /* delete the directory contents */
        do {
            try FileManager().removeItem(atPath: "/private/var/protected/trustd/SupplementalsAssets")
            try FileManager().removeItem(atPath: "/private/var/protected/trustd/valid.sqlite3")
            try FileManager().removeItem(atPath: "/private/var/protected/trustd/valid.sqlite3-shm")
            try FileManager().removeItem(atPath: "/private/var/protected/trustd/valid.sqlite3-wal")
            try FileManager().removeItem(atPath: "/private/var/protected/trustd/pinningrules.sqlite3")
        } catch {
            print("Failed to delete directory: \(error)")
            Foundation.exit(1)
        }
    }
    public init() { }
}

public struct resetPrivate: ParsableCommand {
    public static var configuration = CommandConfiguration(
        commandName: "private",
        abstract: "Reset private trustd files")

    public func run() throws {
        guard os_variant_allows_internal_security_policies("com.apple.security") else {
            print("Cannot reset. Not an internal build.")
            Foundation.exit(1)
        }
        Logger().notice("Deleting /var/protected/trustd/private")
        /* change permissions so we can delete the directory */
        if chmod("/private/var/protected/trustd", S_IRWXU | S_IRWXG | S_IRWXO) != 0 {
            let errStr = strerror(errno)!
            print("Failed to change directory permissions: \(errStr)")
            Foundation.exit(1)
        }

        defer {
            /* change permissions back */
            if chmod("/private/var/protected/trustd", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0 {
                let errStr = strerror(errno)!
                print("Failed to change directory permissions back: \(errStr)")
                Foundation.exit(1)
            }
        }

        /* delete the directory */
        do {
            try FileManager().removeItem(atPath: "/private/var/protected/trustd/private/")
        } catch {
            print("Failed to delete directory: \(error)")
            Foundation.exit(1)
        }
    }
    public init() { }
}
