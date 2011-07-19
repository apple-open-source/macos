##
# Makefile support for Tarball source retrieval
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

##
# Set these variables as needed, then include this file:
#
#  Release
#  UrlBase
#  UrlExt
#  UrlFile
#  Url
#  ExtractedDir
#  ExtractOptions
#  License
#  TEApotVersion
#  TEApotProject
#  ProjectPlistName
#  ImportDate
#  CvsRoot
#  CvsTag
#  SvnUrl
#
##

ifndef CoreOSMakefiles
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make
endif

##
# Defaults
##

ifndef TEApotVersion
ifndef UrlBase
UrlBase               = http://osdn.dl.sourceforge.net/project
endif
ifndef UrlExt
UrlExt                = .tar.gz
endif
ifndef UrlFile
UrlFile               = $(Project)$(if $(Release),-$(Release))
endif
ifndef Url
Url                   = $(UrlBase)/$(Project)/$(ProjectName)/$(Release)/$(UrlFile)$(UrlExt)
endif
ifndef UrlExtract
UrlExtract            = $(TAR) $(ExtractOptions) zxf
endif
ifndef ImportDate
ImportDate            = $(shell $(DATE) '+%Y-%m-%d')
endif
else
# Retrieve sources from TEApot repository
ifndef UrlBase
UrlBase               = http://teapot.activestate.com/package/name
endif
ifndef UrlExt
UrlExt                = .zip
endif
ifndef TEApotProject
TEApotProject         = $(ProjectName)
endif
ifndef Url
Url                   = $(UrlBase)/$(TEApotProject)/ver/$(TEApotVersion)/arch/source/file$(UrlExt)
endif
ifndef UrlExtract
UrlExtract            = $(UNZIP) $(ExtractOptions) -q -d $(ExtractedDir)
endif
ifndef ImportDate
ImportDate            = $(shell echo '$(TEApotVersion)' | awk -F. '{print $$4"-"$$5"-"($$6<9?"0":"")$$6+1}')
endif
endif
ifndef License
License               = license.terms
endif
ifndef ProjectPlistName
ProjectPlistName      = $(Project)
endif
LicenseInstallDir     = $(TclExtLibDir)/$(TclExtDir)
Plist                 = $(SRCROOT)/$(ProjectPlistName).plist
ifdef CvsRoot
ScmGet                = $(CVS) -Q -d '$(CvsRoot)' export $(if $(CvsTag),-r '$(CvsTag)',-D '$(ImportDate)')
PlistSourceKey        = OpenSourceCVS
endif
ifdef SvnUrl
ScmGet                = $(SVN) -q export -r '{$(ImportDate)}' '$(SvnUrl)'
PlistSourceKey        = OpenSourceSVN
endif
ifndef ScmGet
Fetch                 = $(CURL) -L -s -S $(Url)
PlistSourceKey        = OpenSourceURL
PlistSourceValue      = $(Url)
ifndef TEApotVersion
ifndef ExtractedDir
ExtractedDir          = $(UrlFile)
endif
endif
else
Fetch                 = ($(ScmGet) $(ExtractedDir) && $(TAR) cz $(ExtractedDir) && $(RMDIR) $(ExtractedDir))
PlistSourceValue      = $(ScmGet) $(ExtractedDir)
endif
ifndef ExtractedDir
ExtractedDir          = $(Project)
endif

##
# Commands
##

PATCH                ?= /usr/bin/patch
CURL                 ?= /usr/bin/curl
CVS                  ?= /usr/bin/cvs
SVN                  ?= /usr/bin/svn
UNZIP                ?= /usr/bin/unzip
SHA1                 ?= /usr/bin/openssl sha1
DATE                 ?= /bin/date

##
# Targets
##

fetch_targets        := fetch extract wipe install-license

extract:: $(SRCROOT)/$(Project)

