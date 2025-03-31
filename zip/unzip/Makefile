PROJECT		= unzip
VERSION		= 60
DIST		= $(PROJECT)$(shell echo $(VERSION)  | tr -d '.')

OSLICENSES	= /usr/local/OpenSourceLicenses
TESTSDIR	?= /AppleInternal/Tests/$(PROJECT)

SRCROOT		?= $(shell realpath)
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

install: destroot symbols ossinfo testinfo

destroot:
	mkdir -p $(OBJROOT)/$(DIST)
	$(MAKE) -C $(OBJROOT)/$(DIST) \
		-f $(SRCDIR)/unix/Makefile macosx install \
		SRCDIR=$(SRCDIR) \
		prefix=$(DSTROOT)/usr \
		MANDIR=$(DSTROOT)/usr/share/man/'man$$(manext)' \
		LOC="$(CFLAGS) $(RC_CFLAGS)" LD="$(CC) $(RC_CFLAGS)" \
		CC="$(CC)"  INSTALL="$(INSTALL)" DESTDIR=$(DSTROOT) STRIP="echo"

symbols:
	$(CP) $(OBJROOT)/$(DIST)/unzip $(SYMROOT)
	$(DSYMUTIL) $(SYMROOT)/unzip

ossinfo:
	$(MKDIR) $(DSTROOT)/$(OSLICENSES)
	$(INSTALL_FILE) $(SRCDIR)/LICENSE $(DSTROOT)/$(OSLICENSES)/$(PROJECT).txt

testinfo:
	$(MKDIR) $(DSTROOT)/$(TESTSDIR)/
	$(INSTALL_SCRIPT) $(SRCROOT)/tests/*.sh $(DSTROOT)/$(TESTSDIR)/
	$(INSTALL_FILE) $(SRCROOT)/tests/*.in $(DSTROOT)/$(TESTSDIR)/
