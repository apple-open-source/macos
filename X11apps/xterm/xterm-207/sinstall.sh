#!/bin/sh
# $XTermId: sinstall.sh,v 1.13 2005/11/03 13:17:28 tom Exp $
# $XFree86: xc/programs/xterm/sinstall.sh,v 1.4 2005/11/03 13:17:28 dickey Exp $
#
# Install program setuid if the installer is running as root, and if xterm is
# already installed on the system with setuid privilege.  This is a safeguard
# for ordinary users installing xterm for themselves on systems where the
# setuid is not needed to access a PTY, but only for things like utmp.
#
# Options:
#	u+s, g+s as in chmod
#	-u, -g and -m as in install.  If any options are given, $3 is ignored.
#
# Parameters:
#	$1 = program to invoke as "install"
#	$2 = program to install
#	$3 = previously-installed program, for reference
#	$4 = final installed-path, if different from reference

trace=:
trace=echo

OPTS_SUID=
OPTS_SGID=
OPTS_MODE=
OPTS_USR=
OPTS_GRP=

while test $# != 0
do
	case $1 in
	-*)
		OPT="$1"
		shift
		if test $# != 0
		then
			case $OPT in
			-u)	OPTS_USR="$1"; shift;;
			-g)	OPTS_GRP="$1"; shift;;
			-m)	OPTS_MODE="$1"; shift;;
			esac
		else
			break
		fi
		;;
	u+s)	shift;	OPTS_SUID=4000;;
	g+s)	shift;	OPTS_SGID=2000;;
	*)	break
		;;
	esac
done

SINSTALL="$1"
SRC_PROG="$2"
REF_PROG="$3"
DST_PROG="$4"

test -z "$SINSTALL" && SINSTALL=install
test -z "$SRC_PROG" && SRC_PROG=xterm
test -z "$REF_PROG" && REF_PROG=/usr/bin/X11/xterm
test -z "$DST_PROG" && DST_PROG="$REF_PROG"

test -n "$OPTS_SUID" && test -n "$OPTS_USR" && REF_PROG=
test -n "$OPTS_SGID" && test -n "$OPTS_GRP" && REF_PROG=

echo checking for presumed installation-mode

PROG_SUID=
PROG_SGID=
PROG_MODE=
PROG_USR=
PROG_GRP=

if test -z "$REF_PROG" ; then
	$trace "... reference program not used"
elif test -f "$REF_PROG" ; then
	cf_option="-l -L"
	MYTEMP=${TMPDIR-/tmp}/sinstall$$

	# Expect listing to have fields like this:
	#-r--r--r--   1 user      group       34293 Jul 18 16:29 pathname
	ls $cf_option $REF_PROG >$MYTEMP
	read cf_mode cf_links cf_usr cf_grp cf_size cf_date1 cf_date2 cf_date3 cf_rest <$MYTEMP
	$trace "... if \"$cf_rest\" is null, try the ls -g option"
	if test -z "$cf_rest" ; then
		cf_option="$cf_option -g"
		ls $cf_option $REF_PROG >$MYTEMP
		read cf_mode cf_links cf_usr cf_grp cf_size cf_date1 cf_date2 cf_date3 cf_rest <$MYTEMP
	fi
	rm -f $MYTEMP

	# If we have a pathname, and the date fields look right, assume we've
	# captured the group as well.
	$trace "... if \"$cf_rest\" is null, we do not look for group"
	if test -n "$cf_rest" ; then
		cf_test=`echo "${cf_date2}${cf_date3}" | sed -e 's/[0-9:]//g'`
		$trace "... if we have date in proper columns ($cf_date1 $cf_date2 $cf_date3), \"$cf_test\" is null"
		if test -z "$cf_test" ; then
			PROG_USR=$cf_usr;
			PROG_GRP=$cf_grp;
		fi
	fi
	$trace "... derived user \"$PROG_USR\", group \"$PROG_GRP\" of previously-installed $SRC_PROG"

	$trace "... see if mode \"$cf_mode\" has s-bit set"
	case ".$cf_mode" in #(vi
	.???s??s*) #(vi
		PROG_SUID=4000
		PROG_SGID=2000
		;;
	.???s*) #(vi
		PROG_SUID=4000
		PROG_GRP=
		;;
	.??????s*)
		PROG_SGID=2000
		PROG_USR=
		;;
	esac
	PROG_MODE=`echo ".$cf_mode" | sed -e 's/^..//' -e 's/rw./7/g' -e 's/r-./5/g' -e 's/--[sxt]/1/g'`
fi

# passed-in options override the reference
test -n "$OPTS_SUID" && PROG_SUID="$OPTS_SUID"
test -n "$OPTS_SGID" && PROG_SGID="$OPTS_SGID"
test -n "$OPTS_MODE" && PROG_MODE="$OPTS_MODE"
test -n "$OPTS_USR"  && PROG_USR="$OPTS_USR"
test -n "$OPTS_GRP"  && PROG_GRP="$OPTS_GRP"

# we always need a mode
test -z "$PROG_MODE" && PROG_MODE=755

if test -n "${PROG_USR}${PROG_GRP}" ; then
	cf_uid=`id | sed -e 's/^[^=]*=//' -e 's/(.*$//'`
	cf_usr=`id | sed -e 's/^[^(]*(//' -e 's/).*$//'`
	cf_grp=`id | sed -e 's/^.* gid=[^(]*(//' -e 's/).*$//'`
	$trace "... installing $SRC_PROG as user \"$cf_usr\", group \"$cf_grp\""
	if test "$cf_uid" != 0 ; then
		PROG_SUID=
		PROG_SGID=
		PROG_USR=""
		PROG_GRP=""
	fi
	test "$PROG_USR" = "$cf_usr" && PROG_USR=""
	test "$PROG_GRP" = "$cf_grp" && PROG_GRP=""
fi

test -n "${PROG_SUID}${PROG_SGID}" && PROG_MODE=`expr $PROG_MODE % 1000`
test -n "$PROG_SUID" && PROG_MODE=`expr $PROG_SUID + $PROG_MODE`
test -n "$PROG_SGID" && PROG_MODE=`expr $PROG_SGID + $PROG_MODE`

test -n "$PROG_USR" && PROG_USR="-o $PROG_USR"
test -n "$PROG_GRP" && PROG_GRP="-g $PROG_GRP"

echo "$SINSTALL -m $PROG_MODE $PROG_USR $PROG_GRP $SRC_PROG $DST_PROG"
eval "$SINSTALL -m $PROG_MODE $PROG_USR $PROG_GRP $SRC_PROG $DST_PROG"
