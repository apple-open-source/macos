##
# Makefile for CVS
##

# Project info
Project               = cvs_wrapped
Install_Prefix        = /usr
Install_Man           = /usr/share/man
Extra_Configure_Flags = --with-gssapi
UserType              = Developer
ToolType              = Commands
GnuAfterInstall	      = install-man-pages cleanup

# These will get deleted.
Install_HTML          = /Developer

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

install-man-pages:
	rm -rf $(DSTROOT)/Developer

cleanup:
	rm -f $(DSTROOT)/usr/bin/cvsbug $(DSTROOT)/usr/bin/rcs2log
	rm -f $(DSTROOT)/usr/share/man/man5/cvs.5
	rm -f $(DSTROOT)/usr/share/man/man8/cvsbug.8
	mv $(DSTROOT)/usr/bin/cvs $(DSTROOT)/usr/bin/ocvs
	-rmdir $(DSTROOT)/usr/share/man/man{5,8}
	chmod -x $(DSTROOT)/usr/share/man/man1/ocvs.1
	ln -s /usr/bin/cvs-diff-branch $(DSTROOT)/usr/bin/ocvs-diff-branch
	ln -s /usr/bin/cvs-make-branch $(DSTROOT)/usr/bin/ocvs-make-branch
	ln -s /usr/bin/cvs-merge-branch $(DSTROOT)/usr/bin/ocvs-merge-branch
	ln -s /usr/bin/cvs-revert      $(DSTROOT)/usr/bin/ocvs-revert
	ln -s /usr/bin/cvs-view-diffs  $(DSTROOT)/usr/bin/ocvs-view-diffs

