##
# Makefile for tclxml
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tclxml
ProjectName           = TclXML

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_CC_Flags        = -fno-common
Extra_LD_Flags        = -L$(TclFramework)
Extra_Environment     = CC=gcc $(TEA_Environment)
Extra_Install_Flags   = GENERIC_HDRS=

GnuAfterInstall       = strip install-doc fix-config fix-owner

# include TEA Tcl extension build support
include ../TEA.make

TclExtDir             = Tclxml

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)" "$(DSTROOT)/$(TclExtManDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/$(Project).html "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/$(Project).n "$(DSTROOT)/$(TclExtManDir)"

# the tclexpat subproject is built by a separate Makefile
build install clean almostclean::
	$(_v) $(MAKE) -f Makefile.tclexpat $@ \
	    Extra_TEA_Configure_Flags="--with-Tclxml=$(OBJROOT)" \
	    TclExtHtmlDir="$(TclExtHtmlDir)" \
	    OBJROOT="$(OBJROOT)/expat" DSTROOT="$(DSTROOT)"

# the tcldom subproject is built by a separate Makefile
build install clean almostclean::
	$(_v) $(MAKE) -f Makefile.tcldom $@ \
	    TclExtHtmlDir="$(TclExtHtmlDir)" \
	    OBJROOT="$(OBJROOT)/tcldom" DSTROOT="$(DSTROOT)"
