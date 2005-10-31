export USE_APPLE_PB_SUPPORT = all
# For debugging -DDEBUG (used only in error.c)
#DEBUG = -DDEBUG

# For profiling
#PGFLAGS = -pg -DDYLD_PROFILING
#GMON = gmon.o

RC_OS = macos

MIG = $(NEXT_ROOT)/usr/bin/mig
ifneq "mwccppc" "$(notdir $(CC))"
  MIG_FLAGS = $(PGFLAGS)
endif

USE_DEPENDENCY_FILE := $(shell if [ "$(notdir $(CC))" != "mwccppc" ] && \
				  [ "$(RC_XBS)" != "YES" ]; then \
	     echo YES ; else echo NO ; \
	     fi; )

OFLAG = -O

MACOSX_10_1_SEG1ADDR = 41100000
MACOSX_10_2_SEG1ADDR = 8fe00000
ifeq "macos" "$(RC_OS)"
  SEG1ADDR := $(shell if [ "$(RC_RELEASE)" = "Beaker"  ] || \
			 [ "$(RC_RELEASE)" = "Bunsen"  ] || \
			 [ "$(RC_RELEASE)" = "Gonzo"   ] || \
			 [ "$(RC_RELEASE)" = "Kodiak"  ] || \
			 [ "$(RC_RELEASE)" = "Cheetah" ] || \
			 [ "$(RC_RELEASE)" = "Puma"    ]; then \
			    echo $(MACOSX_10_1_SEG1ADDR) ; \
			else \
			    echo $(MACOSX_10_2_SEG1ADDR) ; fi; )
else
   SEG1ADDR = $(MACOSX_10_1_SEG1ADDR)
endif

PRIVATEHEADERS = /System/Library/Frameworks/System.framework/PrivateHeaders

ifeq "macos" "$(RC_OS)"
  X_CFLAGS := $(shell if [ "$(RC_RELEASE)" != "Beaker" ] &&    \
			 [ "$(RC_RELEASE)" != "Bunsen" ] &&    \
			 [ "$(RC_RELEASE)" != "Gonzo"  ] &&    \
			 [ "$(RC_RELEASE)" != "Kodiak" ]; then \
	     echo -Wno-long-double -no-cpp-precomp ; \
	     fi; )
endif
ifeq "mwccppc" "$(notdir $(CC))"
  CFLAGS = $(DEBUG) $(OFLAG) -dynamic -g -DDYLD_BUILD \
	 -I$(SRCROOT)/../include -I$(SRCROOT) -I$(OBJROOT) \
	 -I$(NEXT_ROOT)$(PRIVATEHEADERS)
else
  CFLAGS = $(DEBUG) $(OFLAG) -dynamic -g -Wall -DDYLD_BUILD \
	 -I$(SRCROOT)/../include -I$(SRCROOT) -I$(OBJROOT) $(X_CFLAGS) \
	 -I$(NEXT_ROOT)$(PRIVATEHEADERS)
  ASFLAGS = -force_cpusubtype_ALL
endif
LIBSTUFF = -L$(SYMROOT)/../libstuff -lstuff

ifeq "macos" "$(RC_OS)"
  SYSTEMDIR := $(shell if [ "$(RC_RELEASE)" = "Beaker" ] ||    \
			  [ "$(RC_RELEASE)" = "Bunsen" ] || \
			  [ "$(RC_RELEASE)" = "Gonzo" ]; then \
	 echo /Local/Developer/System; else echo /usr/local/lib/system; fi; )
else
  SYSTEMDIR = /Local/Developer/System
endif
ifeq "gcc3" "$(notdir $(CC))"
  LIBGCC = -lgcc
else
  LIBGCC = -lcc_dynamic
endif
ifeq "macos" "$(RC_OS)"
  LIBC = $(NEXT_ROOT)$(SYSTEMDIR)/libc.a $(LIBGCC)
else
  LIBC = -lc_dynamic $(LIBGCC)
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

