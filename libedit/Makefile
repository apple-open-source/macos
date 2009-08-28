##
# Makefile for libedit
##

Extra_Install_Flags = PREFIX=$(DSTROOT)$(Install_Prefix)
GnuAfterInstall     = install-plist fix-install

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target      = install

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(SED) -e '1,2d' -e '/^$$/,$$d' $(Sources)/src/el.c > $(OSL)/$(Project).txt

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

install_headers:: shadow_source configure
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory)/src $(Environment) $(Install_Flags) install-nobase_includeHEADERS
	$(MKDIR) $(DSTROOT)/usr/include/readline
	$(LN) -fs ../editline/readline.h $(DSTROOT)/usr/include/readline/readline.h
	$(LN) -fs ../editline/readline.h $(DSTROOT)/usr/include/readline/history.h
