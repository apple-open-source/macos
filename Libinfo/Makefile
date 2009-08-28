Project = info
ProductType = staticlib
Install_Dir = /usr/local/lib/system
BuildDebug = YES
BuildProfile = YES

DIRS = dns gen lookup membership netinfo nis rpc util

SubProjects = $(foreach DIR, $(DIRS), $(DIR).subproj)

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LIBTOOL) -static -o $(SYMROOT)/libinfo.a \
		$(foreach DIR, $(DIRS), $(SYMROOT)/lib$(DIR).a)
	$(LIBTOOL) -static -o $(SYMROOT)/libinfo_debug.a \
		$(foreach DIR, $(DIRS), $(SYMROOT)/lib$(DIR)_debug.a)
	$(LIBTOOL) -static -o $(SYMROOT)/libinfo_profile.a \
		$(foreach DIR, $(DIRS), $(SYMROOT)/lib$(DIR)_profile.a)
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/local/lib/system
	$(INSTALL_LIBRARY) $(SYMROOT)/libinfo.a $(DSTROOT)/usr/local/lib/system
	$(INSTALL_LIBRARY) $(SYMROOT)/libinfo_debug.a $(DSTROOT)/usr/local/lib/system
	$(INSTALL_LIBRARY) $(SYMROOT)/libinfo_profile.a $(DSTROOT)/usr/local/lib/system
	$(RMDIR) $(DSTROOT)/scratch
