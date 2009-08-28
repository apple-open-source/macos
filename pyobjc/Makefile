#
# xbs-compatible Makefile for pyobjc.
#

#GCC_VERSION := $(shell cc -dumpversion | sed -e 's/^\([^.]*\.[^.]*\).*/\1/')
#GCC_42 := $(shell perl -e "print ($(GCC_VERSION) >= 4.2 ? 'YES' : 'NO')")

GnuNoConfigure      = YES
Extra_CC_Flags      = -no-cpp-precomp -g
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
AEP_Version    = trunk-20090623
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = fixup.diff PR-6090358-remove-sync.diff parser-fixes.diff NSString.diff float.diff pyobjc-framework-Cocoa_setup.py.diff objc-class.m.diff CGFloat.diff

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xof $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	@set -x && \
	cd $(SRCROOT)/$(Project) && \
	for patchfile in $(AEP_Patches); do \
	    patch -p0 -i $(SRCROOT)/patches/$$patchfile || exit 1; \
	done && \
	for patchfile in `find . -name pyobjc-api.h | xargs fgrep -l __LP64__`; do \
	    mv -f $$patchfile $$patchfile.orig && \
	    { unifdef -DCGFLOAT_DEFINED -DNSINTEGER_DEFINED $$patchfile.orig > $$patchfile || [ $$? -ne 2 ]; } || exit 1; \
	done
	ed - $(SRCROOT)/$(Project)/pyobjc-core/setup.py < '$(SRCROOT)/patches/pyobjc-core_setup.py.ed'
	@set -x && cd '$(SRCROOT)/$(Project)/pyobjc-xcode/Project Templates' && \
	for patchfile in `find . -name project.pbxproj -print0 | xargs -0 fgrep -l MacOSX10.5.sdk | sed 's/ /@/g'`; do \
	    ed - "`echo $$patchfile | sed 's/@/ /g'`" < $(SRCROOT)/patches/PR-6150200-10.6sdk.ed || exit 1; \
	done
	@set -x && for z in `find $(SRCROOT)/$(Project) -name \*.py -size 0c`; do \
	    echo '#' > $$z || exit 1; \
	done
	find $(SRCROOT)/$(Project) -name main -print -delete
	find $(SRCROOT)/$(Project) -name \*.so -print -delete
endif

copysource:
	ditto '$(SRCROOT)' '$(OBJROOT)'

DOCS=/Developer/Documentation/Python/PyObjC
EXAMPLES=/Developer/Examples/Python/PyObjC
EXTRAS=$(shell python -c "import sys, os;print os.path.join(sys.prefix, 'Extras')")
INIT = $(OBJROOT)/init.py

XCODESUPPORT = pyobjc-xcode

real-install:
	echo import site > "$(INIT)"
	echo 'site.addsitedir("$(DSTROOT)$(EXTRAS)/lib/python")' >> "$(INIT)"
	@set -x && \
	cd '$(OBJROOT)/$(Project)' && \
	for pkg in pyobjc-core pyobjc-framework-*; do \
	    cd "$(OBJROOT)/$(Project)/$$pkg" && \
	    ARCHFLAGS="$(RC_CFLAGS) -D_FORTIFY_SOURCE=0" PYTHONSTARTUP="$(INIT)" \
	    python setup.py install --home="$(EXTRAS)" --root="$(DSTROOT)" || exit 1; \
	done
	@set -x && cd "$(DSTROOT)$(EXTRAS)/lib/python" && \
	install -d PyObjC && \
	for x in *; do \
	    if [ "$$x" != PyObjC -a "$$x" != PyObjC.pth ]; then \
		if [ -e PyObjC/$$x ]; then \
		    ditto $$x PyObjC/$$x && \
		    $(RMDIR) $$x; \
		else \
		    $(MV) "$$x" PyObjC; \
		fi; \
	    fi || exit 1; \
	done

install-docs:
	$(INSTALL) -d '$(DSTROOT)$(DOCS)'
	@set -x && \
	for e in `find "$(OBJROOT)/$(Project)" -name Doc ! -empty ! -path '*pyobjc-metadata*'`; do \
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
	for e in `find "$(OBJROOT)/$(Project)" -name Examples ! -empty -maxdepth 2`; do \
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

fix_strip:
	@echo ======== fix verification errors =========
	@echo '=== strip .so files ==='
	@set -x && cd '$(DSTROOT)' && \
	for i in `find . -name \*.so | sed 's,^\./,,'`; do \
	    rsync -R $$i $(SYMROOT) && \
	    $(STRIP) -x $$i || exit 1; \
	done

fix_bogus_files:
	@echo '=== fix bogus_files ==='
	find -d "$(DSTROOT)$(EXAMPLES)" -name '*~.nib' -print -exec rm -rf {} ';'

fix_inappropriate_executables:
	@echo '=== fix inappropriate_executables ==='
	chmod a-x "$(DSTROOT)$(EXAMPLES)/Quartz/Core Graphics/CGRotation/demo.png"

fix_verification-errors: fix_bogus_files fix_inappropriate_executables fix_strip

install:: copysource install-plist real-install install-docs install-examples fix_verification-errors
