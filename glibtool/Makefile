##
# Makefile for glibtool
##

# Project info
Project               = glibtool
UserType              = Developer
ToolType              = Commands
Extra_Configure_Flags = --program-prefix="g"
GnuAfterInstall	      = fixbin install-man-pages install-plist cleanup-html

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install-strip install-html

USRBIN = /usr/bin
fixbin:
	ed - $(DSTROOT)$(USRBIN)/glibtool < $(SRCROOT)/fix/glibtool.ed
	ed - $(DSTROOT)$(USRBIN)/glibtoolize < $(SRCROOT)/fix/glibtoolize.ed

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

MAN1 = $(DSTROOT)/usr/share/man/man1

install-man-pages:
	$(MKDIR) $(MAN1)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).1 $(MAN1)
	$(LN) $(MAN1)/$(Project).1 $(MAN1)/glibtoolize.1

# install-html puts the html files in a libtool.html subdirectory in
# $(RC_Install_HTML).  We remove this subdirectory (move the contents up).
# Both Index.html and index.html files are created, so we rename Index.html
# to FullIndex.html, and patch the other html files appropriately.
SUBDIR = libtool.html
OLDNAME = Index.html
OLDNAMEREGEX = $(subst .,\.,$(OLDNAME))
NEWNAME = FullIndex.html
cleanup-html:
	cd $(RC_Install_HTML)/$(SUBDIR) && \
	$(MV) * .. && \
	$(RMDIR) $(RC_Install_HTML)/$(SUBDIR)
	@set -x && \
	cd $(RC_Install_HTML) && \
	$(MV) $(OLDNAME) $(NEWNAME) && \
	for i in `fgrep -l $(OLDNAME) *`; do \
	    sed 's/$(OLDNAMEREGEX)/$(NEWNAME)/g' $$i > $$i.$$$$ && \
	    $(MV) $$i.$$$$ $$i || exit 1; \
	done
