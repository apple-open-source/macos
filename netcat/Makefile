##
# Makefile for netcat
##

# Project info
Project		= netcat
Extra_CC_Flags	= -no-cpp-precomp
Passed_Targets = nc

lazy_install_source:: shadow_source

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

install::
	install -d -m 755 $(DSTROOT)/usr/bin
	install -d -m 755 $(DSTROOT)/usr/share/man/man1
	$(CC) -O -s -DFREEBSD $(RC_CFLAGS) -o $(DSTROOT)/usr/bin/nc $(SRCROOT)/netcat/netcat.c
	install -c -m 644 $(SRCROOT)/nc.1 $(DSTROOT)/usr/share/man/man1
	chown -R root:wheel $(DSTROOT) $(SYMROOT)
