export USE_APPLE_PB_SUPPORT = all
OFLAG = -O
ifeq "mwcc" "$(notdir $(CC))"
  CFLAGS = -g $(OFLAG) -DBUILTIN_FAT
else
  CFLAGS = -g $(OFLAG) -Wall -DBUILTIN_FAT -Wno-precomp
endif

ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

SRCROOT = .
SYMROOT = .
OBJROOT = .
OFILE_DIR = $(OBJROOT)
VPATH = $(OFILE_DIR)

BINDIR = /usr/bin
MAGDIR = /private/etc
MANDIR = /usr/share/man
DSTDIRS = $(DSTROOT)$(BINDIR) $(DSTROOT)$(MAGDIR) \
	  $(DSTROOT)$(MANDIR)/man1 $(DSTROOT)$(MANDIR)/man5

HFILES = file.h names.h patchlevel.h readelf.h tar.h
CFILES = file.c apprentice.c fsmagic.c softmagic.c ascmagic.c is_tar.c \
	 print.c compress.c readelf.c internat.c readfat.c
MAN1 = file.1
MAN5 = magic.5
MAGFILES = $(SRCROOT)/magdir/Header \
	   $(SRCROOT)/magdir/Localstuff \
	   $(SRCROOT)/magdir/OpenBSD \
	   $(SRCROOT)/magdir/[a-z]*
OBJS = $(CFILES:.c=.o) vers.o
INSTALL_FILES = $(CFILES) $(HFILES) $(MAN1) $(MAN5) magdir Makefile notes

PRODUCT = file

all: $(OFILE_DIR) $(SYMROOT) $(PRODUCT).NEW magic

$(PRODUCT).NEW:	$(OFILE_DIR) $(SYMROOT) $(OBJS)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/$@ $(OBJS)

magic: $(MAGFILES)
	cat $(MAGFILES) > $(OFILE_DIR)/magic

vers.c:
	vers_string -c $(VERS_STRING_FLAGS) $(PRODUCT) > $(OFILE_DIR)/$@

.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -MD -dependency-file $(OFILE_DIR)/$*.d \
		-o $(OFILE_DIR)/$*.o $<
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d

clean: shlib_clean
	-rm -f $(SYMROOT)/$(PRODUCT).NEW $(OFILE_DIR)/vers.c $(OFILE_DIR)/magic

shlib_clean:
	-cd $(OFILE_DIR); rm -f $(OBJS)
	-rm -f $(OFILE_DIR)/Makedep make.out

install: $(RC_OS)

teflon macos: all $(DSTDIRS)
	install -c -s -m 555 $(SYMROOT)/$(PRODUCT).NEW \
		$(DSTROOT)$(BINDIR)/$(PRODUCT)
	install -c -m 444 $(MAN1) $(DSTROOT)$(MANDIR)/man1
	install -c -m 444 $(MAN5) $(DSTROOT)$(MANDIR)/man5
	install -c -m 444 $(OFILE_DIR)/magic $(DSTROOT)$(MAGDIR)/magic

nextstep:

installsrc:
	$(MKDIRS) $(SRCROOT)
	$(MKDIRS) $(SRCROOT)/magdir
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*
	chmod 755 $(SRCROOT)/magdir

$(OFILE_DIR) $(SYMROOT) $(DSTDIRS):
	$(MKDIRS) $@

-include $(OFILE_DIR)/Makedep
