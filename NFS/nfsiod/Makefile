#
# Makefile for nfsiod
#

Project = nfsiod

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SRCROOT ?= .
OBJROOT ?= .
SYMROOT ?= .
DSTROOT ?= .
SDKROOT ?= /

Extra_CC_Flags = -Wall -isysroot $(SDKROOT)
Extra_LD_Flags = -dead_strip

DST_DIR = $(DSTROOT)/sbin
MAN_DIR = $(DSTROOT)/usr/share/man/man8

#
# Standard B&I targets
#
all: nfsiod

install:: $(SYMROOT)/nfsiod
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 555 -s $(SYMROOT)/nfsiod $(DST_DIR)
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/nfsiod.8 $(MAN_DIR)

clean::
	-rm -f *.o nfsiod

#
# Build
#
CFILES = nfsiod.c
OFILES = $(CFILES:.c=.o)

$(OFILES): $(HFILES)

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(SYMROOT)/nfsiod: $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

nfsiod: $(OFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

