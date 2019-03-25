##
# GNUmakefile for hunspell
##

## Configuration ##

Project        = hunspell
Name           = $(Project)
Version        = 1.2.8
Name_Vers      = $(Name)-$(Version)
Compress_Type  = gz
Tarball        = $(Name_Vers).tar.$(Compress_Type)
Extract_Dir    = $(Name_Vers)
Patch_List     = configure.diff hunspell.cxx.diff
Pl_Name        = sjp-myspell-pl
Pl_Version     = 20080831
Pl_Name_Vers   = $(Pl_Name)-$(Pl_Version)
Pl_Zipfile1    = $(Pl_Name_Vers).zip
Pl_Zipfile2    = pl_PL.zip

# Determine correct extract option (default = gzip).
ifeq ($(Compress_Type),bz2)
	Extract_Option = j
else
	Extract_Option = z
endif

no_target:
	@$(MAKE) -f Makefile

# Hijack the install stage to extract/patch the source.
install:
	@echo "-- Extracting distfiles --"
	rm -rf $(OBJROOT)
	cp -r $(SRCROOT) $(OBJROOT)
	rm -rf $(OBJROOT)/$(Project)
	cd $(OBJROOT) && tar $(Extract_Option)xf $(OBJROOT)/$(Tarball)
	mv $(OBJROOT)/$(Extract_Dir) $(OBJROOT)/$(Project)
	cd $(OBJROOT) && unzip $(OBJROOT)/$(Pl_Zipfile1) && unzip $(OBJROOT)/$(Pl_Zipfile2)
	@echo "-- Applying patches --"
	$(_v) for patchfile in $(Patch_List); do \
		cd $(OBJROOT)/$(Project) && patch -p0 < $(OBJROOT)/patches/$$patchfile; \
	done
	@echo "-- Done extracting/patching, continuing --"
	$(MAKE) -C $(OBJROOT) -f Makefile install \
		SRCROOT=$(OBJROOT) \
		OBJROOT=$(OBJROOT)/$(Project)

.DEFAULT:
	@$(MAKE) -f Makefile $@
