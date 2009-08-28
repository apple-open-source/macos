#
# Makefile for nfsd
#

Project = nfsd

inplace:
	@$(MAKE) all SRCROOT=. OBJROOT=. SYMROOT=. DSTROOT=/

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SDKROOT ?= /
LOCKDROOT = $(SRCROOT)/../rpc.lockd

WARNING_FLAGS = -Wall -Wextra -Wformat=2 -Wpointer-arith -Wcast-align \
		-Wwrite-strings -Wno-unknown-pragmas \
		-Wno-missing-field-initializers -Wno-shadow
Extra_CC_Flags = $(WARNING_FLAGS) -D_FORTIFY_SOURCE=2 \
		 -I $(SRCROOT) -I $(LOCKDROOT) -I $(OBJROOT) \
		 -isysroot $(SDKROOT) -fPIE
Extra_LD_Flags = -Wl,-pie -lutil -framework DiskArbitration \
		  -framework ServiceManagement -framework CoreFoundation

DST_DIR = $(DSTROOT)/sbin
LAUNCHD_DIR = $(DSTROOT)/System/Library/LaunchDaemons
MAN5_DIR = $(DSTROOT)/usr/share/man/man5
MAN8_DIR = $(DSTROOT)/usr/share/man/man8

#
# Standard B&I targets
#
all: $(SYMROOT)/nfsd

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
	-rm -f *.o nfsd $(MIG_GEN_FILES)

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

