PROJECT    = doc_cmds
COMPONENTS = checknr colcrt getNAME makewhatis

SRCROOT=$(shell pwd)
OBJROOT=/tmp/$(PROJECT).obj
SYMROOT=/tmp/$(PROJECT).sym
DSTROOT=/tmp/$(PROJECT).dst

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
install:: builddirs install-manpages install-plist

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

install-manpages:
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/share/man/man1
	$(INSTALL_FILE) $(SRCROOT)/man/*.1 $(DSTROOT)/usr/share/man/man1
	$(GZIP) $(DSTROOT)/usr/share/man/man1/*.1

OSV = /usr/local/OpenSourceVersions
OSL = /usr/local/OpenSourceLicenses

install-plist:
	$(INSTALL_DIRECTORY) $(DSTROOT)$(OSV)
	$(INSTALL_FILE) $(SRCROOT)/doc_cmds.plist $(DSTROOT)$(OSV)
	$(INSTALL_DIRECTORY) $(DSTROOT)$(OSL)
	$(INSTALL_FILE) $(SRCROOT)/doc_cmds.txt $(DSTROOT)$(OSL)
