##
# Makefile for gperf
##

# Project info
Project		= gperf
Extra_CC_Flags	= -no-cpp-precomp
GnuAfterInstall	= post-install

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target	= install

post-install:
	strip $(DSTROOT)/usr/bin/gperf
