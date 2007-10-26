XCOMM!/bin/sh
XCOMM Register a login (derived from GiveConsole as follows:)
XCOMM
BINDIR/sessreg  -a -w "/var/log/wtmp" -u "/var/run/utmp" \
	-x "XDMCONFIGDIR/Xservers" -l $DISPLAY -h "" $USER
