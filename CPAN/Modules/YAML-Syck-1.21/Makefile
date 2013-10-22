NAME = YAML-Syck
VERSION = 1.21

include ../Makefile.inc

custompatching:
	ed - $(DIR)/gram.c < $(MYSRC)/gram.c.ed
	ed - $(DIR)/syck.h < $(MYSRC)/syck.h.ed
