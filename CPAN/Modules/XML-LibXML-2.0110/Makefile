NAME = XML-LibXML
VERSION = 2.0110

include ../Makefile.inc

custompatching:
	ed - $(DIR)/Makefile.PL < $(MYSRC)/Makefile.PL.ed
	patch -d "$(DIR)" -p1 -i "$(MYSRC)/xml-libxml-fix-prototypes.patch"
