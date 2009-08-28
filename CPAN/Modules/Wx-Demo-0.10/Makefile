NAME = Wx-Demo
VERSION = 0.10

include ../Makefile.inc

custompatching:
	ed - $(DIR)/bin/wxperl_demo.pl < $(MYSRC)/wxperl_demo.pl.ed
	ed - $(DIR)/Makefile.PL < $(MYSRC)/Makefile.PL.ed
	find $(DIR)/lib/Wx/DemoModules -type f | xargs chmod a-x
