XCOMM!SHELL_CMD

userresources=$HOME/.Xresources
usermodmap=$HOME/.Xmodmap
sysresources=XINITDIR/.Xresources
sysmodmap=XINITDIR/.Xmodmap

XCOMM merge in defaults and keymaps

if [ -f $sysresources ]; then
#ifdef __APPLE__
    if [ -x /usr/bin/cpp ] ; then
        XRDB -merge $sysresources
    else
        XRDB -nocpp -merge $sysresources
    fi
#else
    XRDB -merge $sysresources
#endif
fi

if [ -f $sysmodmap ]; then
    XMODMAP $sysmodmap
fi

if [ -f "$userresources" ]; then
#ifdef __APPLE__
    if [ -x /usr/bin/cpp ] ; then
        XRDB -merge "$userresources"
    else
        XRDB -nocpp -merge "$userresources"
    fi
#else
    XRDB -merge "$userresources"
#endif
fi

if [ -f "$usermodmap" ]; then
    XMODMAP "$usermodmap"
fi

XCOMM start some nice programs

#if defined(__SCO__) || defined(__UNIXWARE__)
if [ -r /etc/default/xdesktops ]; then
  . /etc/default/xdesktops
fi

if [ -r $HOME/.x11rc ]; then
  . $HOME/.x11rc
else
  if [ -r /etc/default/X11 ]; then
  . /etc/default/X11
  fi
fi

#if defined(__SCO__)
if [ -n "$XSESSION" ]; then
  case "$XSESSION" in
    [Yy][Ee][Ss])
      [ -x /usr/bin/X11/scosession ] && exec /usr/bin/X11/scosession
      ;;
  esac
fi

if [ -n "$XDESKTOP" ]; then
  exec `eval echo $"$XDESKTOP"`
else
  if [ -x /usr/bin/X11/pmwm -a -x /usr/bin/X11/scoterm ]; then
    /usr/bin/X11/scoterm 2> /dev/null &
    exec /usr/bin/X11/pmwm    2> /dev/null
  fi
fi
#elif defined(__UNIXWARE__)
if [ -n "$XDESKTOP" ]; then
  exec `eval echo $"$XDESKTOP"`
else
  if [ -x /usr/X/bin/pmwm ]; then
    exec /usr/X/bin/pmwm    2> /dev/null
  fi
fi
#endif

XCOMM This is the fallback case if nothing else is executed above
#endif /* !defined(__SCO__)  && !defined(__UNIXWARE__) */

#ifdef __APPLE__

if [ -d XINITDIR/xinitrc.d ] ; then
	for f in XINITDIR/xinitrc.dXSLASHGLOB.sh ; do
		[ -x "$f" ] && . "$f"
	done
	unset f
fi

#endif

TWM &
XCLOCK -geometry 50x50-1+1 &
XTERM -geometry 80x50+494+51 &
XTERM -geometry 80x20+494-0 &
exec XTERM -geometry 80x66+0+0 -name login
