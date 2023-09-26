NAME = XML-LibXSLT
VERSION = 1.99

include ../Makefile.inc

custompatching:
	ed - $(DIR)/Makefile.PL < $(MYSRC)/Makefile.PL.ed
	patch -d $(DIR) -p1 -i $(MYSRC)/xml-libxslt-use-sdkroot.patch
	patch -d $(DIR) -p1 -i $(MYSRC)/xml-libxslt-fix-check.patch
