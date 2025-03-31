#! /bin/sh

. ${tstdir-.}/conf.sh

: ${NETWRAPD=${tstdir-.}/netwrapd}
: ${NETWRAPD_PORTFILE="daemon.port"}

: ${rsync="rsync"}
: ${RSYNC_CLIENT="$rsync"}
: ${RSYNC_SERVER="$rsync"}
: ${pidfile="daemon.pid"}

dserial=0
dpid=0

daemon_listening() {
	local lport

	lport=$1

	case "$(uname -s)" in
	Darwin)
		netstat -an | grep -F LISTEN | grep -Fq "127.0.0.1.$lport"
		;;
	FreeBSD)
		sockstat -l | grep -Fq "127.0.0.1:$lport"
		;;
	Linux)
		netstat -ln | grep -Fq "127.0.0.1:$lport"
		;;
	*)
		1>&2 echo "the daemon tests have not yet been ported to this platform"
		exit 1
		;;
	esac
}

daemon_port() {
	cat "$NETWRAPD_PORTFILE"
}

daemon_run() {
	local dport iter npid tmo

	rm -f "$NETWRAPD_PORTFILE"
	env NETWRAP_ARGS="$RSYNC_SERVER --daemon $*" \
	    "$NETWRAPD" -p "$NETWRAPD_PORTFILE" &

	npid=$!

	# Wait for the port file to become available, then we'll write a pidfile
	# just to make the caller happy.
	iter=0
	tmo=50	# Wait up to ~5 seconds
	while [ ! -f "$NETWRAPD_PORTFILE" -a "$iter" -lt "$tmo" ]; do
		iter=$((iter + 1))
		sleep 0.1
	done

	if [ $iter -ge $tmo ]; then
		1>&2 echo "timeout waiting for rsyncd to start"
		exit 1
	fi

	read dport < "$NETWRAPD_PORTFILE"
	while ! daemon_listening $dport; do
		if [ $iter -ge $tmo ]; then
			break
		fi

		iter=$((iter + 1))
		sleep 0.1
	done

	if [ $iter -ge $tmo ]; then
		1>&2 echo "timeout waiting for rsyncd to start"
		exit 1
	fi

	echo "$npid" > "$pidfile"
}

daemon_kill() {
	rc=$?

	if [ "$dpid" -gt 0 ]; then
		kill "$dpid"
		wait
	fi

	return "$rc"
}

daemon_cleanup() {
	daemon_kill
	exit "$?"
}

trap 'daemon_cleanup' EXIT

rsyncd() {
	cfgfile="$1"
	: ${logfile=${2-"rsyncd.log"}}

	# We shouldn't be run multiple times, but if we are then cut off a copy
	# of the log and remove it and the pid file.
	if [ -f "$logfile" ]; then
		mv "$logfile" "$logfile.$((dserial - 1))"
	fi
	rm -f "$pidfile"

	# Add the pid file in
	echo "pid file = $pidfile" > "$cfgfile.$dserial"
	cat "$cfgfile" >> "$cfgfile.$dserial"

	daemon_run --config "$cfgfile.$dserial" --log-file "$logfile"

	iter=0
	tmo=50	# Wait up to ~5 seconds
	while [ ! -f "$pidfile" -a "$iter" -lt "$tmo" ]; do
		iter=$((iter + 1))
		sleep 0.1
	done

	if [ ! -f "$pidfile" ]; then
		1>&2 echo "Daemon failed to start within five seconds"
		exit 1
	fi

	read dpid < "$pidfile"
	dserial=$((dserial + 1))
}
