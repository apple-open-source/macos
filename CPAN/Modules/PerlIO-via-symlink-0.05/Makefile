NAME = PerlIO-via-symlink
VERSION = 0.05

include ../Makefile.inc

custompatching:
	ed - $(DIR)/inc/Module/Install/AutoInstall.pm < $(MYSRC)/inc_Module_Install_AutoInstall.pm.ed
