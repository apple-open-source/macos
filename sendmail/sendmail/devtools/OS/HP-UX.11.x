#	$Id: HP-UX.11.x,v 1.1.1.2 2002/03/12 18:00:01 zarzycki Exp $

# +z is to generate position independant code
define(`confCClibsmi', `cc -Ae +Z')
define(`confCC', `cc -Ae')
define(`confMAPDEF', `-DNDBM -DNIS -DMAP_REGEX')
define(`confENVDEF', `-DV4FS -DHPUX11')
define(`confSM_OS_HEADER', `sm_os_hp')
define(`confOPTIMIZE', `+O2')
define(`confLIBS', `-ldbm -lnsl')
define(`confSHELL', `/usr/bin/sh')
define(`confINSTALL', `${BUILDBIN}/install.sh')
define(`confSBINGRP', `mail')
define(`confEBINDIR', `/usr/sbin')

define(`confMTCCOPTS', `-D_POSIX_C_SOURCE=199506L +z')
define(`confMTLDOPTS', `-lpthread')
define(`confLD', `ld')
define(`confLDOPTS_SO', `-b')
define(`confCCOPTS_SO', `')
