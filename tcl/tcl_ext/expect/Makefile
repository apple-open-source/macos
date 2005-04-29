##
# Makefile for expect
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = expect
ToolType              = Commands

Extra_Configure_Flags = $(TEA_Configure_Flags) \
                        --with-tclinclude=$(TclPrivateHeaders) \
                        --enable-shared --enable-stubs --without-x
Extra_LD_Flags        = -L$(TclFramework) -F$(shell dirname $(TclFramework))
Extra_Environment     = TCLHDIRDASHI='-I$(TclHeaders) -I$(TclPrivateHeaders)' \
                        $(TEA_Environment)
Extra_Install_Flags   = tcl_libdir='$$(libdir)' EXP_UNSHARED_LIB_FILE=/dev/null

GnuAfterInstall       = strip install-binary fix-pkgIndex fix-owner

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install_shared_lib
Install_Flags         = INSTALL_ROOT="$(DSTROOT)" $(Extra_Install_Flags)

Version               = $(shell eval $$($(GREP) 'VERSION=' "$(Sources)"/configure.in); echo $${VERSION})
PkgIndex              = $(TclExtLibDir)/$(Project)$(Version)/pkgIndex.tcl

install-binary:
# manually install statically linked expect executable and expect manpage
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/$(USRBINDIR) $(DSTROOT)/$(MANDIR)/man1
	$(_v) $(INSTALL_PROGRAM) $(BuildDirectory)/$(Project) $(DSTROOT)/$(USRBINDIR)
	$(_v) $(INSTALL_FILE) $(Sources)/$(Project).man $(DSTROOT)/$(MANDIR)/man1/$(Project).1

fix-pkgIndex:
# fix incorrect relative path in pkgIndex.tcl
	$(_v) $(SED) -e 's/\.\. //' "$(DSTROOT)/$(PkgIndex)" > "$(DSTROOT)/$(PkgIndex).fix"
	$(_v) $(MV) -f "$(DSTROOT)/$(PkgIndex).fix" "$(DSTROOT)/$(PkgIndex)"

.PHONY: $(GnuAfterInstall)
