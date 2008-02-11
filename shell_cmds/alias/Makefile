Project = alias
Install_Dir = /usr/bin

MANPAGES = alias.1

Extra_CC_Flags = -Wall -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

BUILTINS = bg cd command fc fg getopts hash jobs read type ulimit umask unalias wait

after_install:
	$(INSTALL_SCRIPT) generic.sh $(DSTROOT)$(Install_Dir)/alias
	@for builtin in $(BUILTINS); do \
		$(LN) $(DSTROOT)/$(Install_Dir)/alias \
			"$(DSTROOT)/$(Install_Dir)/$${builtin}" ; \
	done
#		$(LN) $(DSTROOT)/usr/share/man/man1/alias.1 \
#			"$(DSTROOT)/usr/share/man/man1/$${builtin}.1" ; \
