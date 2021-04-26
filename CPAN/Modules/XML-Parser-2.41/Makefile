NAME = XML-Parser
VERSION = 2.41

include ../Makefile.inc

custompatching:
	sed -e 's#@SDKROOT@#$(SDKROOT)#g' "$(MYSRC)/xml-parser-use-sdk.patch" | patch -d "$(DIR)" -p1
