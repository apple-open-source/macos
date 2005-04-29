IOKITHDRS = $(DSTROOT)/System/Library/Frameworks/Kernel.framework/Versions/A/Headers/IOKit/
IOKITUSERHDRS = $(DSTROOT)/System/Library/Frameworks/IOKit.framework/Versions/A/Headers/
IOKITHDRSPRIV = $(DSTROOT)/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders/IOKit/
SCSIHDRS = scsi
CDBHDRS = cdb
SPIHDRS = scsi/spi
SCSIPARALLELHDRS = scsi-parallel
SCSICOMMANDSHDRS = scsi-commands

install: installcompatibilityhdrs installsymlinks
installhdrs: installcompatibilityhdrs installsymlinks

installsrc:
	ditto . $(SRCROOT)
	find $(SRCROOT) -type d -name CVS -exec rm -rf "{}" \; -prune
	
#
# Install IOSCSIFamily header files for backward compatibility
#

installcompatibilityhdrs:
	@echo "make installhdrs installing SCSI compatibility headers";
	mkdir -p $(IOKITHDRSPRIV);
	cp -R $(SCSIHDRS) $(IOKITHDRSPRIV)
	cp -R $(CDBHDRS) $(IOKITHDRSPRIV)

#
# Install symlinks for backward compatibility
#

installsymlinks: installscsiparallelsymlinks installscsicommandssymlinks

#
# Install scsi-parallel directory symlinks
#

installscsiparallelsymlinks: createscsidir
	@echo "make installhdrs installing scsi-parallel symlink";
	cd $(IOKITHDRS); ln -s $(SPIHDRS) $(SCSIPARALLELHDRS)
	
#
# Install scsi-commands directory symlinks
#
	
installscsicommandssymlinks: createscsidir
	@echo "make installhdrs installing scsi-commands symlink";
	cd $(IOKITHDRS); ln -s $(SCSIHDRS) $(SCSICOMMANDSHDRS)
	cd $(IOKITUSERHDRS); ln -s $(SCSIHDRS) $(SCSICOMMANDSHDRS)
	
createscsidir:
	@echo "Creating scsi directory for headers";
	mkdir -p $(IOKITHDRS)$(SCSIHDRS);
	mkdir -p $(IOKITHDRS)$(SPIHDRS);
	mkdir -p $(IOKITUSERHDRS)$(SCSIHDRS);
	
clean:
