NAME = GSSAPI
VERSION = 0.28

include ../Makefile.inc

custompatching:
	ed - $(DIR)/Makefile.PL < $(MYSRC)/Makefile.PL.ed
	sed -e 's#@SDKROOT@#$(SDKROOT)#g' "$(MYSRC)/gssapi-use-sdk.patch" | patch -d "$(DIR)" -p1
