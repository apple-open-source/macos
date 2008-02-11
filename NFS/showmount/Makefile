#
# Makefile for showmount
#

Project = showmount

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SRCROOT ?= .
OBJROOT ?= .
SYMROOT ?= .
DSTROOT ?= .

Extra_CC_Flags = -Wall
Extra_LD_Flags = -framework CoreFoundation -dead_strip

DST_DIR = $(DSTROOT)/usr/bin
MAN_DIR = $(DSTROOT)/usr/share/man/man8

#
# Standard B&I targets
#
all: showmount

install:: $(SYMROOT)/showmount
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 555 -s $(SYMROOT)/showmount $(DST_DIR)
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/showmount.8 $(MAN_DIR)

clean::
	-rm -f *.o showmount

#
# Build
#
CFILES = showmount.c mshow.c
HFILES = showmount.h
OFILES = $(CFILES:.c=.o)

$(OFILES): $(HFILES)

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(SYMROOT)/showmount: $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

showmount: $(OFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

