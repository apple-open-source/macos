#
# Makefile for mount_nfs
#

Project = mount_nfs

pathsearch = $(firstword $(wildcard $(addsuffix /$(1),$(subst :, ,$(PATH)))))
TCONF := $(call pathsearch,tconf)

ifneq "$(TCONF)" ""
Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)
endif

inplace:
	@$(MAKE) all SRCROOT=. OBJROOT=. SYMROOT=. DSTROOT=/

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SDKROOT ?= /

WARNING_FLAGS = -Wall -Wextra -Wformat=2 -Wpointer-arith -Wcast-align \
		-Wwrite-strings -Wno-unknown-pragmas \
		-Wno-missing-field-initializers -Wno-shadow \
		-Wno-deprecated-declarations
Extra_CC_Flags = $(WARNING_FLAGS) -D_FORTIFY_SOURCE=2 -fPIE
Extra_LD_Flags = -Wl,-pie -lutil -dead_strip

ifeq "$(Embedded)" "YES"
Extra_CC_Flags += -isysroot $(SDKROOT)
endif

DST_DIR = $(DSTROOT)/sbin
ETC_DIR = $(DSTROOT)/private/etc
MAN_DIR = $(DSTROOT)/usr/share/man/man8
LAUNCHD_DIR = $(DSTROOT)/System/Library/LaunchDaemons

#
# Standard B&I targets
#
all: $(SYMROOT)/mount_nfs

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
	install -d -o root -g wheel -m 755 $(ETC_DIR)
	install -c -o root -g wheel -m 644 $(SRCROOT)/nfs.conf $(ETC_DIR)

clean::
	-rm -f *.o mount_nfs

#
# Build
#
CFILES = mount_nfs.c
OFILES = $(CFILES:.c=.o)

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(SYMROOT)/mount_nfs: $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

