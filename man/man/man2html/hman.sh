#!/bin/sh
#
# hman - interface to the man2html scripts
#
# Michael Hamilton <michael@actrix.gen.nz>, Apr 1996
# Andries Brouwer <aeb@cwi.nl>, Jan 1998.
#
# Usage examples:
#        hman                    - get start page
#        hman man2html           - get man page for man2html
#        hman 7 locale           - get section 7 man page for locale 
#        hman 1                  - section 1 index of names only
#        hman 3 index            - section 3 index names+descriptions
#        hman -k editor          - search all man pages for some string
#	 hman -P arena ./twm.man - specify browser; specify man page
#
# hman from %version%
#

if [ x"$1" = x"-v" -o x"$1" = x"-V" ]; then
	echo "`basename $0` from %version%"
	exit 0
fi

# The user has to set MANHTMLPAGER (or he will get httpd-free lynx).
# Pick your favorite browser: lynx, xmosaic, netscape, arena, amaya, grail, ...
BROWSER=${MANHTMLPAGER-lynxcgi}
#
# If the man pages are on a remote host, specify it in MANHTMLHOST.
HOST=${MANHTMLHOST-localhost}

# Perhaps the browser was specified on the command line?
if [ $# -gt 1 -a "$1" = "-P" ]; then
    BROWSER="$2"
    shift; shift
fi

# Perhaps the host was specified on the command line?
if [ $# -gt 1 -a "$1" = "-H" ]; then
    HOST="$2"
    shift; shift
fi

# Interface to a live (already running) netscape browser.
function nsfunc () {
	if ( /bin/ps xc | grep -q 'netscape$' ) ; then
		if [ -x  netscape-remote ] ; then
			exec netscape-remote  -remote "openURL($1,new_window)"
		else
			exec netscape -remote "openURL($1,new_window)"
		fi
	else
		netscape $1 &
	fi
}

case $BROWSER in
     lynxcgi)
	BROWSER=lynx
	CG="lynxcgi:/home/httpd/cgi-bin/man"
	;;
     netscape)
        BROWSER=nsfunc
        CG="http://$HOST/cgi-bin/man"
	;;
     *)
	CG="http://$HOST/cgi-bin/man"
	;;
esac

  case $# in
     0)   $BROWSER $CG/man2html ;;
     1)   case "$1" in
	    1|2|3|4|5|6|7|8|l|n)
		$BROWSER "$CG/mansec?$CG+$1" ;;
	    /*)
		$BROWSER "$CG/man2html?$1" ;;
	    */*)
		$BROWSER "$CG/man2html?$PWD/$1" ;;
	    *)
		$BROWSER "$CG/man2html?$1" ;;
          esac ;;
     2)   case "$1" in
            -k)
                $BROWSER "$CG/mansearch?$2" ;;
            *)
		if [ "$2" = index ]; then
		    $BROWSER "$CG/manwhatis?$CG+$1"
                else
		    $BROWSER "$CG/man2html?$1+$2"
                fi ;;
          esac ;;
     *)   echo "bad number of args" ;;
  esac

exit 0
