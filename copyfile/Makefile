##
# Makefile for copyfile
##
# Project info

Project			= copyfile
#Extra_CC_Flags		=

#lazy_install_source:: shadow_source

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SRCROOT ?= .
OBJROOT ?= .
SYMROOT ?= .
DSTROOT ?= .

LIB_DIR = $(DSTROOT)/usr/local/lib/system
MAN_DIR = $(DSTROOT)/usr/share/man/man3
INC_DIR = $(DSTROOT)/usr/include

WFLAGS= -Wno-trigraphs -Wmissing-prototypes -Wreturn-type -Wformat \
	-Wmissing-braces -Wparentheses -Wswitch -Wunused-function \
	-Wunused-label -Wunused-variable -Wunused-value -Wshadow \
	-Wsign-compare -Wall -Wextra -Wpointer-arith -Wreturn-type \
	-Wwrite-strings -Wcast-align -Wbad-function-cast \
	-Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls \
	-Wno-parentheses -Wformat=2 -Wimplicit-function-declaration \
	-Wshorten-64-to-32 -Wformat-security

CFLAGS +=	-D__DARWIN_NON_CANCELABLE=1 $(WFLAGS)
SRC = copyfile.c
VERSOBJ=	$(OBJROOT)/__version.o
OBJ = $(SRC:.c=.o)
HDRS= copyfile.h
LIBS = $(SYMROOT)/libcopyfile.a $(SYMROOT)/libcopyfile_profile.a $(SYMROOT)/libcopyfile_debug.a

installhdrs:: $(HDRS)
	install -d -m 755 $(INC_DIR)
	install -c -m 444 $(SRCROOT)/copyfile.h $(INC_DIR)

install:: $(LIBS)
	install -d -m 755 $(MAN_DIR)
	install -c -m 644 $(SRCROOT)/copyfile.3 $(MAN_DIR)
	for a in fcopyfile copyfile_state_alloc copyfile_state_free \
		copyfile_state_get copyfile_state_set ; do \
			ln $(MAN_DIR)/copyfile.3 $(MAN_DIR)/$$a.3 ; \
		done
	install -d -m 755 $(LIB_DIR)
	install -c -m 644 $(LIBS) $(LIB_DIR)
	install -d -m 755 $(INC_DIR)
	install -c -m 444 $(SRCROOT)/copyfile.h $(INC_DIR)

$(OBJROOT)/__version.c:
	/Developer/Makefiles/bin/version.pl $(Project) > $@

$(VERSOBJ):	$(OBJROOT)/__version.c
	$(CC) -c -Os $(CFLAGS) $(RC_CFLAGS) -o $@ $^

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c -Os $(CFLAGS) $(RC_CFLAGS) -o $@ $^

$(OBJROOT)/%-profile.o: $(SRCROOT)/%.c
	$(CC) -c -pg $(CFLAGS) $(RC_CFLAGS) -o $@ $^

$(OBJROOT)/%-debug.o: $(SRCROOT)/%.c
	$(CC) -c -g $(CFLAGS) $(RC_CFLAGS) -o $@ $^

$(SYMROOT)/libcopyfile.a:: $(OBJROOT)/$(OBJ) $(VERSOBJ)
	libtool -static -o $@ $^

$(SYMROOT)/libcopyfile_profile.a:: $(OBJROOT)/copyfile-profile.o $(VERSOBJ)
	libtool -static -o $@ $^

$(SYMROOT)/libcopyfile.a:: $(OBJROOT)/$(OBJ) $(VERSOBJ)
	libtool -static -o $@ $^

$(SYMROOT)/libcopyfile_debug.a:: $(OBJROOT)/copyfile-debug.o $(VERSOBJ)
	libtool -static -o $@ $^
