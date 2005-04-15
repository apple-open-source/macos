RC_OS = macos
PROFILING = -DDYLD_PROFILING
OFLAG = -O
ifeq "macos" "$(RC_OS)"
  X_CFLAGS := $(shell if [ "$(RC_RELEASE)" != "Beaker" ] &&    \
			 [ "$(RC_RELEASE)" != "Bunsen" ] &&    \
			 [ "$(RC_RELEASE)" != "Gonzo"  ] &&    \
			 [ "$(RC_RELEASE)" != "Kodiak" ]; then \
	     echo -Wno-long-double -no-cpp-precomp ; \
	     fi; )
endif
MIG = $(NEXT_ROOT)/usr/bin/mig
ifneq "mwccppc" "$(notdir $(CC))"
  MIG_FLAGS = $(PROFILING)
endif

ifeq "mwccppc" "$(notdir $(CC))"
  CFLAGS = $(PROFILING) $(OFLAG) -g \
     -I$(SRCROOT) -I$(SRCROOT)/../include -I$(OFILE_DIR) -I$(SRCROOT)/../dyld
else
  CFLAGS = $(PROFILING) $(OFLAG) -g -Wall $(X_CFLAGS) \
     -I$(SRCROOT) -I$(SRCROOT)/../include -I$(OFILE_DIR) -I$(SRCROOT)/../dyld
endif

USE_DEPENDENCY_FILE := $(shell if [ "$(notdir $(CC))" != "mwccppc" ]; then \
	     echo YES ; else echo NO ; \
	     fi; )

ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

SRCROOT = .
SYMROOT = .
OBJROOT = .
OFILE_DIRS = $(OBJROOT)/shlib_obj $(OBJROOT)/profile_obj \
	     $(OBJROOT)/optimized_obj $(OBJROOT)/dynamic_obj \
	     $(OBJROOT)/ptmp_obj $(OBJROOT)/otmp_obj $(OBJROOT)/dtmp_obj
OFILE_DIR = $(OBJROOT)
VPATH = $(OFILE_DIR)

ifeq "macos" "$(RC_OS)"
  SYSTEMDIR := $(shell if [ "$(RC_RELEASE)" = "Beaker" ] || \
			  [ "$(RC_RELEASE)" = "Bunsen" ] || \
			  [ "$(RC_RELEASE)" = "Gonzo" ]; then \
	 echo /Local/Developer/System; else echo /usr/local/lib/system; fi; )
else
  SYSTEMDIR = /Local/Developer/System
endif

CFILES = images.c bind.c debug.c ofi.c runtime_loading.c dyld_support.c \
	 register_funcs.c objc_modules.c private_callback.c dlopen.c core.c
HFILES = dyld_debug_MsgError.h ofi.h dlfcn.h debug.h
INSTALL_FILES = $(CFILES) $(HFILES) shlib.ofileList dylib.ofileList Makefile \
		notes dyld_debug_api

COMMON_OBJS = dyld.o dyld_debug.o objectfileimage.o runtime_loading.o \
	      dyld_support.o
SHLIB_OBJS = $(COMMON_OBJS)
OBJS = $(COMMON_OBJS) dyld_event_server.o

all: lib_ofiles

macos:

teflon: optimized_ofiles

nextstep: optimized_ofiles shlib_ofiles

lib_ofiles: $(OFILE_DIRS) $(SYMROOT) $(RC_OS) dynamic_ofiles profile_ofiles

