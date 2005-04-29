##
# Makefile for ksh
##

Project = ksh

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Version = 2004-02-29
Sources = $(SRCROOT)/$(Project)

EXTRA_CCFLAGS = -mdynamic-no-pic
#find ksh overrides before libSystem
EXTRA_LDFLAGS = -Wl,-search_paths_first

install_source::
	$(MKDIR) $(Sources)
	$(TAR) -C $(Sources) -zxf $(SRCROOT)/INIT.$(Version).tgz
	$(TAR) -C $(Sources) -zxf $(SRCROOT)/ast-ksh.$(Version).tgz
	for project in cmd/ksh93 lib/libast lib/libcmd lib/libdll; do \
		ed - $(Sources)/src/$$project/Mamfile < $(SRCROOT)/patches/add_ARCH_CCFLAGS.ed; \
	done
	cd $(Sources) && patch -p0 < $(SRCROOT)/patches/src__cmd__ksh93__sh.1.diff

PASS_CCFLAGS = $(CC_Debug) $(CC_Optimize) $(CC_Other)
ARCH_CCFLAGS = $(CC_Archs)

build:: shadow_source
	cd $(BuildDirectory) && LDFLAGS="$(EXTRA_LDFLAGS)" CCFLAGS="$(PASS_CCFLAGS) $(EXTRA_CCFLAGS)" ARCH_CCFLAGS="$(ARCH_CCFLAGS)" ./bin/package DEBUG make SHELL=$(SHELL)

KSH_ARCH = $(shell $(BuildDirectory)/bin/package host)
OSV      = $(DSTROOT)/usr/local/OpenSourceVersions
OSL      = $(DSTROOT)/usr/local/OpenSourceLicenses

install::
	$(MKDIR) $(DSTROOT)/bin
	$(INSTALL_PROGRAM) $(BuildDirectory)/arch/$(KSH_ARCH)/bin/ksh \
		$(DSTROOT)/bin/ksh
	$(MKDIR) $(DSTROOT)/usr/share/man/man1
	$(INSTALL_FILE) $(BuildDirectory)/arch/$(KSH_ARCH)/man/man1/sh.1 \
		$(DSTROOT)/usr/share/man/man1/ksh.1
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/lib/package/LICENSES/ast $(OSL)/$(Project).txt
