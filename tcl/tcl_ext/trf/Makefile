##
# Makefile for trf
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = trf
ProjectName           = Trf
License               = doc/license.terms

# Fetch info
Release               = 2.1.4
TEApotVersion         = $(Release).2009.05.20.12.34.07

Extra_Configure_Flags = $(TEA_Configure_Flags) \
			--with-zlibtcl=$(OBJROOT)/../tkimg/zlib \
			--with-zlib=$(SDKROOT)$(USRDIR) \
			--with-ssl-include-dir=$(SDKROOT)$(USRINCLUDEDIR)/openssl \
			--with-ssl-lib-dir=$(SDKROOT)$(USRLIBDIR) \
			--with-bz2=$(SDKROOT)$(USRDIR)

Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries

TclExtDir             = $(ProjectName)

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtManDir)"
	$(_v) umask $(Install_Mask); cd "$(Sources)"/doc; for f in *.man; do \
	    n=$$(basename "$$f" .man) && echo "Generating $$n docs" && \
	    $(MPEXPAND) nroff $$f "$(DSTROOT)/$(TclExtManDir)/$$n.n"; \
	done

extract::
	$(_v) $(RM) $(SRCROOT)/$(Project)/compat/{tclLoadDyld.c,stpncpy.c}
	$(_v) $(RM) $(SRCROOT)/$(Project)/generic/rs-ecc/{Makefile.in,configure*,ecc.{c,1},version.c}
	$(_v) $(RMDIR) $(SRCROOT)/$(Project)/md5-crypt $(SRCROOT)/$(Project)/win
