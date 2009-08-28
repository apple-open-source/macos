##
# Makefile for libclosure
# (Stolen from removefile)
##

Project = libclosure

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

LIB_DIR = $(DSTROOT)/usr/local/lib/system
MAN_DIR = $(DSTROOT)/usr/share/man/man3
INC_DIR = $(DSTROOT)/usr/include
PINC_DIR = $(DSTROOT)/usr/local/include

#CFLAGS +=	-Wall -Werror
CFLAGS +=	$(RC_CFLAGS)

SRC =	runtime.c data.c

VERSOBJ=	$(OBJROOT)/__version.o
OBJ = $(SRC:.c=.o)
HDRS= Block.h Block_private.h
LIBS =	$(SYMROOT)/libclosure.a \
	$(SYMROOT)/libclosure_profile.a \
	$(SYMROOT)/libclosure_debug.a

installhdrs:: $(HDRS)
	$(_v) $(INSTALL_DIRECTORY) $(INC_DIR) $(PINC_DIR)
	$(_v) $(INSTALL) -c -m 444 -o root -g wheel $(SRCROOT)/Block.h $(INC_DIR)
	$(_v) $(INSTALL) -c -m 444 -o root -g wheel $(SRCROOT)/Block_private.h $(PINC_DIR)

$(OBJROOT) $(SYMROOT)::
	$(MKDIR) $@ 

install:: $(OBJROOT) $(SYMROOT) $(LIBS)
	$(_v) $(INSTALL_DIRECTORY) $(LIB_DIR)
	$(_v) $(INSTALL) -c -m 644 $(LIBS) $(LIB_DIR)
	$(_v) $(INSTALL_DIRECTORY) $(INC_DIR)
	$(_v) $(INSTALL_DIRECTORY) $(PINC_DIR)
	$(_v) $(INSTALL) -c -m 444 -o root -g wheel $(SRCROOT)/Block.h $(INC_DIR)
	$(_v) $(INSTALL) -c -m 444 -o root -g wheel $(SRCROOT)/Block_private.h $(PINC_DIR)


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

$(SYMROOT)/libclosure.a:: $(foreach X, $(OBJ), $(OBJROOT)/$(X)) $(VERSOBJ)
	libtool -static -o $@ $^

$(SYMROOT)/libclosure_profile.a:: $(foreach X, $(OBJ:.o=-profile.o), $(OBJROOT)/$(X)) $(VERSOBJ)
	libtool -static -o $@ $^

$(SYMROOT)/libclosure_debug.a:: $(foreach X, $(OBJ:.o=-debug.o), $(OBJROOT)/$(X)) $(VERSOBJ)
	libtool -static -o $@ $^
