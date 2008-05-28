##
# Makefile for emacs
##

App_Dir=$(DSTROOT)/AppleInternal/Applications
Extra_CC_Flags = -no-cpp-precomp -mdynamic-no-pic -Wno-pointer-sign
Extra_LD_Flags = -Wl,-headerpad,0x1000
Extra_Configure_Flags = --without-x --enable-carbon-app=$(App_Dir) ac_cv_host=mac-apple-darwin

# Project info
Project  = emacs
GNUVersion = 22.1
UserType = Developer
ToolType = Commands
#CommonNoInstallSource = YES
GnuAfterInstall = remove-dir install-dumpemacs cleanup install-plist install-default
GnuNoBuild = YES
# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Automatic Extract & Patch
AEP_Project    = $(Project)
AEP_Version    = $(GNUVersion)
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = Apple.diff unexmacosx.c.diff CVE-2007-6109.diff files.el.diff

# Extract the source.
install_source::
	$(TAR) --exclude "*.elc" --exclude info -C "$(SRCROOT)" -zxf "$(SRCROOT)/$(AEP_Filename)"
	$(RMDIR) "$(SRCROOT)/$(AEP_Project)"
	$(MV) "$(SRCROOT)/$(AEP_ExtractDir)" "$(SRCROOT)/$(AEP_Project)"
	for patchfile in $(AEP_Patches); do \
		cd "$(SRCROOT)/$(Project)" && patch -lp0 < "$(SRCROOT)/patches/$$patchfile" || exit 1; \
	done
	$(CP) "$(SRCROOT)/mac.h" "$(SRCROOT)/$(AEP_Project)/src/m"
	for f in $(EXTRAEL); do $(RM) -f "$(SRCROOT)/$$f"; done

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

EXTRAEL=	emacs/lisp/cus-load.el \
		emacs/lisp/eshell/esh-groups.el \
		emacs/lisp/finder-inf.el \
		emacs/lisp/loaddefs.el \
		emacs/lisp/mh-e/mh-loaddefs.el \
		emacs/lisp/subdirs.el

#installsrc :
#	if test ! -d $(SRCROOT) ; then mkdir -p $(SRCROOT); fi;
# --include to workaround .cvsignores
#	rsync -aC --include '*.ps' --include configure --include Resources --include index.texi ./ $(SRCROOT)/

remove-dir :
	rm $(DSTROOT)/usr/share/info/dir

$(OBJROOT)/bo.h:
	printf "char bo[] = {\n" > $@
	hexdump -ve '1/1 "0x%02x,"' < $(OBJROOT)/src/buildobj.lst >> $@
	printf "};\n" >> $@

$(OBJROOT)/isemacsvalid.o: $(SRCROOT)/isemacsvalid.c
	$(CC) -c $(CFLAGS) -o $@ $^ 

$(OBJROOT)/runit.o: $(SRCROOT)/runit.c
	$(CC) -c $(CFLAGS) -o $@ $^ 

$(OBJROOT)/dumpemacs.o: $(OBJROOT)/bo.h 
	$(CC) -I $(OBJROOT) -DkEmacsVersion=$(patsubst %,'"%"', $(GNUVersion)) $(CFLAGS) -o $@ -g -c $(SRCROOT)/dumpemacs.c

$(SYMROOT)/dumpemacs: $(OBJROOT)/dumpemacs.o $(OBJROOT)/isemacsvalid.o $(OBJROOT)/runit.o
	$(CC) $(CFLAGS) -o $@ -g $^

$(OBJROOT)/emacswrapper.o: $(SRCROOT)/emacswrapper.c
	$(CC) -c $(CFLAGS) -o $@ $^ 

$(SYMROOT)/emacswrapper: $(OBJROOT)/emacswrapper.o $(OBJROOT)/isemacsvalid.o $(OBJROOT)/runit.o
	$(CC) $(CFLAGS) -o $@ -g $^

install-dumpemacs: $(SYMROOT)/dumpemacs $(SYMROOT)/emacswrapper
	$(INSTALL) -s -o root -g wheel -m 4555 $(SYMROOT)/dumpemacs $(DSTROOT)/usr/libexec/dumpemacs
	$(INSTALL) -s -o root -g wheel -m 555 $(SYMROOT)/emacswrapper $(DSTROOT)/usr/bin/emacs
#fixup Emacs.app/Contents/Resources/English.lproj/InfoPlist.strings encoding
	iconv -f US-ASCII -t UTF-16 \
		$(SRCROOT)/emacs/mac/Emacs.app/Contents/Resources/English.lproj/InfoPlist.strings > $(App_Dir)/Emacs.app/Contents/Resources/English.lproj/InfoPlist.strings

build::
	@echo "Bootstraping $(Project)..."
	$(_v) $(MAKE) -C $(BuildDirectory) $(Environment) bootstrap
	@echo "Change default LoadPath for installed emacs-undumped"
	$(MV) $(OBJROOT)/src/lread.o $(OBJROOT)/src/lread.o+save
	$(MV) $(OBJROOT)/src/temacs $(OBJROOT)/src/temacs+save
	$(RM) $(DSTROOT)/usr/bin/emacs-undumped
	$(_v) $(MAKE) -C $(BuildDirectory)/src $(Environment) MYCPPFLAGS=-DEMACS_UNDUMPED temacs
	$(CP) $(OBJROOT)/src/temacs $(OBJROOT)/src/emacs-undumped
	$(CP) $(OBJROOT)/src/emacs-undumped $(SYMROOT)
# Don't rebuild anything else
	$(MV) -f $(OBJROOT)/src/lread.o+save $(OBJROOT)/src/lread.o
	$(MV) -f $(OBJROOT)/src/temacs+save $(OBJROOT)/src/temacs

cleanup:			# Return sources to pristine state
	@echo "Cleaning $(Project)..."
	find $(SRCROOT) -type f -name '*.el[c~]' -delete
	$(RM) -r $(SRCROOT)/emacs/info
	$(RM) "$(DSTROOT)/usr/bin/ctags" "$(DSTROOT)/usr/share/man/man1/ctags.1"
	$(RM) -r "$(DSTROOT)/usr/var"
	for f in $(EXTRAEL); do $(RM) -f "$(SRCROOT)/$$f"; done
	$(LN) -sf /usr/bin/emacs "$(App_Dir)/Emacs.app/Contents/MacOS/Emacs"

install-plist:
	$(INSTALL) -d $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(INSTALL) -d $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

install-default:
	$(INSTALL) -o root -g wheel -m 644 default.el "$(DSTROOT)"/usr/share/emacs/site-lisp
	$(INSTALL) -o root -g wheel -m 644 site-start.el "$(DSTROOT)"/usr/share/emacs/site-lisp
