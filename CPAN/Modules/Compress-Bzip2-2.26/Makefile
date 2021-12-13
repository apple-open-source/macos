NAME = Compress-Bzip2
VERSION = 2.26

include ../Makefile.inc

custompatching:
	sed -e 's#@SDKROOT@#$(SDKROOT)#g' "$(MYSRC)/compress-bzip2-use-sdk.patch" | patch -d "$(DIR)" -p1
