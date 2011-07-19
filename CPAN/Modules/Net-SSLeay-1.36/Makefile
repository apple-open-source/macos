NAME = Net-SSLeay
VERSION = 1.36
PROJARGS = --

include ../Makefile.inc

custompatching:
	ed - $(DIR)/inc/Module/Install/PRIVATE/Net/SSLeay.pm < $(MYSRC)/Net_SSLeay.pm.ed
	ed - $(DIR)/Makefile.PL < $(MYSRC)/Makefile.PL.ed
