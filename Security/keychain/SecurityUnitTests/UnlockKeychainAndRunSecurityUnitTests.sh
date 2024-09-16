#!/bin/sh

#  UnlockKeychainAndRunSecurityUnitTests.sh
#  Security
#

PASSWORD=`plutil -extract password raw /private/etc/credentials.plist`
/usr/bin/security unlock-keychain -p "$PASSWORD"
/AppleInternal/CoreOS/tests/Security/KeychainEntitledTestRunner -t SecurityUnitTests
