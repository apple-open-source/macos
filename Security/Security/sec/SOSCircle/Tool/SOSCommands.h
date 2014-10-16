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

SECURITY_COMMAND("sync", keychain_sync,
                 "[options]\n"
                 "    -e     Enable Keychain Syncing (join/create circle)\n"
                 "    -d     Disable Keychain Syncing\n"
                 "    -a     Accept all applicants\n"
                 "    -r     Reject all applicants\n"
                 "    -i     Info\n"
                 "    -k     Pend all registered kvs keys\n"
                 "    -s     Schedule sync with all peers\n"
                 "    -E     Ensure Fresh Parameters\n"
                 "    -R     Reset\n"
                 "    -O     ResetToOffering\n"
                 "    -q     Sign out of Circle\n"                 
                 "    -C     Clear all values from KVS\n"
                 "    -p     Retrieve IDS Device ID\n"
                 "    -g     Set IDS Device ID\n"
                 "    -P    [label:]password  Set password (optionally for a given label) for sync\n"
                 "    -D    [itemName]  Dump contents of KVS\n"
                 "    -P    [label:]password  Set password (optionally for a given label) for sync\n"
                 "    -T    [label:]password  Try password (optionally for a given label) for sync\n"
                 "    -U     Purge private key material cache\n"
                 "    -D    [itemName]  Dump contents of KVS\n"
                 "    -W    itemNames  sync and dump\n",
                 "    -X    [limit]  Best effort bail from circle in limit seconds\n"
                 "Keychain Syncing controls." )

