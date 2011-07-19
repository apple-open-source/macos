NAME = NetAddr-IP
VERSION = 4.037

include ../Makefile.inc

custompatching:
	ed - $(DIR)/Lite/Util/Util.xs < $(MYSRC)/Util.xs.ed
	ed - $(DIR)/Lite/Util/xs_include/miniSocket.inc < $(MYSRC)/miniSocket.inc.ed

patchafterconfigure:
	ed - $(DIR)/Lite/Util/config.h < $(MYSRC)/config.h.ed
