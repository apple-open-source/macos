#
# xbs-compatible Makefile for pyobjc.
#

#GCC_VERSION := $(shell cc -dumpversion | sed -e 's/^\([^.]*\.[^.]*\).*/\1/')
#GCC_42 := $(shell perl -e "print ($(GCC_VERSION) >= 4.2 ? 'YES' : 'NO')")
PYTHON3 := $(shell python -c 'import sys;print("YES" if sys.version_info[0] > 2 else "NO")')

GnuNoConfigure      = YES
Extra_CC_Flags      = -no-cpp-precomp -g
Extra_Install_Flags = PREFIX=$(RC_Install_Prefix)

#include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Install_Target      = install

# Automatic Extract & Patch
AEP            = YES
AEP_Patches    =

# 10742279 & 10943322: remove deprecated frameworks (don't have those tarball)
DEPRECATED_FRAMEWORKS = pyobjc-framework-ServerNotification pyobjc-framework-CalendarStore

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	@set -x && \
	cd $(SRCROOT) && \
	$(MKDIR) $(Project) && \
	for i in *.tar.*; do \
	    $(TAR) -xof $$i -C $(Project) && \
	    $(RM) $$i || exit 1; \
	done
	@set -x && \
	cd $(SRCROOT)/$(Project) && \
	for patchfile in $(AEP_Patches); do \
	    patch -p0 -i $(SRCROOT)/patches/$$patchfile || exit 1; \
	done && \
	find . \( -name \*.h -or -name \*.m \) -print0 | xargs -0 egrep -l 'CGFLOAT_DEFINED|NSINTEGER_DEFINED' | while read patchfile; do \
	    mv -f "$$patchfile" "$$patchfile.orig" && \
	    { unifdef -DCGFLOAT_DEFINED -DNSINTEGER_DEFINED "$$patchfile.orig" > "$$patchfile" || [ $$? -ne 2 ]; } && \
	    rm -f "$$patchfile.orig" || exit 1; \
	done && \
	find . -name pyobjc_setup.py -print0 | xargs -0 fgrep -l '10.4' | while read patchfile; do \
	    ed - "$$patchfile" < '$(SRCROOT)/patches/pyobjc_setup.py.ed' || exit 1; \
	done && \
	find . -type f -print0 | xargs -0 grep -lw iChat | while read patchfile; do \
	    mv -f "$$patchfile" "$$patchfile.orig" && \
	    sed 's/[[:<:]]iChat[[:>:]]/Messages/g' "$$patchfile.orig" > "$$patchfile" && \
	    rm -f "$$patchfile.orig" || exit 1; \
	done && \
	find . -name \*setup.py -print0 | xargs -0 fgrep -l ".extend(['-isysroot'," | while read patchfile; do \
	    ed - "$$patchfile" < '$(SRCROOT)/patches/isysroot.ed' || exit 1; \
	done
	ed - $(SRCROOT)/$(Project)/pyobjc-core-*/setup.py < '$(SRCROOT)/patches/pyobjc-core_setup.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-core-*/Lib/PyObjCTools/TestSupport.py < '$(SRCROOT)/patches/pyobjc-core_Lib_PyObjCTools_TestSupport.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-core-*/Lib/objc/_bridgesupport.py < '$(SRCROOT)/patches/pyobjc-core_Lib_objc__bridgesupport.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-core-*/Modules/objc/module.m < '$(SRCROOT)/patches/pyobjc-core_Modules_objc_module.m.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-core-*/PyObjCTest/test_identity.py < '$(SRCROOT)/patches/pyobjc-core_PyObjCTest_test_identity.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-core-*/PyObjCTest/test_number_proxy.py < '$(SRCROOT)/patches/pyobjc-core_PyObjCTest_test_number_proxy.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-core-*/PyObjCTest/test_testsupport.py < '$(SRCROOT)/patches/pyobjc-core_PyObjCTest_test_testsupport.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-framework-CFNetwork-*/Lib/CFNetwork/_metadata.py < '$(SRCROOT)/patches/pyobjc-framework-CFNetwork_Lib_CFNetwork__metadata.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-framework-CFNetwork-*/PyObjCTest/test_cfhttpstream.py < '$(SRCROOT)/patches/pyobjc-framework-CFNetwork_PyObjCTest_test_cfhttpstream.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-framework-CFNetwork-*/PyObjCTest/test_cfsocketstream.py < '$(SRCROOT)/patches/pyobjc-framework-CFNetwork_PyObjCTest_test_cfsocketstream.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-framework-Cocoa-*/Lib/Foundation/_metadata.py < '$(SRCROOT)/patches/pyobjc-framework-Cocoa_Lib_Foundation__metadata.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-framework-Cocoa-*/Lib/PyObjCTools/Conversion.py < '$(SRCROOT)/patches/pyobjc-framework-Cocoa_Lib_PyObjCTools_Conversion.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-framework-Cocoa-*/PyObjCTest/test_cfmachport.py < '$(SRCROOT)/patches/pyobjc-framework-Cocoa_PyObjCTest_test_cfmachport.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-framework-LaunchServices-*/PyObjCTest/test_lsinfo.py < '$(SRCROOT)/patches/pyobjc-framework-LaunchServices_PyObjCTest_test_lsinfo.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-framework-Quartz-*/Lib/Quartz/CoreGraphics/_metadata.py < '$(SRCROOT)/patches/pyobjc-framework-Quartz_Lib_Quartz_CoreGraphics__metadata.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-framework-Quartz-*/PyObjCTest/test_camediatiming.py < '$(SRCROOT)/patches/pyobjc-framework-Quartz_PyObjCTest_test_camediatiming.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-framework-Quartz-*/PyObjCTest/test_cgdisplayconfiguration.py < '$(SRCROOT)/patches/pyobjc-framework-Quartz_PyObjCTest_test_cgdisplayconfiguration.py.ed'
	ed - $(SRCROOT)/$(Project)/pyobjc-framework-SystemConfiguration-*/PyObjCTest/test_SCDynamicStoreCopyDHCPInfo.py < '$(SRCROOT)/patches/pyobjc-framework-SystemConfiguration_PyObjCTest_test_SCDynamicStoreCopyDHCPInfo.py.ed'
	@set -x && for z in `find $(SRCROOT)/$(Project) -name \*.py -size 0c`; do \
	    echo '#' > $$z || exit 1; \
	done
	find $(SRCROOT)/$(Project) -name \*.so -print -delete
