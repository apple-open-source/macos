##
# Makefile for tclodbc
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tclodbc
ProjectName           = TclODBC

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_LD_Flags        = -L$(TclFramework) -framework Carbon
Extra_Environment     = SHLIB_LD='g++ -dynamiclib $${LDFLAGS}' \
                        $(TEA_Environment)
Extra_Install_Flags   = DOC_FILES= SAMPLE_FILES= MISC_FILES=

GnuAfterInstall       = strip install-doc fix-owner

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/*.htm \
	        "$(DSTROOT)/$(TclExtHtmlDir)"
