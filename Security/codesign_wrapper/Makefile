SRCS := codesign_wrapper.c MISEntitlement.c codesign.c # MISCrypto_OpenSSL.c
OBJS := $(SRCS:.c=.o)
CFLAGS := -Wall -Wmost -Werror -g -O2 -I. -std=c99 -DDEBUG_ASSERT_PRODUCTION_CODE=0
LDFLAGS := -framework CoreFoundation -framework Security
VERSION := 0.5.4

all: codesign_wrapper check_entitlements

codesign_wrapper: $(OBJS)

check_entitlements: check_entitlements.o

tags: $(SRCS)
	ctags $(SRCS)

clean:
	rm -f tags $(OBJS) codesign_wrapper

zip:
	(cd ..; zip -r codesign_wrapper-$(VERSION).zip codesign_wrapper -x \*/.git/\*)

sign:
	rm -r AccelerometerGraph.app
	cp -R ../codesign_wrapper-unused/AccelerometerGraph.app .
	cp ../codesign_wrapper-unused/Core_OS_IMG_v1_profile_052808.mobileprovision AccelerometerGraph.app/embedded.mobileprovision
	CODESIGN_ALLOCATE=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/codesign_allocate ./codesign_wrapper -s "iPhone Developer: Katherine Kojima" -f -vvvv -r'=designated => anchor apple generic' AccelerometerGraph.app

demosign:
	rm -r AccelerometerGraph.app
	cp -R ../codesign_wrapper-unused/AccelerometerGraph.app .
	CODESIGN_ALLOCATE=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/codesign_allocate ./codesign_wrapper --no-profile -s "iPhone Developer: Katherine Kojima" -f -vvvv -r'=designated => anchor apple generic' AccelerometerGraph.app
