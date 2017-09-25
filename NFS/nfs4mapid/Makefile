#
# Makefile for nf4mapid
#

Project = nfs4mapid

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
		-Wno-missing-field-initializers -Wno-shadow
Extra_CC_Flags = $(WARNING_FLAGS) -D_FORTIFY_SOURCE=2 -fPIE $(CC_COVERAGE_FLAGS)
Extra_LD_Flags = -Wl,-pie -dead_strip $(LD_COVERAGE_FLAGS)

ifeq "$(RC_TARGET_CONFIG)" "iPhone"
Extra_CC_Flags += -isysroot $(SDKROOT)
endif

DST_DIR = $(DSTROOT)/usr/sbin
MAN_DIR = $(DSTROOT)/usr/share/man/man8
LOCAL_DST_DIR = $(DSTROOT)/usr/local/bin
DSYM = $(Project).dSYM

#
# Standard B&I targets
#
all: $(SYMROOT)/$(DSYM) $(OBJROOT)/nfs4mapid-tester

install:: all
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 555 -s $(SYMROOT)/nfs4mapid $(DST_DIR)
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/nfs4mapid.8 $(MAN_DIR)
	install -d -o root -g wheel -m 755 $(LOCAL_DST_DIR)
	install -c -o root -g wheel -m 555 $(OBJROOT)/nfs4mapid-tester $(LOCAL_DST_DIR)
clean::
	-rm -rf *.o nfs4mapid $(DSYM) nfs4mapid-tester

#
# Build
#
CFILES = nfs4mapid.c
OFILES = $(CFILES:.c=.o)

$(OFILES): $(HFILES)

$(OBJROOT)/nfs4mapid-tester: $(SRCROOT)/nfs4mapid-tester.sh
		 cat $< > $@
		 chmod a+x $@

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(SYMROOT)/nfs4mapid: $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(SYMROOT)/$(DSYM): $(SYMROOT)/nfs4mapid
	$(DSYMUTIL) -o $@ $<
