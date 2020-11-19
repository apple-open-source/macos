NAME = YAML-Syck
VERSION = 1.32

include ../Makefile.inc

custompatching:
	ed - $(DIR)/gram.c < $(MYSRC)/gram.c.ed
	ed - $(DIR)/syck.h < $(MYSRC)/syck.h.ed
	patch -d "$(DIR)" -F0 -p0 -i "$(MYSRC)/Makefile.patch"
	patch -d "$(DIR)" -p1 -i "$(MYSRC)/include.patch"
