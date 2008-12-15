#
# Makefile for nfsstat
#

Project = nfsstat

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SRCROOT ?= .
OBJROOT ?= .
SYMROOT ?= .
DSTROOT ?= .
SDKROOT ?= /

Extra_CC_Flags = -Wall -isysroot $(SDKROOT)
Extra_LD_Flags = -dead_strip

DST_DIR = $(DSTROOT)/usr/bin
MAN_DIR = $(DSTROOT)/usr/share/man/man1

#
# Standard B&I targets
#
all: nfsstat

install:: $(SYMROOT)/nfsstat
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 555 -s $(SYMROOT)/nfsstat $(DST_DIR)
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/nfsstat.1 $(MAN_DIR)

clean::
	-rm -f *.o nfsstat

#
# Build
#
CFILES = nfsstat.c
OFILES = $(CFILES:.c=.o)

$(OFILES): $(HFILES)

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(SYMROOT)/nfsstat: $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

nfsstat: $(OFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

