##
# Top-level wrapper Makefile for zip and unzip tools
##

PROJECT=zip
COMPONENTS=zip unzip

SRCROOT=$(shell pwd)
OBJROOT?=/tmp/$(PROJECT).obj
SYMROOT?=/tmp/$(PROJECT).sym
DSTROOT?=/tmp/$(PROJECT).dst
TESTSDIR?= /AppleInternal/Tests/$(PROJECT)

OSVERSIONS	= /usr/local/OpenSourceVersions
BATSDIR		= /AppleInternal/CoreOS/BATS/unit_tests

.PHONY: installsrc install clean installhdrs builddirs

ifndef CoreOSMakefiles
CoreOSMakefiles = $(MAKEFILEPATH)/CoreOS
endif

include $(CoreOSMakefiles)/Standard/Standard.make 

all: install

installsrc:
	$(MKDIR) $(SRCROOT)
	$(PAX) -rw . $(SRCROOT)

install installhdrs:: $(OBJROOT) $(SYMROOT) $(DSTROOT)
install:: builddirs ossinfo testinfo

install clean installhdrs::
	for comp in $(COMPONENTS) ; do (		\
		$(MAKE) -C $${comp} $@ $(MAKEFLAGS) 	\
			SRCROOT=$(SRCROOT)/$${comp} 	\
			OBJROOT=$(OBJROOT)/$${comp} 	\
			SYMROOT=$(SYMROOT)/$${comp} 	\
			DSTROOT=$(DSTROOT) 		\
			TESTSDIR=$(TESTSDIR)/$${comp} 	\
	) || exit 1; done

$(OBJROOT) $(SYMROOT) $(DSTROOT):
	$(_v) $(MKDIR) $@

builddirs: $(OBJROOT) $(SYMROOT)
	for comp in $(COMPONENTS) ; do			\
		$(MKDIR) $(OBJROOT)/$${comp};		\
		$(MKDIR) $(SYMROOT)/$${comp};		\
	done

ossinfo: builddirs
	$(MKDIR) $(DSTROOT)/$(OSVERSIONS)
	$(INSTALL_FILE) $(SRCROOT)/$(PROJECT).plist $(DSTROOT)/$(OSVERSIONS)/

testinfo:
	$(MKDIR) $(DSTROOT)/$(BATSDIR)
	$(INSTALL_FILE) $(SRCROOT)/tests/$(PROJECT).plist $(DSTROOT)/$(BATSDIR)/
