NAME = IO-Tty
VERSION = 1.14

include ../Makefile.inc

patchafterconfigure:
	ed - $(DIR)/Makefile < $(MYSRC)/Makefile.ed

custompatching:
	sed -e 's#@SDKROOT@#$(SDKROOT)#g' "$(MYSRC)/io-tty-use-sdkroot.patch" | patch -d "$(DIR)" -p1
	patch -d $(DIR) -p1 -i $(MYSRC)/io-tty-util-header.patch
