#
# Makefile for nfsd
#

Project = nfsd

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SRCROOT ?= .
OBJROOT ?= .
SYMROOT ?= .
DSTROOT ?= .
LOCKDROOT = $(SRCROOT)/../rpc.lockd
SDKROOT ?= /

Extra_CC_Flags = -Wall -I $(SRCROOT) -I $(LOCKDROOT) -I $(OBJROOT) -isysroot $(SDKROOT)
Extra_LD_Flags = -lutil -framework DiskArbitration -framework CoreFoundation

DST_DIR = $(DSTROOT)/sbin
LAUNCHD_DIR = $(DSTROOT)/System/Library/LaunchDaemons
MAN5_DIR = $(DSTROOT)/usr/share/man/man5
MAN8_DIR = $(DSTROOT)/usr/share/man/man8

#
# Standard B&I targets
#
all: nfsd

install:: $(SYMROOT)/nfsd
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 555 -s $(SYMROOT)/nfsd $(DST_DIR)
	install -d -o root -g wheel -m 755 $(LAUNCHD_DIR)
	install -c -o root -g wheel -m 444 -s $(SRCROOT)/com.apple.nfsd.plist $(LAUNCHD_DIR)
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

# Even though we don't ask for it mig will generate lockd_machServer.c
$(MIG_OUT_FILES): $(MIG_DEF_FILES)
	mig \
		-user    $(OBJROOT)/lockd_machUser.c \
		-header  $(OBJROOT)/lockd_mach.h \
		$(MIG_DEF_FILES)
	rm -f $(OBJROOT)/lockd_machServer.c

$(MIG_GEN_FILES): $(MIG_DEF_FILES)
	mig \
		-user    lockd_machUser.c \
		-header  lockd_mach.h \
		$(MIG_DEF_FILES)
	rm -f lockd_machServer.c

$(SYMROOT)/nfsd: $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

nfsd: $(OFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

