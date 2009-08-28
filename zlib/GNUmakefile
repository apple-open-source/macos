##
# GNUmakefile for zlib
##

## Configuration ##

Project        = zlib
Name           = $(Project)
Vers           = 1.2.3
Name_Vers      = $(Name)-$(Vers)
Compress_Type  = bz2
Tarball        = $(Name_Vers).tar.$(Compress_Type)
Extract_Dir    = $(Name_Vers)
Patch_List     = Makefile.in.diff compress.c.diff configure.diff strlcpy.diff zconf.in.h.diff zlib.h.diff Makefile.msc.diff win32_zlib1.rc.diff

## Don't modify below here ##
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Determine correct extract option (default = gzip).
ifeq ($(Compress_Type),bz2)
	Extract_Option = j
else
	Extract_Option = z
endif

no_target:
	@$(MAKE) -f Makefile

# patch the source (install_source in Common.make has already copied to SRCROOT)
install_source::
	@echo "-- Extracting distfiles --"
	$(RMDIR) "$(SRCROOT)/$(Project)"
	$(TAR) -C "$(SRCROOT)" -$(Extract_Option)xof "$(Tarball)"
	$(MV) "$(SRCROOT)/$(Extract_Dir)" "$(SRCROOT)/$(Project)"
	@echo "-- Applying patches --"
	@set -x && \
	cd "$(SRCROOT)/$(Project)" && \
	for patchfile in $(Patch_List); do \
		patch -p0 -i $(SRCROOT)/patches/$$patchfile || exit 1; \
	done

# Copy, build and install
build::
	@echo "-- Copying to OBJROOT --"
	$(RMDIR) "$(OBJROOT)"
	$(CP) "$(SRCROOT)" "$(OBJROOT)"
	@echo "-- Building/Installing --"
	$(MAKE) -C $(OBJROOT) -f Makefile install \
		SRCROOT=$(OBJROOT) \
		OBJROOT=$(OBJROOT)/$(Project)

.DEFAULT:
	@$(MAKE) -f Makefile $@
