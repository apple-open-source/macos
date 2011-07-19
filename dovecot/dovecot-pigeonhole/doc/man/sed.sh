#!/bin/sh

SRCDIR="${1:-`pwd`}"
RUNDIR="${2:-/usr/local/var/run/dovecot}"
PKGSYSCONFDIR="${3:-/usr/local/etc/dovecot}"

sed -e "/^@INCLUDE:reporting-bugs@$/{
		r ${SRCDIR}/reporting-bugs.inc
		d
	}" | sed -e "s|@pkgsysconfdir@|${PKGSYSCONFDIR}|" -e "s|@rundir@|${RUNDIR}|"

