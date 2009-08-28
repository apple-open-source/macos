NAME = Convert-UUlib
VERSION = 1.12

include ../Makefile.inc

custompatching:
	ed - $(DIR)/Makefile.PL < $(MYSRC)/Makefile.PL.ed

postinstall:
	rm -rf $(installarchlib)/Convert/UUlib
