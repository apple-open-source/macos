##
# Templates
#  This project is similar to files, but runs at the end of a build
#  cycle; it can therefore expect that files in the build root are
#  final build output for the rest of the build.
##
# Wilfredo Sanchez | wsanchez@apple.com
# Copyright 1999 Apple Computer, Inc.
##

# Project info
Project     = templates
Destination = $(DSTROOT)

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Templates = $(USRDIR)/template/client
ClientDir = $(Templates)/client

install::
	@echo "Building the locate database"
	$(INSTALL_DIRECTORY) $(DSTROOT)/private/var/db
	TMPDIR=/tmp DBDIR="$(DSTROOT)/private/var/db" /usr/libexec/locate.updatedb
