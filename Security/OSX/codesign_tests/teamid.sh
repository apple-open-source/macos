#!/bin/bash

echo "[TEST] team identifier verification"

echo "[BEGIN] executable with false team identifier"

MY_TEMP=$(mktemp /tmp/codesign.XXXXXX)
codesign --verify --verbose=3 $1 2> $MY_TEMP

if grep -s "invalid or unsupported format for signature" $MY_TEMP
then
	echo "[PASS]"
else
	echo "[FAIL]"
fi
rm -f $MY_TEMP

