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
