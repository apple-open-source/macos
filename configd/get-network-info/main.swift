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




// MARK: - run tables

let runTableForDefaultCommands: [(String, String?, Bool)] = [
	("/usr/sbin/scutil -d -v --nwi",				"network-information.txt",		false),
	("/usr/sbin/scutil -d -v --dns",				"dns-configuration.txt",		false),
	("/usr/sbin/scutil -d -v --proxy",				"proxy-configuration.txt",		false),
	("/usr/sbin/scutil -d -v -r www.apple.com",			"reachability-info.txt",		false),
	("/usr/sbin/scutil -d -v -r 0.0.0.0", 				"reachability-info.txt",		false),
	("/usr/sbin/scutil -d -v -r 169.254.0.0", 			"reachability-info.txt",		false),
	("/usr/sbin/scutil --nc list", 					"nc-info.txt",				false),
	("/usr/sbin/scutil --rank \"\"", 				"interface-rank-assertions.txt",	false),
	("/usr/sbin/scutil --advisory \"\"",				"interface-advisories.txt",		false),
	("/usr/sbin/scutil -p --snapshot /dev/stdout",			"SCDynamicStore.plist",			false),
	("/usr/sbin/scutil --nwi",					"network-information.txt",		true),
	("/usr/bin/dig +time=2 -t any -c any www.apple.com", 		"dig-info.txt",				false),
	("/sbin/ifconfig -a -L -b -m -r -v -v",				"ifconfig.txt",				false),
	("/usr/sbin/netstat -n -r -a -l",				"netstat.txt",				false),
	("/usr/sbin/netstat -A -a -l -n -v -W",				"netstat.txt",				false),
	("/usr/sbin/netstat -s",					"netstat.txt",				false),
	("/usr/sbin/netstat -rs",					"netstat.txt",				false),
	("/usr/sbin/netstat -mmm",					"netstat.txt",				false),
	("/usr/sbin/netstat -i -n -d",					"netstat.txt",				false),
	("/usr/sbin/netstat -i -x -R",					"netstat.txt",				false),
	("/usr/sbin/netstat -anp mptcp",				"netstat.txt",				false),
	("/usr/sbin/netstat -s -p mptcp",				"netstat.txt",				false),
	("/usr/sbin/netstat -L -a -n -v",				"netstat.txt",				false),
	("/usr/sbin/netstat -g -n -s",					"netstat.txt",				false),
	("/usr/sbin/netstat -B",					"netstat.txt",				false),
	("/usr/sbin/netstat -n -I",					"netstat.txt",				true),
	("/usr/sbin/netstat -n -I",					"netstat.txt",				true),
	("/usr/sbin/netstat -n -s -I",					"netstat.txt",				true),
	("/sbin/pfctl -s all",						"pf.txt",				false),
	("/sbin/pfctl -s References",					"pf.txt",				false),
	("/usr/sbin/setkey -Pp -D",					"ipsec.txt",				false),
	("/usr/sbin/skywalkctl status",					"skywalk.txt",				false),
	("/usr/sbin/skywalkctl show",					"skywalk.txt",				false),
	("/usr/sbin/skywalkctl flow -n",				"skywalk.txt",				false),
	("/usr/sbin/skywalkctl flow-route -n",				"skywalk.txt",				false),
	("/usr/sbin/skywalkctl flow-switch -n",				"skywalk.txt",				false),
	("/usr/sbin/skywalkctl flow-owner -n",				"skywalk.txt",				false),
	("/usr/sbin/skywalkctl flow-adv -n",				"skywalk.txt",				false),
	("/usr/sbin/skywalkctl netstat -s",				"skywalk.txt",				false),
	("/usr/sbin/skywalkctl netstat -s --global",			"skywalk.txt",				false),
	("/usr/sbin/skywalkctl interface",				"skywalk.txt",				false),
	("/usr/sbin/skywalkctl channel",				"skywalk.txt",				false),
	("/usr/sbin/skywalkctl provider -D",				"skywalk.txt",				false),
	("/usr/sbin/skywalkctl netns -a",				"skywalk.txt",				false),
	("/usr/sbin/skywalkctl memory",					"skywalk.txt",				false),
	("/usr/local/bin/nsputil privacy-proxy-configuration",		"networkserviceproxy.txt",		false),
	("/usr/local/bin/nsputil privacy-proxy-service-status -status-timeline","networkserviceproxy.txt",	false),
	("/sbin/route -n -v get www.apple.com",				"route-info.txt",			false),
	("/sbin/route -n -v get 0.0.0.0",				"route-info.txt",			false),
	("/bin/hostname",						"hostname.txt",				false),

	// implemented directly in the caller function due to running with contingencies
	// ("/usr/sbin/netstat -qq -I %@",				"netstat.txt",				true),
	// ("/usr/sbin/netstat -Q -I %@",				"netstat.txt",				true),
	// ("/usr/sbin/ipconfig getsummary %@",				"ipconfig-info.txt",			true),
	// ("pfctl -s all -a %@",					"pf.txt",				true),
	// ("/usr/sbin/setkey -D | /usr/bin/perl -l -n -e <script>",	"ipsec.txt",				false),
	// ("cat /var/run/racoon/*.conf | /usr/bin/perl -l -n -e <script>","ipsec.txt",				false),
	// ("cat /var/log/vpnd.log",					"vpnd.txt",				false),
	// ("cat /var/log/racoon.log",					"racoon.txt",				false),

]

