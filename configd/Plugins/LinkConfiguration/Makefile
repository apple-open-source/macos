SDK=$(PLATFORM).internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) clang
PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks
SCPRIV=-DUSE_SYSTEMCONFIGURATION_PRIVATE_HEADERS

SYSPRIV=-I$(SYSROOT)/System/Library/Frameworks/System.framework/PrivateHeaders
SC_INCLUDE=-I../common -I../../SystemConfiguration.fproj -I../../IPMonitorControl

linkconfig: linkconfig.c ../common/WiFiUtil.m
	$(CC) -DTEST_LINKCONFIG $(SC_INCLUDE) -isysroot $(SYSROOT) $(ARCH_FLAGS) $(SYSPRIV) $(PF_INC) -framework CoreFoundation -framework SystemConfiguration -framework CoreWiFi -Wall -g -o $@ $^

clean:
	rm -rf *.o linkconfig *.dSYM

