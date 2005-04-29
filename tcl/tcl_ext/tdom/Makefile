##
# Makefile for tdom
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tdom
ProjectName           = tDOM

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_LD_Flags        = -L$(TclFramework)
Extra_CC_Flags        = -fno-common -DXMLCALL=''
Extra_Environment     = CC=gcc $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-config fix-owner

# include TEA Tcl extension build support
include ../TEA.make

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/*.{html,css} \
	    "$(DSTROOT)/$(TclExtHtmlDir)"

# the tnc subproject is built by a separate Makefile
build install clean almostclean::
	$(_v) $(MAKE) -f Makefile.tnc $@ \
	    Extra_TEA_Configure_Flags="--with-tdom=$(OBJROOT)" \
	    OBJROOT="$(OBJROOT)/tnc" DSTROOT="$(DSTROOT)"
