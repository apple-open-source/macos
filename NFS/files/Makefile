#
# Makefile for files
#

Project = files

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SRCROOT ?= .
OBJROOT ?= .
SYMROOT ?= .
DSTROOT ?= .

MAN_DIR = $(DSTROOT)/usr/share/man/man5

#
# Standard B&I targets
#
all:

install::
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/nfs.conf.5 $(MAN_DIR)

clean::