let runTableForNDFInfo: [(String, String?, Bool)] = [
	("/usr/sbin/lsof +c 0 -X -n -O -P -T q",		"lsof.txt",			false),
	("/usr/sbin/sysctl -a",					"sysctl.txt",			false),
]

let runTableForSensitiveInfo: [(String, String?, Bool)] = [
	("/usr/bin/dns-sd -O -stdout",				"mDNSResponder_state_dump.txt",	false),
	("/usr/sbin/ndp -n -a",					"ndp-info.txt",			false),
	("/usr/sbin/ndp -n -p",					"ndp-info.txt",			false),
	("/usr/sbin/ndp -n -r",					"ndp-info.txt",			false),
	("/usr/sbin/ndp -i",					"ndp-info.txt",			true),
	("/usr/sbin/arp -n -a",					"arp-info.txt",			false),
	("/usr/local/bin/neutil policy dump",			"necp.txt",			false),
	("/usr/local/bin/neutil agent dump",			"network-agents.txt",		false),
	("/usr/local/bin/neutil session log-file-handles",	nil,				false), // TODO: need sleep(1)?
	("/usr/bin/nettop -n -l 1",				"nettop.txt",			false),
	("/usr/local/bin/network_test path_watcher -dump -sysdiagnose",	"nw_path.txt",		false),
]

let copyTableForConfigurationFiles: [(String, String?)] = [
	("/Library/Preferences/com.apple.networkd.plist",					nil),
	("/Library/Preferences/com.apple.networkd.networknomicon.plist",			nil),
	("/Library/Preferences/com.apple.networkextension.plist",				nil),
	("/Library/Preferences/com.apple.networkextension.control.plist",			nil),
	("/Library/Preferences/com.apple.networkextension.necp.plist",				nil),
	("/Library/Preferences/com.apple.networkextension.cache.plist",				nil),
	("/Library/Preferences/com.apple.networkextension.uuidcache.plist",			nil),
	("/Library/Preferences/SystemConfiguration/com.apple.nat.plist",			nil),
	("/Library/Preferences/SystemConfiguration/com.apple.RemoteAccessServers.plist",	nil),
	("/Library/Preferences/SystemConfiguration/com.apple.smb.server.plist",			nil),
	("/Library/Preferences/SystemConfiguration/NetworkInterfaces.plist",			nil),
	("/Library/Preferences/SystemConfiguration/preferences.plist",				nil),
	("/etc/resolv.conf",									"etc-resolv-conf.txt"),
	("/var/run/resolv.conf",								"var-run-resolv-conf.txt"),
]


