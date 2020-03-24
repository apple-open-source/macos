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


#include "SecurityTool/sharedTool/security_tool_commands.h"

SECURITY_COMMAND(
	"sync", keychain_sync,
	"[options]\n"
	"Keychain Syncing\n"
	"    -d     disable\n"
	"    -e     enable (join/create circle)\n"
	"    -i     info (current status)\n"
	"    -m     dump my peer\n"
	"\n"
	"Account/Circle Management\n"
	"    -a     accept all applicants\n"
	"    -r     reject all applicants\n"
    "    -b     device|all|single Register a backup bag - THIS RESETS BACKUPS!\n"
	"    -N     (re-)set to new account (USE WITH CARE: device will not leave circle before resetting account!)\n"
	"    -O     reset to offering\n"
	"    -R     reset circle\n"
    "    -o     list view unaware peers in circle\n"
    "    -0     boot view unaware peers from circle\n"
    "    -5     cleanup old KVS keys in KVS\n"
    "\n"
    "Circle Tools\n"
    "    --remove-peer SPID     Remove a peer identified by the first 8 or more\n"
    "                           characters of its spid. Specify multiple times to\n"
    "                           remove more than one peer.\n"
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
	"    -U     purge private key material cache\n"
    "    -V     Report View Sync Status on all known clients.\n",
	"Keychain Syncing controls." )


