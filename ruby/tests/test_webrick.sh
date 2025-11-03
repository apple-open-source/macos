#!/bin/sh

scriptdir=$(dirname $(realpath "$0"))

portno=8675
if lsof -i TCP:"$portno" -s TCP:LISTEN >/dev/null; then
	1>&2 echo "tcp port $portno is already taken"
	exit 1
fi

ruby "$scriptdir"/webrick_simple.rb 2>webrick.log 1>&2 &
spid=$!

tmo=50
i=0

fail() {
	msg="$1"
	shift

	1>&2 echo "$msg"
	1>&2 echo "== webrick.log =="
	1>&2 cat webrick.log

	for f in "$@"; do
		1>&2 echo "== $f =="
		1>&2 cat "$f"
	done

	exit 1
}

while ! lsof -i TCP:"$portno" -s TCP:LISTEN >/dev/null; do
	if [ "$i" -eq "$tmo" ]; then
		fail "webrick server did not come online in 5 seconds"
	fi

	i=$((i + 1))
	sleep 0.1
done

curl -o response "http://localhost:${portno}/"
wait "$spid"
rc=$?

if [ "$rc" -ne 0 ]; then
	fail "webrick test server exited non-zero"
fi

if ! grep -q "^PASS$" response; then
	fail "bad response from webrick test zero:" response
fi

echo "OK"
