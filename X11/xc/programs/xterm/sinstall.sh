#!/bin/sh
# $XFree86: xc/programs/xterm/sinstall.sh,v 1.3 2000/03/03 20:02:35 dawes Exp $
#
# Install program setuid if the installer is running as root, and if xterm is
# already installed on the system with setuid privilege.  This is a safeguard
# for ordinary users installing xterm for themselves on systems where the
# setuid is not needed to access a PTY, but only for things like utmp.
#
# Parameters:
#	$1 = program to invoke as "install"
#	$2 = program to install
#	$3 = previously-installed program, for reference
#	$4 = final installed-path, if different from reference
#
#
trace=:
trace=echo

SINSTALL="$1"
NEW_PROG="$2"
REF_PROG="$3"
TST_PROG="$4"

test -z "$SINSTALL" && SINSTALL=install
test -z "$NEW_PROG" && NEW_PROG=xterm
test -z "$REF_PROG" && REF_PROG=/usr/bin/X11/xterm
test -z "$TST_PROG" && TST_PROG="$REF_PROG"

PROG_MODE=755
PROG_USR=
PROG_GRP=

echo checking for presumed installation-mode
if test -f "$REF_PROG" ; then
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
	$trace "... derived user \"$PROG_USR\", group \"$PROG_GRP\" of previously-installed $NEW_PROG"

	$trace "... see if mode \"$cf_mode\" has s-bit set"
	case ".$cf_mode" in #(vi
	.???s*) #(vi
		PROG_MODE=4711
		PROG_GRP=
		;;
	.??????s*)
		PROG_MODE=2711
		PROG_USR=
		;;
	esac
fi

if test -n "${PROG_USR}${PROG_GRP}" ; then
	cf_uid=`id | sed -e 's/^[^=]*=//' -e 's/(.*$//'`
	cf_usr=`id | sed -e 's/^[^(]*(//' -e 's/).*$//'`
	cf_grp=`id | sed -e 's/^.* gid=[^(]*(//' -e 's/).*$//'`
	$trace "... installing $NEW_PROG as user \"$cf_usr\", group \"$cf_grp\""
	if test "$cf_uid" != 0 ; then
		PROG_MODE=755
		PROG_USR=""
		PROG_GRP=""
	fi
	test "$PROG_USR" = "$cf_usr" && PROG_USR=""
	test "$PROG_GRP" = "$cf_grp" && PROG_GRP=""
fi

test -n "$PROG_USR" && PROG_USR="-o $PROG_USR"
test -n "$PROG_GRP" && PROG_GRP="-g $PROG_GRP"

echo "$SINSTALL -m $PROG_MODE $PROG_USR $PROG_GRP $NEW_PROG $TST_PROG"
eval "$SINSTALL -m $PROG_MODE $PROG_USR $PROG_GRP $NEW_PROG $TST_PROG"
