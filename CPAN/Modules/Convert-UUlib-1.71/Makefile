NAME = Convert-UUlib
VERSION = 1.71

include ../Makefile.inc

custompatching:
	ed - $(DIR)/Makefile.PL < $(MYSRC)/Makefile.PL.ed
	ed - $(DIR)/uulib/uuencode.c < $(MYSRC)/uulib_uuencode.c.ed
	patch -d "$(DIR)" -p1 -i "$(MYSRC)/convert-uulib-fix-configure.patch"
