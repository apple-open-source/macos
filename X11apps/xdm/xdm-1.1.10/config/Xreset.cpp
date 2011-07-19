XCOMM!/bin/sh
XCOMM Deregister a login. (Derived from TakeConsole as follows:)
XCOMM
BINDIR/sessreg -d -w WTMP_FILE -u UTMP_FILE \
	-x XDMCONFIGDIR/Xservers -l $DISPLAY -h "" $USER
