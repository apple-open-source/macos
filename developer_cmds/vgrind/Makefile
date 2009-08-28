Project = vfontedpr
Install_Dir = /usr/libexec

HFILES = extern.h pathnames.h
CFILES = regexp.c vfontedpr.c vgrindefs.c
MANPAGES = vgrind.1 vgrindefs.5

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/bin
	$(INSTALL_SCRIPT) -c vgrind.sh $(DSTROOT)/usr/bin/vgrind
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/share/groff/site-tmac
	$(INSTALL_FILE) -c tmac.vgrind $(DSTROOT)/usr/share/groff/site-tmac/tmac.vgrind
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/share/misc
	cap_mkdb -f $(DSTROOT)/usr/share/misc/vgrindefs vgrindefs.src
	$(INSTALL_FILE) -c vgrindefs.src $(DSTROOT)/usr/share/misc/vgrindefs
	$(LN) -f $(DSTROOT)/usr/share/man/man1/vgrind.1 $(DSTROOT)/usr/share/man/man1/vfontedpr.1
