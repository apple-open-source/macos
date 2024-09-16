##
# Makefile for ksh
##

Project = ksh

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Version = 2012-08-01
Sources = $(SRCROOT)/$(Project)

#find ksh overrides before libSystem
EXTRA_LDFLAGS = -Wl,-search_paths_first

PASS_CCFLAGS = $(CC_Debug) $(CC_Optimize) $(CC_Other) -DSHOPT_SPAWN=0 -D_ast_int8_t=int64_t -D_lib_memccpy
ARCH_CCFLAGS = $(CC_Archs)

build:: shadow_source
	cd $(BuildDirectory) && LDFLAGS="$(EXTRA_LDFLAGS)" CCFLAGS="$(PASS_CCFLAGS) $(EXTRA_CCFLAGS)" ARCH_CCFLAGS="$(ARCH_CCFLAGS) $(CC_Debug) $(CC_Optimize)" ./bin/package make SHELL=$(SHELL)
	@fgrep '*** exit code' $(OBJROOT)/arch/$(shell $(BuildDirectory)/bin/package)/lib/package/gen/make.out && exit 1 || exit 0
	cd $(BuildDirectory) && ./bin/package results SHELL=$(SHELL)

KSH_ARCH = $(shell $(BuildDirectory)/bin/package)
OSV      = $(DSTROOT)/usr/local/OpenSourceVersions
OSL      = $(DSTROOT)/usr/local/OpenSourceLicenses

install::
	$(CP) $(BuildDirectory)/arch/$(KSH_ARCH)/bin/ksh $(SYMROOT)/ksh
	$(DSYMUTIL) $(SYMROOT)/ksh
	$(INSTALL_DIRECTORY) $(DSTROOT)/bin
	$(INSTALL_PROGRAM) $(BuildDirectory)/arch/$(KSH_ARCH)/bin/ksh $(DSTROOT)/bin/ksh
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/share/man/man1
	$(INSTALL_FILE) $(BuildDirectory)/arch/$(KSH_ARCH)/man/man1/sh.1 $(DSTROOT)/usr/share/man/man1/ksh.1
	$(INSTALL_DIRECTORY) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/
	$(INSTALL_DIRECTORY) $(OSL)
	$(INSTALL_FILE) $(Project).txt $(OSL)/
