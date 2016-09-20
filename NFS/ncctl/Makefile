#
# Makefile for ncctl
#

Project = ncctl

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
Extra_CC_Flags = $(WARNING_FLAGS) -D_FORTIFY_SOURCE=2 -fPIE -F$(SDKROOT)/System/Library/PrivateFrameworks $(CC_COVERAGE_FLAGS)
Extra_LD_Flags = -Wl,-pie -dead_strip -framework Heimdal $(LD_COVERAGE_FLAGS)

ifeq "$(RC_TARGET_CONFIG)" "iPhone"
Extra_CC_Flags += -isysroot $(SDKROOT)
endif

DST_DIR = $(DSTROOT)/usr/bin
MAN_DIR = $(DSTROOT)/usr/share/man/man1
DSYM = $(Project).dSYM

#
# Standard B&I targets
#
all: $(SYMROOT)/$(DSYM)

install:: all
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 555 -s $(SYMROOT)/$(Project) $(DST_DIR)
	ln -f $(DST_DIR)/$(Project) $(DST_DIR)/ncdestroy
	ln -f $(DST_DIR)/$(Project) $(DST_DIR)/ncinit
	ln -f $(DST_DIR)/$(Project) $(DST_DIR)/nclist
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/$(Project).1 $(MAN_DIR)
	ln -sf $(Project).1 $(MAN_DIR)/ncdestroy.1
	ln -sf $(Project).1 $(MAN_DIR)/ncinit.1
	ln -sf $(Project).1 $(MAN_DIR)/nclist.1
clean::
	-rm -rf *.o $(Project) ncestroy ncinit nclist $(Project).DSYM

#
# Build
#
CFILES = $(Project).c
OFILES = $(CFILES:.c=.o)

$(OFILES): $(HFILES)

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(SYMROOT)/$(Project): $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(SYMROOT)/$(DSYM): $(SYMROOT)/$(Project)
	$(DSYMUTIL) -o $@ $<
