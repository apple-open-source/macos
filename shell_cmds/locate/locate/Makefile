Project = locate
Install_Dir = /usr/bin

HFILES = locate.h pathnames.h
CFILES = util.c locate.c
MANPAGES = locate.1 locate.updatedb.8
#OTHERSRCS = Makefile Makefile.preamble Makefile.postamble \
            fastfind.c \
            locate.1 locate.updatedb.8 locate.rc \
            concatdb.sh mklocatedb.sh updatedb.sh

Extra_CC_Flags = -Wall -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/libexec
	$(INSTALL_SCRIPT) concatdb.sh $(DSTROOT)/usr/libexec/locate.concatdb
	$(INSTALL_SCRIPT) mklocatedb.sh $(DSTROOT)/usr/libexec/locate.mklocatedb
	$(INSTALL_SCRIPT) updatedb.sh $(DSTROOT)/usr/libexec/locate.updatedb
	$(LN) $(DSTROOT)/usr/share/man/man8/locate.updatedb.8 $(DSTROOT)/usr/share/man/man8/locate.concatdb.8
	$(LN) $(DSTROOT)/usr/share/man/man8/locate.updatedb.8 $(DSTROOT)/usr/share/man/man8/locate.mklocatedb.8
	$(INSTALL_DIRECTORY) $(DSTROOT)/private/etc
	$(INSTALL_FILE) locate.rc $(DSTROOT)/private/etc
