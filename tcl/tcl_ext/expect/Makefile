##
# Makefile for expect
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = expect
ToolType              = Commands

# Fetch info
Release               = 5.45
ImportDate            = 2010-09-02
CvsRoot               = :pserver:anonymous@$(Project).cvs.sourceforge.net:/cvsroot/$(Project)

#Release               = 5.44.1.15
#TEApotVersion         = 0.0.0.2010.08.18.08.50.42
#TEApotProject         = Expect-sf

Extra_Configure_Flags = $(TEA_Configure_Flags) --no-recursion \
                        --with-tclinclude=$(TclPrivateHeaders)
Extra_Environment     = $(TEA_Environment) bin_BINARIES=$(Project) \
                        MAKE_LIB='$${SHLIB_LD} -o $$@ $$(PKG_OBJECTS) $${SHLIB_LD_LIBS} -install_name $${pkglibdir}/$$@'

GnuAfterInstall       = lipo_ppc strip install-doc fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries

install-doc:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/$(MANDIR)/man1
	$(_v) $(INSTALL_FILE) $(Sources)/$(Project).man $(DSTROOT)/$(MANDIR)/man1/$(Project).1

strip::
	$(_v) $(CP) $(DSTROOT)/$(USRBINDIR)/$(Project) $(SYMROOT)
	$(_v) $(DSYMUTIL) $(SYMROOT)/$(Project)
	$(_v) $(STRIP) $(DSTROOT)/$(USRBINDIR)/$(Project)

lipo_ppc:
ifeq '$(RC_ppc)' 'YES'
	$(_v) lipo -remove ppc7400 -output $(DSTROOT)$(USRBINDIR)/$(Project) \
		$(DSTROOT)$(USRBINDIR)/$(Project)
endif

.PHONY: lipo_ppc
