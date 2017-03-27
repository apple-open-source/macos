/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#include <SecurityTool/security_tool_commands.h>

SECURITY_COMMAND(
	"sync", keychain_sync,
	"[options]\n"
	"Keychain Syncing\n"
	"    -d     disable\n"
	"    -e     enable (join/create circle)\n"
	"    -i     info (current status)\n"
	"    -m     dump my peer\n"
	"    -s     schedule sync with all peers\n"
	"\n"
	"Account/Circle Management\n"
	"    -a     accept all applicants\n"
	"    -l     [reason] sign out of circle + set custom departure reason\n"
	"    -q     sign out of circle\n"
	"    -r     reject all applicants\n"
	"    -E     ensure fresh parameters\n"
    "    -b     device|all|single Register a backup bag - THIS RESETS BACKUPS!\n"
    "    -A     Apply to a ring\n"
    "    -B     Withdrawl from a ring\n"
    "    -G     Enable Ring\n"
    "    -F     Ring Status\n"
    "    -I     Dump Ring Information\n"
	"    -N     (re-)set to new account (USE WITH CARE: device will not leave circle before resetting account!)\n"
	"    -O     reset to offering\n"
	"    -R     reset circle\n"
	"    -X     [limit]  best effort bail from circle in limit seconds\n"
    "    -o     list view unaware peers in circle\n"
    "    -0     boot view unaware peers from circle\n"
    "    -1     grab account state from the keychain\n"
    "    -2     delete account state from the keychain\n"
    "    -3     grab engine state from the keychain\n"
    "    -4     delete engine state from the keychain\n"

	"\n"
	"IDS\n"
	"    -g     set IDS device id\n"
	"    -p     retrieve IDS device id\n"
	"    -x     ping all devices in an IDS account\n"
	"    -w     check IDS availability\n"
	"    -z     retrieve IDS id through KeychainSyncingOverIDSProxy\n"
	"\n"
	"Password\n"
	"    -P     [label:]password  set password (optionally for a given label) for sync\n"
	"    -T     [label:]password  try password (optionally for a given label) for sync\n"
	"\n"
	"KVS\n"
	"    -k     pend all registered kvs keys\n"
	"    -C     clear all values from KVS\n"
	"    -D     [itemName]  dump contents of KVS\n"
	"    -W     sync and dump\n"
	"\n"
	"Misc\n"
	"    -v     [enable|disable|query:viewname] enable, disable, or query my PeerInfo's view set\n"
	"             viewnames are: keychain|masterkey|iclouddrive|photos|cloudkit|escrow|fde|maildrop|icloudbackup|notes|imessage|appletv|homekit\n"
    "                            wifi|passwords|creditcards|icloudidentity|othersyncable\n"
    "    -L     list all known view and their status\n"
	"    -S     [enable|disable|propertyname] enable, disable, or query my PeerInfo's Security Property set\n"
	"             propertynames are: hasentropy|screenlock|SEP|IOS\n"
	"    -U     purge private key material cache\n"
    "    -V     Report View Sync Status on all known clients.\n"
    "    -Y     Report yet to initial sync views\n"
    "    -H     Set escrow record.\n"
    "    -J     Get the escrow record.\n"
    "    -M     Check peer availability.\n",
	"Keychain Syncing controls." )
