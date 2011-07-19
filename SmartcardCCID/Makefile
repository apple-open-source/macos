# Makefile for doing a test build and then installing source for SmartcardCCID
# Created on 07/19/05 By John Hurley <jhurley@apple.com>
# Based on Makefile for CoreDataExamples

BNIProject = SmartcardCCID
Project = ccid

#
# Top-level Makefile for SmartcardCCID Allows build or clean
# of all directories in one swoop.  
#

.PHONY: installsrc clean installhdrs install

SUBPROJECTS = libusb ccid

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

installsrc::
	@cp -R Makefile COPYING $(BNIProject).plist $(SUBPROJECTS) $(SRCROOT)
	
install::
	@echo "the proj is: " $(Project)
	@for proj in $(SUBPROJECTS); do \
		mkdir -p $(SYMROOT)/$${proj}; \
	done
	@echo "Calling configure"
	(cd $(SRCROOT)/$(Project)/ccid && ./MacOSX/configure --no-configure --disable-opensc ) && echo "Configure complete"
	@echo "Copying files to open source location for: " $(BNIProject)
	-mkdir -p $(OSV)
	cp $(SRCROOT)/$(BNIProject).plist $(OSV)/$(BNIProject).plist
	-mkdir -p $(OSL)
	cp $(SRCROOT)/COPYING $(OSL)/$(BNIProject).txt

installsrc clean installhdrs install::
	@for proj in $(SUBPROJECTS); do \
		(cd $${proj} && make $@ \
			SRCROOT=$(SRCROOT)/$${proj} \
			OBJROOT=$(OBJROOT)/$${proj} \
			SYMROOT=$(SYMROOT)/$${proj} \
			DSTROOT=$(DSTROOT) \
		) || exit 1; \
	done

