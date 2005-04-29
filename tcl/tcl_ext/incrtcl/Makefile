##
# Makefile for incrtcl
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = incrTcl

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_CC_Flags        = -fno-common
Extra_CPP_Flags       = -I$(TclHeaders) -I$(TclPrivateHeaders) -I$(TkHeaders)
Extra_LD_Flags        = -L$(TclFramework) -L$(TkFramework)
Extra_Environment     = CPPFLAGS="$(CPPFLAGS)" $(TEA_Environment)
Extra_Install_Flags   = GENERIC_HDRS=

GnuAfterInstall       = strip fix-config fix-owner

# include TEA Tcl extension build support
include ../TEA.make

TclExtDir             = {itcl,itk}
TclExtStubConfig      = itclConfig.sh

# the iwidgets subproject is built by a separate Makefile
build install clean almostclean::
	$(_v) $(MAKE) -f Makefile.iwidgets $@ \
	    Extra_TEA_Configure_Flags="--with-itcl=$(Sources)" \
	    OBJROOT="$(OBJROOT)/iwidgets" DSTROOT="$(DSTROOT)"
