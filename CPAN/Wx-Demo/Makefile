NAME = Wx-Demo
VERSION = 0.07

include ../Makefile.inc

custompatching:
	ed - $(DIR)/bin/wxperl_demo.pl < $(SRCROOT)/$(NAME)/wxperl_demo.pl.ed
	ed - $(DIR)/Makefile.PL < $(SRCROOT)/$(NAME)/Makefile.PL.ed
