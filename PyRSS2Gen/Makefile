## 
# Apple Makefile to install PyRSS2Gen
# Copyright (c) 2006 by Apple Computer, Inc.
##

ProjectName = PyRSS2Gen
ProjectDir = PyRSS2Gen
PYTHON_EXEC = /usr/bin/python
MKDIR = /bin/mkdir

OBJROOT = .
SYMROOT = .

INSTALL-LIB = /Library/Python/2.4/site-packages
EXTRAS := $(shell $(PYTHON_EXEC) -c 'import sys; print sys.prefix')/Extras
#EXTRAS = /System/Library/Frameworks/Python.framework/Versions/2.4/Extras

# These includes provide the proper paths to system utilities

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

install: install-ossfiles
	$(CD) $(SRCROOT); $(PYTHON_EXEC) setup.py install --root $(DSTROOT) --home=$(EXTRAS)

build:
	$(PYTHON_EXEC) setup.py build
		
clean:
	$(CD) $(SRCROOT); $(PYTHON_EXEC) setup.py clean
	
installsrc:
	$(ECHO) "Installing sources"
	$(CP) PyRSS2Gen.py $(SRCROOT)
	$(CP) setup.py $(SRCROOT)
	$(CP) Makefile $(SRCROOT)
	$(CP) LICENSE $(SRCROOT)
	$(CP) PyRSS2Gen.plist $(SRCROOT)
	
installhdrs:
	$(ECHO) "Installing headers"

#
# Open Source Hooey
#

OSV = /usr/local/OpenSourceVersions
OSL = /usr/local/OpenSourceLicenses

install-ossfiles::
	$(_v) $(MKDIR) -p $(DSTROOT)/$(OSV)
	$(_v) $(CP) $(SRCROOT)/$(ProjectName).plist $(DSTROOT)/$(OSV)/$(ProjectName).plist
	$(_v) $(MKDIR) -p $(DSTROOT)/$(OSL)
	$(_v) $(CP) $(SRCROOT)/LICENSE $(DSTROOT)/$(OSL)/$(ProjectName).txt