endif

copysource:
	ditto '$(SRCROOT)' '$(OBJROOT)'
	@set -x && \
	cd '$(OBJROOT)' && \
	remove=`ls | grep '^[0-9]\.[0-9]$$'` && \
	[ -z "$$remove" ] || $(RMDIR) $$remove

custompatching:
ifeq ($(PYTHON3),YES)
	! { unifdef -t -DPY3K -o $(OBJROOT)/$(Project)/pyobjc-framework-Cocoa-*/Lib/PyObjCTools/Conversion.py{,} || \
	[ $$? -ne 1 ]; }
else
	! { unifdef -t -UPY3K -o $(OBJROOT)/$(Project)/pyobjc-framework-Cocoa-*/Lib/PyObjCTools/Conversion.py{,} || \
	[ $$? -ne 1 ]; }
endif

DOCS=/Developer/Documentation/Python/PyObjC
EXAMPLES=/Developer/Examples/Python/PyObjC
EXTRAS:=$(shell python -c "import sys, os;print(os.path.join(sys.prefix, 'Extras'))")
EXTRASLIBPYTHON=$(EXTRAS)/lib/python
EXTRASPYOBJC=$(EXTRASLIBPYTHON)/PyObjC

real-install:
	@set -x && \
	cd '$(OBJROOT)/$(Project)' && \
	for pkg in pyobjc-core* pyobjc-framework-Cocoa* `ls -d pyobjc-framework-* | grep -v pyobjc-framework-Cocoa`; do \
	    cd "$(OBJROOT)/$(Project)/$$pkg" && \
	    ARCHFLAGS="$(RC_CFLAGS) -D_FORTIFY_SOURCE=0" PYTHONPATH="$(DSTROOT)$(EXTRASPYOBJC)" \
	    python setup.py install --home="$(EXTRAS)" --root="$(DSTROOT)" || exit 1; \
	done
	@set -x && cd "$(DSTROOT)$(EXTRASLIBPYTHON)" && \
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
	cp -f $(Project).txt "$(OSL)/$(Project)-$(AEP_Version).txt"
	cp -f $(Project).partial "$(OSV)/$(Project)-$(AEP_Version).partial"

ADDMODULES = AVFoundation
add-module:
	@set -x && \
	for i in $(ADDMODULES); do \
	    install -d $(DSTROOT)$(EXTRASPYOBJC)/$$i && \
	    sed "s/@XXX@/$$i/g" $(SRCROOT)/patches/newmoduletemplate.py > "$(DSTROOT)$(EXTRASPYOBJC)/$$i/__init__.py" && \
	    python -c "import py_compile;py_compile.compile('$(DSTROOT)$(EXTRASPYOBJC)/$$i/__init__.py')" && \
	    chmod 0644 "$(DSTROOT)$(EXTRASPYOBJC)/$$i/__init__.py"* || exit 1; \
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
	find "$(DSTROOT)$(EXAMPLES)" -name '*.png' -perm +0111 -print0 | xargs -0 chmod -vv -x

fix_verification-errors: fix_bogus_files fix_inappropriate_executables fix_strip

install:: copysource custompatching real-install add-module install-docs install-examples fix_verification-errors