// TODO: gate running on non-macos non-internal builds with managed preferences check (from SC)



// MARK: - setup

let usageString = """
Usage: network-info [-c] [-n] [-s] [-P] <info-directory>
 -c		    collects system configuration files
 -n		    collects NDF information (lsof)
 -P		    doesn't collect a packet capture
 -s		    collects sensitive information (ARP/NDP/mDNS cache, pcap)
 <info-directory>   path to directory where all the information will be collected
"""

// parses cmd line args accordingly
var cmdLineArgs = CommandLine.arguments
cmdLineArgs.remove(at: 0) // remove own pathname
guard let gniDirectoryPathString = cmdLineArgs.popLast() else {
	print(usageString)
	exit(1)
}
let gniDirectory: FilePath = FilePath(gniDirectoryPathString)
let enableSystemConfigurationFiles: Bool = cmdLineArgs.contains("-c")
let enableSensitiveInformation: Bool = cmdLineArgs.contains("-s")
let enableNDFInformation: Bool = cmdLineArgs.contains("-n")
let disablePacketCapture: Bool = cmdLineArgs.contains("-P")
var pcapRunning: Bool = false
if enableSystemConfigurationFiles { cmdLineArgs.remove(at: cmdLineArgs.firstIndex(of: "-c")!) }
if enableSensitiveInformation { cmdLineArgs.remove(at: cmdLineArgs.firstIndex(of: "-s")!) }
if enableNDFInformation { cmdLineArgs.remove(at: cmdLineArgs.firstIndex(of: "-n")!) }
if disablePacketCapture { cmdLineArgs.remove(at: cmdLineArgs.firstIndex(of: "-P")!) }
guard cmdLineArgs.isEmpty else {
	print("ERROR: unrecognized argument(s) '\(cmdLineArgs)'")
	print(usageString)
	exit(1)
}

// creates shared instance of subprocess runner
guard let gnisr = GNISubprocessRunner(pathRoot: gniDirectory) else {
	print("ERROR: couldn't create instance of GNISubprocessRunner")
	exit(1)
}

// this provides an internal workaround in case running the old script is necessary
let callerHasNewGNI = !UserDefaults.standard.bool(forKey: "get-network-info.use-old-gni")
guard callerHasNewGNI else {
	let executablePath: String
#if os(macOS)
	executablePath = "/System/Library/Frameworks/SystemConfiguration.framework/Versions/A/Resources/deprecated-get-network-info"
#elseif os(watchOS)
	executablePath = "/System/Library/PrivateFrameworks/SystemConfiguration.framework/deprecated-get-network-info"
#else
	executablePath = "/System/Library/Frameworks/SystemConfiguration.framework/deprecated-get-network-info"
#endif
	let cmd = executablePath.appending(" \(CommandLine.arguments[1...].joined(separator: " "))")
	let success = gnisr.runClassic(command: cmd)
	if !success {
		print("ERROR: couldn't run script \(executablePath)")
	}
	exit(success ? 0 : 1)
}

// saves a list of all network interfaces in the system used by collection functions below
let (_, iflistStr) = gnisr.run(command: "/sbin/ifconfig -l")
guard let iflistStr = iflistStr else {
	gnisr.errorlog("couldn't get interface list")
	exit(1)
}
let interfaceList: [String] = iflistStr.components(separatedBy: .whitespacesAndNewlines).filter { !$0.isEmpty }



// MARK: - log collection functions

func collectInformation(_ commandTable: [(String, String?, Bool)], _ inputList: [String] = interfaceList) -> Void {
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
			try FileManager.default.copyItem(atPath: src, toPath: newDst)
		} catch {
			gnisr.errorlog("FAILED: 'cp \(src) \(newDst)' with error '\(error)'")
		}
	}
}

func collectDefaultInformation() -> Void {
	collectInformation(runTableForDefaultCommands)

	// ("/usr/sbin/netstat -qq -I %@",				"netstat.txt",				true)
	// ("/usr/sbin/netstat -Q -I %@",				"netstat.txt",				true)
	interfaceList.forEach { ifname in
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

	// ("/usr/sbin/setkey -D | /usr/bin/perl -l -n -e <script>",	"ipsec.txt",				false)
	let (success, _) = gnisr.run(command: "/usr/sbin/setkey -D", stdout: "tmp-perl-stdin.txt")
	if success {
		let perlCommand =
		"""
		/usr/bin/perl -l -n -e '
		if (/^(\\s+[AE]:\\s+\\S+\\s+)"?(.*)"?\\s*$/) {
			printf "%s[redacted]%s\\n", $1, $3;
		} else {
			printf "%s\\n", $_;
		}
		'
		"""
		let perlStdinPath = gniDirectory.appending("tmp-perl-stdin.txt").description
		if FileManager.default.fileExists(atPath: perlStdinPath) {
			collectInformation([(perlCommand.appendingFormat(" %@", perlStdinPath), "ipsec.txt", false),])
			try? FileManager.default.removeItem(atPath: perlStdinPath)
		}
	}

	// ("cat /var/run/racoon/*.conf | /usr/bin/perl -l -n -e <script>",	"ipsec.txt",			false)
	let perlCommand =
	"""
	/usr/bin/perl -l -n -e '
	if (/^(\\s+shared_secret\\s+use\\s+)"?([^\\s;"]+)"?(.*)/) {
		printf "%s[redacted]%s\\n", $1, $3;
	} else {
		printf "%s\\n", $_;
	}
	'
	"""
	try? FileManager.default.contentsOfDirectory(atPath: "/var/run/racoon").forEach { perlStdinPath in
		if perlStdinPath.hasSuffix(".conf") {
			collectInformation([(perlCommand.appendingFormat(" %@", perlStdinPath), "ipsec.txt", false),])
		}
	}


	// ("cat /var/log/vpnd.log",						"vpnd.txt",			false)
	// ("cat /var/log/racoon.log",						"racoon.txt",			false)
	let vpnInfoCopyTable: [(String, String)] = [("/var/log/vpnd.log", "vpnd.txt"),
						    ("/var/log/racoon.log", "racoon.txt")]
	copyInformation(vpnInfoCopyTable)
}

func collectNDFInformation() -> Void {
	guard enableNDFInformation else { return }

	collectInformation(runTableForNDFInfo)
}

func startPacketCapture() -> Bool {
	if !disablePacketCapture {
		let (success, _) = gnisr.run(command: "/usr/local/bin/netdiagnose -p \(gniDirectory) start sysdiagpcap",
					     stdout: "/dev/null")
		return success
	}
	return false
}

func stopPacketCapture(_ pcapOngoing: Bool) -> Void {
	if !disablePacketCapture && pcapOngoing {
		let (_, _) = gnisr.run(command: "/usr/local/bin/netdiagnose stop sysdiagpcap",
				       stdout: "/dev/null")
	}
}

func collectSensitiveInformation() -> Bool {
	var pcapRunning = false
	guard enableSensitiveInformation else { return pcapRunning }

	pcapRunning = startPacketCapture()
	collectInformation(runTableForSensitiveInfo)

	return pcapRunning
}

func collectSystemConfigurationFiles() -> Void {
	guard enableSystemConfigurationFiles else { return }

	copyInformation(copyTableForConfigurationFiles)
}

func collectExtraSystemConfigurationFiles() -> Void {
	let cmdBase = "/usr/bin/tar -c -H"
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
		let cmd = "tar -c -H".appendingFormat(" %@", migrate)
		let (success, _) = gnisr.run(command: cmd, stdout: "migration.tar", stderr: "/dev/null")
		if !success {
			gnisr.errorlog("FAILED: \(cmd)")
		}
	}
}



// MARK: - main

pcapRunning = collectSensitiveInformation()
collectDefaultInformation()
collectNDFInformation()
collectSystemConfigurationFiles()
collectExtraSystemConfigurationFiles()
stopPacketCapture(pcapRunning)
