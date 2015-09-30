#!/bin/sh

#  LocalCaspianTestRun.sh
#  Security
#
#  Created by Greg Kerr on 2/10/15.
#

sudo ditto /SWE/Teams/CoreOS/SecEng/BATS/broken-AppleScript-app.tgz /AppleInternal/CoreOS/codesign_tests/
sudo ditto /SWE/Teams/CoreOS/SecEng/BATS/caspian-tests.tar.gz /AppleInternal/CoreOS/codesign_tests/
sudo ditto /SWE/Teams/CoreOS/SecEng/BATS/xar /AppleInternal/CoreOS/codesign_tests/xar
sudo chmod g+r /AppleInternal/CoreOS/codesign_tests/xar
sudo chmod o+r /AppleInternal/CoreOS/codesign_tests/xar

sudo /AppleInternal/CoreOS/codesign_tests/CaspianTests