##
# Makefile for mk4tcl
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = metakit
ProjectName           = Mk4Tcl

Configure             = $(Sources)/unix/configure
Extra_Configure_Flags = $(TEA_Configure_Flags) \
                        --with-tcl=$(TclHeaders),$(TclExtLibDir)
Extra_Environment     = CC=g++ $(TEA_Environment)

GnuAfterInstall       = install-doc fix-owner

# include TEA Tcl extension build support
include ../TEA.make

# Force gcc-4.0 (or configure will fail)
CC                    = gcc-4.0
CXX                   = g++-4.0

Install_Target        = install-tcl

# patch config.h
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	cp $(BuildDirectory)/config.h $(BuildDirectory)/config.h.bak
	$(_v) ed - $(BuildDirectory)/config.h < $(SRCROOT)/config.h.ed
	$(_v) $(TOUCH) $(ConfigStamp2)
	cp $(SRCROOT)/Mk4tcl.exports $(BuildDirectory)/../

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/tcl.html \
	        "$(DSTROOT)/$(TclExtHtmlDir)/$(ProjectName).html"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/*.gif "$(DSTROOT)/$(TclExtHtmlDir)"
