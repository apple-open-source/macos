#
# Makefile for rpc.statd
#

Project = rpc.statd

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SRCROOT ?= .
OBJROOT ?= .
SYMROOT ?= .
DSTROOT ?= .
SDKROOT ?= /

Extra_CC_Flags = -Wall -isysroot $(SDKROOT)
Extra_LD_Flags = -lutil -dead_strip

DST_DIR = $(DSTROOT)/usr/sbin
MAN_DIR = $(DSTROOT)/usr/share/man/man8
LAUNCHD_DIR = $(DSTROOT)/System/Library/LaunchDaemons

#
# Standard B&I targets
#
all: rpc.statd

install:: $(SYMROOT)/rpc.statd
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 555 -s $(SYMROOT)/rpc.statd $(DST_DIR)
	install -d -o root -g wheel -m 755 $(LAUNCHD_DIR)
	install -c -o root -g wheel -m 444 -s $(SRCROOT)/com.apple.statd.notify.plist $(LAUNCHD_DIR)
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/rpc.statd.8 $(MAN_DIR)

clean::
	-rm -f *.o rpc.statd

#
# Build
#
CFILES = file.c procs.c statd.c sm_inter_svc.c sm_inter_xdr.c
HFILES = statd.h
OFILES = $(CFILES:.c=.o)

$(OFILES): $(HFILES)

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(SYMROOT)/rpc.statd: $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

rpc.statd: $(OFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

