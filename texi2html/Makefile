# Project info
Project = texi2html

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

install::
	@echo "Installing texi2html..."
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(USRBINDIR)"
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(SHAREDIR)"
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(MANDIR)/man1"
	$(_v) $(INSTALL_SCRIPT) -c "$(Project)/texi2html"      "$(DSTROOT)/usr/bin"
	$(_v) $(INSTALL_FILE)   -c "$(Project)/texi2html.init" "$(DSTROOT)/usr/share"
	$(_v) $(LN) "$(DSTROOT)/usr/bin/texi2html" "$(DSTROOT)$(MANDIR)/man1/texi2html.1"
