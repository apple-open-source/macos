XCOMM!/bin/sh

XCOMM $Xorg: startx.cpp,v 1.3 2000/08/17 19:54:29 cpqbld Exp $
XCOMM
XCOMM This is just a sample implementation of a slightly less primitive
XCOMM interface than xinit.  It looks for user .xinitrc and .xserverrc
XCOMM files, then system xinitrc and xserverrc files, else lets xinit choose
XCOMM its default.  The system xinitrc should probably do things like check
XCOMM for .Xresources files and merge them in, startup up a window manager,
XCOMM and pop a clock and serveral xterms.
XCOMM
XCOMM Site administrators are STRONGLY urged to write nicer versions.
XCOMM
XCOMM $XFree86: xc/programs/xinit/startx.cpp,v 3.16 2003/01/24 21:30:02 herrb Exp $

#ifdef SCO

XCOMM Check for /usr/bin/X11 and BINDIR in the path, if not add them.
XCOMM This allows startx to be placed in a place like /usr/bin or /usr/local/bin
XCOMM and people may use X without changing their PATH

XCOMM First our compiled path

bindir=BINDIR
if expr $PATH : ".*`echo $bindir | sed 's?/?\\/?g'`.*" > /dev/null 2>&1; then
	:
else
	PATH=$PATH:BINDIR
fi

XCOMM Now the "SCO" compiled path

if expr $PATH : '.*\/usr\/bin\/X11.*' > /dev/null 2>&1; then
	:
else
	PATH=$PATH:/usr/bin/X11
fi

XCOMM Set up the XMERGE env var so that dos merge is happy under X

if [ -f /usr/lib/merge/xmergeset.sh ]; then
	. /usr/lib/merge/xmergeset.sh
elif [ -f /usr/lib/merge/console.disp ]; then
	XMERGE=`cat /usr/lib/merge/console.disp`
	export XMERGE
fi

scoclientrc=$HOME/.startxrc
#endif

userclientrc=$HOME/.xinitrc
userserverrc=$HOME/.xserverrc
sysclientrc=XINITDIR/xinitrc
sysserverrc=XINITDIR/xserverrc
defaultclient=BINDIR/xterm
defaultserver=BINDIR/X
defaultclientargs=""
defaultserverargs=""
clientargs=""
serverargs=""

#ifdef SCO
if [ -f $scoclientrc ]; then
    defaultclientargs=$scoclientrc
else
#endif
if [ -f $userclientrc ]; then
    defaultclientargs=$userclientrc
elif [ -f $sysclientrc ]; then
    defaultclientargs=$sysclientrc
fi
#ifdef SCO
fi
#endif

if [ -f $userserverrc ]; then
    defaultserverargs=$userserverrc
elif [ -f $sysserverrc ]; then
    defaultserverargs=$sysserverrc
fi

whoseargs="client"
while [ x"$1" != x ]; do
    case "$1" in
    # '' required to prevent cpp from treating "/*" as a C comment.
    /''*|\./''*)
	if [ "$whoseargs" = "client" ]; then
	    if [ x"$clientargs" = x ]; then
		client="$1"
	    else
		clientargs="$clientargs $1"
	    fi
	else
	    if [ x"$serverargs" = x ]; then
		server="$1"
	    else
		serverargs="$serverargs $1"
	    fi
	fi
	;;
    --)
	whoseargs="server"
	;;
    *)
	if [ "$whoseargs" = "client" ]; then
	    clientargs="$clientargs $1"
	else
	    # display must be the FIRST server argument
	    if [ x"$serverargs" = x ] && \
		 expr "$1" : ':[0-9][0-9]*$' > /dev/null 2>&1; then
		display="$1"
	    else
		serverargs="$serverargs $1"
	    fi
	fi
	;;
    esac
    shift
done

XCOMM process client arguments
if [ x"$client" = x ]; then
    # if no client arguments either, use rc file instead
    if [ x"$clientargs" = x ]; then
	client="$defaultclientargs"
    else
	client=$defaultclient
    fi
fi

XCOMM process server arguments
if [ x"$server" = x ]; then
    # if no server arguments or display either, use rc file instead
    if [ x"$serverargs" = x -a x"$display" = x ]; then
	server="$defaultserverargs"
    else
	server=$defaultserver
    fi
fi

if [ x"$XAUTHORITY" = x ]; then
    XAUTHORITY=$HOME/.Xauthority
    export XAUTHORITY
fi

removelist=

#if defined(HAS_COOKIE_MAKER) && defined(MK_COOKIE)
XCOMM set up default Xauth info for this machine
case `uname` in
Linux*)
	if [ -z "`hostname --version 2>&1 | grep GNU`" ]; then
		hostname=`hostname -f`
	else
		hostname=`hostname`
	fi
	;;
*)
	hostname=`hostname`
	;;
esac

authdisplay=${display:-:0}
mcookie=`MK_COOKIE`
for displayname in $authdisplay $hostname$authdisplay; do
    if ! xauth list "$displayname" | grep "$displayname " >/dev/null 2>&1; then
        xauth << EOF 
add $displayname . $mcookie
EOF
	removelist="$displayname $removelist"
    fi
done
#endif

xinit $client $clientargs -- $server $display $serverargs

if [ x"$removelist" != x ]; then
    xauth remove $removelist
fi

/*
 * various machines need special cleaning up
 */
#ifdef __linux__
if command -v deallocvt > /dev/null 2>&1; then
    deallocvt
fi
#endif

#ifdef macII
Xrepair
screenrestore
#endif

#if defined(sun) && !defined(i386)
kbd_mode -a
#endif
