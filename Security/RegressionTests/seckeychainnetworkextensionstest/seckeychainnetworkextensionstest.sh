#!/bin/sh
# exit with non-zero on failure of a command
set -e

/AppleInternal/CoreOS/tests/Security/seckeychainnetworkextensionstest
/AppleInternal/CoreOS/tests/Security/seckeychainnetworkextensionsystemdaemontest
/AppleInternal/CoreOS/tests/Security/seckeychainnetworkextensionunauthorizedaccesstest

exit 0
