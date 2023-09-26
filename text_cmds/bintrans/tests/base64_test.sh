#
# Copyright (c) 2022 Apple Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
#
# This file contains Original Code and/or Modifications of
# Original Code as defined in and that are subject to the Apple Public
# Source License Version 1.0 (the 'License').  You may not use this file
# except in compliance with the License.  Please obtain a copy of the
# License at http://www.apple.com/publicsource and read it before using
# this file.
#
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License."
#
# @APPLE_LICENSE_HEADER_END@
#

cmd=base64
#cmd="sh base64.sh"

sampinp="aoijeasdflkbnoiqenfoaisdfjlkadjslkjdf"
sampout="YW9pamVhc2RmbGtibm9pcWVuZm9haXNkZmpsa2FkanNsa2pkZgo="
sampoutb20="YW9pamVhc2RmbGtibm9p
cWVuZm9haXNkZmpsa2Fk
anNsa2pkZgo="
sampoutb10="YW9pamVhc2
RmbGtibm9p
cWVuZm9haX
NkZmpsa2Fk
anNsa2pkZg
o="
fails=0

if [ "$sampout" != "$(
    echo "$sampinp" | $cmd )" ]; then
	echo test 1 failed
	fails=$((fails + 1))
fi

if [ "$sampoutb10" != "$(
    echo "$sampinp" | $cmd  -b 10)" ]; then
	echo test 2 failed
	fails=$((fails + 1))
fi

if [ "$sampoutb10" != "$(
    echo "$sampinp" | $cmd  --break=10)" ]; then
	echo test 3 failed
	fails=$((fails + 1))
fi

if [ "$sampoutb20" != "$(
    echo "$sampinp" | $cmd  -b 20)" ]; then
	echo test 4 failed
	fails=$((fails + 1))
fi

if [ "$sampoutb20" != "$(
    echo "$sampinp" | $cmd  --break=20)" ]; then
	echo test 5 failed
	fails=$((fails + 1))
fi

if [ "$sampinp" != "$(
    echo "$sampout" | $cmd  -d)" ]; then
	echo test 6 failed
	fails=$((fails + 1))
fi

if [ "$sampinp" != "$(
    echo "$sampinp" | $cmd  -b 20 | $cmd  -d)" ]; then
	echo test 7 failed
	fails=$((fails + 1))
fi

if [ "$sampinp" != "$(
    echo "$sampoutb20" | $cmd  -d)" ]; then
	echo test 8 failed
	fails=$((fails + 1))
fi

if [ "$sampinp" != "$(
    echo "$sampinp" | $cmd  -b 20 | $cmd  -D)" ]; then
	echo test 9 failed
	fails=$((fails + 1))
fi

if [ "$sampinp" != "$(
    echo "$sampinp" | $cmd  -b 20 | $cmd  --decode)" ]; then
	echo test 10 failed
	fails=$((fails + 1))
fi

res=$($cmd  -w 2>/dev/null)
if [ "$?" -ne 64 ]; then
	echo test 11 failed
	fails=$((fails + 1))
fi
if [ ! -z "$res" ]; then
	echo test 12 failed
	fails=$((fails + 1))
fi

if [ "$sampout" != "$(
    echo "$sampinp" | $cmd  -o - -i -)" ]; then
	echo test 13 failed
	fails=$((fails + 1))
fi

echo "$sampinp" | $cmd  -b 20 -o testfile.$$
if [ "$sampoutb20" != "$(cat testfile.$$)" ]; then
	echo test 14 failed
	fails=$((fails + 1))
fi
rm testfile.$$

echo "$sampinp" | $cmd  -b 20 --output=testfile.$$
if [ "$sampoutb20" != "$(cat testfile.$$)" ]; then
	echo test 15 failed
	fails=$((fails + 1))
fi

# the -d -b 20 seems nonsensical, but we need to make sure that
# we don't break the output when decoding
$cmd  -d -b 20 -i testfile.$$ --output=testfile.out.$$
if [ "$sampinp" != "$(cat testfile.out.$$)" ]; then
	echo test 16 failed
	fails=$((fails + 1))
fi
rm testfile.out.$$

$cmd  -d --input=testfile.$$ --output=testfile.out.$$
if [ "$sampinp" != "$(cat testfile.out.$$)" ]; then
	echo test 17 failed
	fails=$((fails + 1))
fi

# Ensure base64 throws an error for an unreadable input file...
echo "$sampinp" > testfile.$$
chmod -r testfile.$$
if $cmd -i testfile.$$ -o /dev/null 2>/dev/null; then
	echo test 18 failed
	fails=$((fails + 1))
fi

# ... and an unwritable output file.
chmod +r-w testfile.$$
if echo "$sampinp" | $cmd -i - -o testfile.$$ 2>/dev/null; then
	echo test 19 failed
	fails=$((fails + 1))
fi

rm -f testfile.$$

# Test that base64 --decode doesn't eat our newlines.
printf "ABCD\nEFGH\n" > testfile.$$

$cmd -b 64 -i testfile.$$ -o - | $cmd -D -i - -o testfile.out.$$
if ! cmp -s testfile.$$ testfile.out.$$; then
	echo "test 20 failed"
	fails=$((fails + 1))
fi

# Test that base64 --decode can handle inputs without a newline at all.
$cmd -i testfile.$$ -o - | tr -d '[[:space:]]' | \
    $cmd -D -i - -o testfile.out.$$
if ! cmp -s testfile.$$ testfile.out.$$; then
	echo "test 21 failed"
	fails=$((fails + 1))
fi

# Large inputs have to be broken up because OpenSSL wants to see a newline
# within the first ~80 characters, it seems, based on experimentation.
seq 1 1024 > testfile.$$
$cmd -i testfile.$$ -o - | $cmd -D -i - -o testfile.out.$$
if ! cmp -s testfile.$$ testfile.out.$$; then
	echo "test 22 failed"
	fails=$((fails + 1))
fi

rm -f testfile.$$
rm testfile.out.$$

printf "0#?0" > testfile.$$
echo "MCM_MA==" | $cmd -D -o testfile.out.$$
if ! cmp -s testfile.$$ testfile.out.$$; then
	echo "test 23 failed"
	fails=$((fails + 1))
fi

rm -f testfile.$$
rm testfile.out.$$

if [ "$fails" -ne 0 ]; then
	echo "$fails tests failed"
else
	echo "all tests passed"
fi
exit $fails
