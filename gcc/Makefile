##
##  Apple C Compiler Makefile.  
##
##  Ask Kresten if you have any troubles / questions.
##  (He probably won't be able to help, but ask him anyway!)
##

#
# ** MANUAL BUILDS **
#
# Use: 
#	setenv RC_ARCHS='i386 m68k ...'
#	setenv RC_CFLAGS='-arch i386 -arch m68k ...'
#	make debug TARGETS='i386 m68k ...' HOSTS='i386 ...'
#
# from this directory to do manual builds.  The build compiler(s) will
# appear in ./obj/cc-<target>-on-<host>/ subdirectories of here.  Use
# "./xgcc -B./" in there to run the specific compiler.  You can also
# just cd to a specific ./obj subdirectory and type "make <foo>" to build
# a specific program, like "cpp" or "cc1objplus".  

#
# ** AUTOMATIC BUILDS **
#
# Automatic builds strictly follow the RC makefile API standards.
#

# Enable NeXT extensions to (gnu)make.
USE_APPLE_PB_SUPPORT = all

HOSTS = `arch`
TARGET_OS = $(RC_OS)
targets = case "$(TARGET_OS)" in *win*) echo i386 ;; \
  *) if [ "$(TARGET_OS)" = teflon -o "$(TARGET_OS)" = macos \
	  -o -f /usr/bin/uname ]; then \
	echo $${TARGETS:-'ppc i386'}; \
     elif [ "$(RC_RELEASE)" = Grail ]; then \
	echo $${TARGETS:-'m68k i386 sparc ppc'}; \
     else \
	echo $${TARGETS:-'m68k i386 sparc'}; \
     fi ;; esac
TARGETS := $(shell $(targets))

RC_ARCHS = $(HOSTS)

SRCROOT = .

SRC = `cd $(SRCROOT) && pwd | sed s,/private,,`
OBJROOT = $(SRC)/obj
SYMROOT = $(OBJROOT)/../sym
DSTROOT = $(OBJROOT)/../dst

BOOTSTRAP = --bootstrap

VERSION = 2.95.2

ARCH = `arch`
CHOWN = `if [ -f /usr/etc/chown ]; then echo /usr/etc/chown; \
	elif [ -f /usr/sbin/chown ]; then echo /usr/sbin/chown; \
	else echo chown; fi`
MKDIRS = `case "$(RC_OS)" in *win*) echo mkdirs ;; \
	*) if [ -f /bin/mkdirs ]; then echo mkdirs; else echo mkdir -p; fi ;; \
	esac`
RM = `case "$(RC_OS)" in *win*) echo rm ;; *) echo /bin/rm ;; esac`

install: installhdrs build install_no_src libkeymgr
	if [ "$(RC_RELEASE)" = Flask ]; then \
	  cd $(DSTROOT)/usr/libexec && \
	  for dir in *; do \
	    cd $(DSTROOT)/usr/libexec/$$dir && \
	    ln -s */cpp .; \
	  done; \
	fi
	if [ "$(TARGET_OS)" = macos ]; then \
	  cd $(DSTROOT)/usr/libexec/gcc/darwin && \
	  for dir in *; do \
	    cd $(DSTROOT)/usr/libexec/gcc/darwin/$$dir && \
	    ln -s $(VERSION) default; \
	  done; \
	fi
	if [ "$(TARGET_OS)" = nextstep -o "$(TARGET_OS)" = teflon \
	     -o "$(TARGET_OS)" = macos ]; then \
	   cd $(DSTROOT)/usr/lib/gcc/darwin && \
	   ln -s $(VERSION) default && \
	   if [ "$$CHROOTED" ]; then \
	      echo "** cleaning unnessecary objects **"; \
	      $(RM) -Rf $(OBJROOT)/cc-*-on-*; \
	   fi; \
	fi

