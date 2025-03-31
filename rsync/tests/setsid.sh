#! /bin/sh

set -e

# Test running rsync in a new session to make sure that we didn't break running
# rsync, for instance, via ssh on a remote host for a transfer local to that
# host.
if command -v realpath >/dev/null; then
	scriptdir=$(dirname $(realpath "$0"))
else
	scriptdir=/AppleInternal/Tests/rsync
fi

rm -rf src dst

mkdir -p src
echo "contents" > src/file

"$scriptdir"/setsid $rsync -a src/ dst/

if ! cmp src/file dst/file; then
	1>&2 echo "Transfer did not succeed."
	exit 1
fi
