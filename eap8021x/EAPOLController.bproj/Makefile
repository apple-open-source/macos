SDK=macosx.internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) cc
PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks
SCPRIV += -DUSE_SYSTEMCONFIGURATION_PRIVATE_HEADERS=1
ARCH=-arch arm64

auto_detect_test: controller.c
	$(CC) $(ARCH) $(SCPRIV) -isysroot $(SYSROOT) -Wall -DTEST_AUTO_DETECT_STATUS -o auto_detect_test controller.c -g -I../EAP8021X.fproj $(PF_INC) -framework EAP8021X -framework CoreFoundation -framework SystemConfiguration -F/System/Library/PrivateFrameworks

clean:
	rm -rf *.dSYM/
	rm -f auto_detect_test *~
