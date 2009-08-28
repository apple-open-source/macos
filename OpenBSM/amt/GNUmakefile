#
# Makefile for amt
#

#(sub) Project info
Project 	= amt
#ProjectName 	= AMT 
UserType	= Administrator
ToolType 	= Services

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Extra_CC_Flags += -no-cpp-precomp

Environment = ARCH_FLAGS="$(CC_Archs)"  \
		   COPTS="$(CFLAGS)"    \
	      MAKEOBJDIR="$(OBJROOT)"   \
	      $(Extra_Environment)

build::
	@echo "Building $(Project)..."
	$(_v) $(MKDIR) $(OBJROOT)
	$(_v) $(Environment) $(BSDMAKE)

depend::
	@echo "Making dependancy file"
	$(_v) $(Environment) $(BSDMAKE) depend


install:: install-bins install-man-pages

install-bins:
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/sbin
	$(INSTALL_FILE) -C -m 0555 $(OBJROOT)/amt $(DSTROOT)/usr/sbin

install-man-pages:
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/share/man/man8
	$(INSTALL_FILE) -C -m 444 $(SRCROOT)/amt.8 $(DSTROOT)/usr/share/man/man8
