PROJECT		= zip
VERSION		= 3.0
DIST		= $(PROJECT)$(shell echo $(VERSION)  | tr -d '.')
DISTFILE	= $(DIST).zip

OSLICENSES	= /usr/local/OpenSourceLicenses
PATCHFILES	= patch-Apple

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
	$(MAKE) -C $(OBJROOT)/$(DIST) \
		-f unix/Makefile generic install \
		prefix=$(DSTROOT)/usr \
		MANDIR=$(DSTROOT)/usr/share/man/man1 \
		LOCAL_ZIP="-DLARGE_FILE_SUPPORT -Wall -Os -g -mdynamic-no-pic $(RC_CFLAGS)" BIND="$(CC) $(RC_CFLAGS)" \
		CC="$(CC)"  INSTALL_PROGRAM="$(INSTALL) -s" DESTDIR=$(DSTROOT) LFLAGS2=

symbols:
	$(CP) $(OBJROOT)/$(DIST)/zip $(SYMROOT)

ossinfo: unpacksrc
	$(MKDIR) $(DSTROOT)/$(OSLICENSES)
	$(INSTALL_FILE) $(OBJROOT)/$(DIST)/LICENSE $(DSTROOT)/$(OSLICENSES)/$(PROJECT).txt

unpacksrc:
	cd $(OBJROOT); unzip $(SRCROOT)/$(DISTFILE)
	> $(OBJROOT)/$(DIST)/flags

patchsrc: unpacksrc
	for p in $(PATCHFILES); do						\
		cd $(OBJROOT)/$(DIST) && patch -f -p1 --posix < $(SRCROOT)/$${p} || exit 1;	\
	done
