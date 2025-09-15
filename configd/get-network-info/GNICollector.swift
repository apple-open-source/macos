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
import os
import RegexBuilder

final class GNICollector {

	private var gniDirectory: FilePath
	public var gnisr: GNISubprocessRunner
	private let interfaceList: [String]
	private let _collectSystemConfigurationFiles: Bool
	private let _collectNDFInformation: Bool
	private let _collectSensitiveInformation: Bool
	private let _disablePacketCapture: Bool
	private var pcapRunning: Bool = false

	init(_ outputDirectory: String, _ collectSystemConfigurationFiles: Bool, _ collectNDFInformation: Bool,
	     _ collectSensitiveInformation: Bool, _ disablePacketCapture: Bool) {
		self.gniDirectory = FilePath(outputDirectory)
		guard let gnisr = GNISubprocessRunner(pathRoot: self.gniDirectory) else {
			print("ERROR: couldn't create instance of GNISubprocessRunner")
			Foundation.exit(1)
		}
		self.gnisr = gnisr
		// saves a list of all network interfaces in the system used by collection functions below
		let (_, iflistStr) = gnisr.run(command: "/sbin/ifconfig -l")
		guard let iflistStr = iflistStr else {
			gnisr.errorlog("couldn't get interface list")
			Foundation.exit(1)
		}
		self.interfaceList = iflistStr.components(separatedBy: .whitespacesAndNewlines).filter { !$0.isEmpty }
		self._collectSystemConfigurationFiles = collectSystemConfigurationFiles
		self._collectNDFInformation = collectNDFInformation
		self._collectSensitiveInformation = collectSensitiveInformation
		self._disablePacketCapture = disablePacketCapture
	}

	func collectInformation(_ commandTable: [(String, String?, Bool)], _ list: [String] = []) -> Void {
		var inputList: [String] = []
		if list.isEmpty {
			// run over iflist produced by ifconfig if optional
			inputList = self.interfaceList
		} else {
			inputList = list
		}
		for (cmd, stdout, runOverInputList) in commandTable {
			if runOverInputList {
				for input in inputList {
					let (success, _) = gnisr.run(command: cmd.appendingFormat(" %@", input), stdout: stdout)
					if !success {
						gnisr.errorlog("FAILED: '\(cmd)'")
					}
				}
			} else {
				let (success, _) = gnisr.run(command: cmd, stdout: stdout)
				if !success {
					gnisr.errorlog("FAILED: '\(cmd)'")
				}
			}
		}
	}

	func copyInformation(_ copyTable: [(String, String?)]) -> Void {
		for (src, dst) in copyTable {
			guard FileManager.default.fileExists(atPath: src) else {
				gnisr.log(String(format:"skipping '%@' because it doesn't exist", src), at: .info)
				continue
			}
			let newDst: String
			if dst == nil {
				newDst = gniDirectory.appending(FilePath(src).lastComponent!).description
			} else {
				newDst = gniDirectory.appending(dst!).description
			}
			do {
				var isSymlink: Bool = false
				if let attrs = try? FileManager.default.attributesOfItem(atPath: src) {
					isSymlink = (attrs[.type] as? FileAttributeType == .typeSymbolicLink)
				}
				if isSymlink {
					// manages slight redirection through filesystem
					if let newSrc = try? FileManager.default.destinationOfSymbolicLink(atPath: src) {
						let relativeSrcURL = URL(fileURLWithPath: newSrc, relativeTo: URL(fileURLWithPath: src))
						let newDstURL = URL(fileURLWithPath: newDst)
						try FileManager.default.copyItem(at: relativeSrcURL, to: newDstURL)
					}
				} else {
					try FileManager.default.copyItem(atPath: src, toPath: newDst)
				}
			} catch {
				gnisr.errorlog("FAILED: 'cp \(src) \(newDst)' with error '\(error)'")
			}
		}
	}

