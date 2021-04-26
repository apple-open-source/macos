#!/bin/sh
rm /tmp/secsecstaticcodeapitest_unsigned
cp /AppleInternal/CoreOS/tests/Security/secsecstaticcodeapitest /tmp/secsecstaticcodeapitest_unsigned
codesign -s - -f /tmp/secsecstaticcodeapitest_unsigned
/tmp/secsecstaticcodeapitest_unsigned unsigned
