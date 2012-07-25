NAME = NetAddr-IP
VERSION = 4.055

include ../Makefile.inc

custompatching:
	ed - $(DIR)/Lite/Util/Util.xs < $(MYSRC)/Util.xs.ed

patchafterconfigure:
	ed - $(DIR)/Lite/Util/config.h < $(MYSRC)/config.h.ed