ifeq "macos" "$(RC_OS)"
 USRLIBDIR := $(shell if [ "$(RC_RELEASE)" = "Tachyon" ]; then \
 echo /System/Library/PrivateFrameworks/ZeroLink.framework/Versions/A/Resources ; \
 else echo /usr/lib; \
 fi; )
else
  USRLIBDIR = /usr/lib
endif

DSTDIRS = $(DSTROOT)$(USRLIBDIR)

CFILES1 = dyld_init.c images.c errors.c allocate.c symbols.c reloc.c \
	  generic_reloc.c hppa_reloc.c entry_point.c dyld_libfuncs.c \
	  lock.c debug.c sparc_reloc.c mod_init_funcs.c register_funcs.c \
	  malloc.c getsecbyname.c mig_support.c ppc_reloc.c trace.c
CFILES2 = dyld_reloc.c
SFILES1 = section_order.s stub_binding_helper.s machdep_lock.s cache_flush.s \
	  fp_save_restore.s halt.s
SFILES2 = dyld_start.s
HFILES = dyld_init.h images.h errors.h allocate.h symbols.h reloc.h \
	 entry_point.h lock.h debug.h mod_init_funcs.h register_funcs.h \
	 zoneprotect.h cthread_internals.h fp_save_restore.h inline_bsearch.h \
	 inline_strcmp.h trace.h
MIGFILES = dyld_debug.defs dyld_debug_defs.h dyld_event.defs dyld_event_defs.h \
	   dyld_event_MsgError.h dyld_prebind.defs dyld_prebind_defs.h
OBJS1 = $(SFILES1:.s=.o) $(CFILES1:.c=.o) profile_client.o vers.o
OBJS2 = $(CFILES2:.c=.o) $(SFILES2:.s=.o)
INSTALL_FILES = $(CFILES1) $(CFILES2) $(SFILES1) $(SFILES2) $(HFILES) \
		$(MIGFILES) Makefile notes dyld_api gmon.c

PRODUCT = dyld
all: $(OFILE_DIR) $(SYMROOT) $(PRODUCT).NEW

$(PRODUCT).NEW:	$(OFILE_DIR) $(SYMROOT) $(OBJS1) $(OBJS2) _dyld_debugServer.o \
		_dyld_eventUser.o _dyld_prebindUser.o $(GMON)
	$(CC) $(CFLAGS) $(RC_CFLAGS) $(OBJS1) $(OBJS2) _dyld_debugServer.o \
		_dyld_eventUser.o _dyld_prebindUser.o $(GMON) $(LIBSTUFF) \
	        $(LIBC) -segprot __TEXT rwx rwx -seg1addr $(SEG1ADDR) \
		-o $(SYMROOT)/$@ -Wl,-e,__dyld_start -nostdlib \
		-Wl,-dylinker -Wl,-dylinker_install_name,$(USRLIBDIR)/$(PRODUCT)

# This object file is used as a hack to link into a program with a crt1.o that
# calls _dyld_init() directly in place of having the kernel support.
#dyld.o: $(OFILE_DIR) $(OBJS1)
#	$(CC) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r -d \
#		-o $(OFILE_DIR)/dyld.o $(OBJS1) _dyld_debugServer.o \
#		_dyld_eventUser.o _dyld_prebindUser.o $(LIBSTUFF)
#	nmedit -arch all -s dyld_api $(OFILE_DIR)/dyld.o

_dyld_debugServer.c: dyld_debug.defs
	@if [ $(SRCROOT) = . ];						\
	then								\
	    echo "$(MIG) $(MIG_FLAGS) $(SRCROOT)/dyld_debug.defs";	\
	    $(MIG) $(MIG_FLAGS) $(SRCROOT)/dyld_debug.defs;		\
	    rm -f _dyld_debugUser.c _dyld_debug.h;			\
	else								\
	    echo -n "cd $(OBJROOT); "					\
	    echo "$(MIG) $(MIG_FLAGS) $(SRCROOT)/dyld_debug.defs";	\
	    cd $(OBJROOT); $(MIG) $(MIG_FLAGS) $(SRCROOT)/dyld_debug.defs; \
	    cd $(OBJROOT); rm -f _dyld_debugUser.c _dyld_debug.h;	\
	fi

