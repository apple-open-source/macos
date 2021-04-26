NAME = Net-SSLeay
VERSION = 1.72
PROJARGS = --

include ../Makefile.inc

custompatching:
	ed - $(DIR)/inc/Module/Install/PRIVATE/Net/SSLeay.pm < $(MYSRC)/Net_SSLeay.pm.ed
	ed - $(DIR)/Makefile.PL < $(MYSRC)/Makefile.PL.ed
	ed - $(DIR)/SSLeay.xs < $(MYSRC)/SSLeay.xs.ed
	patch -d "$(DIR)" -F0 -p0 -i "$(MYSRC)/PR-42682363.diff"
	sed -e 's#@SDKROOT@#$(SDKROOT)#g' "$(MYSRC)/net-ssleay-use-sdk.patch" | patch -d "$(DIR)" -p1
