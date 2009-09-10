##
# Makefile for CVS
##

# Project info
Project               = cvs_wrapped
Install_Prefix        = /usr/local
Install_Man           = $(Install_Prefix)/share/man
Extra_Configure_Flags = --with-gssapi
UserType              = Developer
ToolType              = Commands
GnuAfterInstall	      = install-man-pages cleanup install-misc

# 4568501
Extra_CC_Flags        = -D_NONSTD_SOURCE

Extra_CC_Flags       += -D_DARWIN_NO_64_BIT_INODE

# These will get deleted.
Install_HTML          = /Developer

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

install-man-pages:
	rm -rf $(DSTROOT)/Developer

cleanup:
	rm -f $(DSTROOT)$(Install_Prefix)/bin/cvsbug $(DSTROOT)$(Install_Prefix)/bin/rcs2log
	rm -f $(DSTROOT)$(Install_Man)/man5/cvs.5
	rm -f $(DSTROOT)$(Install_Man)/man8/cvsbug.8
	mv $(DSTROOT)$(Install_Prefix)/bin/cvs $(DSTROOT)$(Install_Prefix)/bin/ocvs
	-rmdir $(DSTROOT)$(Install_Man)/man{5,8}
	chmod -x $(DSTROOT)$(Install_Man)/man1/ocvs.1
	for script in diff-branch make-branch merge-branch revert view-diffs; do \
		ln -s cvs-$${script} $(DSTROOT)$(Install_Prefix)/bin/ocvs-$${script}; \
	done

install-misc:
	$(MKDIR) $(DSTROOT)/Developer/Extras $(DSTROOT)/Developer/Tools
	$(INSTALL) $(SRCROOT)/misc/cvswrappers $(DSTROOT)/Developer/Extras
	$(INSTALL) $(SRCROOT)/misc/cvs-wrap $(DSTROOT)$(Install_Prefix)/bin
	$(LN) -s ../..$(Install_Prefix)/bin/cvs-wrap $(DSTROOT)/Developer/Tools/cvs-wrap
	$(INSTALL) $(SRCROOT)/misc/cvs-unwrap $(DSTROOT)$(Install_Prefix)/bin
	$(LN) -s ../..$(Install_Prefix)/bin/cvs-unwrap $(DSTROOT)/Developer/Tools/cvs-unwrap