installhdrs: DSTROOT cplusplus_hdrs
	if [ "$(TARGET_OS)" = teflon -o "$(TARGET_OS)" = macos \
	     -o -f /usr/bin/uname ]; then \
	  std_include_dir=$(DSTROOT)/` \
	    if [ -f /usr/bin/uname -a "$(TARGET_OS)" = macos ]; then \
	      echo usr/include; \
	    else \
	      if [ -f /usr/bin/uname ]; then \
		echo System/Library/Frameworks/System.framework/Versions/B/Headers/bsd; \
	      else echo NextLibrary/Frameworks/System.framework/Versions/B/Headers/bsd; \
	      fi; \
	    fi`; \
	  os_name=` \
	    if [ "$(TARGET_OS)" = macos -o -f /usr/bin/uname ]; then \
	      echo darwin; \
	    else echo openstep; \
	    fi`; \
	  hdr_dir=$$std_include_dir/gcc/$$os_name/$(VERSION); \
	  $(MKDIRS) $$std_include_dir/machine && \
	  $(MKDIRS) $$hdr_dir/machine && \
	  ln -s $(VERSION) $$std_include_dir/gcc/$$os_name/default && \
	  cd gcc && \
	  for file in assert ../float ../inttypes ../stdint ginclude/stdarg \
	      ginclude/stdbool ginclude/varargs ginclude/va-ppc; do \
	    install -c -m 444 $$file.h $$hdr_dir && \
	    ln -s gcc/$$os_name/default/`echo $$file | sed 's;[-A-Za-z0-9_.]*/;;'`.h $$std_include_dir; \
	  done && \
	  $(RM) -f /tmp/limits.$$$$ && \
	  echo '#if !defined (_LIMITS_H___) && !defined (_MACH_MACHLIMITS_H_)' > /tmp/limits.$$$$ && \
	  echo '#if defined (__ppc__)'			>>/tmp/limits.$$$$ && \
	  echo '#include <ppc/limits.h>'		>>/tmp/limits.$$$$ && \
	  echo '#elif defined (__i386__)'		>>/tmp/limits.$$$$ && \
	  echo '#include <i386/limits.h>'		>>/tmp/limits.$$$$ && \
	  echo '#else'					>>/tmp/limits.$$$$ && \
	  echo '#error architecture not supported'	>>/tmp/limits.$$$$ && \
	  echo '#endif'					>>/tmp/limits.$$$$ && \
	  echo '#undef MB_LEN_MAX'			>>/tmp/limits.$$$$ && \
	  echo '#endif'					>>/tmp/limits.$$$$ && \
	  cat glimits.h					>>/tmp/limits.$$$$ && \
	  install -c -m 444 /tmp/limits.$$$$	$$hdr_dir/machine/limits.h && \
	  ln -s ../gcc/$$os_name/default/machine/limits.h		      \
						$$std_include_dir/machine  && \
	  $(RM) -f /tmp/limits.$$$$; \
	fi
#	OPENSTEP doesn't have machine/ansi.h, which is needed to build libcc.a,
#	so create an empty one.
	if [ "$(TARGET_OS)" = nextstep -a ! -f /usr/bin/uname ]; then \
	  $(MKDIRS) $(DSTROOT)/LocalDeveloper/Headers/machine && \
	  touch $(DSTROOT)/LocalDeveloper/Headers/machine/ansi.h && \
	  $(CHOWN) -f -R root.wheel $(DSTROOT)/LocalDeveloper && \
	  chmod -f -R a+r-w+X $(DSTROOT)/LocalDeveloper; \
	fi

build: OBJROOT SYMROOT
	APPLE_CC=`if echo $(SRCROOT) | grep '[0-9]$$' >/dev/null; then \
		    vers_string -f cc 2>/dev/null | sed -e 's/[-A-Za-z_]*//' \
			| sed -e 's/\.[0-9.]*//'; \
		  elif [ -f /usr/bin/uname ]; then \
		    date +%Y%m%d%H%M%S; \
		  fi`; \
	export APPLE_CC; \
	./build_gcc --thins \
		--srcroot=$(SRC) \
		--dstroot=$(DSTROOT) \
		--objroot=$(OBJROOT) \
		--symroot=$(SYMROOT) \
		--cflags="`if [ \( !  "$(TARGET_OS)" \
				   -o "$(TARGET_OS)" = nextstep \) \
				-a ! -f /usr/bin/uname ]; then \
			     echo -DOPENSTEP; \
			   elif [ "$(TARGET_OS)" = macos ]; then \
			     echo -DMACOSX; \
			   fi` $(RC_CFLAGS) $(OTHER_CFLAGS) -g" \
		--hosts="$(RC_ARCHS)" \
		--targets="$(TARGETS)" \
		--targetos="$(TARGET_OS)" \
		--bison="$(BISON)" \
		$(BOOTSTRAP)
