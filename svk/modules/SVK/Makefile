NAME = SVK
VERSION = v2.0.1

include ../Makefile.inc

custompatching:
	(cd $(DIR) && patch -p0) < disable-log4perl.diff
	(cd $(DIR) && patch -p0) < munge-inc.diff
	(cd $(DIR) && patch -p0) < munge-inc-Makefile.diff
