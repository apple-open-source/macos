NAME = Convert-UUlib
VERSION = 1.06

include ../Makefile.inc

custompatching:
	ed - $(DIR)/Makefile.PL < $(SRCROOT)/$(NAME)/Makefile.PL.ed

postinstall:
	rm -rf $(installarchlib)/Convert/UUlib