#	The following lines are for building a Mac-OS-X-Server-hosted
#	cross compiler for Mac OS.
#	cd $(OBJROOT) && \
#	$(SRC)/gcc/configure --host=powerpc-apple-macosx_server \
#		--target=powerpc-apple-macosx --srcdir=$(SRC)/gcc && \
#	$(MAKE) LANGUAGES=c LIBGCC2_DEBUG_CFLAGS=-g0 AR_FOR_TARGET=ar \
#		X_CPPFLAGS=-traditional-cpp LIBGCC2_INCLUDES=-I/usr/include \
#		SYSTEM_HEADER_DIR=/usr/include TCFLAGS=-g0 \
#		CFLAGS="-g `echo $(RC_CFLAGS) | sed 's/-arch [a-z0-9]*//g'` $(OTHER_CFLAGS)"

install_no_src:
	if [ "$(TARGET_OS)" = nextstep -o "$(TARGET_OS)" = teflon \
	     -o "$(TARGET_OS)" = macos ]; then \
	    APPLE_CC=`if echo $(SRCROOT) | grep '[0-9]$$' >/dev/null; then \
		    vers_string -f cc 2>/dev/null | sed -e 's/[-A-Za-z_]*//' \
			| sed -e 's/\.[0-9.]*//'; \
		  elif [ -f /usr/bin/uname ]; then \
		    date +%Y%m%d%H%M%S; \
		  fi`; \
	    export APPLE_CC; \
	    ./build_gcc --fats \
		--srcroot=$(SRC) \
		--dstroot=$(DSTROOT) \
		--objroot=$(OBJROOT) \
		--symroot=$(SYMROOT) \
		--cflags="`if [ \( !  "$(TARGET_OS)" \
				   -o "$(TARGET_OS)" = nextstep \) \
				-a ! -f /usr/bin/uname ]; then \
			     echo -DOPENSTEP; \
			   elif [ "$(TARGET_OS)" = macos ]; then \
			     echo -DMACOSX; \
			   fi` $(RC_CFLAGS) $(OTHER_CFLAGS)" \
		--hosts="$(RC_ARCHS)" \
		--targets="$(TARGETS)" \
		--targetos="$(TARGET_OS)" && \
	    if [ ! -f /usr/bin/uname ]; then \
		install -m 555 cc++ $(DSTROOT)/bin && \
		install -m 555 ld++ $(DSTROOT)/bin; \
	    fi; \
	fi

installGNUsrc: installsrc 
	$(CHOWN) -f -R root.wheel $(SRCROOT)
	chmod -f -R a+r-w+X $(SRCROOT)
	find $(SRCROOT) -type d -exec chmod -f 755 {} \;
	$(RM) -rf $(SRCROOT)/../egcs
	mv $(SRCROOT)/gcc $(SRCROOT)/../egcs
	$(RM) -rf $(SRCROOT)/../libstdc++
	mv $(SRCROOT)/libstdc++ $(SRCROOT)/..
	$(RM) -rf $(SRCROOT)

installsrc: SRCROOT
	if [ $(SRCROOT) != . ]; then \
	  $(MKDIRS) $(SRCROOT); \
	  pax -r -w -v -s ',.*/tests.*/.*,,' -s ',^gcc/f/.*,,' \
		-s ',^gcc/config/[^ria].*/.*,,' \
		Makefile Makefile.libiberty build_gcc configure config.if \
		config.sub config-ml.in include install-sh move-if-change gcc \
		libiberty libio libstdc++ \
		_G_config.h float.h inttypes.h stdint.h \
		keymgr.c cc1*.order \
		$(SRCROOT); \
	fi
	find -d "$(SRCROOT)" -type d -a -name CVS -exec $(RM) -rf {} \;
	cd "$(SRCROOT)" && find -d . -type d -a \
	  \( -name f -o -name 'tests*' \) -exec $(RM) -rf {} \;
	cd "$(SRCROOT)/gcc/config" && find -d . -type d -a \! -name . \
	  -a \! -name rs6000 -a \! -name i386 -a \! -name apple \
	  -exec $(RM) -rf {} \;
	find "$(SRCROOT)" \
	  \( -name \*~ -o -name \*\# -o -name \*.rej -o -name \*.orig \
	     -o -name \*.old -o -name \*.new -o -name .dir\* -o -name .\#\* \
	     -o -name .nfs\* \) \
	  -exec $(RM) -f {} \;


# keymgr build into system framework.
# Since keymgr must be built 3 times (debug, profile, optimized)
# and named according to the option set used (libxxx.a, libxxx_debug.a,
# libxxx_profile.a), this build directly places the build result in the dst
# directory.

