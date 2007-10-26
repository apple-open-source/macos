#!/bin/sh
#
# amavisd	This script controls the amavisd-new daemon.
#		(to be used with version amavisd-new-20020630 or later)
#

# chkconfig: 2345 79 31
# description: amavisd is an interface between MTA and content checkers
# processname: amavisd
# pidfile: /var/amavis/amavisd.pid

# Source function library.
. /etc/rc.d/init.d/functions

# Source networking configuration.
. /etc/sysconfig/network

#prog="/opt/amavisd-new/sbin/amavisd"
prog="/usr/sbin/amavisd"
prog_base="$(basename ${prog})"

prog_config_file="/etc/amavisd.conf"

# Source configuration.
[ -e /etc/sysconfig/${prog_base} ] && . /etc/sysconfig/${prog_base}

## Check that networking is up.
#[ ${NETWORKING} = "no" ] && exit 0

RETVAL=0

# See how we were called.
case "$1" in
  start)
	action $"Starting ${prog_base}:" ${prog} -c ${prog_config_file}
	RETVAL=$?
	[ $RETVAL -eq 0 ] && touch /var/lock/subsys/${prog_base}
	echo
	;;
  stop)
	action $"Shutting down ${prog_base}:" ${prog} -c ${prog_config_file} stop
	RETVAL=$?
	if [ $RETVAL -eq 0 ] ; then
	        echo "${prog_base} stopped"
        	rm -f /var/lock/subsys/${prog_base}
	else
		echo
	fi
	;;
  status)
	status ${prog_base}
	RETVAL=$?
	;;
  restart)
	$0 stop
	$0 start
	RETVAL=$?
	;;
  reload)
	action $"Reloading ${prog_base}:" ${prog} -c ${prog_config_file} reload
	RETVAL=$?
	;;
  *)
	echo "Usage: $0 {start|stop|status|restart|reload}"
	exit 1
esac

exit $RETVAL
