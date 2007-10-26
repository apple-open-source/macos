OBJ_FILES = SetActive.o PMSettings.o PrivateLib.o AutoWakeScheduler.o \
	RepeatingAutoWake.o BatteryTimeRemaining.o PSLowPower.o  pmconfigd.o \
	powermanagementServer.o IOUPSPrivate.o ioupspluginUser.o TTYKeepAwake.o
H_FILES = PrivateLib.h AutoWakeScheduler.h PMSettings.h RepeatingAutoWake.h \
	BatteryTimeRemaining.h PSLowPower.h SetActive.h
MIG_PRODUCTS = powermanagement.h powermanagementServer.c powermanagementServer.h \
	powermanagementUser.c ioupspluginUser.c ioupsplugin.h ioupspluginServer.c

PROJ_ROOT = ..
UPS_DIR = $(PROJ_ROOT)/ioupsd

CFLAGS = -g -DSTANDALONE -Wall -arch i386 -arch ppc
FRAMEWORKS = -framework IOKit -framework CoreFoundation -framework SystemConfiguration

all: pmconfigd

pmconfigd: $(OBJ_FILES) $(H_FILES)
	cc -o pm $(CFLAGS) $(OBJ_FILES) $(FRAMEWORKS)

loud: $(OBJ_FILES) $(H_FILES)
	cc -o pm -Wall $(CFLAGS) $(OBJ_FILES) $(FRAMEWORKS)

powermanagement.h powermanagementServer.c: powermanagement.defs
	mig powermanagement.defs
	
powermanagementServer.h: powermanagement.defs
	mig -sheader powermanagementServer.h powermanagement.defs

pmconfigd.o : powermanagementServer.h

SetActive.c: powermanagementServer.h

powermanagementServer.o: powermanagementServer.c powermanagementServer.h

IOUPSPrivate.o: $(UPS_DIR)/IOUPSPrivate.c
	cc $(CFLAGS) -c $(UPS_DIR)/IOUPSPrivate.c
	
ioupspluginUser.o: ioupspluginUser.c

ioupspluginUser.c: $(UPS_DIR)/ioupsplugin.defs
	mig $(UPS_DIR)/ioupsplugin.defs

clean:
	rm pm $(OBJ_FILES) $(MIG_PRODUCTS)

