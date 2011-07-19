XCOMM!/bin/sh
XCOMM Register a login (derived from GiveConsole as follows:)
XCOMM
exec BINDIR/sessreg  -a -w WTMP_FILE -u UTMP_FILE \
	-x XDMCONFIGDIR/Xservers -l $DISPLAY -h "" $USER
