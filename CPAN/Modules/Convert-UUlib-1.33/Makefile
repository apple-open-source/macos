NAME = Convert-UUlib
VERSION = 1.33

include ../Makefile.inc

custompatching:
	ed - $(DIR)/Makefile.PL < $(MYSRC)/Makefile.PL.ed
	ed - $(DIR)/uulib/uuencode.c < $(MYSRC)/uulib_uuencode.c.ed

postinstall:
	rm -rf $(installarchlib)/Convert/UUlib
