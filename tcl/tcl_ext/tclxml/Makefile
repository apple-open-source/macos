##
# Makefile for tclxml
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tclxml
ProjectName           = TclXML
License               = LICENSE

# Fetch info
Release               = 2.6

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)
Extra_Install_Flags   = GENERIC_HDRS=

GnuAfterInstall       = strip install-doc fix-config fix-perms

# include TEA Tcl extension build support
include ../TEA.make

TclExtDir             = Tclxml

MAKE_ARGS             = TclExtHtmlDir="$(TclExtHtmlDir)" DSTROOT="$(DSTROOT)"

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)" "$(DSTROOT)/$(TclExtManDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/README.html "$(DSTROOT)/$(TclExtHtmlDir)"/ReadMe-$(ProjectName).html
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/$(Project).html "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/$(Project).n "$(DSTROOT)/$(TclExtManDir)"

# the tclexpat subproject is built by a separate Makefile
build install clean almostclean::
	$(_v) $(MAKE) -f Makefile.tclexpat $@ $(MAKE_ARGS) \
	    Extra_TEA_Configure_Flags="--with-Tclxml=$(OBJROOT)" \
	    OBJROOT="$(OBJROOT)/expat"

# the tcldom subproject is built by a separate Makefile
build install clean almostclean::
	$(_v) $(MAKE) -f Makefile.tcldom $@ $(MAKE_ARGS) \
	    OBJROOT="$(OBJROOT)/tcldom"

$(fetch_targets)::
	$(_v) $(MAKE) -f Makefile.tcldom  $@ $(MAKE_ARGS)
	$(_v) $(MAKE) -f Makefile.tclxslt $@ $(MAKE_ARGS)
