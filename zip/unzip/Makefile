PROJECT		= unzip
VERSION		= 60
DISTFILE	= $(PROJECT)$(shell echo $(VERSION)  | tr -d '.').tgz

OSLICENSES	= /usr/local/OpenSourceLicenses
PATCHFILES	= cve-2014-8140-test-compr-eb \
                  cve-2014-8139-crc-overflow \
                  cve-2014-8141-getzip64data \
                  cve-2014-9636-test-compr-eb \
                  allow-greater-hostver-values \
                  include-unistd-for-kfreebsd \
                  handle-pkware-verification-bit \
                  initialize-the-symlink-flag \
                  increase-size-of-cfactorstr \
                  patch-quarantine \
                  PR-25537454.diff \
                  PR-4126223.diff

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
	$(MAKE) -C $(OBJROOT)/$(PROJECT)$(VERSION) \
		-f unix/Makefile macosx install \
		prefix=$(DSTROOT)/usr \
		MANDIR=$(DSTROOT)/usr/share/man/'man$$(manext)' \
		LOC="$(CFLAGS) $(RC_CFLAGS)" LD="$(CC) $(RC_CFLAGS)" \
		CC="$(CC)"  INSTALL="$(INSTALL)" DESTDIR=$(DSTROOT) STRIP="echo"

symbols:
	$(CP) $(OBJROOT)/$(PROJECT)$(VERSION)/unzip $(SYMROOT)
	$(DSYMUTIL) $(SYMROOT)/unzip

ossinfo: unpacksrc
	$(MKDIR) $(DSTROOT)/$(OSLICENSES)
	$(INSTALL_FILE) $(OBJROOT)/$(PROJECT)$(VERSION)/LICENSE $(DSTROOT)/$(OSLICENSES)/$(PROJECT).txt

unpacksrc:
	mkdir -p $(OBJROOT)
	tar zxvf $(SRCROOT)/$(DISTFILE) -C $(OBJROOT)

patchsrc: unpacksrc
	patch -d $(OBJROOT)/$(PROJECT)$(VERSION)/unix/ -f -p4 --posix < Apple-patch
	for p in $(PATCHFILES); do			\
		patch -d $(OBJROOT)/$(PROJECT)$(VERSION) -f -p1 --posix < $(SRCROOT)/$${p} || exit 1;	\
	done
