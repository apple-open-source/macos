#!/bin/sh

set -e

cd $(dirname $(realpath $0))

export PATH=/tmp/text_cmds.dst/usr/bin:$PATH
export JQ=jq

for test in tests/*.test ; do
	test=${test#tests/}
	test=${test%.test}
	/tmp/text_cmds.dst/AppleInternal/Tests/text_cmds/jq/tests/${test}test
done
