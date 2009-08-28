PROJECT		= unzip
VERSION		= 5.52
DISTFILE	= $(PROJECT)$(shell echo $(VERSION)  | tr -d '.').tar.gz

OSLICENSES	= /usr/local/OpenSourceLicenses
PATCHFILES	= patch-unix_unix.c patch-quarantine patch-CVE-2008-0888

SRCROOT		?= $(shell pwd)
OBJROOT		?= $(SRCROOT)/obj
SYMROOT		?= $(SRCROOT)/sym
DSTROOT		?= $(SRCROOT)/dst

ifndef CoreOSMakefiles
CoreOSMakefiles = $(MAKEFILEPATH)/CoreOS
endif

include $(MAKEFILEPATH)/CoreOS/Standard/Standard.make

all: install

installhdrs:

clean:
	rm -rf $(SYMROOT) $(OBJROOT)

install: destroot symbols manpages ossinfo

installsrc:
	pax -rw . $(SRCROOT)

destroot: patchsrc unpacksrc
	$(MAKE) -C $(OBJROOT)/$(PROJECT)-$(VERSION) \
		-f unix/Makefile macosx install \
		prefix=$(DSTROOT)/usr \
		MANDIR=$(DSTROOT)/usr/share/man/'man$$(manext)' \
		LOC="-Wall -Os -g -mdynamic-no-pic $(RC_CFLAGS)" LD="$(CC) $(RC_CFLAGS)" \
		CC="$(CC)"  INSTALL="$(INSTALL) -s" DESTDIR=$(DSTROOT)

manpages: destroot
	$(COMPRESSMANPAGES) /usr/share/man

symbols:
	$(CP) $(OBJROOT)/$(PROJECT)-$(VERSION)/unzip $(SYMROOT)

ossinfo: unpacksrc
	$(MKDIR) $(DSTROOT)/$(OSLICENSES)
	$(INSTALL_FILE) $(OBJROOT)/$(PROJECT)-$(VERSION)/LICENSE $(DSTROOT)/$(OSLICENSES)/$(PROJECT).txt

unpacksrc:
	mkdir -p $(OBJROOT)
	tar zxvf $(SRCROOT)/$(DISTFILE) -C $(OBJROOT)

patchsrc: unpacksrc
	for p in $(PATCHFILES); do			\
		cd $(OBJROOT)/$(PROJECT)-$(VERSION) && patch -f -p0 --posix < $(SRCROOT)/$${p};	\
	done
