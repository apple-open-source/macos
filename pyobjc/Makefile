#
# xbs-compatible Makefile for bzip2.
#

Project             = pyobjc
GnuNoConfigure      = YES
Extra_CC_Flags      = -no-cpp-precomp
Extra_Install_Flags = PREFIX=$(RC_Install_Prefix)

#include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Install_Target      = install

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).txt $(OSL)/$(Project).txt

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = private-Leopard-branch-20071207
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = fixup.diff

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	@set -x && for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile || exit 1; \
	done
	@set -x && for z in `find $(SRCROOT)/$(Project) -name \*.py -size 0c`; do \
		echo '#' > $$z || exit 1; \
	done
	ditto $(SRCROOT)/$(Project)/pyobjc/ez_setup $(SRCROOT)/$(Project)/stable/pyobjc-xcode/ez_setup
	find $(SRCROOT)/$(Project) -name main -print -delete
	find $(SRCROOT)/$(Project) -name \*.so -print -delete
	@set -x && for z in `find $(SRCROOT)/$(Project) -name dist`; do \
		$(RMDIR) $$z || exit 1; \
	done
endif

copysource:
	ditto '$(SRCROOT)' '$(OBJROOT)'

DOCS=/Developer/Documentation/Python/PyObjC
EXAMPLES=/Developer/Examples/Python/PyObjC
EXTRAS=$(shell python -c "import sys, os;print os.path.join(sys.prefix, 'Extras')")
INIT = $(OBJROOT)/init.py

TEMPLATES=/Developer/Library/Xcode
APPTEMPLATES=$(TEMPLATES)/Project Templates/Application
FILETEMPLATES=$(TEMPLATES)/File Templates
TEMPLATEDATA = $(OBJROOT)/$(Project)/stable/pyobjc-xcode/Lib/PyObjCTools/XcodeSupport/template-data
XCODESUPPORT = Xcode Support

real-install:
	echo import site > "$(INIT)"
	echo 'site.addsitedir("$(SYMROOT)$(EXTRAS)/lib/python")' >> "$(INIT)"
	@set -x && \
	cd '$(OBJROOT)/$(Project)/stable' && \
	for pkg in pyobjc-core pyobjc-framework-* pyobjc-xcode; do \
	    cd "$(OBJROOT)/$(Project)/stable/$$pkg" && \
	    CFLAGS="$(RC_CFLAGS) $(MYEXTRACFLAGS)" LDFLAGS="$(RC_CFLAGS)" \
	    PYTHONSTARTUP="$(INIT)" \
	    python setup.py install --home="$(EXTRAS)" --root="$(SYMROOT)" || exit 1; \
	done
	@set -x && cd "$(SYMROOT)$(EXTRAS)/lib/python" && \
	for x in *; do \
	    if [ "$$x" != PyObjC -a "$$x" != PyObjC.pth ]; then \
		ditto "$$x" "PyObjC/$$x" && \
		$(RM) -r "$$x" || exit 1; \
	    fi; \
	done
	ditto "$(SYMROOT)"/System "$(DSTROOT)"/System

install-templates:
	$(INSTALL) -d '$(DSTROOT)$(TEMPLATES)'
	$(INSTALL) -d '$(DSTROOT)$(APPTEMPLATES)'
	$(INSTALL) -d '$(DSTROOT)$(FILETEMPLATES)'
	rsync -rplt '$(TEMPLATEDATA)/File Templates/Pure Python' '$(DSTROOT)$(FILETEMPLATES)'
	rsync -rplt '$(OBJROOT)/$(Project)/$(XCODESUPPORT)/File Templates/' '$(DSTROOT)$(FILETEMPLATES)'
	@set -x && \
	cd '$(OBJROOT)/$(Project)/$(XCODESUPPORT)/Project Templates' && \
	./project-tool.py -k -v --template 'Cocoa-Python Application/CocoaApp.xcodeproj/TemplateInfo.plist' 'Cocoa-Python Application' '$(DSTROOT)$(APPTEMPLATES)/Cocoa-Python Application' && \
	./project-tool.py -k -v --template 'Cocoa-Python Core Data Application/CocoaApp.xcodeproj/TemplateInfo.plist' 'Cocoa-Python Core Data Application' '$(DSTROOT)$(APPTEMPLATES)/Cocoa-Python Core Data Application' && \
	./project-tool.py -k -v --template 'Cocoa-Python Core Data Document-based Application/CocoaDocApp.xcodeproj/TemplateInfo.plist' 'Cocoa-Python Core Data Document-based Application' '$(DSTROOT)$(APPTEMPLATES)/Cocoa-Python Core Data Document-based Application' && \
	./project-tool.py -k -v --template 'Cocoa-Python Document-based Application/CocoaDocApp.xcodeproj/TemplateInfo.plist' 'Cocoa-Python Document-based Application' '$(DSTROOT)$(APPTEMPLATES)/Cocoa-Python Document-based Application'

install-docs:
	$(INSTALL) -d '$(DSTROOT)$(DOCS)'
	@set -x && \
	for e in `find "$(OBJROOT)/$(Project)/stable" -name Doc \! -empty -maxdepth 2`; do \
	    d=`dirname $$e` && \
	    n=`basename $$d` && \
	    case $$n in \
	    pyobjc-core) \
		rsync -rplt $$e/ "$(DSTROOT)$(DOCS)" || exit 1;; \
	    pyobjc-*) \
		b=`echo $$n | sed 's/^.*-//'` && \
		rsync -rplt $$e/ "$(DSTROOT)$(DOCS)/$$b" || exit 1;; \
	    esac \
	done

install-examples:
	$(INSTALL) -d '$(DSTROOT)$(EXAMPLES)'
	@set -x && \
	for e in `find "$(OBJROOT)/$(Project)/stable" -name Examples \! -empty -maxdepth 2`; do \
	    d=`dirname $$e` && \
	    n=`basename $$d` && \
	    case $$n in \
	    pyobjc-core) \
		rsync -rplt $$e/ "$(DSTROOT)$(EXAMPLES)" || exit 1;; \
	    pyobjc-*) \
		b=`echo $$n | sed 's/^.*-//'` && \
		rsync -rplt $$e/ "$(DSTROOT)$(EXAMPLES)/$$b" || exit 1;; \
	    esac \
	done

# 5171430 - PyObjC's CoreGraphics conflicts with Apple's CoreGraphics, so
# remove ours until we come up with a better solution
remove-CoreGraphics:
	$(RMDIR) $(DSTROOT)$(EXTRAS)/lib/python/PyObjC/CoreGraphics

fix_strip:
	@echo ======== fix verification errors =========
	@echo '=== strip .so files ==='
	find "$(DSTROOT)" -name \*.so -print -exec $(STRIP) -x {} ';'

fix_bogus_files:
	@echo '=== fix bogus_files ==='
	find -d "$(DSTROOT)$(EXAMPLES)" -name '*~.nib' -print -exec rm -rf {} ';'

fix_verification-errors: fix_strip fix_bogus_files

install:: copysource install-plist real-install install-templates install-docs install-examples remove-CoreGraphics fix_verification-errors
