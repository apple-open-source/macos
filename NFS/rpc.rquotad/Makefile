#
# Makefile for rpc.rquotad
#

Project = rpc.rquotad

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SRCROOT ?= .
OBJROOT ?= .
SYMROOT ?= .
DSTROOT ?= .

Extra_CC_Flags = -Wall
Extra_LD_Flags = -lutil -dead_strip

DST_DIR = $(DSTROOT)/usr/libexec
MAN_DIR = $(DSTROOT)/usr/share/man/man8

#
# Standard B&I targets
#
all: rpc.rquotad

install:: $(SYMROOT)/rpc.rquotad
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 555 -s $(SYMROOT)/rpc.rquotad $(DST_DIR)
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/rpc.rquotad.8 $(MAN_DIR)

clean::
	-rm -f *.o rpc.rquotad

#
# Build
#
CFILES = rquotad.c rquota_xdr.c
HFILES = rquota.h
OFILES = $(CFILES:.c=.o)

$(OFILES): $(HFILES)

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(SYMROOT)/rpc.rquotad: $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

rpc.rquotad: $(OFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

