OFLAG = -O
CFLAGS = $(OFLAG) -g -Wall $(RC_NONARCH_CFLAGS)
ifneq (,$(findstring ppc, $(RC_ARCHS)))
CFLAGS += -mlong-branch
endif
IFLAGS = -S -S -c -m 444

SRCROOT = .
SYMROOT = .
OBJROOT = .

USRLIBDIR = /usr/lib
LOCLIBDIR = /usr/local/lib
DSTDIRS = $(DSTROOT)$(USRLIBDIR) $(DSTROOT)$(LOCLIBDIR)
DYLD = $(NEXT_ROOT)/usr/lib/dyld
#CC = /usr/bin/gcc-x.x
ifneq "$(wildcard /bin/pax)" ""
PAX = /bin/pax -rw
else
PAX = /bin/cp -p
endif
ifneq "$(wildcard /bin/mkdirs)" ""
MKDIR = /bin/mkdirs
else
MKDIR = /bin/mkdir -p
endif
CHMOD = /bin/chmod
INSTALL = /usr/bin/install

ifeq (,$(RC_ARCHS))
# build for the local arch only
STATIC_ARCH_CFLAGS =
DYNAMIC_ARCH_CFLAGS =
else
# assume the toolchain supports static compilation for all request archs
STATIC_ARCH_CFLAGS = $(patsubst %,-arch %,$(RC_ARCHS))

DYNAMIC_ARCH_CFLAGS = $(patsubst %,-arch %,$(RC_ARCHS))
endif

CFILES = crt.c icplusplus.c
SFILES = start.s dyld.s dylib.s bundle1.s
INSTALLSRC_FILES = $(CFILES) $(SFILES) Makefile PB.project notes

USRLIB_INSTALL_FILES = crt1.o gcrt1.o dylib1.o bundle1.o
LOCLIB_INSTALL_FILES = crt0.o gcrt0.o pscrt0.o pscrt1.o


all: $(USRLIB_INSTALL_FILES) $(LOCLIB_INSTALL_FILES)

crt0.o: sstart.o xcrt0.o sdyld.o
	$(CC) $(CFLAGS) $(STATIC_ARCH_CFLAGS) \
		-r -static -nostdlib \
		$(OBJROOT)/sstart.o $(OBJROOT)/xcrt0.o \
		$(OBJROOT)/sdyld.o \
		-o $(SYMROOT)/crt0.o

sstart.o: start.s
	$(CC) -static -DCRT0 $(STATIC_ARCH_CFLAGS) \
		-c -o $(OBJROOT)/sstart.o $(SRCROOT)/start.s

xcrt0.o: crt.c
	$(CC) -static -DCRT0 $(CFLAGS) $(STATIC_ARCH_CFLAGS) \
		-c -o $(OBJROOT)/xcrt0.o $(SRCROOT)/crt.c

sdyld.o: dyld.s
	$(CC) -static -DCRT0 $(STATIC_ARCH_CFLAGS) \
		-c -o $(OBJROOT)/sdyld.o $(SRCROOT)/dyld.s

gcrt0.o: sstart.o xgcrt0.o sdyld.o
	$(CC) $(CFLAGS) $(STATIC_ARCH_CFLAGS) \
		-r -static -nostdlib \
		$(OBJROOT)/sstart.o $(OBJROOT)/xgcrt0.o \
		$(OBJROOT)/sdyld.o \
		-o $(SYMROOT)/gcrt0.o

xgcrt0.o: crt.c
	$(CC) -static -DCRT0 -DGCRT $(CFLAGS) $(STATIC_ARCH_CFLAGS) \
		-c -o $(OBJROOT)/xgcrt0.o $(SRCROOT)/crt.c

pscrt0.o: sstart.o xpscrt0.o sdyld.o
	$(CC) $(CFLAGS) $(STATIC_ARCH_CFLAGS) \
		-r -static -nostdlib \
		$(OBJROOT)/sstart.o $(OBJROOT)/xpscrt0.o \
		$(OBJROOT)/sdyld.o \
		-o $(SYMROOT)/pscrt0.o

xpscrt0.o: crt.c
	$(CC) -static -DCRT0 -DPOSTSCRIPT $(CFLAGS) $(STATIC_ARCH_CFLAGS) \
		-c -o $(OBJROOT)/xpscrt0.o $(SRCROOT)/crt.c

crt1.o: dstart.o xcrt1.o ddyld.o
	$(CC) $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) $(DYLD) -arch_errors_fatal \
		-r -dynamic -nostdlib -keep_private_externs \
		$(OBJROOT)/dstart.o $(OBJROOT)/xcrt1.o \
		$(OBJROOT)/ddyld.o \
		-o $(SYMROOT)/crt1.o

