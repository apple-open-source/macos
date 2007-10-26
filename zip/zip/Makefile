PROJECT		= zip
VERSION		= 2.32
DISTFILE	= $(PROJECT)$(shell echo $(VERSION)  | tr -d '.').tar.gz

OSLICENSES	= /usr/local/OpenSourceLicenses
PATCHFILES	= 

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

install: destroot manpages ossinfo

installsrc:
	pax -rw . $(SRCROOT)

destroot: unpacksrc
	$(MAKE) -C $(OBJROOT)/$(PROJECT)-$(VERSION) \
		-f unix/Makefile generic install \
		prefix=$(DSTROOT)/usr \
		MANDIR=$(DSTROOT)/usr/share/man/'man$$(manext)' \
		LOCAL_ZIP="-Os -mdynamic-no-pic $(RC_CFLAGS)" BIND="$(CC) $(RC_CFLAGS)" \
		CC="$(CC) -fno-builtin -x c"  INSTALL="$(INSTALL)" DESTDIR=$(DSTROOT)

manpages: destroot
	mkdir -p $(DSTROOT)/usr/share/man
	$(LN) $(DSTROOT)/usr/share/man/man1/zip.1 $(DSTROOT)/usr/share/man/man1/zipcloak.1
	$(LN) $(DSTROOT)/usr/share/man/man1/zip.1 $(DSTROOT)/usr/share/man/man1/zipnote.1
	$(LN) $(DSTROOT)/usr/share/man/man1/zip.1 $(DSTROOT)/usr/share/man/man1/zipsplit.1
	$(COMPRESSMANPAGES) /usr/share/man

ossinfo: unpacksrc
	$(MKDIR) $(DSTROOT)/$(OSLICENSES)
	$(INSTALL_FILE) $(OBJROOT)/$(PROJECT)-$(VERSION)/LICENSE $(DSTROOT)/$(OSLICENSES)/$(PROJECT).txt

unpacksrc:
	mkdir -p $(OBJROOT)
	tar zxvf $(SRCROOT)/$(DISTFILE) -C $(OBJROOT)

patch_source_local: install_source_local
	for p in $(PATCHFILES); do						\
		cd $(BuildDirectory) && patch -f -p1 --posix < $(SRCROOT)/$${p};	\
	done
