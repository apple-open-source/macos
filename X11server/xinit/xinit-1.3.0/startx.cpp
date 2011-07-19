XCOMM!SHELL_CMD

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

unset DBUS_SESSION_BUS_ADDRESS
unset SESSION_MANAGER

#if defined(__SCO__) || defined(__UNIXWARE__) || defined(__APPLE__)

XCOMM Check for /usr/bin/X11 and BINDIR in the path, if not add them.
XCOMM This allows startx to be placed in a place like /usr/bin or /usr/local/bin
XCOMM and people may use X without changing their PATH.
XCOMM Note that we put our own bin directory at the front of the path, and
XCOMM the standard system path at the back, since if you are using the Xorg
XCOMM server theres a pretty good chance you want to bias the Xorg clients
XCOMM over the old system's clients.

XCOMM First our compiled path
bindir=__bindir__

case $PATH in
    *:$bindir | *:$bindir:* | $bindir:*) ;;
    *) PATH=$bindir:$PATH ;;
esac

XCOMM Now the "old" compiled path
#ifdef __APPLE__
oldbindir=/usr/X11R6/bin
#else
oldbindir=/usr/bin/X11
#endif

if [ -d "$oldbindir" ] ; then
    case $PATH in
        *:$oldbindir | *:$oldbindir:* | $oldbindir:*) ;;
        *) PATH=$PATH:$oldbindir ;;
    esac
fi

XCOMM Bourne shell does not automatically export modified environment variables
XCOMM so export the new PATH just in case the user changes the shell
export PATH
#endif

#if defined(__SCO__) || defined(__UNIXWARE__)
XCOMM Set up the XMERGE env var so that dos merge is happy under X

if [ -f /usr/lib/merge/xmergeset.sh ]; then
	. /usr/lib/merge/xmergeset.sh
elif [ -f /usr/lib/merge/console.disp ]; then
	XMERGE=`cat /usr/lib/merge/console.disp`
	export XMERGE
fi

userclientrc=$HOME/.startxrc
sysclientrc=LIBDIR/sys.startxrc
scouserclientrc=$HOME/.xinitrc
scosysclientrc=XINITDIR/xinitrc
#else
userclientrc=$HOME/.xinitrc
sysclientrc=XINITDIR/xinitrc
#endif

userserverrc=$HOME/.xserverrc
sysserverrc=XINITDIR/xserverrc
defaultclient=XTERM
defaultserver=XSERVER
defaultclientargs=""
defaultserverargs=""
defaultdisplay=":0"
clientargs=""
serverargs=""

#ifdef __APPLE__

if [ "x$X11_PREFS_DOMAIN" = x ] ; then
    export X11_PREFS_DOMAIN=LAUNCHD_ID_PREFIX".X11"
fi

XCOMM Initialize defaults (this will cut down on "safe" error messages)
if ! defaults read $X11_PREFS_DOMAIN cache_fonts > /dev/null 2>&1 ; then
    defaults write $X11_PREFS_DOMAIN cache_fonts -bool true
fi

if ! defaults read $X11_PREFS_DOMAIN no_auth > /dev/null 2>&1 ; then
    defaults write $X11_PREFS_DOMAIN no_auth -bool false
fi

if ! defaults read $X11_PREFS_DOMAIN nolisten_tcp > /dev/null 2>&1 ; then
    defaults write $X11_PREFS_DOMAIN nolisten_tcp -bool true
fi

XCOMM First, start caching fonts
if [ x`defaults read $X11_PREFS_DOMAIN cache_fonts` = x1 ] ; then
    if [ -x $bindir/font_cache ] ; then
        $bindir/font_cache &
    elif [ -x $bindir/font_cache.sh ] ; then
        $bindir/font_cache.sh &
    elif [ -x $bindir/fc-cache ] ; then
        $bindir/fc-cache &
    fi
fi

if [ -x XINITDIR/privileged_startx ] ; then
	# Don't push this into the background becasue it can cause
	# a race to create /tmp/.X11-unix
	XINITDIR/privileged_startx
fi

if [ x`defaults read $X11_PREFS_DOMAIN no_auth` = x0 ] ; then
    enable_xauth=1
else
    enable_xauth=0
fi

if [ x`defaults read $X11_PREFS_DOMAIN nolisten_tcp` = x1 ] ; then
    defaultserverargs="$defaultserverargs -nolisten tcp"
fi

if defaults read $X11_PREFS_DOMAIN dpi > /dev/null 2>&1 ; then
    defaultserverargs="$defaultserverargs -dpi `defaults read $X11_PREFS_DOMAIN dpi`"
fi

d=0
while true ; do
    [ -e /tmp/.X$d-lock ] || break
    d=$(($d + 1))
done
defaultdisplay=":$d"
unset d

#else
enable_xauth=1
#endif

#if defined(__SCO__) || defined(__UNIXWARE__)

XCOMM SCO -t option: do not start an X server
case $1 in
  -t)   if [ -n "$DISPLAY" ]; then
                REMOTE_SERVER=TRUE
                shift
        else
                echo "DISPLAY environment variable not set"
                exit 1
        fi
        ;;