SYS_FRAMEWORK_DST = $(DSTROOT)/usr/local/lib/system
KEYMGR_OBJ = $(OBJROOT)/keymgr
KEYMGR_DEFINES = -DMACOSX -DPART_OF_SYSTEM_FRAMEWORK
CFLAGS = -I$(SRCROOT)/gcc/config/apple $(RC_CFLAGS) $(OTHER_CFLAGS) $(KEYMGR_DEFINES)
CC = $(DSTROOT)/usr/bin/cc -traditional-cpp

libkeymgr: $(KEYMGR_OBJ)/libkeymgr.a $(KEYMGR_OBJ)/libkeymgr_debug.a \
	   $(KEYMGR_OBJ)/libkeymgr_profile.a
	install -d $(SYS_FRAMEWORK_DST)
	install -c -m 444 $(KEYMGR_OBJ)/libkeymgr.a \
			  $(KEYMGR_OBJ)/libkeymgr_debug.a \
			  $(KEYMGR_OBJ)/libkeymgr_profile.a \
		$(SYS_FRAMEWORK_DST)

$(KEYMGR_OBJ)/libkeymgr.a: keymgr.c $(SRCROOT)/gcc/config/apple/keymgr.h \
			   KEYMGR_OBJ
	$(CC) $(CFLAGS) -O2 -c -o $(KEYMGR_OBJ)/keymgr.o keymgr.c
	libtool -static -o $@ $(KEYMGR_OBJ)/keymgr.o

$(KEYMGR_OBJ)/libkeymgr_debug.a: keymgr.c $(SRCROOT)/gcc/config/apple/keymgr.h\
				 KEYMGR_OBJ
	$(CC) $(CFLAGS) -g -c -o $(KEYMGR_OBJ)/keymgr.o keymgr.c
	libtool -static -o $@ $(KEYMGR_OBJ)/keymgr.o

$(KEYMGR_OBJ)/libkeymgr_profile.a: keymgr.c \
				   $(SRCROOT)/gcc/config/apple/keymgr.h \
				   KEYMGR_OBJ
	$(CC) $(CFLAGS) -pg -c -o $(KEYMGR_OBJ)/keymgr.o keymgr.c
	libtool -static -o $@ $(KEYMGR_OBJ)/keymgr.o
				

#######################################################################
# C++-related targets
#
#   Headers go into $$hdr_dir, set below, dependent upon
#     TARGET_OS  and  uname:
#
#     teflon or macos, or uname exists  ==> 
#         /NextLibrary/Frameworks/System.framework/Versions/B/Headers/bsd/gcc/openstep/$(VERSION)/g++
# or   /System/Library/Frameworks/System.framework/Versions/B/Headers/bsd/gcc/darwin/$(VERSION)/g++
# or   /usr/include/gcc/darwin/$(VERSION)/g++
#
# For now, be explicit instead of theoretically "FSF-correct"
#

# Most of libstdc++
LIBSTDCPP_HDRS = cassert cctype cerrno cfloat ciso646 \
    climits clocale cmath complex complex.h csetjmp \
    csignal cstdarg cstddef cstdio cstdlib cstring ctime \
    cwchar cwctype fstream iomanip iosfwd iostream \
    stdexcept stl.h string strstream

LIBSTDCPP_STL_HDRS = algo.h algobase.h algorithm alloc.h bvector.h	\
    defalloc.h deque deque.h function.h functional hash_map hash_map.h	\
    hash_set hash_set.h hashtable.h heap.h iterator iterator.h list	\
    list.h map map.h memory multimap.h multiset.h numeric pair.h	\
    pthread_alloc pthread_alloc.h queue rope rope.h ropeimpl.h set	\
    set.h slist slist.h stack stack.h stl_algo.h stl_algobase.h		\
    stl_alloc.h stl_bvector.h stl_config.h stl_construct.h stl_deque.h	\
    stl_function.h stl_hash_fun.h stl_hash_map.h stl_hash_set.h		\
    stl_hashtable.h stl_heap.h stl_iterator.h stl_list.h stl_map.h	\
    stl_multimap.h stl_multiset.h stl_numeric.h stl_pair.h stl_queue.h	\
    stl_raw_storage_iter.h stl_relops.h stl_rope.h stl_set.h		\
    stl_slist.h stl_stack.h stl_tempbuf.h stl_tree.h			\
    stl_uninitialized.h stl_vector.h tempbuf.h tree.h type_traits.h	\
    utility vector vector.h

LIBSTDCPP_STD_HDRS = bastring.cc bastring.h complext.cc complext.h	\
    dcomplex.h fcomplex.h ldcomplex.h straits.h

