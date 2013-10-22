//
//  SOSCommands.h
//  sec
//
//  Created by Mitch Adler on 1/9/13.
//
//

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
                 "    -R     Reset\n"
                 "    -O     ResetToOffering\n"
                 "    -C     Clear all values from KVS\n"
                 "    -P    [label:]password  Set password (optionally for a given label) for sync\n"
                 "    -D    [itemName]  Dump contents of KVS\n"
                 "    -P    [label:]password  Set password (optionally for a given label) for sync\n"
                 "    -T    [label:]password  Try password (optionally for a given label) for sync\n"
                 "    -U     Purge private key material cache\n"
                 "    -D    [itemName]  Dump contents of KVS\n"
                 "    -W    itemNames  sync and dump\n",
                 "    -X    [limit]  Best effort bail from circle in limit seconds\n"
                 "Keychain Syncing controls." )

