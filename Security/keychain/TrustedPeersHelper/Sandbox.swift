/*
 * Copyright (c) 2024 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

import Foundation
import os.log
import SandboxPrivate

private let logger = Logger(subsystem: "com.apple.security.trustedpeers", category: "container")

// MARK: Sandbox

struct Sandbox {
    #if os(macOS)
    static var sandboxParameters: [String: String] {
        let homeDirectory: String

        if let home = NSHomeDirectory().realpath {
            homeDirectory = home
        } else {
            logger.info("User does not have a home directory! -- Falling back to /private/var/empty")
            homeDirectory = "/private/var/empty"
        }

        guard let tempDirectory = Self._confstr(_CS_DARWIN_USER_TEMP_DIR)?.realpath else {
            fatalError("Unable to read _CS_DARWIN_USER_TEMP_DIR!")
        }

        guard let cacheDirectory = Self._confstr(_CS_DARWIN_USER_CACHE_DIR)?.realpath else {
            fatalError("Unable to read _CS_DARWIN_USER_CACHE_DIR!")
        }

        return [
            "_DARWIN_USER_CACHE": cacheDirectory,
            "_DARWIN_USER_TEMP": tempDirectory,
            "_HOME": homeDirectory,
        ]
    }

    static func enterSandbox(identifier: String, profile: String) {
        guard _set_user_dir_suffix(identifier) else {
            fatalError("_set_user_dir_suffix() failed!")
        }

        _sandboxInit(profile: profile, parameters: sandboxParameters)
    }
    #else
    static func enterSandbox(identifier: String) {
        guard _set_user_dir_suffix(identifier) else {
            fatalError("_set_user_dir_suffix() failed!")
        }

        guard (Self._confstr(_CS_DARWIN_USER_TEMP_DIR)) != nil else {
            fatalError("Unable to read _CS_DARWIN_USER_TEMP_DIR!")
        }
    }
    #endif

    #if os(macOS)
    private static func _flatten(_ dictionary: [String: String]) -> [String] {
        var result = [String]()

        dictionary.keys.forEach { key in
            guard let value = dictionary[key] else {
                return
            }

            result.append(key)
            result.append(value)
        }

        return result
    }

    private static func _sandboxInit(profile: String, parameters: [String: String]) {
        var sbError: UnsafeMutablePointer<Int8>?
        let flatParameters = _flatten(parameters)
        logger.debug("Sandbox parameters: \(String(describing: parameters))")

        withArrayOfCStrings(flatParameters) { ptr -> Void in
            let result = sandbox_init_with_parameters(profile, UInt64(SANDBOX_NAMED), ptr, &sbError)
            guard result == 0 else {
                guard let sbError = sbError else {
                    fatalError("sandbox_init_with_parameters failed! (no error)")
                }

                fatalError("sandbox_init_with_parameters failed!: [\(String(cString: sbError))]")
            }
        }

        _ = sbError
    }
    #endif

    private static func _confstr(_ name: Int32) -> String? {
        var directory = Data(repeating: 0, count: Int(PATH_MAX))

        return directory.withUnsafeMutableBytes { body -> String? in
            guard let ptr = body.bindMemory(to: Int8.self).baseAddress else {
                logger.error("failed to bind memory")
                return nil
            }
            errno = 0
            let status = confstr(name, ptr, Int(PATH_MAX))

            guard status > 0 else {
                logger.error("confstr \(name) failed: \(errno)")
                return nil
            }
            return String(cString: ptr)
        }
    }
}

// For calling C functions with arguments like: `const char *const parameters[]`
private func withArrayOfCStrings<R>(_ args: [String], _ body: ([UnsafePointer<CChar>?]) -> R) -> R {
    let mutableStrings = args.map { strdup($0) }
    var cStrings = mutableStrings.map { UnsafePointer($0) }

    defer { mutableStrings.forEach { free($0) } }

    cStrings.append(nil)

    return body(cStrings)
}

private extension String {
    var realpath: String? {
        let retValue: String?

        guard let real = Darwin.realpath(self, nil) else {
            return nil
        }

        retValue = String(cString: real)

        real.deallocate()

        return retValue
    }
}
