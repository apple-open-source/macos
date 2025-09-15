#!/bin/zsh

set -e
: ${racer:=/AppleInternal/Tests/rsync/symlink_racer}
: ${rsync:=/usr/bin/rsync}

setup() {
	if ! sudo true; then
		1>&2 echo "This proof-of-concept script requires root."
		return 1
	fi

	# Remove previous files
	sudo rm -rf src etc

	# Prepare user-accessible directories
	mkdir -p src
	echo "Not even remotely secret" > src/backup_file

	ln -s $PWD/etc/secret src/malicious_link

	# Prepare "secret" directory
	mkdir etc
	echo "This is a secret" > etc/secret
	sudo chown -R root etc
	sudo chmod 600 etc/secret
}

1>&2 echo "=== Initial setup === "
setup

# Fire up the racer
$racer src/backup_file src/malicious_link &
rpid=$!

# Wait up to 2 seconds
cur=0
tmo=20
while [ ! -f flag ] && [ "$cur" -lt "$tmo" ]; do
	sleep 0.1
	cur=$((cur + 1))
done

if [ "$cur" -eq "$tmo" ]; then
	1>&2 echo "Timeout waiting for symlink_racer"
	exit 1
fi

trap "kill -9 $rpid" EXIT

1>&2 echo "=== Testing rsync race ==="
# Race it 500 times and make sure it never succeeds,
iter=0
limit=500

while [ "$iter" -lt "$limit" ]; do
	iter=$((iter + 1))

	if [ "$((iter % 100))" -eq 0 ]; then
		echo "Iter $iter"
	fi

	sudo rm -rf dst
	mkdir dst
	sudo $rsync -r src/backup_file dst/ 2>stderr >stdout || true

	if [ ! -f dst/backup_file ]; then
		# The transfer was rejected, excellent.
		continue
	fi

	if grep -Fxq "This is a secret" dst/backup_file; then
		1>&2 echo "FAILED: Hit the race window!"
		1>&2 echo
		1>&2 echo "=== stdout ==="
		1>&2 cat stdout
		1>&2 echo "=== stderr ==="
		1>&2 cat stderr
		exit 1
	fi
done
