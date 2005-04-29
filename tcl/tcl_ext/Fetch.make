##
# Makefile support for CVS source retrieval
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

##
# Set $(Project) variable, then include this file, and
# then define following variables if needed:
#
#  Release
#  UrlBase
#  UrlExt
#  UrlFile
#  Url
#
#  UseCvs
#  CvsServer (or SFProject)
#  CvsTag
#  CvsModule
#
##

ifndef CoreOSMakefiles
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make
endif

##
# Defaults
##

Release              =
UrlBase              = http://$(SFMirror).dl.sourceforge.net/sourceforge/$(SFProject)
SFMirror             = osdn
UrlExt               = .tar.gz
UrlFile              = $(Project)$(if $(Release),-$(Release))
Url                  = $(UrlBase)/$(UrlFile)$(UrlExt)
ExtractedDir         = $(UrlFile)
UrlFetch             = $(CURL) -L -s -S -R
UrlExtract           = $(TAR) zxf

CvsServer            = cvs.sourceforge.net:/cvsroot/$(SFProject)
SFProject            = $(Project)
CvsTag               = 
CvsModule            = $(Project)
CvsUser              ?=
CvsPass              = $${HOME}/.cvspass
CvsRoot              = $(if $(CvsUser),:ext:$(CvsUser),:pserver:anonymous)@$(CvsServer)
CvsCheckout          = co -d $(Project) $(CvsTag) $(CvsModule)
Cvs                  = $(CVS) -q -z3

##
# Commands
##

CVS                  ?= /usr/bin/cvs
PATCH                ?= /usr/bin/patch
CURL                 ?= /usr/bin/curl

##
# Targets
##

fetch:: $(SRCROOT)/$(Project)

$(SRCROOT)/$(Project):
	$(_v) cd $(SRCROOT) && $(UrlExtract) $(Project)$(UrlExt) && \
	    if [ ! -d $(Project) ]; then $(MV) $(ExtractedDir) $(Project); fi
	$(_v) if [ -e $(CURDIR)/$(Project).diff ]; then \
	    cd $(SRCROOT)/$(Project) && $(PATCH) -Np0 < $(CURDIR)/$(Project).diff; fi
ifdef Configure
	$(_v) $(CHMOD) +x $(Configure)
endif

wipe::
	cd $(SRCROOT) && $(RMDIR) $(Project)


.PHONY: fetch wipe
