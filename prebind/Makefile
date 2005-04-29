SHELL = /bin/sh

RC_CFLAGS ?= -arch ppc -arch i386

CC     ?= gcc
CFLAGS ?= -Wall -g
nolink = -c

LD      ?= ld
LDFLAGS ?= -dynamic -arch_multiple

RM      = rm
RMFLAGS = -f

MKDIR      = mkdir
MKDIRFLAGS =

CP      = cp
CPFLAGS = -r

STRIP      = strip
STRIPFLAGS =

INSTALL      = install
INSTALLFLAGS =

SRCROOT ?= .
DSTROOT ?= .
OBJROOT ?= .

$(OBJROOT)/%.o: %.s
	$(CC) $(nolink) $(CFLAGS) $(RC_CFLAGS) -o $@ $<

$(OBJROOT)/%.o: %.c
	$(CC) $(nolink) $(CFLAGS) $(RC_CFLAGS) -o $@ $<

all: bin/update_prebinding_core

$(OBJROOT)/dstart.o: start.s
	$(CC) $(nolink) $(CFLAGS) $(RC_CFLAGS) -dynamic -DCRT1 -o $@ $^

$(OBJROOT)/dyld.o: dyld.s
	$(CC) $(nolink) $(CFLAGS) $(RC_CFLAGS) -dynamic -DCRT1 -o $@ $^

$(OBJROOT)/ddyld.o: $(OBJROOT)/dyld.o /usr/lib/dyld
	$(LD) -r -keep_private_externs $(LDFLAGS) -arch ppc  -o $*-ppc.o  $^
	$(LD) -r -keep_private_externs $(LDFLAGS) -arch i386 -o $*-i386.o $^
	lipo -create -arch ppc $*-ppc.o -arch i386 $*-i386.o -o $@

bin/update_prebinding_core: $(OBJROOT)/dstart.o $(OBJROOT)/ddyld.o $(OBJROOT)/update_prebinding_core.o
	$(LD) $(LDFLAGS) -arch ppc  -o $@-ppc  $^
	$(LD) $(LDFLAGS) -arch i386 -o $@-i386 $^
	lipo -create -arch ppc $@-ppc -arch i386 $@-i386 -o $@

### B&I Makefile APIs
clean::
	$(RM) $(RMFLAGS) $(SRCROOT)/bin/update_prebinding_core
	$(RM) $(RMFLAGS) $(SRCROOT)/bin/update_prebinding_core-{ppc,i386}
	$(RM) $(RMFLAGS) $(OBJROOT)/update_prebinding_core.o
	$(RM) $(RMFLAGS) $(OBJROOT)/{dstart,dyld,ddyld}.o
	$(RM) $(RMFLAGS) $(OBJROOT)/{dyld,ddyld}-{ppc,i386}.o
installhdrs::
install:: all installhdrs
	$(INSTALL) $(INSTALLFLAGS) -m 0755 -o root -g wheel -d $(DSTROOT)/usr/bin
	$(INSTALL) $(INSTALLFLAGS) -m 0755 -o root -g wheel -d $(DSTROOT)/usr/share/man/man1
	$(INSTALL) $(INSTALLFLAGS) -m 0755 -o root -g wheel bin/update_prebinding $(DSTROOT)/usr/bin
	$(STRIP) $(STRIPFLAGS) bin/update_prebinding_core
	$(INSTALL) $(INSTALLFLAGS) -m 0755 -o root -g wheel bin/update_prebinding_core $(DSTROOT)/usr/bin
	$(INSTALL) $(INSTALLFLAGS) -m 0644 -o root -g wheel man/man1/update_prebinding.1 $(DSTROOT)/usr/share/man/man1
	$(INSTALL) $(INSTALLFLAGS) -m 0644 -o root -g wheel man/man1/update_prebinding_core.1 $(DSTROOT)/usr/share/man/man1
	$(MAKE) clean
installsrc::
	$(MKDIR) $(MKDIRFLAGS) $(SRCROOT)/bin
	$(MKDIR) $(MKDIRFLAGS) $(SRCROOT)/man
	$(MKDIR) $(MKDIRFLAGS) $(SRCROOT)/man/man1
	$(CP) $(CPFLAGS) Makefile $(SRCROOT)
	$(CP) $(CPFLAGS) bin/update_prebinding $(SRCROOT)/bin
	$(CP) $(CPFLAGS) update_prebinding_core.c $(SRCROOT)
	$(CP) $(CPFLAGS) start.s $(SRCROOT)
	$(CP) $(CPFLAGS) dyld.s $(SRCROOT)
	$(CP) $(CPFLAGS) man/man1/update_prebinding.1 $(SRCROOT)/man/man1
	$(CP) $(CPFLAGS) man/man1/update_prebinding_core.1 $(SRCROOT)/man/man1