_dyld_debugServer.o: _dyld_debugServer.c
	cd $(OBJROOT); \
	$(CC) $(PGFLAGS) $(CFLAGS) $(RC_CFLAGS) -c _dyld_debugServer.c

debug.c: _dyld_event.h
_dyld_event.h: _dyld_eventUser.c

_dyld_eventUser.c: dyld_event.defs dyld_event_MsgError.h
	@if [ $(SRCROOT) = . ];						\
	then								\
	    echo "$(MIG) $(MIG_FLAGS) $(SRCROOT)/dyld_event.defs";	\
	    $(MIG) $(MIG_FLAGS) $(SRCROOT)/dyld_event.defs;		\
	    rm -f _dyld_eventServer.c;					\
	else								\
	    echo -n "cd $(OBJROOT); "					\
	    echo "$(MIG) $(MIG_FLAGS) $(SRCROOT)/dyld_event.defs";	\
	    cd $(OBJROOT); $(MIG) $(MIG_FLAGS) $(SRCROOT)/dyld_event.defs; \
	    cd $(OBJROOT); rm -f _dyld_eventServer.c;			\
	fi


_dyld_eventUser.o: _dyld_eventUser.c
	cd $(OBJROOT); \
	$(CC) $(PGFLAGS) $(CFLAGS) $(RC_CFLAGS) -c _dyld_eventUser.c

images.c: _dyld_prebind.h
_dyld_prebind.h: _dyld_prebindUser.c

_dyld_prebindUser.c: dyld_prebind.defs
	@if [ $(SRCROOT) = . ];						\
	then								\
	    echo "$(MIG) $(MIG_FLAGS) $(SRCROOT)/dyld_prebind.defs";	\
	    $(MIG) $(MIG_FLAGS) $(SRCROOT)/dyld_prebind.defs;		\
	    rm -f _dyld_prebindServer.c;				\
	else								\
	    echo -n "cd $(OBJROOT); "					\
	    echo "$(MIG) $(MIG_FLAGS) $(SRCROOT)/dyld_prebind.defs";	\
	    cd $(OBJROOT); $(MIG) $(MIG_FLAGS) $(SRCROOT)/dyld_prebind.defs; \
	    cd $(OBJROOT); rm -f _dyld_prebindServer.c;			\
	fi

_dyld_prebindUser.o: _dyld_prebindUser.c
	cd $(OBJROOT); \
	$(CC) $(PGFLAGS) $(CFLAGS) $(RC_CFLAGS) -c _dyld_prebindUser.c

vers.c:
	vers_string -c $(VERS_STRING_FLAGS) $(PRODUCT) > $(OFILE_DIR)/$@

ifeq "NO" "$(USE_DEPENDENCY_FILE)"
.c.o:
	$(CC) $(PGFLAGS) $(CFLAGS) $(RC_CFLAGS) -c -o $(OFILE_DIR)/$*.o $<
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d
else
.c.o:
	$(CC) $(PGFLAGS) $(CFLAGS) $(RC_CFLAGS) -c -MD -dependency-file \
		$(OFILE_DIR)/$*.d -o $(OFILE_DIR)/$*.o $<
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d
endif

profile_client.o: $(SRCROOT)/../profileServer/profile_client.c
ifeq "NO" "$(USE_DEPENDENCY_FILE)"
	$(CC) $(PGFLAGS) $(CFLAGS) $(RC_CFLAGS) -c -o $(OFILE_DIR)/$*.o \
		$(SRCROOT)/../profileServer/profile_client.c
else
	$(CC) $(PGFLAGS) $(CFLAGS) $(RC_CFLAGS) -c -MD -dependency-file \
		$(OFILE_DIR)/$*.d -o $(OFILE_DIR)/$*.o \
		$(SRCROOT)/../profileServer/profile_client.c
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d
endif

