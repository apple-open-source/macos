##
# Toplevel Makefile wrapper for Tcl extensions
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

subprojects           = tcllib thread tclx tclvfs expect incrtcl \
                        xotcl tbcload memchan mk4tcl \
                        tls tdom tclxml tclsoap \
                        tclodbc \
                        tclapplescript tclresource tclAE \
                        tklib bwidget tktable tkimg trf tktreectrl \
                        snack quicktimetcl

# use mpexpand/doctools in tcllib sources to avoid depending on tcllib being installed
Tcllib_Environment    = MPEXPAND='/usr/bin/tclsh $(SRCROOT)/tcllib/tcllib/modules/doctools/mpexpand' \
                        TCLLIBPATH='$(SRCROOT)/tcllib/tcllib/modules/doctools \
                                    $(SRCROOT)/tcllib/tcllib/modules/textutil'

build install clean almostclean::
	$(_v) $(MAKE) action=$@ $(subprojects)

$(subprojects):
	$(_v) $(MAKE) -C $@ $(action) \
	        OBJROOT="$(OBJROOT)/$@" \
	        SYMROOT="$(SYMROOT)/$@" \
	        SRCROOT="$(SRCROOT)/$@" \
	        DSTROOT="$(DSTROOT)" \
	        $(Tcllib_Environment)

.PHONY: $(subprojects)