	func collectDefaultInformation() -> Void {
		collectInformation(runTableForDefaultCommands)

		// ("/usr/sbin/netstat -qq -I %@",				"netstat.txt",				true)
		// ("/usr/sbin/netstat -Q -I %@",				"netstat.txt",				true)
		self.interfaceList.forEach { ifname in
			let cmd = String(format: "/sbin/ifconfig -v %@", ifname)
			let (_, ifconfigVerboseOutput) = gnisr.run(command: cmd)
			guard let ifconfigVerboseOutput = ifconfigVerboseOutput else {
				gnisr.errorlog("FAILED: '\(cmd)'")
				return
			}
			if ifconfigVerboseOutput.contains("TXSTART") {
				collectInformation([(String(format: "/usr/sbin/netstat -qq -I %@", ifname), "netstat.txt", false),])
			}
			if ifconfigVerboseOutput.contains("RXPOLL") {
				collectInformation([(String(format: "/usr/sbin/netstat -Q -I %@", ifname), "netstat.txt", false),])
			}
		}

		// ("/usr/sbin/ipconfig getsummary %@",				"ipconfig-info.txt",			true)
		let (_, ipconfigIflistStr) = gnisr.run(command: "/usr/sbin/ipconfig getiflist")
		if ipconfigIflistStr != nil {
			let ipconfigInterfaceList: [String] = ipconfigIflistStr!.components(separatedBy: .whitespacesAndNewlines).filter {
				!$0.isEmpty
			}
			collectInformation([("/usr/sbin/ipconfig getsummary", "ipconfig-info.txt", true),], ipconfigInterfaceList)
		} else {
			gnisr.errorlog("couldn't get interface list from ipconfig")
		}

		// ("pfctl -s all -a %@",					"pf.txt",				true)
		let (_, pfAnchorsListStr) = gnisr.run(command: "/sbin/pfctl -s Anchors -v", stderr: "/dev/null")
		if pfAnchorsListStr != nil {
			let pfAnchorsList: [String] = pfAnchorsListStr!.components(separatedBy: .whitespacesAndNewlines).filter {
				!$0.isEmpty
			}
			collectInformation([("/sbin/pfctl -s all -a", "pf.txt", true),], pfAnchorsList)
		} else {
			gnisr.errorlog("couldn't get packet filter anchor list")
		}

		// ("/usr/sbin/setkey -D | <redaction_script>",			"ipsec.txt",				false)
		let stdinFilename = "tmp-redaction-stdin.txt"
		let (success, _) = gnisr.run(command: "/usr/sbin/setkey -D", stdout: stdinFilename)
		if success {
			let stdinPath = gniDirectory.appending(stdinFilename).description
			if let stdinForRedaction = try? String(contentsOfFile: stdinPath, encoding: .utf8) {
				let outputFilePath = gniDirectory.appending("ipsec.txt").description
				if let outputHandle = FileHandle(forUpdatingAtPath: outputFilePath) {
					outputHandle.seekToEndOfFile()
					let re = Regex {
						Capture { /\s+[AE]:\s+\S+\s+/ }
						/"?(.*)"?/
						Capture { /\s*/ }
					}
					// Example redaction: "A: hmac-sha1 6b3cac5f de6fe506 ..." to "A: hmac-sha1 [redacted] ..."
					let printLines = stdinForRedaction.components(separatedBy: .newlines).map { line -> String in
						if let match = line.wholeMatch(of: re) {
							return "\(match.1)[redacted]\(match.3)"
						} else {
							return line
						}
					}
					try? outputHandle.write(contentsOf: printLines.joined(separator: "\n").data(using: .utf8)!)
					try? outputHandle.close()
				}
			}
			try? FileManager.default.removeItem(atPath: stdinPath)
		}

		// ("cat /var/run/racoon/*.conf | <redaction_script>",			"ipsec.txt",			false)
		try? FileManager.default.contentsOfDirectory(atPath: "/var/run/racoon").forEach { stdinPath in
			guard stdinPath.hasSuffix(".conf") else {
				return
			}
			if let stdinForRedaction = try? String(contentsOfFile: stdinPath, encoding: .utf8) {
				let outputFilePath = gniDirectory.appending("ipsec.txt").description
				if let outputHandle = FileHandle(forUpdatingAtPath: outputFilePath) {
					outputHandle.seekToEndOfFile()
					let cmdHeader = "#\n# cat /var/run/racoon/*.conf\n#\n"
					try? outputHandle.write(contentsOf: cmdHeader.data(using: .utf8)!)
					let re = Regex {
						Capture { /\s+shared_secret\s+use\s+/ }
						/"?([^\s;"]+)"?/
						Capture { /.*/ }
					}
					let printLines = stdinForRedaction.components(separatedBy: .newlines).map { line -> String in
						if let match = line.prefixMatch(of: re) {
							return "\(match.1)[redacted]\(match.3)"
						} else {
							return line
						}
					}
					try? outputHandle.write(contentsOf: printLines.joined(separator: "\n").data(using: .utf8)!)
					try? outputHandle.close()
				}
			}
		}

		// ("cat /var/log/vpnd.log",						"vpnd.txt",			false)
		// ("cat /var/log/racoon.log",						"racoon.txt",			false)
		let vpnInfoCopyTable: [(String, String)] = [("/var/log/vpnd.log", "vpnd.txt"),
							    ("/var/log/racoon.log", "racoon.txt")]
		copyInformation(vpnInfoCopyTable)
	}

	func collectNDFInformation() -> Void {
		guard self._collectNDFInformation else { return }

		collectInformation(runTableForNDFInfo)
	}

	func startPacketCapture() -> Bool {
		if !self._disablePacketCapture {
			let (success, _) = gnisr.run(command: "/usr/local/bin/netdiagnose -p \(gniDirectory) start sysdiagpcap",
						     stdout: "/dev/null")
			return success
		}
		return false
	}

	func stopPacketCapture() -> Void {
		if !self._disablePacketCapture && self.pcapRunning {
			let (_, _) = gnisr.run(command: "/usr/local/bin/netdiagnose stop sysdiagpcap",
					       stdout: "/dev/null")
		}
	}

	func collectSensitiveInformation() -> Void {
		guard self._collectSensitiveInformation else {
			return
		}

		self.pcapRunning = startPacketCapture()
		collectInformation(runTableForSensitiveInfo)
	}

	func collectSystemConfigurationFiles() -> Void {
		guard self._collectSystemConfigurationFiles else { return }

		copyInformation(copyTableForConfigurationFiles)
	}

	func collectExtraSystemConfigurationFiles() -> Void {
		let cmdBase = "/usr/bin/tar -c -H "
		var cmd: String? = nil
		var stdout: String? = nil
		let path1 = "/etc/resolver"
		let path2 = "/Library/Preferences/SystemConfiguration/resolver"
		if FileManager.default.fileExists(atPath: path1) {
			cmd = cmdBase.appending(path1)
			stdout = "etc-resolver.tar"
		} else if FileManager.default.fileExists(atPath: path2) {
			cmd = cmdBase.appending(path2)
			stdout = "LPS-resolver.tar"
		}
		if let cmd = cmd, let stdout = stdout {
			let (success, _) = gnisr.run(command: cmd, stdout: stdout, stderr: "/dev/null")
			if !success {
				gnisr.errorlog("FAILED: \(cmd)")
			}
		}

		var migrate: String = ""
		if let allMigrationDirFiles = try? FileManager.default.contentsOfDirectory(atPath: "/Library/Preferences/SystemConfiguration") {
			(allMigrationDirFiles.filter {
				(FilePath($0).lastComponent!.stem.hasPrefix("preferences-pre-")
				 || FilePath($0).lastComponent!.stem.hasPrefix("NetworkInterfaces-pre-"))
				&& FilePath($0).lastComponent!.extension == "plist"
			}).forEach { filePath in
				migrate.append(filePath.appending(" "))
			}
		}
		if !migrate.isEmpty {
			migrate.append("/Library/Preferences/SystemConfiguration/preferences.plist ")
			migrate.append("/Library/Preferences/SystemConfiguration/NetworkInterfaces.plist")
			let cmd = "/usr/bin/tar -c -H".appendingFormat(" %@", migrate)
			let (success, _) = gnisr.run(command: cmd, stdout: "migration.tar", stderr: "/dev/null")
			if !success {
				gnisr.errorlog("FAILED: \(cmd)")
			}
		}
	}

	func collectAll() {
		collectSensitiveInformation()
		collectDefaultInformation()
		collectNDFInformation()
		collectSystemConfigurationFiles()
		collectExtraSystemConfigurationFiles()
		stopPacketCapture()
	}

}
