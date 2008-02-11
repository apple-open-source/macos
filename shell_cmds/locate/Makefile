Project = locate
Install_Dir = /usr/bin

SubProjects = bigram code locate
MANPAGES = locate.code.8

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) $(DSTROOT)/usr/share/man/man8/locate.code.8 \
		$(DSTROOT)/usr/share/man/man8/locate.bigram.8
