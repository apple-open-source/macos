OFLAG = -O
ifeq "mwcc" "$(notdir $(CC))"
  CFLAGS = -g $(OFLAG)
else
  CFLAGS = -g $(OFLAG) -Wall -Wno-precomp
endif

ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

OBJROOT = .
OFILE_DIR = $(OBJROOT)
VPATH = $(OFILE_DIR)
SYMROOT = .

BINDIR = /usr/bin
MANDIR = /usr/share/man
DSTDIRS = $(DSTROOT)$(BINDIR) $(DSTROOT)$(MANDIR)/man1 $(DSTROOT)$(MANDIR)/man5

HFILES = archive.h extern.h pathnames.h
CFILES = append.c ar.c archive.c contents.c delete.c extract.c misc.c move.c \
	 print.c replace.c 
MAN1 = ar.1
MAN5 = ar.5
OBJS = $(CFILES:.c=.o) vers.o
INSTALL_FILES = $(CFILES) $(HFILES) $(MAN1) $(MAN5) Makefile notes

PRODUCT = ar

all: $(OFILE_DIR) $(SYMROOT) $(PRODUCT).NEW

$(PRODUCT).NEW:	$(OFILE_DIR) $(SYMROOT) $(OBJS)
	$(CC) $(CFLAGS) $(RC_CFLAGS) -o $(SYMROOT)/$@ $(OBJS)

vers.c:
	vers_string -c $(VERS_STRING_FLAGS) $(PRODUCT) > $(OFILE_DIR)/$@

.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -MD -dependency-file $(OFILE_DIR)/$*.d \
		-o $(OFILE_DIR)/$*.o $<
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d

clean: shlib_clean
	-rm -f $(SYMROOT)/$(PRODUCT).NEW $(OFILE_DIR)/vers.c

shlib_clean:
	-cd $(OFILE_DIR); rm -f $(OBJS)
	-rm -f $(OFILE_DIR)/Makedep make.out

install: $(RC_OS)

teflon macos: all $(DSTDIRS)
	install -c -s -m 555 $(SYMROOT)/$(PRODUCT).NEW \
		$(DSTROOT)$(BINDIR)/$(PRODUCT)
	install -c -m 444 $(MAN1) $(DSTROOT)$(MANDIR)/man1
	install -c -m 444 $(MAN5) $(DSTROOT)$(MANDIR)/man5

nextstep:

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(OFILE_DIR) $(SYMROOT) $(DSTDIRS):
	$(MKDIRS) $@

-include $(OFILE_DIR)/Makedep
