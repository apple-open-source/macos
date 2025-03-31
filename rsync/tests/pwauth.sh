#!/bin/sh

# We'll use this under netwrapd, but the *client* that talks to rsyncd will just
# be rsync from $PATH because it's not the part that's under test.
: ${rsync=/usr/bin/rsync}

NETWRAPD="/AppleInternal/Tests/rsync/openrsync/netwrapd"

dport=0

cfgfile="/etc/rsyncd.conf"
portfile="$PWD/rsyncd.port"
secretsfile="$PWD/rsyncd.secrets"
destdir="$PWD/root"

product=$(sw_vers --productName)
rsyncd_enabled=0

needpw_root="$PWD"/needpw
nopw_root="$PWD"/nopw

# On macOS, there's nothing wrong with rsyncd with or without password
# authentication, so we'll just kind of expect it to work.  On every other
# platform, rsyncd is disabled by default unless a bootarg is set.  We can't
# easily test bootargs, but on those platforms we'll confirm that it's disabled
# on a normal boot and that it's enabled if the bootarg is set.
case "$product" in
macOS)
	rsyncd_enabled=1
	;;
*)
	if sysctl -n kern.bootargs | grep -q 'rdar102068389=[Yy][Ee][Ss]'; then
		rsyncd_enabled=1
		1>&2 echo "Embedded platform has rsyncd enabled via bootarg"
	fi
	;;
esac

# Setup
rm -f contents
echo "darwin:insecure" > "$secretsfile"
chmod 0600 "$secretsfile"
mkdir -p "$needpw_root" "$nopw_root"

echo "needpw" > "$needpw_root"/contents
echo "nopw" > "$nopw_root"/contents

mkdir -p "$destdir"/private/etc
ln -s private/etc "$destdir"/etc
cat <<EOF > "$destdir$cfgfile"
read only = true
use chroot = no
max verbosity = 3

[needpw]
	path = $needpw_root
	auth users = darwin
	secrets file = $secretsfile
[nopw]
	path = $nopw_root
EOF

echo "Starting rsyncd"

netwrapd_pid=0

start_rsyncd() {
	rm -f "$portfile"

	# netwrapd is openrsync specific, we can just kill it.
	env NETWRAP_ARGS="$rsync --daemon $*" \
	    $NETWRAPD -p "$portfile" &
	netwrapd_pid=$!

	# Wait up to ~5 seconds
	tmo=0
	while [ ! -s "$portfile" -a $tmo -lt 50 ]; do
		sleep 0.1
		tmo=$((tmo + 1))
	done

	if [ $tmo -eq 50 ]; then
		1>&2 echo "timeout waiting for rsyncd to start"
		exit 1
	fi

	read dport < "$portfile"

	while ! netstat -an | grep LISTEN | grep -q "127.0.0.1\.$dport"; do
		echo "$tmo"
		if [ $tmo -ge 50 ]; then
			break
		fi

		sleep 0.1
		tmo=$((tmo + 1))
	done

	if [ $tmo -eq 50 ]; then
		1>&2 echo "timeout waiting for rsyncd to start"
		exit 1
	fi
}

trap '[ $netwrapd_pid -ne 0 ] && kill $netwrapd_pid' EXIT

if [ $rsyncd_enabled -eq 0 ]; then
	# If rsyncd isn't enabled by default, we start it without --config to
	# start with, then restart it later with --config to ensure that it
	# does actually work.  We'll always use --config later if rsyncd is
	# supposed to be enabled to avoid having to write to the root fs in case
	# it's configured to boot from snapshots.
	darwinup install "$destdir"
	start_rsyncd

	# Embedded platforms, test that user auth is disabled!
	if env USER="darwin" RSYNC_PASSWORD="insecure" \
	    rsync -vvv rsync://localhost:"$dport"/needpw/contents "$PWD"; then
		1>&2 echo "rsyncd with password authentication appears to be enabled."
		exit 1
	fi

	if [ -s contents ]; then
		1>&2 echo "rsyncd did not succeed but appears to have downloaded the file:"
		1>&2 cat contents
		exit 1
	fi

	# nopw should also be disabled.
	if rsync -vvv rsync://localhost:"$dport"/nopw/contents "$PWD"; then
		1>&2 echo "rsyncd without password authentication appears to be enabled."
		exit 1
	fi

	if [ -s contents ]; then
		1>&2 echo "rsyncd did not succeed but appears to have downloaded the file:"
		1>&2 cat contents
		exit 1
	fi

	# We can override the bootarg by using a specific --config, which will
	# then match the macOS behavior.
	kill "$netwrapd_pid"
	rm -f contents

	netwrapd_pid=0
fi

start_rsyncd --config "$destdir$cfgfile"

# Test that we *have* to specify a username/password
if env USER="darwin" RSYNC_PASSWORD="" \
    rsync -vvv rsync://localhost:"$dport"/needpw/contents "$PWD"; then
	1>&2 echo "rsyncd succeeded without the password."
	exit 1
fi

if [ -s contents ]; then
	1>&2 echo "rsyncd failed but still downloaded the file:"
	1>&2 cat contents
	exit 1
fi

# Now test that the password works.
if ! env USER="darwin" RSYNC_PASSWORD="insecure" \
    rsync -vvv rsync://localhost:"$dport"/needpw/contents "$PWD"; then
	1>&2 echo "rsyncd did not succeed with the password."
	exit 1
fi

if [ ! -s contents ]; then
	1>&2 echo "rsyncd succeeded but did not download the file."
	exit 1
fi

if ! grep -q "needpw" contents; then
	1>&2 echo "rsyncd seems to have downloaded the wrong file:"
	1>&2 cat contents
	exit 1
fi

# Success
echo "All tests passed"
