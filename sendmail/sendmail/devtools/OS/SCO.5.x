#	$Id: SCO.5.x,v 1.1.1.1 2000/06/10 00:40:47 wsanchez Exp $
define(`confCC', `cc -b elf')
define(`confLIBS', `-lsocket -lndbm -lprot -lcurses -lm -lx -lgen')
define(`confMAPDEF', `-DMAP_REGEX -DNDBM')
define(`confSBINGRP', `bin')
define(`confMBINDIR', `/usr/lib')
define(`confSBINDIR', `/usr/etc')
define(`confUBINDIR', `/usr/bin')
define(`confINSTALL', `${BUILDBIN}/install.sh')
