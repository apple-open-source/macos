NAME = IO-Tty
VERSION = 1.10

include ../Makefile.inc

custompatching:
	sed -e 's#@SDKROOT@#$(SDKROOT)#g' "$(MYSRC)/io-tty-use-sdk.patch" | patch -d "$(DIR)" -p1

patchafterconfigure:
	ed - $(DIR)/Makefile < $(MYSRC)/Makefile.ed
