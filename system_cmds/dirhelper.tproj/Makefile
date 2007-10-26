
ifeq "$(SRCROOT)" ""
	SRCROOT=$(shell pwd)
endif
ifeq "$(OBJROOT)" ""
	OBJROOT=$(shell pwd)
endif
ifeq "$(SYMROOT)" ""
	SYMROOT=$(shell pwd)
endif
ifeq "$(DSTROOT)" ""
	DSTROOT=
endif

ifneq "$(shell basename $(SRCROOT))" "dirhelper.tproj"
SRCDIR=$(SRCROOT)/dirhelper.tproj
else
SRCDIR=$(SRCROOT)
endif
OBJDIR=$(OBJROOT)/dirhelper.obj

CFLAGS = -g -mdynamic-no-pic -Os -Wall -Wextra -Wshadow -Wmissing-prototypes -Wmissing-declarations -Werror -D__MigTypeCheck=1 $(RC_CFLAGS) -I $(OBJDIR)

LDFLAGS = -lbsm

all: dirhelper

dirhelper: mighdrs
	cc -c $(CFLAGS) -o $(OBJDIR)/dirhelperServer.o $(OBJDIR)/dirhelperServer.c
	cc -c $(CFLAGS) -o $(OBJDIR)/dirhelperUser.o $(OBJDIR)/dirhelperUser.c
	cc -c $(CFLAGS) -o $(OBJDIR)/dirhelper.o $(SRCDIR)/dirhelper.c
	cc $(CFLAGS) $(LDFLAGS) -o $(SYMROOT)/dirhelper $(OBJDIR)/dirhelperServer.o $(OBJDIR)/dirhelperUser.o $(OBJDIR)/dirhelper.o

client: mighdrs
	cc $(CFLAGS) $(LDFLAGS) -o $(SYMROOT)/client $(SRCDIR)/client.c $(OBJDIR)/dirhelperUser.c

install: dirhelper
	install -m 0644 -o root -g wheel -d $(DSTROOT)/System/Library/LaunchDaemons
	install -m 0644 -o root -g wheel com.apple.bsd.dirhelper.plist $(DSTROOT)/System/Library/LaunchDaemons/
	install -m 0755 -o root -g wheel -d $(DSTROOT)/usr/libexec
	install -m 0755 -o root -g wheel $(SYMROOT)/dirhelper $(DSTROOT)/usr/libexec/
	strip -S $(DSTROOT)/usr/libexec/dirhelper
	mkdir -p "$(DSTROOT)/usr/share/man/man8"
	gzip -c "$(SRCDIR)/dirhelper.8" > "$(DSTROOT)/usr/share/man/man8/dirhelper.8.gz"

installsrc:
	mkdir "$(SRCDIR)"
	tar cf - . | tar xf - -C "$(SRCDIR)"

mighdrs:
	mkdir -p $(OBJDIR)
	cd $(OBJDIR) && mig -sheader dirhelperServer.h /usr/local/include/dirhelper.defs

installhdrs:
	@echo nothing to install

#installhdrs: mighdrs
#	install -m 0755 -o root -g wheel -d $(DSTROOT)/usr/local/include
#	install -m 0644 -o root -g wheel $(OBJDIR)/dirhelper.h $(DSTROOT)/usr/local/include/

clean:
	-rm -f *.o client dirhelper
	-rm -rf dirhelper.obj
