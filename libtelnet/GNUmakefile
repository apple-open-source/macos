##
# Makefile for libtelnet
##

# Project info
Project  = libtelnet
UserType = Developer
ToolType = Libraries

Extra_Install_Environment = LIBDIR=/usr/local/lib

# It's a BSD project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSD.make

BSD_install_dirs::
	$(_v) $(MKDIR) $(DSTROOT)/usr/local/lib


install:: installhdrs

installhdrs::
	$(MKDIR) -p $(DSTROOT)/usr/local/include/libtelnet
	$(INSTALL) -c -m 444 $(SRCROOT)/encrypt.h $(DSTROOT)/usr/local/include/libtelnet
	$(INSTALL) -c -m 444 $(SRCROOT)/enc-proto.h $(DSTROOT)/usr/local/include/libtelnet

	$(INSTALL) -c -m 444 $(SRCROOT)/auth.h $(DSTROOT)/usr/local/include/libtelnet
	$(INSTALL) -c -m 444 $(SRCROOT)/auth-proto.h $(DSTROOT)/usr/local/include/libtelnet
	$(INSTALL) -c -m 444 $(SRCROOT)/misc.h $(DSTROOT)/usr/local/include/libtelnet
	$(INSTALL) -c -m 444 $(SRCROOT)/misc-proto.h $(DSTROOT)/usr/local/include/libtelnet

