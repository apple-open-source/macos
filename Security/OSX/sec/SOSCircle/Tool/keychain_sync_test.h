//
//  keychain_sync_test.h
//  sec
//
//

#include <SecurityTool/security_tool_commands.h>

SECURITY_COMMAND(
                 "sync-test", keychain_sync_test,
                 "[options]\n"
                 "Keychain Sync Test\n"
                 "    -p|--enabled-peer-views <view-name-list>\n"
                 "    -m|--message-pending-state\n"
                 "\n",
                 "Keychain Syncing test commands." )