shlib_ofiles: $(OBJROOT)/shlib_obj
	@if [ $(SRCROOT) = . ];						\
	then								\
	    (cd shlib_obj;						\
		$(MAKE) -f ../Makefile $(SHLIB_OBJS)			\
		VPATH=.. 						\
		OFILE_DIR=.						\
		SRCROOT=..						\
		COPTS="-static -DSHLIB -I/LocalDeveloper/Headers/libsys" \
		LIBSTUFF="-L../../libstuff -lstuff_s"			\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    rm -f shlib_obj/libdyld.ofileList;				\
	    cp shlib.ofileList shlib_obj/libdyld.ofileList;		\
	else								\
	    (cd $(OBJROOT)/shlib_obj;					\
		$(MAKE) -f $(SRCROOT)/Makefile $(SHLIB_OBJS)		\
		VPATH=$(SRCROOT)					\
		OFILE_DIR=$(OBJROOT)/shlib_obj				\
		SRCROOT=$(SRCROOT)					\
		SYMROOT=$(SYMROOT)					\
		COPTS="-static -DSHLIB -I/LocalDeveloper/Headers/libsys" \
		LIBSTUFF="-L$(SYMROOT)/../libstuff -lstuff_s"		\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    rm -f $(OBJROOT)/shlib_obj/libdyld.ofileList;		\
	    cp shlib.ofileList $(OBJROOT)/shlib_obj/libdyld.ofileList;	\
	fi

profile_ofiles:	$(OBJROOT)/profile_obj $(OBJROOT)/ptmp_obj
	@if [ $(SRCROOT) = . ];						\
	then								\
	    (cd ptmp_obj;						\
		$(MAKE) -f ../Makefile libdyld_pg.a			\
		VPATH=.. 						\
		OFILE_DIR=.						\
		SRCROOT=..						\
		COPTS="-dynamic -pg"					\
		LIBSTUFF="-L../../libstuff -lstuff_p"			\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd profile_obj; rm -f $(OBJS));				\
	    (cd ptmp_obj; cp $(OBJS) ../profile_obj);			\
	    rm -f dynamic_profile_obj;					\
	    ln -s ./profile_obj dynamic_profile_obj;			\
	    rm -f profile_obj/libdyld.ofileList;			\
	    cp dylib.ofileList profile_obj/libdyld.ofileList;		\
	else								\
	    (cd $(OBJROOT)/ptmp_obj;					\
		$(MAKE) -f $(SRCROOT)/Makefile libdyld_pg.a		\
		VPATH=$(SRCROOT)					\
		OFILE_DIR=$(OBJROOT)/ptmp_obj				\
		SRCROOT=$(SRCROOT)					\
		SYMROOT=$(SYMROOT)					\
		LIBSTUFF="-L$(SYMROOT)/../libstuff -lstuff_p"		\
		COPTS="-dynamic -pg"					\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd $(OBJROOT)/profile_obj; rm -f $(OBJS));			\
	    (cd $(OBJROOT)/ptmp_obj; cp $(OBJS) $(OBJROOT)/profile_obj); \
	    rm -f $(OBJROOT)/dynamic_profile_obj;			\
	    ln -s ./profile_obj $(OBJROOT)/dynamic_profile_obj;		\
	    rm -f $(OBJROOT)/profile_obj/libdyld.ofileList;		\
	    cp dylib.ofileList $(OBJROOT)/profile_obj/libdyld.ofileList; \
	fi

optimized_ofiles: $(OBJROOT)/optimized_obj $(OBJROOT)/otmp_obj
	@if [ $(SRCROOT) = . ];						\
	then								\
	    (cd otmp_obj;						\
		$(MAKE) -f ../Makefile libdyld_static.a			\
		VPATH=.. 						\
		OFILE_DIR=.						\
		SRCROOT=..						\
		COPTS="-static"						\
		LIBSTUFF="-L../../libstuff -lstuff"			\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd optimized_obj; rm -f $(OBJS));				\
	    (cd otmp_obj; cp $(OBJS) ../optimized_obj);			\
	    rm -f static_obj;						\
	    ln -s ./optimized_obj static_obj;				\
	    rm -f optimized_obj/libdyld.ofileList;			\
	    cp dylib.ofileList optimized_obj/libdyld.ofileList;		\
	else								\
	    (cd $(OBJROOT)/otmp_obj;					\
		$(MAKE) -f $(SRCROOT)/Makefile libdyld_static.a		\
		VPATH=$(SRCROOT)					\
		OFILE_DIR=$(OBJROOT)/otmp_obj				\
		SRCROOT=$(SRCROOT)					\
		SYMROOT=$(SYMROOT)					\
		COPTS="-static"						\
		LIBSTUFF="-L$(SYMROOT)/../libstuff -lstuff"		\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd $(OBJROOT)/optimized_obj; rm -f $(OBJS));		\
	    (cd $(OBJROOT)/otmp_obj; cp $(OBJS) $(OBJROOT)/optimized_obj); \
	    rm -f $(OBJROOT)/static_obj;				\
	    ln -s ./optimized_obj $(OBJROOT)/static_obj;		\
	    rm -f $(OBJROOT)/optimized_obj/libdyld.ofileList;		\
	    cp dylib.ofileList $(OBJROOT)/optimized_obj/libdyld.ofileList; \
	fi

dynamic_ofiles: $(OBJROOT)/dynamic_obj $(OBJROOT)/dtmp_obj
	@if [ $(SRCROOT) = . ];						\
	then								\
	    (cd dtmp_obj;						\
		$(MAKE) -f ../Makefile libdyld.a			\
		VPATH=.. 						\
		OFILE_DIR=.						\
		SRCROOT=..						\
		COPTS="-dynamic"					\
		LIBSTUFF="-L../../libstuff -lstuff"			\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd dynamic_obj; rm -f $(OBJS));				\
	    (cd dtmp_obj; cp $(OBJS) ../dynamic_obj);			\
	    rm -f dynamic_obj/libdyld.ofileList;			\
	    cp dylib.ofileList dynamic_obj/libdyld.ofileList;		\
	else								\
	    (cd $(OBJROOT)/dtmp_obj;					\
		$(MAKE) -f $(SRCROOT)/Makefile libdyld.a		\
		VPATH=$(SRCROOT)					\
		OFILE_DIR=$(OBJROOT)/dtmp_obj				\
		SRCROOT=$(SRCROOT)					\
		SYMROOT=$(SYMROOT)					\
		LIBSTUFF="-L$(SYMROOT)/../libstuff -lstuff"		\
		COPTS="-dynamic"					\
		OFLAG="$(OFLAG)"					\
		RC_CFLAGS="$(RC_CFLAGS)"				\
		RC_ARCHS="$(RC_ARCHS)");				\
	    (cd $(OBJROOT)/dynamic_obj; rm -f $(OBJS));			\
	    (cd $(OBJROOT)/dtmp_obj; cp $(OBJS) $(OBJROOT)/dynamic_obj); \
	    rm -f $(OBJROOT)/dynamic_obj/libdyld.ofileList;		\
	    cp dylib.ofileList $(OBJROOT)/dynamic_obj/libdyld.ofileList; \
	fi

dyld.o: images.o bind.o register_funcs.o objc_modules.o
	$(CC) $(COPTS) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r -o $@ \
	    images.o bind.o register_funcs.o objc_modules.o

dyld_debug.o: debug.o _dyld_debugUser.o _dyld_eventServer.o core.o
	$(CC) $(COPTS) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r -o $@ \
	    debug.o _dyld_debugUser.o _dyld_eventServer.o core.o
	nmedit -arch all -s $(SRCROOT)/dyld_debug_api $@

dyld_event_server.o: _dyld_eventServer.o private_callback.o
	$(CC) $(COPTS) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r -o $@ \
	    _dyld_eventServer.o private_callback.o

objectfileimage.o: ofi.o ofile.o
	$(CC) $(COPTS) $(CFLAGS) $(RC_CFLAGS) -nostdlib -r -o $@ \
	    ofi.o ofile.o $(LIBSTUFF)

ofile.o: $(SRCROOT)/../libstuff/ofile.c
ifeq "NO" "$(USE_DEPENDENCY_FILE)"
	$(CC) $(COPTS) $(CFLAGS) $(RC_CFLAGS) -c -o $(OFILE_DIR)/$*.o \
		$(SRCROOT)/../libstuff/ofile.c -DOFI
else
	$(CC) $(COPTS) $(CFLAGS) $(RC_CFLAGS) -c -MD -dependency-file \
		$(OFILE_DIR)/$*.d -o $(OFILE_DIR)/$*.o \
		$(SRCROOT)/../libstuff/ofile.c -DOFI
	md -d -u $(OFILE_DIR)/Makedep $(OFILE_DIR)/$*.d
endif

debug.c: _dyld_debug.h
_dyld_debug.h: _dyld_debugUser.c

_dyld_debugUser.c: $(SRCROOT)/../dyld/dyld_debug.defs dyld_debug_MsgError.h
	@if [ $(SRCROOT) = .. ];					\
	then								\
	    echo "$(MIG) $(MIG_FLAGS) $(SRCROOT)/../dyld/dyld_debug.defs"; \
	    $(MIG) $(MIG_FLAGS) $(SRCROOT)/../dyld/dyld_debug.defs;	\
	    rm -f _dyld_debugServer.c;					\
	else								\
	    echo -n "cd $(OFILE_DIR)" ;					\
	    echo "$(MIG) $(MIG_FLAGS) $(SRCROOT)/../dyld/dyld_debug.defs"; \
	    cd $(OFILE_DIR); 						\
		$(MIG) $(MIG_FLAGS) $(SRCROOT)/../dyld/dyld_debug.defs;	\
	    cd $(OFILE_DIR); rm -f _dyld_debugServer.c;			\
	fi

_dyld_debugUser.o: _dyld_debugUser.c
ifeq "mwccppc" "$(notdir $(CC))"
	cd $(OFILE_DIR); \
	$(CC) $(CFLAGS) $(RC_CFLAGS) $(COPTS) -c _dyld_debugUser.c
else
	cd $(OFILE_DIR); \
	$(CC) $(CFLAGS) $(RC_CFLAGS) $(COPTS) \
		-Dmig_external=__private_extern__ -c _dyld_debugUser.c
endif

_dyld_eventServer.c: $(SRCROOT)/../dyld/dyld_event.defs
	@if [ $(SRCROOT) = .. ];					\
	then								\
	    echo "$(MIG) $(MIG_FLAGS) $(SRCROOT)/../dyld/dyld_event.defs"; \
	    $(MIG) $(MIG_FLAGS) $(SRCROOT)/../dyld/dyld_event.defs;	\
	    rm -f _dyld_eventUser.c _dyld_event.h;			\
	else								\
	    echo -n "cd $(OFILE_DIR)";					\
	    echo "$(MIG) $(MIG_FLAGS) $(SRCROOT)/../dyld/dyld_event.defs"; \
	    cd $(OFILE_DIR); 						\
		$(MIG) $(MIG_FLAGS) $(SRCROOT)/../dyld/dyld_event.defs;	\
	    cd $(OFILE_DIR); rm -f _dyld_eventUser.c _dyld_event.h;	\
	fi

_dyld_eventServer.o: _dyld_eventServer.c
	cd $(OFILE_DIR); \
	sed -e 's/static const SERVER_STUB_PROC/static SERVER_STUB_PROC/' \
		_dyld_eventServer.c > sed._dyld_eventServer.c ; \
	$(CC) $(CFLAGS) $(RC_CFLAGS) $(COPTS) -c sed._dyld_eventServer.c \
		-o _dyld_eventServer.o

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

clean:
	-rm -r -f $(OFILE_DIRS) $(OBJROOT)/static_obj

shlib_clean:
	-rm -r -f $(OBJROOT)/ptmp_obj $(OBJROOT)/otmp_obj $(OBJROOT)/dtmp_obj

libdyld.a libdyld_pg.a libdyld_static.a: $(OBJS)
	libtool -static -o $(SYMROOT)/$@ $(OBJS)

install: $(RC_OS)_install

macos_install:

teflon_install nextstep_install: all


lib_ofiles_install: $(RC_OS)_lib_ofiles_install

macos_lib_ofiles_install: all
	@if [ $(OLD_DYLD_STUFF) ];					\
	then								\
	    $(MKDIRS) $(DSTROOT)$(SYSTEMDIR); 				\
	    if [ $(SRCROOT) = . ];					\
	    then							\
		install -c -m 444 dtmp_obj/libdyld.a			\
			 $(DSTROOT)$(SYSTEMDIR)/libdyld.a;		\
		ranlib $(DSTROOT)$(SYSTEMDIR)/libdyld.a;		\
		(cd $(DSTROOT)$(SYSTEMDIR); 				\
		rm -f libdyld_debug.a; ln -s libdyld.a libdyld_debug.a); \
		install -c -m 444 ptmp_obj/libdyld_pg.a 		\
			 $(DSTROOT)$(SYSTEMDIR)/libdyld_profile.a;	\
		ranlib $(DSTROOT)$(SYSTEMDIR)/libdyld_profile.a;	\
	    else							\
		install -c -m 444 $(SYMROOT)/libdyld.a			\
			 $(DSTROOT)$(SYSTEMDIR)/libdyld.a;		\
		ranlib $(DSTROOT)$(SYSTEMDIR)/libdyld.a;		\
		(cd $(DSTROOT)$(SYSTEMDIR); 				\
		rm -f libdyld_debug.a; ln -s libdyld.a libdyld_debug.a); \
		install -c -m 444 $(SYMROOT)/libdyld_pg.a 		\
			 $(DSTROOT)$(SYSTEMDIR)/libdyld_profile.a;	\
		ranlib $(DSTROOT)$(SYSTEMDIR)/libdyld_profile.a;	\
	    fi; 							\
	fi

teflon_lib_ofiles_install nextstep_lib_ofiles_install:

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(OFILE_DIRS) $(SYMROOT):
	$(MKDIRS) $@

-include $(OFILE_DIR)/Makedep