# dyld_reloc.o must be compiled with no $(PGFLAGS)
dyld_reloc.o: dyld_reloc.c
ifeq "NO" "$(USE_DEPENDENCY_FILE)"
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -o $(OFILE_DIR)/$*.o $<
else
	$(CC) $(CFLAGS) $(RC_CFLAGS) -c -MD -dependency-file \
		$(OFILE_DIR)/$*.d -o $(OFILE_DIR)/$*.o $<
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d
endif

# gmon.o must be compiled with no $(PGFLAGS)
gmon.o: gmon.c
ifeq "NO" "$(USE_DEPENDENCY_FILE)"
	$(CC) -DDYLD_PROFILING $(CFLAGS) $(RC_CFLAGS) -c -o $(OFILE_DIR)/$*.o $<
else
	$(CC) -DDYLD_PROFILING $(CFLAGS) $(RC_CFLAGS) -c -MD -dependency-file \
		$(OFILE_DIR)/$*.d -o $(OFILE_DIR)/$*.o $<
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d
endif

.s.o:
ifeq "mwccppc" "$(notdir $(CC))"
	cc -c -o $(OFILE_DIR)/$*.o $<
else
	$(CC) $(RC_CFLAGS) $(ASFLAGS) -c -o $(OFILE_DIR)/$*.o $<
endif

clean:	shlib_clean
	-rm -f $(SYMROOT)/$(PRODUCT).NEW $(OFILE_DIR)/vers.c

shlib_clean:
	-cd $(OFILE_DIR); rm -f $(OBJS1) $(OBJS2) $(GMON)
	-cd $(OFILE_DIR); rm -r -f _dyld_debugUser.c _dyld_debugServer.c
	-cd $(OFILE_DIR); rm -r -f _dyld_debugUser.o _dyld_debugServer.o
	-cd $(OFILE_DIR); rm -r -f _dyld_eventUser.c _dyld_eventServer.c
	-cd $(OFILE_DIR); rm -r -f _dyld_eventUser.o _dyld_eventServer.o
	-cd $(OFILE_DIR); rm -r -f _dyld_prebindUser.c _dyld_prebindServer.c
	-cd $(OFILE_DIR); rm -r -f _dyld_prebindUser.o _dyld_prebindServer.o
	-cd $(OFILE_DIR); rm -r -f _dyld_debug.h _dyld_event.h _dyld_prebind.h
	-rm -f $(OFILE_DIR)/dyld.o
	-rm -f $(OFILE_DIR)/Makedep make.out

install: all $(DSTDIRS)
	strip -S $(SYMROOT)/$(PRODUCT).NEW -o $(SYMROOT)/$(PRODUCT).NEW.strip
	install -c -m 555 $(SYMROOT)/$(PRODUCT).NEW.strip \
		$(DSTROOT)$(USRLIBDIR)/$(PRODUCT)

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

nextstep_LOCINCDIR = /LocalDeveloper/Headers
teflon_LOCINCDIR = /System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders
ifeq "macos" "$(RC_OS)"
 macos_LOCINCDIR := $(shell if [ "$(RC_RELEASE)" = "Beaker" ] ||    \
			       [ "$(RC_RELEASE)" = "Bunsen" ] ||    \
			       [ "$(RC_RELEASE)" = "Gonzo"  ] ||    \
			       [ "$(RC_RELEASE)" = "Kodiak" ]; then \
 echo /System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders; \
 else echo /usr/local/include; \
 fi; )
else
  macos_LOCINCDIR = /System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders
endif

installhdrs:
	$(MKDIRS) ${DSTROOT}${$(RC_OS)_LOCINCDIR}/dyld
	install -c -m 444 images.h ${DSTROOT}${$(RC_OS)_LOCINCDIR}/dyld

$(OFILE_DIR) $(SYMROOT) $(DSTDIRS):
	$(MKDIRS) $@

-include $(OFILE_DIR)/Makedep