dstart.o: start.s
	$(CC) -dynamic -DCRT1 $(DYNAMIC_ARCH_CFLAGS) \
		-c -o $(OBJROOT)/dstart.o $(SRCROOT)/start.s

xcrt1.o: crt.c
	$(CC) -dynamic -DCRT1 $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) \
		-c -o $(OBJROOT)/xcrt1.o $(SRCROOT)/crt.c

ddyld.o: dyld.s
	$(CC) -dynamic -DCRT1 $(DYNAMIC_ARCH_CFLAGS) \
		-c -o $(OBJROOT)/ddyld.o $(SRCROOT)/dyld.s

gcrt1.o: dstart.o xgcrt1.o ddyld.o
	$(CC) $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) $(DYLD) -arch_errors_fatal \
		-r -dynamic -nostdlib -keep_private_externs \
		$(OBJROOT)/dstart.o $(OBJROOT)/xgcrt1.o $(OBJROOT)/ddyld.o \
		-o $(SYMROOT)/gcrt1.o

xgcrt1.o: crt.c
	$(CC) -dynamic -DCRT1 -DGCRT $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) \
		-c -o $(OBJROOT)/xgcrt1.o $(SRCROOT)/crt.c

pscrt1.o: dstart.o xpscrt1.o ddyld.o
	$(CC) $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) $(DYLD) -arch_errors_fatal \
		-r -dynamic -nostdlib -keep_private_externs \
		$(OBJROOT)/dstart.o $(OBJROOT)/xpscrt1.o $(OBJROOT)/ddyld.o \
		-o $(SYMROOT)/pscrt1.o

xpscrt1.o: crt.c
	$(CC) -dynamic -DCRT1 -DPOSTSCRIPT $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) \
		-c -o $(OBJROOT)/xpscrt1.o $(SRCROOT)/crt.c

dylib1.o: dylib.o icplusplus.o
	$(CC) $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) \
		-r -dynamic -nostdlib -keep_private_externs \
		$(OBJROOT)/dylib.o $(OBJROOT)/icplusplus.o \
		-o $(SYMROOT)/dylib1.o

dylib.o: dylib.s
	$(CC) -dynamic $(DYNAMIC_ARCH_CFLAGS) \
		-c -o $(OBJROOT)/dylib.o $(SRCROOT)/dylib.s

icplusplus.o: icplusplus.c
	$(CC) -dynamic $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) \
		-c -o $(OBJROOT)/icplusplus.o $(SRCROOT)/icplusplus.c

bundle1.o: bundle1.s
	$(CC) -dynamic $(CFLAGS) $(DYNAMIC_ARCH_CFLAGS) \
		-c -o $(SYMROOT)/bundle1.o $(SRCROOT)/bundle1.s

clean:
	rm -f $(OBJROOT)/sstart.o $(OBJROOT)/dstart.o \
	      $(OBJROOT)/sdyld.o $(OBJROOT)/ddyld.o
	rm -f $(OBJROOT)/xcrt0.o \
	      $(OBJROOT)/xgcrt0.o \
	      $(OBJROOT)/xpscrt0.o \
	      $(OBJROOT)/xcrt1.o \
	      $(OBJROOT)/xgcrt1.o \
	      $(OBJROOT)/xpscrt1.o
	rm -f $(SYMROOT)/crt0.o \
	      $(SYMROOT)/gcrt0.o \
	      $(SYMROOT)/pscrt0.o \
	      $(SYMROOT)/crt1.o \
	      $(SYMROOT)/gcrt1.o \
	      $(SYMROOT)/pscrt1.o
	rm -f $(OBJROOT)/dylib.o \
	      $(OBJROOT)/icplusplus.o
	rm -f $(SYMROOT)/dylib1.o $(SYMROOT)/bundle1.o

install: all $(DSTDIRS)
	for obj in $(USRLIB_INSTALL_FILES); do					\
		(set -x;							\
			$(INSTALL) $(IFLAGS) $(SYMROOT)/$${obj} $(DSTROOT)$(USRLIBDIR)/$${obj}; \
		)								\
	done
	for obj in $(LOCLIB_INSTALL_FILES); do					\
		(set -x;							\
			$(INSTALL) $(IFLAGS) $(SYMROOT)/$${obj} $(DSTROOT)$(LOCLIBDIR)/$${obj}; \
		)								\
	done

installhdrs:

installsrc:
	$(MKDIR) $(SRCROOT)
	$(CHMOD) 755 $(SRCROOT)
	$(PAX) $(INSTALLSRC_FILES) $(SRCROOT)
	$(CHMOD) 444 $(SRCROOT)/*

$(OJBROOT) $(SYMROOT) $(DSTDIRS):
	$(MKDIR) $@

