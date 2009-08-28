XCOMM!/bin/sh
XCOMM Deregister a login. (Derived from TakeConsole as follows:)
XCOMM
BINDIR/sessreg -d -w "/var/log/wtmp" -u "/var/run/utmp" \
	-x "XDMCONFIGDIR/Xservers" -l $DISPLAY -h "" $USER
