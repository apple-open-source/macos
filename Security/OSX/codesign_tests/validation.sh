#!/bin/bash

echo "[TEST] codesign dynamic validation"

echo "[BEGIN] Dynamic validate pid 1"
codesign --verify --verbose=3 1

if [ $? -ne 0 ]
then
	echo "[FAIL]"
else
	echo "[PASS]"
fi

echo "[BEGIN] Dynamic validate a universal binary"

$1 &
pid=$!
codesign --verify --verbose=3 $!

if [ $? -ne 0 ]
then
	echo "[FAIL]"
else
	echo "[PASS]"
fi

echo "[BEGIN] Dynamic validate a universal binary, 32 bit slice"

arch -i386 $1 &
pid=$!
codesign --verify --verbose=3 $!

if [ $? -ne 0 ]
then
	echo "[FAIL]"
else
	echo "[PASS]"
fi

# Will exit with status of last command.

exit $?
