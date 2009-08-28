NAME = IO-Stty
VERSION = .02

include ../Makefile.inc

custompatching:
	sed "s/@@@VERSION@@@/$$VERSIONER_PERL_VERSION/" $(MYSRC)/stty.pl.ed | ed - $(DIR)/stty.pl
