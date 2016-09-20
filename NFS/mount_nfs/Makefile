#
# Makefile for mount_nfs
#

Project = mount_nfs

CCOVERAGEDIR ?= /tmp/cc/nfs

inplace:
	@$(MAKE) all SRCROOT=. OBJROOT=. SYMROOT=. DSTROOT=/

coverage:
	@$(MAKE) inplace CC_COVERAGE_FLAGS="-fprofile-instr-generate -fcoverage-mapping -fprofile-generate=$(CCOVERAGEDIR)/$(Project).%p -O0" LD_COVERAGE_FLAGS="-fprofile-instr-generate"

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

ifndef SDKROOT
SDKROOT := $(shell xcrun --sdk macosx.internal --show-sdk-path)
endif

WARNING_FLAGS = -Wall -Wextra -Wformat=2 -Wpointer-arith -Wcast-align \
		-Wwrite-strings -Wno-unknown-pragmas \
		-Wno-missing-field-initializers -Wno-shadow \
		-Wno-deprecated-declarations
Extra_CC_Flags = $(WARNING_FLAGS) -D_FORTIFY_SOURCE=2 -fPIE $(CC_COVERAGE_FLAGS)
Extra_LD_Flags = -Wl,-pie -lutil -dead_strip -framework CoreFoundation -framework IOKit $(LD_COVERAGE_FLAGS)

ifeq "$(RC_TARGET_CONFIG)" "iPhone"
Extra_CC_Flags += -isysroot $(SDKROOT)
endif

DST_DIR = $(DSTROOT)/sbin
ETC_DIR = $(DSTROOT)/private/etc
MAN_DIR = $(DSTROOT)/usr/share/man/man8
LAUNCHD_DIR = $(DSTROOT)/System/Library/LaunchDaemons
DSYM = $(Project).dSYM

#
# Standard B&I targets
#
all: $(SYMROOT)/$(DSYM)

install:: all
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 555 -s $(SYMROOT)/mount_nfs $(DST_DIR)
	install -d -o root -g wheel -m 755 $(DSTROOT)/usr
	install -d -o root -g wheel -m 755 $(DSTROOT)/usr/share
	install -d -o root -g wheel -m 755 $(DSTROOT)/usr/share/man
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/mount_nfs.8 $(MAN_DIR)
	install -d -o root -g wheel -m 755 $(LAUNCHD_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/com.apple.nfsconf.plist $(LAUNCHD_DIR)
	plutil  -convert binary1 $(LAUNCHD_DIR)/com.apple.nfsconf.plist
	install -d -o root -g wheel -m 755 $(ETC_DIR)
	install -c -o root -g wheel -m 644 $(SRCROOT)/nfs.conf $(ETC_DIR)

clean::
	-rm -rf *.o mount_nfs $(DSYM)

#
# Build
#
CFILES = mount_nfs.c
OFILES = $(CFILES:.c=.o)

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(SYMROOT)/mount_nfs: $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(SYMROOT)/$(DSYM): $(SYMROOT)/mount_nfs
	$(DSYMUTIL) -o $@ $<
