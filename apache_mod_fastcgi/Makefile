##
# Makefile for sqlite3-ruby 
##

Project = mod_fastcgi

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

build::
	(cd $(SRCROOT)/$(Project) \
	&& $(CP) Makefile.AP2 Makefile \
	&& $(MAKE) CFLAGS="$(CFLAGS)" top_dir=/usr/share/httpd DESTDIR=$(DSTROOT) all install)
	$(STRIP) -x $(DSTROOT)/usr/libexec/apache2/mod_fastcgi.so
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/docs/LICENSE.TERMS $(OSL)/$(Project).txt

installhdrs::
	@echo Nothing to be done for installhdrs

# Automatic Extract & Patch
AEP_Project    = $(Project)
AEP_Version    = 2.4.2
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = support_apache22.diff 

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p1 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
