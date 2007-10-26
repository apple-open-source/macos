ARCHS = $(foreach arch, $(RC_ARCHS), -arch $(arch))
CC = gcc
CFLAGS = -g -Wall -Werror
CFLAGS+= $(RC_CFLAGS)
LDFLAGS = $(ARCHS) $(FRAMEWORKS)
FRAMEWORKS = -framework CoreFoundation

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
DATDIR = $(PREFIX)/share/TargetConfigs

CFILES = tconf.c utils.c
OBJS = $(CFILES:%.c=%.o)

ifeq "$(SRCROOT)" ""
SRCROOT="$(shell pwd)"
endif
ifeq "$(OBJROOT)" ""
OBJROOT=.
endif
ifeq "$(SYMROOT)" ""
SYMROOT=.
endif
ifeq "$(DSTROOT)" ""
DSTROOT=.
endif

all: tconf
install: tconf

installsrc: distclean
ifneq "$(SRCROOT)" "$(shell pwd)"
	tar cf - . | tar xf - -C "$(SRCROOT)"
endif

installhdrs:
	@echo Nothing to be done.

$(OBJROOT)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

tconf: $(foreach obj, $(OBJS), $(OBJROOT)/$(obj))
	$(CC) $(LDFLAGS) -o $(SYMROOT)/$@ $^
ifneq "$(SYMROOT)" "$(DSTROOT)"
	mkdir -p $(DSTROOT)/$(BINDIR)
	cp $(SYMROOT)/$@ $(DSTROOT)/$(BINDIR)/$@
	strip $(DSTROOT)/$(BINDIR)/$@
	mkdir -p $(DSTROOT)/$(DATDIR)
	cp *.plist $(DSTROOT)/$(DATDIR)
endif

clean:
	rm -f tconf
	rm -f *.o

distclean: clean
	rm -f .DS_Store
	rm -f .gdb_history
