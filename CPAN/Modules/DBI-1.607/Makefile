NAME = DBI
VERSION = 1.607

include ../Makefile.inc

custompatching:
	ed - $(DIR)/DBIXS.h < $(MYSRC)/DBIXS.h.ed
	ed - $(DIR)/DBI.xs < $(MYSRC)/DBI.xs.ed
