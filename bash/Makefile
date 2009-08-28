Project  = bash
UserType = Administration
ToolType = Commands

#COPY_SOURCES=YES

Extra_CC_Flags        = -no-cpp-precomp -mdynamic-no-pic -DM_UNIX -DSSH_SOURCE_BASHRC
# SIZEOF_LONG and SIZEOF_CHAR_P are unused
# WORDS_BIGENDIAN is also unused in this configuration
Extra_Configure_Flags = --bindir=/bin \
	--mandir=/usr/share \
	--enable-debugger \
	ac_cv_sizeof_long=0 \
	ac_cv_sizeof_char_p=0 \
	ac_cv_c_bigendian=no
Extra_Install_Flags   = bindir=$(DSTROOT)/bin
Extra_LD_Flags        = -Wl,-search_paths_first -dead_strip
GnuAfterInstall       = after-install
# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
# Install in /usr
USRDIR	       = /usr

ifneq ($(SDKROOT),)
Extra_CC_Flags += -isysroot "$(SDKROOT)"
Extra_LD_Flags += -Wl,-syslibroot,"$(SDKROOT)"
endif

# Bash makefiles are a bit screwy...
# Setting CCFLAGS upsets bash, so we override Environment
# so that it doesn't.
# CFLAGS_FOR_BUILD is defined as "-g" in bash
# and LDFLAGS_FOR_BUILD=$(CFLAGS) which has the -arch flags
# so redefine it to match
Environment = \
	CFLAGS="$(CFLAGS)"  \
        LDFLAGS="$(LDFLAGS)" \
	LDFLAGS_FOR_BUILD="-g" \
        $(Extra_Environment)

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

# Automatic Extract & Patch
AEP_Project    = $(Project)
AEP_Version    = 3.2
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = $(shell jot -w bash32-%03d 48) \
	doc_Makefile.in.diff shell.c.diff execute_cmd.c.diff \
	text.c.diff setid.diff shell-_BASH_IMPLICIT_DASH_PEE.c.diff \
	display.c.diff conftypes.h.diff strtrans.c.diff

# Extract the source.
install_source::
	$(TAR) -C "$(SRCROOT)" -zxf "$(SRCROOT)/$(AEP_Filename)"
	$(RMDIR) "$(SRCROOT)/$(AEP_Project)"
	$(MV) "$(SRCROOT)/$(AEP_ExtractDir)" "$(SRCROOT)/$(AEP_Project)"
	@for patchfile in $(AEP_Patches); do \
		echo Applying $$patchfile; \
		cd "$(SRCROOT)/$(Project)" && patch -lp0 < "$(SRCROOT)/patches/$$patchfile" || exit 1; \
	done

after-install:
	$(INSTALL_DIRECTORY) "$(DSTROOT)/usr/bin"
	$(MV) "$(DSTROOT)/bin/bashbug" "$(DSTROOT)/usr/bin"
	$(INSTALL_DIRECTORY) "$(DSTROOT)/usr/local/bin"
	$(LN) -sf /bin/bash "$(DSTROOT)/usr/local/bin/bash"
#build/install /bin/sh
	$(MKDIR) -p "$(OBJROOT)/sh"
	$(MAKE) OBJROOT=$(OBJROOT)/sh configure build GnuAfterInstall= Extra_Configure_Flags="$(Extra_Configure_Flags) --enable-strict-posix-default" GnuNoInstall=YES
	$(CP) "$(OBJROOT)/bash" "$(SYMROOT)"
	$(CP) "$(OBJROOT)/sh/bash" "$(SYMROOT)/sh"
	$(INSTALL_PROGRAM) "$(OBJROOT)/sh/bash" "$(DSTROOT)/bin/sh"
	$(INSTALL_DIRECTORY) "$(DSTROOT)/private/etc"
	$(INSTALL_FILE) "$(SRCROOT)/bashrc" "$(DSTROOT)/private/etc/bashrc"
	$(INSTALL_FILE) "$(SRCROOT)/profile" "$(DSTROOT)/private/etc/profile"
	$(RM) -rf "$(DSTROOT)/usr/html"
	$(LN) -sf bash.1 "$(DSTROOT)/usr/share/man/man1/sh.1"
	$(INSTALL_DIRECTORY) "$(DSTROOT)/usr/share/doc/bash"
	cd "$(SRCROOT)/doc"; $(INSTALL_FILE) *.pdf *.html "$(DSTROOT)/usr/share/doc/bash"
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) "$(SRCROOT)/$(Project).plist" $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
	$(RM) "$(DSTROOT)/usr/share/info/dir"
	find -d "$(DSTROOT)" -type d -empty -exec rmdir '{}' ';'
#reset owners of newly installed files.
	$(_v)- $(CHOWN) -R $(Install_User):$(Install_Group)  "$(DSTROOT)" "$(SYMROOT)"
