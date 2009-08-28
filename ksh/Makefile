##
# Makefile for ksh
##

Project = ksh

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Version = 2007-11-05
Sources = $(SRCROOT)/$(Project)
Patches=src__cmd__ksh93__sh.1.diff \
	src__lib__libast__features__lib.diff \
	src__lib__libast__features__common.diff \
	src__lib__libast__features__common-2.diff \
	src__lib__libast__features__common-3.diff \
	bin__package.diff \
	src__cmd__INIT__package.sh.diff \
	src__cmd__ksh93__jobs.c.diff \
	src__lib__libast__string__strmatch.c.diff

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
	for p in $(Patches); do \
		cd $(Sources) && patch -p0 < $(SRCROOT)/patches/$$p || exit 1; \
	done

PASS_CCFLAGS = $(CC_Debug) $(CC_Optimize) $(CC_Other) -DSHOPT_SPAWN=0
ARCH_CCFLAGS = $(CC_Archs)

build:: shadow_source
	cd $(BuildDirectory) && LDFLAGS="$(EXTRA_LDFLAGS)" CCFLAGS="$(PASS_CCFLAGS) $(EXTRA_CCFLAGS)" ARCH_CCFLAGS="$(ARCH_CCFLAGS)" ./bin/package DEBUG make SHELL=$(SHELL) AR=/Developer/Makefiles/bin/ar.sh
	@grep -q '*** exit code' $(OBJROOT)/arch/darwin.*/lib/package/gen/make.out && exit 1 || exit 0

KSH_ARCH = $(shell $(BuildDirectory)/bin/package host)
OSV      = $(DSTROOT)/usr/local/OpenSourceVersions
OSL      = $(DSTROOT)/usr/local/OpenSourceLicenses

install::
	$(CP) $(BuildDirectory)/arch/$(KSH_ARCH)/bin/ksh $(SYMROOT)/ksh
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
