PROJECT		= unzip
VERSION		= 5.52
DISTFILE	= $(PROJECT)$(shell echo $(VERSION)  | tr -d '.').tar.gz

OSLICENSES	= /usr/local/OpenSourceLicenses
PATCHFILES	= patch-unix_unix.c patch-quarantine patch-CVE-2008-0888 \
			unzip-6.0_overflow2.diff patch-list.c

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

install: destroot symbols ossinfo

installsrc:
	pax -rw . $(SRCROOT)

destroot: patchsrc unpacksrc
	$(MAKE) -C $(OBJROOT)/$(PROJECT)-$(VERSION) \
		-f unix/Makefile macosx install \
		prefix=$(DSTROOT)/usr \
		MANDIR=$(DSTROOT)/usr/share/man/'man$$(manext)' \
		LOC="$(CFLAGS) $(RC_CFLAGS)" LD="$(CC) $(RC_CFLAGS)" \
		CC="$(CC)"  INSTALL="$(INSTALL)" DESTDIR=$(DSTROOT) STRIP="echo"

symbols:
	$(CP) $(OBJROOT)/$(PROJECT)-$(VERSION)/unzip $(SYMROOT)
	$(DSYMUTIL) $(SYMROOT)/unzip

ossinfo: unpacksrc
	$(MKDIR) $(DSTROOT)/$(OSLICENSES)
	$(INSTALL_FILE) $(OBJROOT)/$(PROJECT)-$(VERSION)/LICENSE $(DSTROOT)/$(OSLICENSES)/$(PROJECT).txt

unpacksrc:
	mkdir -p $(OBJROOT)
	tar zxvf $(SRCROOT)/$(DISTFILE) -C $(OBJROOT)

patchsrc: unpacksrc
	for p in $(PATCHFILES); do			\
		patch -d $(OBJROOT)/$(PROJECT)-$(VERSION) -f -p0 --posix < $(SRCROOT)/$${p} || exit 1;	\
	done
