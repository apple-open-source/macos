##
# Makefile for mk4tcl
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = metakit
ProjectName           = Mk4Tcl
ProjectPlistName      = mk4tcl

# Fetch info
Release               = 2.4.9.7
ImportDate            = 2008-09-06
SvnUrl                = svn://svn.equi4.com/$(Project)/trunk

Configure             = $(Sources)/tcl/configure
Configure_Products    = config.h
Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = CC=g++ $(TEA_Environment) CONFIG_SITE= \
                        MAKE_LIB='$${SHLIB_LD} -o $$@ $$(PKG_OBJECTS) $${SHLIB_LD_LIBS} -Wl,-exported_symbols_list -Wl,$(SRCROOT)/$(ProjectName).exports'

GnuAfterInstall       = strip install-doc fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries

TclExtDir             = Mk4tcl

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/tcl.html \
	        "$(DSTROOT)/$(TclExtHtmlDir)/$(ProjectName).html"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/{format.html,*.gif} \
	        "$(DSTROOT)/$(TclExtHtmlDir)"
