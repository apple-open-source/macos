PROJECT		= zip
VERSION		= 30
DIST		= $(PROJECT)$(VERSION)

OSLICENSES	= /usr/local/OpenSourceLicenses

SRCROOT		?= $(shell pwd)
SRCDIR		?= $(SRCROOT)/$(DIST)
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

destroot:
	mkdir -p $(OBJROOT)/$(DIST)
	:> $(OBJROOT)/$(DIST)/flags
	$(MAKE) -C $(OBJROOT)/$(DIST) \
		-f $(SRCDIR)/unix/Makefile generic install \
		SRCDIR=$(SRCDIR) \
		prefix=$(DSTROOT)/usr \
		MANDIR=$(DSTROOT)/usr/share/man/man1 \
		LOCAL_ZIP="-DLARGE_FILE_SUPPORT -Wall -Os -g -mdynamic-no-pic $(RC_CFLAGS)" BIND="$(CC) $(RC_CFLAGS)" \
		CC="$(CC)"  INSTALL_PROGRAM="$(INSTALL) -s" DESTDIR=$(DSTROOT) LFLAGS2=

symbols:
	$(CP) $(OBJROOT)/$(DIST)/zip $(SYMROOT)

ossinfo:
	$(MKDIR) $(DSTROOT)/$(OSLICENSES)
	$(INSTALL_FILE) $(SRCDIR)/LICENSE $(DSTROOT)/$(OSLICENSES)/$(PROJECT).txt
