##
# Makefile for removefile
##

Project = removefile

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

LIB_DIR = $(DSTROOT)/usr/local/lib/system
MAN_DIR = $(DSTROOT)/usr/share/man/man3
INC_DIR = $(DSTROOT)/usr/include

CFLAGS +=	-D__DARWIN_NON_CANCELABLE=1
CFLAGS +=	-Wall -Werror
CFLAGS +=	$(RC_CFLAGS)

SRC =	removefile.c \
	removefile_random.c \
	removefile_sunlink.c \
	removefile_rename_unlink.c \
	removefile_tree_walker.c

VERSOBJ=	$(OBJROOT)/__version.o
OBJ = $(SRC:.c=.o)
HDRS= removefile.h checkint.h
LIBS =	$(SYMROOT)/libremovefile.a \
	$(SYMROOT)/libremovefile_profile.a \
	$(SYMROOT)/libremovefile_debug.a

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) LICENSE $(OSL)/$(Project).txt

installhdrs:: $(HDRS)
	$(_v) $(INSTALL_DIRECTORY) $(INC_DIR)
	$(_v) $(INSTALL) -c -m 444 -o root -g wheel $(SRCROOT)/removefile.h $(INC_DIR)
	$(_v) $(INSTALL) -c -m 444 -o root -g wheel $(SRCROOT)/checkint.h $(INC_DIR)

$(OBJROOT) $(SYMROOT)::
	$(MKDIR) $@ 

install:: $(OBJROOT) $(SYMROOT) $(LIBS) install-files compress_man_pages install-plist
	$(_v) $(INSTALL_DIRECTORY) $(LIB_DIR)
	$(_v) $(INSTALL) -c -m 644 $(LIBS) $(LIB_DIR)
	$(_v) $(INSTALL_DIRECTORY) $(INC_DIR)
	$(_v) $(INSTALL) -c -m 444 -o root -g wheel $(SRCROOT)/removefile.h $(INC_DIR)
	$(_v) $(INSTALL) -c -m 444 -o root -g wheel $(SRCROOT)/checkint.h $(INC_DIR)

install-files::
	$(_v) $(INSTALL_DIRECTORY) $(MAN_DIR)
	$(_v) $(INSTALL) -c -m 644 -o root -g wheel $(SRCROOT)/removefile.3 $(MAN_DIR)
	$(_v) $(INSTALL) -c -m 644 -o root -g wheel $(SRCROOT)/checkint.3 $(MAN_DIR)
	for a in removefile_state_alloc removefile_state_free \
		removefile_state_get removefile_state_set ; do \
			ln -s removefile.3 $(MAN_DIR)/$$a.3 ; \
		done
	for a in check_int32_add check_uint32_add check_int64_add check_uint64_add \
		 check_int32_sub check_uint32_sub check_int64_sub check_uint64_sub \
		 check_int32_mul check_uint32_mul check_int64_mul check_uint64_mul \
		 check_int32_div check_uint32_div check_int64_div check_uint64_div ; do \
			ln -s checkint.3 $(MAN_DIR)/$$a.3 ; \
		done

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

$(SYMROOT)/libremovefile.a:: $(foreach X, $(OBJ), $(OBJROOT)/$(X)) $(VERSOBJ)
	libtool -static -o $@ $^

$(SYMROOT)/libremovefile_profile.a:: $(foreach X, $(OBJ:.o=-profile.o), $(OBJROOT)/$(X)) $(VERSOBJ)
	libtool -static -o $@ $^

$(SYMROOT)/libremovefile_debug.a:: $(foreach X, $(OBJ:.o=-debug.o), $(OBJROOT)/$(X)) $(VERSOBJ)
	libtool -static -o $@ $^

.PHONY : test
test: 
	$(CC) -g test/test-removefile.c -o /tmp/test-removefile
	/tmp/test-removefile
