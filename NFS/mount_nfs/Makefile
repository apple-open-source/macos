#
# Makefile for mount_nfs
#

Project = mount_nfs

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SRCROOT ?= .
OBJROOT ?= .
SYMROOT ?= .
DSTROOT ?= .
SDKROOT ?= /

Extra_CC_Flags = -Wall -isysroot $(SDKROOT)
Extra_LD_Flags = -lutil -dead_strip

DST_DIR = $(DSTROOT)/sbin
MAN_DIR = $(DSTROOT)/usr/share/man/man8

#
# Standard B&I targets
#
all: mount_nfs

install:: $(SYMROOT)/mount_nfs
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 4555 -s $(SYMROOT)/mount_nfs $(DST_DIR)
	install -d -o root -g wheel -m 755 $(DSTROOT)/usr
	install -d -o root -g wheel -m 755 $(DSTROOT)/usr/share
	install -d -o root -g wheel -m 755 $(DSTROOT)/usr/share/man
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/mount_nfs.8 $(MAN_DIR)

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

mount_nfs: $(OFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

