XCOMM!/bin/sh
XCOMM
XCOMM $XFree86: xc/programs/Xserver/hw/xfree86/sdk/mkmf.cpp,v 1.1 1999/08/14 10:50:09 dawes Exp $

XCOMM
XCOMM Build Makefile for Driver SDK
XCOMM

if [ ! -x ./mkmf ]; then
    echo "mkmf cannot be executed from this directory"
    exit 1
fi

if [ -d ./config ]; then
    CONFIG_DIR_SPEC=-I./config/cf
    IMAKE_COMMAND=./config/imake/imake
elif [ x"$XWINHOME" != x ]; then
    CONFIG_DIR_SPEC=-I$XWINHOME/lib/X11/config
    IMAKE_COMMAND="imake -DUseInstalled"
else
    CONFIG_DIR_SPEC=CONFIGDIRSPEC
    IMAKE_COMMAND="imake -DUseInstalled"
fi

if [ -f Makefile ]; then
  (set -x
    rm -f Makefile.bak
    mv Makefile Makefile.bak
  )
fi
rm -f Makefile
(set -x
  $IMAKE_COMMAND -I. $CONFIG_DIR_SPEC -DXF86DriverSDK=1 -DTOPDIR=. -DCURDIR=.
  make Makefiles
XCOMM make clean
  make depend
)

