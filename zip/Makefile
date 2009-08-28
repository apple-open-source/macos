##
# Top-level wrapper Makefile for zip and unzip tools
##

PROJECT=zip
COMPONENTS=zip unzip

SRCROOT=$(shell pwd)
OBJROOT?=/tmp/$(PROJECT).obj
SYMROOT?=/tmp/$(PROJECT).sym
DSTROOT?=/tmp/$(PROJECT).dst

OSVERSIONS	= /usr/local/OpenSourceVersions

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
install:: builddirs ossinfo

install clean installhdrs::
	for proj in $(COMPONENTS) ; do			\
		( cd $${proj} && $(MAKE) $@ $(MAKEFLAGS) SRCROOT=$(SRCROOT)/$${proj} \
			OBJROOT=$(OBJROOT)/$${proj} SYMROOT=$(SYMROOT)/$${proj} \
			DSTROOT=$(DSTROOT) ) || exit 1; \
	done

$(OBJROOT) $(SYMROOT) $(DSTROOT):
	$(_v) $(MKDIR) $@

builddirs: $(OBJROOT) $(SYMROOT)
	for proj in $(COMPONENTS) ; do			\
		$(MKDIR) $(OBJROOT)/$${proj};		\
		$(MKDIR) $(SYMROOT)/$${proj};		\
	done

ossinfo: builddirs
	$(MKDIR) $(DSTROOT)/$(OSVERSIONS)
	$(INSTALL_FILE) $(SRCROOT)/$(PROJECT).plist $(DSTROOT)/$(OSVERSIONS)/$(PROJECT).plist
