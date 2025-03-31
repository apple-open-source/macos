#!/bin/sh

(
	set -e

	# First, we test that -e still works as expected.
	/usr/bin/false

	# Unreachable
	exit 0
)

rc=$?
if [ "$rc" -ne 1 ]; then
	1>&2 echo "subshell with set -e and a single command did not terminate as expected"
	1>&2 echo "expected exit 0, got $rc"
	exit 1
fi

(
	set -e

	# Next, that -e through the command builtin isn't generally broken now.
	command /usr/bin/false

	# Unreachable
	exit 0
)

rc=$?
if [ "$rc" -ne 1 ]; then
	1>&2 echo "subshell with set -e and a single command via builtin did not terminate as expected"
	1>&2 echo "expected exit 0, got $rc"
	exit 1
fi

(
	set -e

	# This is naturally false, but sets the tone that directly executing the
	# command does not abort in the face of set -e.
	if /usr/bin/false; then
		1>&2 echo "false succeeded"
		exit 1
	fi

	# This one we expected to succeed, and if `set -e` is inappropriately
	# propagating then we'll just abort here instead and exit 1.
	if ! command /usr/bin/false; then
		# Thus, this exit 0 is where we expect to leave.
		exit 0
	fi

	1>&2 echo "false succeeded"
	exit 1
)

rc=$?
if [ "$rc" -ne 0 ]; then
	1>&2 echo "subshell with set -e and conditionals did not terminate as expected"
	1>&2 echo "expected exit 0, got $rc"
	exit 1
fi