esac
#endif

whoseargs="client"
while [ x"$1" != x ]; do
    case "$1" in
    XCOMM '' required to prevent cpp from treating "/*" as a C comment.
    /''*|\./''*)
	if [ "$whoseargs" = "client" ]; then
	    if [ x"$client" = x ] && [ x"$clientargs" = x ]; then
		client="$1"
	    else
		clientargs="$clientargs $1"
	    fi
	else
	    if [ x"$server" = x ] && [ x"$serverargs" = x ]; then
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
	    XCOMM display must be the FIRST server argument
	    if [ x"$serverargs" = x ] && @@
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
    client=$defaultclient

    XCOMM if no client arguments either, use rc file instead
    if [ x"$clientargs" = x ]; then
        if [ -f "$userclientrc" ]; then
            client=$userclientrc
        elif [ -f "$sysclientrc" ]; then
            client=$sysclientrc
#if defined(__SCO__) || defined(__UNIXWARE__)
        elif [ -f "$scouserclientrc" ]; then
            client=$scouserclientrc
        elif [ -f "$scosysclientrc" ]; then
            client=$scosysclientrc
#endif
        fi

        clientargs=$defaultclientargs
    fi
fi

XCOMM process server arguments
if [ x"$server" = x ]; then
    server=$defaultserver

    XCOMM if no server arguments or display either, use defaults
    if [ x"$serverargs" = x -a x"$display" = x ]; then
	XCOMM For compatibility reasons, only use xserverrc if there were no server command line arguments
	if [ -f "$userserverrc" ]; then
	    server=$userserverrc
	elif [ -f "$sysserverrc" ]; then
	    server=$sysserverrc
	fi

	serverargs=$defaultserverargs
	display=$defaultdisplay
    fi
fi

if [ x"$enable_xauth" = x1 ] ; then
    if [ x"$XAUTHORITY" = x ]; then
        XAUTHORITY=$HOME/.Xauthority
        export XAUTHORITY
    fi

    removelist=

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
#if defined(HAS_COOKIE_MAKER) && defined(MK_COOKIE)
    mcookie=`MK_COOKIE`
#else
    if [ -r /dev/urandom ]; then
        mcookie=`dd if=/dev/urandom bs=16 count=1 2>/dev/null | hexdump -e \\"%08x\\"`
    else
        mcookie=`dd if=/dev/random bs=16 count=1 2>/dev/null | hexdump -e \\"%08x\\"`
    fi
#endif
    if test x"$mcookie" = x; then
        echo "Couldn't create cookie"
        exit 1
    fi
    dummy=0

    XCOMM create a file with auth information for the server. ':0' is a dummy.
    xserverauthfile=$HOME/.serverauth.$$
    trap "rm -f '$xserverauthfile'" HUP INT QUIT ILL TRAP KILL BUS TERM
    xauth -q -f "$xserverauthfile" << EOF
add :$dummy . $mcookie
EOF
#if defined(__APPLE__) || defined(__CYGWIN__)
    xserverauthfilequoted=$(echo ${xserverauthfile} | sed "s/'/'\\\\''/g")
    serverargs=${serverargs}" -auth '"${xserverauthfilequoted}"'"
#else
    serverargs=${serverargs}" -auth "${xserverauthfile}
#endif

    XCOMM now add the same credentials to the client authority file
    XCOMM if '$displayname' already exists do not overwrite it as another
    XCOMM server man need it. Add them to the '$xserverauthfile' instead.
    for displayname in $authdisplay $hostname$authdisplay; do
        authcookie=`XAUTH list "$displayname" @@
        | sed -n "s/.*$displayname[[:space:]*].*[[:space:]*]//p"` 2>/dev/null;
        if [ "z${authcookie}" = "z" ] ; then
            XAUTH -q << EOF 
add $displayname . $mcookie
EOF
        removelist="$displayname $removelist"
        else
            dummy=$(($dummy+1));
            XAUTH -q -f "$xserverauthfile" << EOF
add :$dummy . $authcookie
EOF
        fi
    done
fi

#if defined(__SCO__) || defined(__UNIXWARE__)
if [ "$REMOTE_SERVER" = "TRUE" ]; then
        exec SHELL_CMD ${client}
else
        XINIT "$client" $clientargs -- "$server" $display $serverargs
fi
#else

#if defined(__APPLE__) || defined(__CYGWIN__)
eval XINIT \"$client\" $clientargs -- \"$server\" $display $serverargs
#else
XINIT "$client" $clientargs -- "$server" $display $serverargs
#endif

#endif
retval=$?

if [ x"$enable_xauth" = x1 ] ; then
    if [ x"$removelist" != x ]; then
        XAUTH remove $removelist
    fi
    if [ x"$xserverauthfile" != x ]; then
        rm -f "$xserverauthfile"
    fi
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

#if defined(sun)
kbd_mode -a
#endif

exit $retval

