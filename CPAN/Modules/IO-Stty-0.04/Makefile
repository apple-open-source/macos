NAME = IO-Stty
VERSION = 0.04

include ../Makefile.inc

custompatching:
	sed "s/@@@VERSION@@@/$$VERSIONER_PERL_VERSION/" $(MYSRC)/stty.pl.ed | ed - $(DIR)/scripts/stty.pl
