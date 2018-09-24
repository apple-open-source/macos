#
# Makefile for nfsd
#

Project = nfsd

CCOVERAGEDIR ?= /tmp/cc/nfs

inplace:
	@$(MAKE) all SRCROOT=. OBJROOT=. SYMROOT=. DSTROOT=/

coverage:
	@$(MAKE) inplace CC_COVERAGE_FLAGS="-fprofile-instr-generate -fcoverage-mapping -fprofile-generate=$(CCOVERAGEDIR)/$(Project).%p -O0" LD_COVERAGE_FLAGS="-fprofile-instr-generate"

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

ifndef SDKROOT
SDKROOT := $(shell xcrun --sdk macosx.internal --show-sdk-path)
endif
LOCKDROOT = $(SRCROOT)/../rpc.lockd

WARNING_FLAGS = -Wall -Wextra -Wformat=2 -Wpointer-arith -Wcast-align \
		-Wwrite-strings -Wno-unknown-pragmas \
		-Wno-missing-field-initializers -Wno-shadow
Extra_CC_Flags = $(WARNING_FLAGS) -D_FORTIFY_SOURCE=2 \
		 -F$(SDKROOT)/System/Library/PrivateFrameworks -I $(SRCROOT) -I $(LOCKDROOT) -I $(OBJROOT) -fPIE $(CC_COVERAGE_FLAGS)
Extra_LD_Flags = -Wl,-pie -lutil -framework oncrpc -framework DiskArbitration \
		  -framework ServiceManagement -framework IOKit -framework CoreFoundation $(LD_COVERAGE_FLAGS)

ifeq "$(RC_TARGET_CONFIG)" "iPhone"
Extra_CC_Flags += -isysroot $(SDKROOT)
endif

DST_DIR = $(DSTROOT)/sbin
LAUNCHD_DIR = $(DSTROOT)/System/Library/LaunchDaemons
MAN5_DIR = $(DSTROOT)/usr/share/man/man5
MAN8_DIR = $(DSTROOT)/usr/share/man/man8
DSYM = $(Project).dSYM

#
# Standard B&I targets
#
all: $(SYMROOT)/$(DSYM)

install:: all
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 555 -s $(SYMROOT)/nfsd $(DST_DIR)
	install -d -o root -g wheel -m 755 $(LAUNCHD_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/com.apple.nfsd.plist $(LAUNCHD_DIR)
	install -d -o root -g wheel -m 755 $(MAN8_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/nfsd.8 $(MAN8_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/mountd.8 $(MAN8_DIR)
	install -d -o root -g wheel -m 755 $(MAN5_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/exports.5 $(MAN5_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/netgroup.5 $(MAN5_DIR)

clean::
	-rm -rf *.o nfsd $(MIG_GEN_FILES) $(DSYM)

#
# Build
#
MIG_DEF_FILES = $(SRCROOT)/../rpc.lockd/lockd_mach.defs
MIG_GEN_CFILES = lockd_machUser.c
MIG_GEN_HFILES = lockd_mach.h
MIG_GEN_FILES = $(MIG_GEN_CFILES) $(MIG_GEN_HFILES)
MIG_OUT_FILES = $(MIG_GEN_FILES:%=$(OBJROOT)/%)
HFILES = common.h pathnames.h
CFILES = main.c nfsd.c mountd.c
OFILES = $(CFILES:.c=.o) $(MIG_GEN_CFILES:.c=.o)

CODE_SIGN_IDENTITY = "-"
CODE_SIGN_ENTITLEMENTS = nfsd-entitlements.plist

$(OFILES:%=$(OBJROOT)/%): $(HFILES:%=$(SRCROOT)/%) $(MIG_GEN_HFILES:%=$(OBJROOT)/%)
$(OFILES): $(HFILES) $(MIG_GEN_HFILES)

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(MIG_OUT_FILES): $(MIG_DEF_FILES)
	mig \
		-user    $(OBJROOT)/lockd_machUser.c \
		-header  $(OBJROOT)/lockd_mach.h \
		-server  /dev/null \
		$(MIG_DEF_FILES)

$(SYMROOT)/nfsd: $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
	xcrun codesign --force --sign $(CODE_SIGN_IDENTITY) \
	    --entitlements $(CODE_SIGN_ENTITLEMENTS) \
	    --timestamp=none $@

$(SYMROOT)/$(DSYM): $(SYMROOT)/nfsd
	$(DSYMUTIL) -o $@ $<
