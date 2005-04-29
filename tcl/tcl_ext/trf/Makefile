##
# Makefile for tcltrf
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = trf
ProjectName           = Trf

Extra_Configure_Flags = $(TEA_Configure_Flags) --with-zlibtcl=$(DSTROOT)/usr/lib
Extra_CC_Flags        = -fno-common
Extra_LD_Flags        = -L$(TclFramework)
Extra_Environment     = AM_CPPFLAGS='-I/usr/include/openssl \
                        -DZ_LIB_NAME=\"libz.dylib\" \
                        -DBZ2_LIB_NAME=\"libbz2.dylib\" \
                        -DSSL_LIB_NAME=\"libcrypto.dylib\" \
                        -DLIBDES_LIB_NAME=\"libdes425.dylib\"' \
                        $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-owner

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries

TclExtDir             = $(ProjectName)

# generate docs from doctools format, requires tcllib to be installed
MPEXPAND              = $(Tclsh) $(Sources)/tools/mpexpand.tcl

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)" "$(DSTROOT)/$(TclExtManDir)"
	$(_v) umask $(Install_Mask); for f in $(Sources)/doc/*.man; do \
	    echo "Generating $$(basename $$f .man) docs" && \
	    $(MPEXPAND) html $$f "$(DSTROOT)/$(TclExtHtmlDir)/"$$(basename $$f .man).html && \
	    $(MPEXPAND) nroff $$f "$(DSTROOT)/$(TclExtManDir)/"$$(basename $$f .man).n; \
	done