# Some of the standard headers (e.g., <fstream>, <iomanip>)
# just #include these <*.h> versions from ./libio.
LIBIO_HDRS = fstream.h iomanip.h iostream.h \
  libio.h streambuf.h strfile.h strstream.h

# DO WE NEED THE OTHERS?  These are all of the *other* .h
#   files in libio.  I suspect we'll need to install some
#   of them.  Any that we find we need, just move from here
#   into LIBIO_HDRS.
#
LIBIO_OTHER_H = PlotFile.h SFile.h builtinbuf.h editbuf.h \
  floatio.h indstream.h iolibio.h iostdio.h iostreamP.h \
  istream.h libioP.h ostream.h parsestream.h pfstream.h \
  procbuf.h stdiostream.h stream.h

cplusplus_hdrs: DSTROOT
	if [ "$(TARGET_OS)" = teflon -o "$(TARGET_OS)" = macos \
	     -o -f /usr/bin/uname ]; then \
	  hdr_dir=$(DSTROOT)/` \
	    if [ -f /usr/bin/uname -a "$(TARGET_OS)" = macos ]; then \
	      echo usr/include; \
	    else \
	      if [ -f /usr/bin/uname ]; then \
		echo System/Library/Frameworks/System.framework/Versions/B/Headers/bsd; \
	      else echo NextLibrary/Frameworks/System.framework/Versions/B/Headers/bsd; \
	      fi; \
	    fi \
	    `/gcc/` \
	    if [ "$(TARGET_OS)" = macos -o -f /usr/bin/uname ]; then \
	      echo darwin; \
	    else echo openstep; \
	    fi \
	    `/$(VERSION)/g++; \
	  $(MKDIRS) $$hdr_dir && \
	  for file in exception new new.h typeinfo; do \
	    install -c -m 444 gcc/cp/inc/$$file $$hdr_dir; \
	  done; \
	  for file in $(LIBSTDCPP_HDRS); do \
	    install -c -m 444 libstdc++/$$file $$hdr_dir; \
	  done; \
	  for file in $(LIBSTDCPP_STL_HDRS); do \
	    install -c -m 444 libstdc++/stl/$$file $$hdr_dir; \
	  done; \
	  for file in $(LIBSTDCPP_STD_HDRS); do \
	    install -c -m 444 libstdc++/std/$$file $$hdr_dir; \
	  done; \
	  for file in $(LIBIO_HDRS); do \
	    install -c -m 444 libio/$$file $$hdr_dir; \
	  done; \
	  install -c -m 444 _G_config.h $$hdr_dir; \
	  cd $$hdr_dir ; \
	  if [ ! -h std ] ; then ln -s . std ; fi ; \
	fi

########################################################################

lib_ofiles:
	APPLE_CC=`if echo $(SRCROOT) | grep '[0-9]$$' >/dev/null; then \
		    vers_string -f cc 2>/dev/null | sed -e 's/[-A-Za-z_]*//' \
			| sed -e 's/\.[0-9.]*//'; \
		  elif [ -f /usr/bin/uname ]; then \
		    date +%Y%m%d%H%M%S; \
		  fi`; \
	export APPLE_CC; \
	./build_gcc --lib_ofiles \
		--srcroot=$(SRC) \
		--dstroot=$(DSTROOT) \
		--objroot=$(OBJROOT) \
		--symroot=$(SYMROOT) \
		--cflags="$(RC_CFLAGS)" \
		--hosts="$(RC_ARCHS)" \
		--targets="$(TARGETS)"

clean:
	@if [ -d $(OBJROOT) -a "$(OBJROOT)" != / ]; then \
	  echo '*** DELETING ' $(OBJROOT); \
	  $(RM) -rf $(OBJROOT); \
	fi
	@if [ -d $(SYMROOT) -a "$(SYMROOT)" != / ]; then \
	  echo '*** DELETING ' $(SYMROOT); \
	  $(RM) -rf $(SYMROOT); \
	fi
	@if [ -d $(DSTROOT) -a "$(DSTROOT)" != / ]; then \
	  echo '*** DELETING ' $(DSTROOT); \
	  $(RM) -rf $(DSTROOT); \
	fi


OBJROOT SYMROOT DSTROOT KEYMGR_OBJ:
	$(MKDIRS) $($@)

SRCROOT:
	@if [ -n "$($@)" ]; \
	then \
		exit 0; \
	else \
		echo Must define $@; \
		exit 1; \
	fi
