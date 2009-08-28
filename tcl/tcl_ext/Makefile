##
# Toplevel Makefile wrapper for Tcl extensions
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

CoreOSMakefiles = $(MAKEFILEPATH)/CoreOS
include $(CoreOSMakefiles)/Standard/Standard.make

tcl_ext_compiled      = thread tclx tclvfs expect incrtcl xotcl \
                        tbcload memchan mk4tcl tls tdom tclxml ffidl \
                        tktable tkimg trf $(tcl_ext_compiled_84)
ifneq ($(Tcl84Ext),NO)
tcl_ext_compiled_84   = tktreectrl quicktimetcl snack tclresource tclae tclapplescript
endif
tcl_ext_puretcl       = tcllib tklib bwidget tkcon tclsoap

ifndef PureTclExt
SubProjects           = $(tcl_ext_compiled) $(tcl_ext_puretcl)
else
ifeq ($(PureTclExt),YES)
SubProjects           = incrtcl $(tcl_ext_puretcl)
else
SubProjects           = $(tcl_ext_compiled)
endif
endif

# use mpexpand/doctools in tcllib sources to avoid depending on tcllib being installed
MAKE_ARGS             = MPEXPAND='$(Tclsh) $(SRCROOT)/tcllib/tcllib/modules/doctools/mpexpand' \
                        TCLLIBPATH='$(SRCROOT)/tcllib/tcllib/modules/doctools \
                                    $(SRCROOT)/tcllib/tcllib/modules/textutil' \
                        DOCTOOLS_NROFF_INCLUDE='$(SRCROOT)/tcllib/tcllib/support/installation'

unexport LD_SPLITSEGS_NEW_LIBRARIES

build install::
	$(_v) $(RM) "$(TCL_CONFIG_DIR)/config.cache"

Actions              := build install clean almostclean fetch extract wipe install-license
include SubprojActions.make
