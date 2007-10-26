##
# Makefile for libedit
##

Project             = libedit
Extra_Install_Flags = PREFIX=$(DSTROOT)$(Install_Prefix)
GnuAfterInstall     = install-plist fix-install cleanup

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target      = install
export LIBTOOL_CMD_SEP = 

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(Sources)/src/el.c | $(SED) -e '1,2d' -e '/^$$/,$$d' $(Sources)/src/el.c > $(OSL)/$(Project).txt

fix-install:
	$(MKDIR) $(DSTROOT)/usr/include/readline
	$(LN) -fs ../editline/readline.h $(DSTROOT)/usr/include/readline/readline.h
	$(LN) -fs ../editline/readline.h $(DSTROOT)/usr/include/readline/history.h
	$(MKDIR) $(DSTROOT)/usr/local/lib
	$(MV) $(DSTROOT)/usr/lib/libedit.a $(DSTROOT)/usr/lib/libedit.la $(DSTROOT)/usr/local/lib
	$(LN) -fs libedit.a $(DSTROOT)/usr/local/lib/libreadline.a
	@set -x && \
	cd $(DSTROOT)/usr/lib && \
	$(RM) libedit.dylib libedit.*.*.*.dylib && \
	$(MV) libedit.*.dylib libedit.$(basename $(AEP_Version)).dylib && \
	install_name_tool -id /usr/lib/libedit.$(basename $(AEP_Version)).dylib libedit.*.dylib && \
	$(CP) libedit.*.dylib $(SYMROOT) && \
	$(STRIP) -x libedit.*.dylib && \
	$(LN) -fs libedit.*.dylib libreadline.dylib && \
	$(LN) -fs libedit.*.dylib libedit.dylib && \
	$(LN) -fs libedit.*.dylib libedit.$(AEP_Version).dylib

cleanup:
	$(RMDIR) $(SRCROOT)/$(Project)/autom4te.cache

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 2.9
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
#AEP_Filename   = $(AEP_ProjVers).tar.gz
#AEP_ExtractDir = $(AEP_ProjVers)
AEP_Filename   = $(AEP_ExtractDir).tar.gz
AEP_ExtractDir = libedit-20060829-2.9
AEP_Patches    = patch-configure.ac \
                 patch-el_push \
		 patch-histedit.h \
		 patch-history.c \
		 patch-readline.c \
		 patch-readline.h

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	@set -x && for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
	done
	@set -x && \
	cd $(SRCROOT)/$(Project) && \
	aclocal && \
	glibtoolize --force -c && \
	automake && \
	autoheader && \
	autoconf && \
	$(RMDIR) autom4te.cache
endif

install_headers:: shadow_source configure
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory)/src $(Environment) $(Install_Flags) install-nobase_includeHEADERS
	$(MKDIR) $(DSTROOT)/usr/include/readline
	$(LN) -fs ../editline/readline.h $(DSTROOT)/usr/include/readline/readline.h
	$(LN) -fs ../editline/readline.h $(DSTROOT)/usr/include/readline/history.h
