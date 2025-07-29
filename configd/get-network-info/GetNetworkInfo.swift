/*
 * Copyright (c) 2024-2025 Apple Inc. All rights reserved.
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
import System
import ArgumentParserInternal

@main
struct GetNetworkInfo: ParsableCommand {
	static var configuration: CommandConfiguration {
		CommandConfiguration(
			commandName: "get-network-info",
			abstract: "log collection tool for Networking information",
			subcommands: [

			]
		)
	}

	// cmd line args
	@Flag(name: .customShort(Character("c")), help: "collects system configuration files")
	var collectSystemConfigurationFiles: Bool = false

	@Flag(name: .customShort(Character("n")), help: "collects NDF information (sysctl, lsof)")
	var collectNDFInformation: Bool = false

	@Flag(name: .customShort(Character("s")), help: "collects sensitive information (ARP/NDP/mDNS cache, pcap)")
	var collectSensitiveInformation: Bool = false

	@Flag(name: .customShort(Character("P")), help: "doesn't collect a packet capture")
	var disablePacketCapture: Bool = false

	@Argument(help: "path to directory where all the information will be collected")
	var infoDirectory: String

	mutating func run() throws {
		let gniCollector: GNICollector = GNICollector(self.infoDirectory, self.collectSystemConfigurationFiles,
							      self.collectNDFInformation, self.collectSensitiveInformation,
							      self.disablePacketCapture)

		// internal workaround in case running the old script is necessary
		let runNewGetNetworkInfo: Bool = !UserDefaults.standard.bool(forKey: "get-network-info.use-old-gni")
		guard runNewGetNetworkInfo else {
			let executablePath: String
#if os(macOS)
			executablePath = "/System/Library/Frameworks/SystemConfiguration.framework/Versions/A/Resources/deprecated-get-network-info"
#elseif os(watchOS)
			executablePath = "/System/Library/PrivateFrameworks/SystemConfiguration.framework/deprecated-get-network-info"
#else
			executablePath = "/System/Library/Frameworks/SystemConfiguration.framework/deprecated-get-network-info"
#endif
			let cmd = executablePath.appending(" \(CommandLine.arguments[1...].joined(separator: " "))")
			let success = gniCollector.gnisr.runClassic(command: cmd)
			if !success {
				print("ERROR: couldn't run script \(executablePath)")
			}
			Foundation.exit(success ? 0 : 1)
		}

		// new get-network-info path
		gniCollector.collectAll()

	}

}
