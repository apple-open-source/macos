#	$Id: HP-UX.10.x,v 1.1.1.1 2000/06/10 00:40:47 wsanchez Exp $
define(`confCC', `cc -Aa')
define(`confMAPDEF', `-DNDBM -DNIS -DMAP_REGEX')
define(`confENVDEF', `-D_HPUX_SOURCE -DV4FS')
define(`confOPTIMIZE', `+O3')
define(`confLIBS', `-lndbm')
define(`confSHELL', `/usr/bin/sh')
define(`confINSTALL', `${BUILDBIN}/install.sh')
define(`confSBINGRP', `mail')
