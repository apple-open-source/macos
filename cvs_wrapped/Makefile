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
GnuAfterInstall	      = install-man-pages cleanup install-misc

# 4568501
Extra_CC_Flags        = -D_NONSTD_SOURCE

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
	mkdir -p $(DSTROOT)/usr/local/bin
	for script in diff-branch make-branch merge-branch revert view-diffs; do \
		mv $(DSTROOT)/usr/bin/cvs-$${script} $(DSTROOT)/usr/local/bin/cvs-$${script}; \
		ln -s cvs-$${script} $(DSTROOT)/usr/local/bin/ocvs-$${script}; \
	done

install-misc:
	$(MKDIR) $(DSTROOT)/Developer/Extras $(DSTROOT)/Developer/Tools
	$(INSTALL) $(SRCROOT)/misc/cvswrappers $(DSTROOT)/Developer/Extras
	$(INSTALL) $(SRCROOT)/misc/cvs-wrap $(DSTROOT)/usr/bin
	$(LN) -s ../../usr/bin/cvs-wrap $(DSTROOT)/Developer/Tools/cvs-wrap
	$(INSTALL) $(SRCROOT)/misc/cvs-unwrap $(DSTROOT)/usr/bin
	$(LN) -s ../../usr/bin/cvs-unwrap $(DSTROOT)/Developer/Tools/cvs-unwrap
