Project               = openmpi
UserType              = Administrator
ToolType              = Commands
GnuAfterInstall       = post-install install-plist
Extra_Configure_Flags = 
Extra_CC_Flags = 
GnuNoBuild = YES
GnuNoConfigure = YES
GnuNoInstall = YES

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
build::
	@echo "Building $(Project)..."
	./buildfat.sh

Install_Target  = install
Install_Flags   = DESTDIR=$(DSTROOT)

# Post-install cleanup
post-install:
	@for binary in ompi_info opal_wrapper orted orterun; do \
		$(CP) $(DSTROOT)/usr/bin/$${binary} $(SYMROOT); \
		$(STRIP) -x $(DSTROOT)/usr/bin/$${binary}; \
	done
	@for library in mca_common_sm mpi mpi_cxx open-pal open-rte; do \
		$(CP) $(DSTROOT)/usr/lib/lib$${library}.0.dylib $(SYMROOT); \
		$(STRIP) -x $(DSTROOT)/usr/lib/lib$${library}.0.dylib; \
		$(RM) $(DSTROOT)/usr/lib/lib$${library}.la; \
	done
	$(CP) $(DSTROOT)/usr/lib/openmpi/*.so $(SYMROOT)
	$(STRIP) -S $(DSTROOT)/usr/lib/openmpi/*.so

# Automatic Extract & Patch
AEP_Project    = openmpi
AEP_Version    = 1.2.8
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = romio-suspend.diff xgrid.diff Makefile.in.diff

# Extract the source.
installsrc:
	$(TAR) -C $(SRCROOT) -jxf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 -F0 < $(SRCROOT)/files/$$patchfile) || exit 1; \
	done

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE $(OSL)/$(Project).txt
