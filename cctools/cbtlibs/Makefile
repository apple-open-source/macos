RC_OS = macos
OFLAG = -Os
USE_DEPENDENCY_FILE := $(shell if [ "$(notdir $(CC))" != "mwccppc" ] && \
				  [ "$(RC_XBS)" != "YES" ]; then \
	     echo YES ; else echo NO ; \
	     fi; )
ifneq "" "$(SDKROOT)"
  SDK = -isysroot $(SDKROOT)
  CC = $(shell xcrun -find -sdk $(SDKROOT) cc)
endif
	
ifeq "mwccppc" "$(notdir $(CC))"
  CFLAGS = $(OFLAG) -g -gccinc -I$(SRCROOT)/../include $(SDK)
else
  CFLAGS = $(OFLAG) -g -I$(SRCROOT)/../include -Wall $(X_CFLAGS) $(SDK)
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

CFILES = libsyminfo.c
OBJS = $(CFILES:.c=.o)
INSTALL_FILES = $(CFILES) Makefile notes
LOCLIBDIR = /usr/local/lib
LIBSTUFF = -L$(SYMROOT)/../libstuff -lstuff

all: $(OFILE_DIR) $(SYMROOT) lib_ofiles

lib_ofiles: $(OFILE_DIR) $(SYMROOT) libsyminfo.a 

ifeq "NO" "$(USE_DEPENDENCY_FILE)"
.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) $(COPTS) -c -o $(OFILE_DIR)/$*.o $<
else
.c.o:
	$(CC) $(CFLAGS) $(RC_CFLAGS) $(COPTS) -c \
		-MD -dependency-file $(OFILE_DIR)/$*.d \
		-o $(OFILE_DIR)/$*.o $<
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d
endif

clean shlib_clean:
	-rm -r -f $(OFILE_DIRS)
	-rm -f libsyminfo.a

install: all

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(SYMROOT):
	$(MKDIRS) $@

libsyminfo.o: libsyminfo.c
ifeq "NO" "$(USE_DEPENDENCY_FILE)"
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c \
		-o $(OFILE_DIR)/$*.o $(SRCROOT)/libsyminfo.c -DLIBRARY_API
else
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c \
		-MD -dependency-file $(OFILE_DIR)/$*.d -o $(OFILE_DIR)/$*.o \
		$(SRCROOT)/libsyminfo.c -DLIBRARY_API
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d
endif

libsyminfo.a: libsyminfo.o
	$(CC) $(RC_CFLAGS) -nostdlib -r \
		-o $(OBJROOT)/libsyminfo.o.tmp \
		libsyminfo.o $(LIBSTUFF)
	libtool -static -o $(SYMROOT)/libsyminfo.a \
		$(OBJROOT)/libsyminfo.o.tmp


lib_ofiles_install: lib_ofiles
	$(MKDIRS) $(DSTROOT)$(LOCLIBDIR)
	install -p -c -m 444 $(SYMROOT)/libsyminfo.a \
		$(DSTROOT)$(LOCLIBDIR)/libsyminfo.a
	ranlib $(DSTROOT)$(LOCLIBDIR)/libsyminfo.a

-include $(OFILE_DIR)/Makedep

$(OFILE_DIR) $(SYMROOT):
	$(MKDIRS) $@
