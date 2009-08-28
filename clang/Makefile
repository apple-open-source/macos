##
# cxxfilt Makefile
##

# Project info
Project               = clang
UserType              = Developer
ToolType              = Commands
Install_Prefix	      = /Developer/usr
Extra_Configure_Flags = --enable-targets=x86 \
			--enable-optimized \
			--disable-assertions \
			--with-extra-options='-g -DDISABLE_SMART_POINTERS' \
			--disable-pic \
			--disable-doxygen \
			$(HOST_TARGET_FLAGS)
Extra_CC_Flags        = -mdynamic-no-pic
GnuAfterInstall       = post-install install-plist

# It's a GNU Source project
include ./GNUSource.make

CXX := /usr/bin/g++

# Automatic Extract & Patch
#AEP            = YES
#AEP_Project    = clang
#AEP_Version    = 090201
#AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
#AEP_Filename   = $(AEP_ProjVers).tar.bz2
#AEP_ExtractDir = $(AEP_ProjVers)
#AEP_Patches    = 

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

Install_Target = KEEP_SYMBOLS=1 -s --no-print-directory install-clang
Build_Target = KEEP_SYMBOLS=1 -s --no-print-directory clang-only

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && \
		patch -p0 < $(SRCROOT)/patches/$$patchfile || exit 1 ;  \
	done
endif

# Remove the parts of the destroot we don't need
post-install:
	$(MKDIR) -p $(DSTROOT)/usr/bin
	ln -sf ../../Developer/usr/bin/clang $(DSTROOT)/usr/bin/clang

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE.TXT $(OSL)/$(Project)-llvm.txt
	$(INSTALL_FILE) $(Sources)/tools/clang/LICENSE.TXT $(OSL)/$(Project).txt

SVN_BASE = $(shell svn info | sed -n 's/^URL: //; s,/llvm-project/.*$$,/llvm-project,p')
SVN_CLANG = $(shell svn info | sed -n 's/^URL: //p')
SVN_TAGS = $(shell dirname $(SVN_CLANG))

update-clang:
	if [ -n "$(LLVM_VERSION)" ]; then \
	  svn rm -m 'Update.' $(SVN_CLANG)/clang; \
	  svn cp -m 'Update.' $(SVN_BASE)/llvm/tags/Apple/llvmCore-$(LLVM_VERSION) $(SVN_CLANG)/clang; \
	  svn cp -m 'Update.' $(SVN_BASE)/cfe/branches/Apple/Dib $(SVN_CLANG)/clang/tools/clang; \
	else \
	  echo "Usage: make LLVM_VERSION=2108 update-clang"; \
	fi

update-clang-trunk:
	svn rm -m 'Update.' $(SVN_CLANG)/clang
	svn cp -m 'Update.' $(SVN_BASE)/llvm/trunk $(SVN_CLANG)/clang
	svn cp -m 'Update.' $(SVN_BASE)/cfe/trunk $(SVN_CLANG)/clang/tools/clang

update-to-tot:
	rm -rf clang
	svn co $(SVN_BASE)/llvm/trunk clang
	svn co $(SVN_BASE)/cfe/trunk clang/tools/clang

tag-clang:
	if [ -n "$(VERSION)" ]; then \
	  svn cp -m 'Tag.' $(SVN_TAGS)/clang $(SVN_TAGS)/clang-$(VERSION); \
	else \
	  echo Usage: make VERSION=25 tag-clang; \
	fi

retag-clang:
	if [ -n "$(VERSION)" ]; then \
	  svn rm -m 'Retag.' $(SVN_TAGS)/clang-$(VERSION) && \
	  svn cp -m 'Retag.' $(SVN_TAGS)/clang $(SVN_TAGS)/clang-$(VERSION); \
	else \
	  echo Usage: make VERSION=25 retag-clang; \
	fi

build-and-test: fast-build

SUDO := sudo

fast-build:
	svn up .
	cd .. && BUILDIT_DIR=`pwd`/build $(SUDO) /usr/local/bin/buildit -arch i386 -arch x86_64 -noinstallsrc -noinstallhdrs -nosum -noverify `pwd`/clang -noclean

install-check-i386: llvm-binaries-i386
	cd ../build/clang.roots/clang~obj/i386/tools/clang && { CLANG=/Developer/usr/bin/clang CLANGCC=/Developer/usr/libexec/clang-cc $(SUDO) make VERBOSE=0 test && echo PASS: $@ || { echo FAIL: $@; make report; } }
install-check-x86_64: llvm-binaries-x86_64
	cd ../build/clang.roots/clang~obj/x86_64/tools/clang && { CLANG=/Developer/usr/bin/clang CLANGCC=/Developer/usr/libexec/clang-cc $(SUDO) make VERBOSE=0 test && echo PASS: $@ || { echo FAIL: $@; make report; } }

check: llvm-binaries-i386 llvm-binaries-x86_64 check-i386 check-x86_64 unittests-i386 unittests-x86_64 clang-i386 clang-x86_64
llvm-binaries-i386:
	cd ../build/clang.roots/clang~obj/i386 && $(SUDO) make DESTDIR=`pwd`/../../../llvm-i386 install
llvm-binaries-x86_64:
	cd ../build/clang.roots/clang~obj/x86_64 && $(SUDO) make DESTDIR=`pwd`/../../../llvm-x86_64 install
check-i386: llvm-binaries-i386
	cd ../build/clang.roots/clang~obj/i386 && $(SUDO) make check && echo PASS: $@ || echo FAIL: $@
check-x86_64: llvm-binaries-x86_64
	cd ../build/clang.roots/clang~obj/x86_64 && $(SUDO) make check && echo PASS: $@ || echo FAIL: $@
unittests-i386: llvm-binaries-i386
	cd ../build/clang.roots/clang~obj/i386 && $(SUDO) make unittests && echo PASS: $@ || echo FAIL: $@
unittests-x86_64: llvm-binaries-x86_64
	cd ../build/clang.roots/clang~obj/x86_64 && $(SUDO) make unittests && echo PASS: $@ || echo FAIL: $@
clang-i386: llvm-binaries-i386
	pwd=`pwd`; cd ../build/clang.roots/clang~obj/i386/tools/clang && { CLANG=$$pwd/../build/llvm-i386/Developer/usr/bin/clang CLANGCC=$$pwd/../build/llvm-i386/Developer/usr/libexec/clang-cc $(SUDO) make VERBOSE=0 test && echo PASS: $@ || { echo FAIL: $@; make report; } }
clang-x86_64: llvm-binaries-x86_64
	pwd=`pwd`; cd ../build/clang.roots/clang~obj/x86_64/tools/clang && { CLANG=$$pwd/../build/llvm-x86_64/Developer/usr/bin/clang CLANGCC=$$pwd/../build/llvm-x86_64/Developer/usr/libexec/clang-cc $(SUDO) make VERBOSE=0 test && echo PASS: $@ || { echo FAIL: $@; make report; } }
