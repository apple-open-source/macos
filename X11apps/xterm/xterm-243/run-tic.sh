#!/bin/sh
# $XTermId: run-tic.sh,v 1.3 2007/06/17 15:30:03 tom Exp $
#
# Run tic, either using ncurses' extension feature or filtering out harmless
# messages for the extensions which are otherwise ignored by other versions of
# tic.

TMP=run-tic$$.log
VER=`tic -V 2>/dev/null`
OPT=

case .$VER in
.ncurses*)
	OPT="-x"
	;;
esac

echo "** tic $OPT" "$@"
tic $OPT "$@" 2>$TMP
RET=$?

fgrep -v 'Unknown Capability' $TMP | \
fgrep -v 'Capability is not recognized:' | \
fgrep -v 'tic: Warning near line ' >&2
rm -f $TMP

exit $RET
