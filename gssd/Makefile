#
# Makefile for Generic Security Services Daemon - gssd
#

Project = gssd

SRCROOT ?= .
OBJROOT ?= .
SYMROOT ?= .
DSTROOT ?= .

DST_DIR = $(DSTROOT)/usr/sbin
MAN_DIR = $(DSTROOT)/usr/share/man/man8

#
# Standard B&I targets
#
all: $(SYMROOT)/gssd $(SRCROOT)/gssd.8 $(SYMROOT)/gsstest 

install: all
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 644 $(SRCROOT)/gssd.8 $(MAN_DIR)
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 555 -s $(SYMROOT)/gssd   $(DST_DIR)
	install -d -o root -g wheel -m 755 $(DSTROOT)/System/Library/LaunchAgents
	install -c -o root -g wheel -m 644 $(SRCROOT)/com.apple.gssd-agent.plist  $(DSTROOT)/System/Library/LaunchAgents
	install -d -o root -g wheel -m 644 $(DSTROOT)/System/Library/LaunchDaemons
	install -c -o root -g wheel -m 644 $(SRCROOT)/com.apple.gssd.plist $(DSTROOT)/System/Library/LaunchDaemons

installhdrs:
	@echo installhdrs

clean:
	-rm -f *.o gssd gsstest \
		gssd_mach.h gssd_machServer.c gssd_machUser.c gssd_machServer.h
installsrc: clean
	ditto . $(SRCROOT)

#
# Build
#
CFLAGS		= -g -Os -Wall -Wextra -Wshadow -Wmissing-prototypes \
		  -Wmissing-declarations -Wno-discard-qual \
		  -I $(SRCROOT) -I $(OBJROOT)

CFLAGS		+= $(RC_CFLAGS)
LDFLAGS	= -lbsm -framework Kerberos

$(SYMROOT)/gssd: $(OBJROOT)/gssd.o $(OBJROOT)/gssd_machUser.o $(OBJROOT)/gssd_machServer.o $(OBJROOT)/gssd_validate.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(SRCROOT)/gssd.c: $(OBJROOT)/gssd_mach.h $(OBJROOT)/gssd_machServer.h

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJROOT)/gssd_mach.h $(OBJROOT)/gssd_machServer.c $(OBJROOT)/gssd_machUser.c \
	$(OBJROOT)/gssd_machServer.h: $(SRCROOT)/gssd_mach.defs
	mig  \
		-user    $(OBJROOT)/gssd_machUser.c \
		-header  $(OBJROOT)/gssd_mach.h \
		-server  $(OBJROOT)/gssd_machServer.c \
		-sheader $(OBJROOT)/gssd_machServer.h \
		$(SRCROOT)/gssd_mach.defs


# gsstest is a client used to test gssd - not installed

$(SYMROOT)/gsstest: $(OBJROOT)/gsstest.o $(OBJROOT)/gssd_machUser.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(SRCROOT)/gsstest.c: $(OBJROOT)/gssd_mach.h