$(SRCROOT)/$(Project):
	@echo "Extracting $(Project)..."
	$(_v) cd $(SRCROOT) && $(UrlExtract) $(Project)$(UrlExt) && $(RM) $(Project)$(UrlExt) && \
	    if [ ! -d $(Project) ]; then $(MV) $(ExtractedDir) $(Project); fi
	$(_v) shopt -s nullglob; for p in $(SRCROOT)/$(Project)*.diff; do \
	    $(PATCH) -d $(SRCROOT)/$(Project) -Np0 < $${p} && $(RM) $${p}; done
ifdef Configure
ifneq ($(Configure),:)
ifdef TEA_TclConfig
	$(_v) $(TEA_TclConfig)/updt_tcl_m4.sh $@
endif
	$(_v) $(CHMOD) +x $(Configure)
endif
endif

wipe::
	cd $(SRCROOT) && $(RMDIR) $(Project)

install-license::
	$(_v) $(MKDIR) $(DSTROOT)$(LicenseInstallDir) && \
	     $(INSTALL_FILE) $(SRCROOT)/$(Project)/$(License) $(DSTROOT)$(LicenseInstallDir)/$(Project).txt

fetch:: SRCROOT = $(CURDIR)
fetch:: $(SRCROOT)/$(Project)$(UrlExt)

$(SRCROOT)/$(Project)$(UrlExt):
	@echo "Fetching $(Project)..."
	$(_v) cd $(SRCROOT) && $(Fetch) > $(Project)$(UrlExt)
	@ if [ ! -f $(Plist) ]; then printf '<?xml version="1.0" encoding="utf-8"?>\n<plist version="1.0">\n<array>\n</array>\n</plist>\n' > $(Plist); fi
	@ sha="$$(cat $(SRCROOT)/$(Project)$(UrlExt) | $(SHA1))" &&\
	awk '/^\t<dict>/ {s=$$0; do {getline; s=s"\n"$$0} while($$0 !~ /^\t<\/dict>/);'\
	'if (match(s,/<key>OpenSourceProject<\/key>\n\t+<string>$(ProjectPlistName)<\/string>/)) {x=1;'\
	'sub(/<key>OpenSourceVersion<\/key>\n\t+<string>[^\n]*<\/string>/,"<key>OpenSourceVersion</key>\n\t\t<string>$(Release)</string>",s);'\
	'sub(/<key>$(PlistSourceKey)<\/key>\n\t+<string>[^\n]*<\/string>/,"<key>$(PlistSourceKey)</key>\n\t\t<string>$(PlistSourceValue)</string>",s);'\
	'sub(/<key>OpenSourceSHA1<\/key>\n\t+<string>[^\n]*<\/string>/,"<key>OpenSourceSHA1</key>\n\t\t<string>'"$${sha}"'</string>",s);'\
	'sub(/<key>OpenSourceImportDate<\/key>\n\t+<string>[^\n]*<\/string>/,"<key>OpenSourceImportDate</key>\n\t\t<string>'"$(ImportDate)"'</string>",s);'\
	'}; print s; next}; /^<\/array>/ && !x {print "\t<dict>\n'\
	'\t\t<key>OpenSourceProject</key>\n\t\t<string>$(ProjectPlistName)</string>\n'\
	'\t\t<key>OpenSourceVersion</key>\n\t\t<string>$(Release)</string>\n'\
	'\t\t<key>OpenSourceWebsiteURL</key>\n\t\t<string>http://$(Project).sourceforge.net/</string>\n'\
	'\t\t<key>$(PlistSourceKey)</key>\n\t\t<string>$(PlistSourceValue)</string>\n'\
	'\t\t<key>OpenSourceSHA1</key>\n\t\t<string>'"$${sha}"'</string>\n'\
	'\t\t<key>OpenSourceImportDate</key>\n\t\t<string>'"$(ImportDate)"'</string>\n'\
	'\t\t<key>OpenSourceLicense</key>\n\t\t<string>Tcl</string>\n'\
	'\t\t<key>OpenSourceLicenseFile</key>\n\t\t<string>$(Project).txt</string>\n'\
	'\t</dict>"}; {print}' $(Plist) > $(Plist).1 && $(MV) $(Plist).1 $(Plist)

.PHONY: $(fetch_targets)
.NOTPARALLEL:
