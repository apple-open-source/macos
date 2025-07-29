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

/*
 * A run table is defined as a list of 3-tuples [(String, String?, Bool)]:
 * 	1st tuple element: String = the fully specified command (full path to binary + arguments)
 * 	2nd tuple element: String (Swift Optional) = the output file name
 *		nil entry is used for output that's not persisted in a particular file,
 *		for example, commands that may have side effects like dumping something to system logs
 *	3rd tuple element: Bool = whether to run the associated command over the system's interface list
 *		for commands needing particular subsets of the system interface list, or any other inputs,
 *		create a custom function in GNICollector with the proper contingency
 */

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
	("/usr/sbin/scutil -p --snapshot /dev/stdout", 			"SCDynamicStore.plist",			false),
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
	("/usr/sbin/skywalkctl flow-switch",				"skywalk.txt",				false),
	("/usr/sbin/skywalkctl flow-owner",				"skywalk.txt",				false),
	("/usr/sbin/skywalkctl flow-adv",				"skywalk.txt",				false),
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
	("/sbin/route -n -v get -inet6 ::",				"route-info.txt",			false),
	("/bin/hostname",						"hostname.txt",				false),

	// implemented directly in the caller function due to running with contingencies
	// ("/usr/sbin/netstat -qq -I %@",				"netstat.txt",				true),
	// ("/usr/sbin/netstat -Q -I %@",				"netstat.txt",				true),
	// ("/usr/sbin/ipconfig getsummary %@",				"ipconfig-info.txt",			true),
	// ("pfctl -s all -a %@",					"pf.txt",				true),
	// ("/usr/sbin/setkey -D | <redaction_script>",			"ipsec.txt",				false),
	// ("cat /var/run/racoon/*.conf | <redaction_script>",		"ipsec.txt",				false),
	// ("cat /var/log/vpnd.log",					"vpnd.txt",				false),
	// ("cat /var/log/racoon.log",					"racoon.txt",				false),

]

let runTableForNDFInfo: [(String, String?, Bool)] = [
	("/usr/sbin/sysctl -a",					"sysctl.txt",			false),
	("/usr/sbin/lsof +c 0 -X -n -O -P -T q",		"lsof.txt",			false),
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
	("/usr/local/bin/neutil session log-file-handles",	nil,				false),
	("/usr/bin/nettop -n -l 1",				"nettop.txt",			false),
	("/usr/local/bin/network_test path_watcher -dump -sysdiagnose",	"nw_path.txt",		false),
]

/*
 * A copy table is defined as a list of tuples [(String, String?)]:
 * 	1st tuple element: String = the file to copy (full path to file)
 * 	2nd tuple element: String (Swift Optional) = whether
 */

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
