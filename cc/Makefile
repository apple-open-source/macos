##
##  NeXT C Compiler Makefile.  
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
# The makefile will automatically make a clone of the latest binaries
# the first time "make debug" is run.  The binaries are kept outside
# of the revision control system.

#
# ** AUTOMATIC BUILDS **
#
# Automatic builds strictly follow the RC makefile API standards.  However,
# in order to do a full build, a bootstrap is needed.  To do this, simply
# do "make bootstrap" on the build machine, and install the resulting
# symbols in the BuildRoot.  Then, do an ordinary "make install". 
#

# Enable NeXT extensions to (gnu)make.
USE_APPLE_PB_SUPPORT = all
 
HOSTS= `arch`
TARGET_OS= $(RC_OS)
targets= case "$(TARGET_OS)" in *win*) echo i386 ;; \
  *) if [ "$(TARGET_OS)" = teflon -o "$(TARGET_OS)" = macos \
	  -o -f /usr/bin/uname ]; then \
	echo $${TARGETS:-'i386 ppc'}; \
     elif [ "$(RC_RELEASE)" = Grail ]; then \
	echo $${TARGETS:-'m68k i386 sparc ppc'}; \
     else \
	echo $${TARGETS:-'m68k i386 sparc'}; \
     fi ;; esac
TARGETS:= $(shell $(targets))

RC_ARCHS= $(HOSTS)

SRCROOT= .

SRC= `cd $(SRCROOT) && pwd | sed s,/private,,`
DSTROOT= $(SRC)/dst
SYMROOT= $(SRC)/sym
OBJROOT= $(SRC)/obj

BOOTSTRAP= --bootstrap

ARCH= `arch`
CHOWN= `if [ -f /usr/etc/chown ]; then echo /usr/etc/chown; \
	elif [ -f /usr/sbin/chown ]; then echo /usr/sbin/chown; \
	else echo chown; fi`
MKDIRS= `case "$(TARGET_OS)" in *win*) echo mkdirs ;; \
	*) if [ -f /bin/mkdirs ]; then echo mkdirs; else echo mkdir -p; fi ;; \
	esac`
RM= `case "$(TARGET_OS)" in *win*) echo rm ;; *) echo /bin/rm ;; esac`
shell= case "$(TARGET_OS)" in *win*) echo sh ;; *) echo /bin/sh ;; esac
SHELL:= $(shell $(shell))

# Bison cannot be built properly with /bin/make as installed on NEXTSTEP.
make= `if [ -f /bin/gnumake ]; then echo /bin/gnumake; else echo make; fi` \
  $(MFLAGS)

##
## build compilers for targets=$(TARGETS), and hosts=$(RC_ARCHS).
## bootstrap target must have been run before this.  
##
debug: obj.clone 
	./build_gcc --thins \
		--srcroot=$(SRC) \
		--dstroot=$(DSTROOT) \
		--objroot=$(OBJROOT) \
		--symroot=$(SYMROOT) \
		--cflags="$(RC_CFLAGS) -g" \
		--hosts="$(RC_ARCHS)" \
		--targets="$(TARGETS)"

obj.clone:
	@root=$(OBJROOT); \
	if ls -lgd $$root | egrep '^l'; then \
	  echo "============================================"; \
	  echo "== CLONING BINARIES, WILL TAKE A WHILE... =="; \
	  echo "============================================"; \
	  mv $$root $$root.lnk; \
	  mkdir $$root; cd $$root.lnk; \
	  find . -depth -print | cpio -pdl $$root; \
	fi; \
	touch obj.clone

obj.unclone:
	root=$(OBJROOT); \
	if ls -lgd $$root.lnk | egrep '^l'; then \
	  echo "============================================"; \
	  echo "==  REMOVING BINARIES, WILL TAKE A WHILE  =="; \
	  echo "============================================"; \
	  mv $$root $$root.tree; \
	  mv $$root.lnk $$root; \
	  $(RM) -Rf $$root.tree; \
	fi; \
	$(RM) -f obj.clone

config: 
	./build_gcc --configure \
		--srcroot=$(SRC) \
		--dstroot=$(DSTROOT) \
		--objroot=$(OBJROOT) \
		--symroot=$(SYMROOT) \
		--cflags="$(RC_CFLAGS) -g" \
		--hosts="$(RC_ARCHS)" \
		--targets="$(TARGETS)"

build: build_bison
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

install_no_src: install_bison
	if [ "$(TARGET_OS)" = nextstep -o "$(TARGET_OS)" = teflon \
	     -o "$(TARGET_OS)" = macos ]; then \
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

install: installhdrs build install_no_src
	if [ "$(TARGET_OS)" = macos -o "$(RC_RELEASE)" = Flask ]; then \
	  $(MKDIRS) $(DSTROOT)/usr/local/lib/gcc/darwin/2.7.2.1 && \
	  mv $(DSTROOT)/usr/libexec $(DSTROOT)/usr/local && \
	  $(RM) -f $(DSTROOT)/usr/lib/libcc.a && \
	  mv $(DSTROOT)/usr/lib/libcc*.a \
	     $(DSTROOT)/usr/local/lib/gcc/darwin/2.7.2.1 && \
	  cd $(DSTROOT)/usr/local/lib && \
	  ln -s gcc/darwin/2.7.2.1/libcc*.a . && \
	  rmdir $(DSTROOT)/usr/lib && \
	  cd $(DSTROOT)/usr/local/libexec/gcc/darwin && \
	  for dir in *; do \
	    cd $(DSTROOT)/usr/local/libexec/gcc/darwin/$$dir && \
	    ln -s */cc1objplus . && \
	    if [ "$(RC_RELEASE)" = Flask ]; then \
	      $(MKDIRS) $(DSTROOT)/usr/libexec/$$dir && \
	      cd $(DSTROOT)/usr/libexec/$$dir && \
	      ln -s ../../local/libexec/$$dir/2.7.2.1 .; \
	    fi; \
	  done; \
	fi
	if [ "$(TARGET_OS)" = nextstep -o "$(TARGET_OS)" = teflon ]; then \
	  $(MKDIRS) $(DSTROOT)/usr/local/RelNotes && \
	  install -c -m 444 CompilerPrivate.rtf \
		$(DSTROOT)/usr/local/RelNotes && \
	  install -c -m 444 PowerPC_Compiler.rtf \
		$(DSTROOT)/usr/local/RelNotes; \
	fi
#	Compiler sources don't need to be copied anymore,
#	since we now provide these sources on the Darwin site.
#	$(MAKE) installGNUsrc SRCROOT=$(DSTROOT)/` \
#	  case "$(TARGET_OS)" in *win*) ;; \
#	    *) if [ -f /usr/bin/uname ]; then echo System/; \
#	       else echo Next; fi ;; \
#	  esac \
#	  `Developer/Source/GNU/cc
	if [ \( "$(TARGET_OS)" = nextstep -o "$(TARGET_OS)" = teflon \
	     -o "$(TARGET_OS)" = macos \) -a "$$CHROOTED" ]; then \
	   echo "** cleaning unnessecary objects **"; \
	   $(RM) -Rf $(OBJROOT)/cc-*-on-* $(OBJROOT)/bison_*_obj; \
	fi

##
## build compilers for target=$(RC_ARCHS), and host=`arch`
##
bootstrap: bootstrap_bison
	./build_gcc --fats \
		--srcroot=$(SRC) \
		--dstroot=$(DSTROOT) \
		--objroot=$(OBJROOT) \
		--symroot=$(SYMROOT) \
		--cflags="$(RC_CFLAGS)" \
		--hosts=$(ARCH) \
		--targets="$(RC_ARCHS)"

clean: clean_bison
	./build_gcc --clean \
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
		--targets="$(TARGETS)"


installsrc: SRCROOT
	if [ $(SRCROOT) != . ]; then \
	  $(MKDIRS) $(SRCROOT)/cc; \
	  gnutar cf - `ls -1 | egrep -v '^(obj|dst|sym|SGS)'` \
		 | (cd $(SRCROOT); gnutar xvf -); \
	fi
	find "$(SRCROOT)" \
	   \( -name \*~ -o -name \*\# -o -name \*.rej -o -name \*.orig \
	   -o -name \*.old -o -name \*.new -o -name .dir\* -o -name .nfs\* \) \
	   -exec $(RM) -f {} \;

installGNUsrc: installsrc 
	if [ "$(TARGET_OS)" = nextstep -o "$(TARGET_OS)" = teflon \
	     -o "$(TARGET_OS)" = macos ]; then \
	  $(CHOWN) -f -R root.wheel $(SRCROOT); \
	fi
	chmod -f -R a+r-w+X $(SRCROOT)
	find $(SRCROOT) -type d -exec chmod -f 755 {} \;
	$(RM) -rf $(SRCROOT)/../bison
	if [ -f /usr/bin/uname ]; then \
	  $(RM) -rf $(SRCROOT)/bison; \
	else \
	  mv $(SRCROOT)/bison $(SRCROOT)/..; \
	fi
	$(RM) -rf $(SRCROOT)/../gcc
	mv $(SRCROOT)/cc $(SRCROOT)/../gcc
	$(RM) -rf $(SRCROOT)

installhdrs: DSTROOT
	if [ "$(TARGET_OS)" = teflon -o "$(TARGET_OS)" = macos \
	     -o -f /usr/bin/uname  ]; then \
	  hdr_dir=$(DSTROOT)/` \
	    if [ -f /usr/bin/uname -a "$(TARGET_OS)" = macos ]; then \
	      echo usr/local/include/gcc/darwin/2.7.2.1; \
	    else \
	      if [ -f /usr/bin/uname ]; then \
		echo System/Library/Frameworks/System.framework/Versions/B/Headers/bsd; \
	      else echo NextLibrary/Frameworks/System.framework/Versions/B/Headers/bsd; \
	      fi; \
	    fi`; \
	  $(MKDIRS) $$hdr_dir/machine && \
	  cd cc && \
	  for file in assert ginclude/stdarg ginclude/varargs ../float; \
	  do \
	    install -c -m 444 $$file.h $$hdr_dir; \
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
	  install -c -m 444 /tmp/limits.$$$$ $$hdr_dir/machine/limits.h && \
	  $(RM) -f /tmp/limits.$$$$; \
	fi
	if [ "$(TARGET_OS)" = nextstep -a ! -f /usr/bin/uname ]; then \
	  $(MKDIRS) $(DSTROOT)/LocalDeveloper/Headers/machine && \
	  touch $(DSTROOT)/LocalDeveloper/Headers/machine/ansi.h && \
	  $(CHOWN) -f -R root.wheel $(DSTROOT)/LocalDeveloper && \
	  chmod -f -R a+r-w+X $(DSTROOT)/LocalDeveloper; \
	fi

DSTROOT:
	$(MKDIRS) $(DSTROOT)

SRCROOT:
	@if [ -n "$($@)" ]; \
	then \
		exit 0; \
	else \
		echo Must define $@; \
		exit 1; \
	fi


build_bison: $(OBJROOT)/bison_$(ARCH)_obj/Makefile
	(cd $(OBJROOT)/bison_$(ARCH)_obj; \
	 $(make) CFLAGS="-O $(RC_CFLAGS) $(OTHER_CFLAGS)" \
		LDFLAGS="$(RC_CFLAGS) `case "$(TARGET_OS)" in *solaris* | *hpux*) \
			      echo -nopdolib ;; \
			  esac`" \
		datadir="`case "$(TARGET_OS)" in *win*) \
			      echo /Developer/Executables/Support ;; \
			   *solaris* | *hpux*) echo /Local/Libraries ;; \
			   *) echo /usr/local/lib ;; \
			  esac`" \
		exeext="`case "$(TARGET_OS)" in *win*) echo .exe ;; esac`" \
		SHELL="$(SHELL)")

bootstrap_bison: $(OBJROOT)/bison_$(ARCH)_obj/Makefile
	(cd $(OBJROOT)/bison_$(ARCH)_obj; \
	 $(make) CFLAGS="-g" \
		LDFLAGS="" \
		exeext="`case "$(TARGET_OS)" in *win*) echo .exe ;; esac`")

$(OBJROOT)/bison_$(ARCH)_obj:
	$(MKDIRS) $(OBJROOT)/bison_$(ARCH)_obj

$(OBJROOT)/bison_$(ARCH)_obj/Makefile: $(OBJROOT)/bison_$(ARCH)_obj
	(cd $(OBJROOT)/bison_$(ARCH)_obj; \
	 eval `case "$(TARGET_OS)" in *win*) \
		 echo CONFIG_SHELL=sh \
		 CC=\"$$NEXT_ROOT/Developer/Executables/gcc\" ;; \
	       esac` \
	      $(SRCROOT)/bison/configure \
	      --bindir="`case "$(TARGET_OS)" in *win*) \
			     echo /Developer/Executables/Utilities ;; \
			  *solaris* | *hpux*) echo /Local/Executables ;; \
			  *) echo /usr/local/bin ;; \
			 esac`" \
	      --datadir="`case "$(TARGET_OS)" in *win*) \
			      echo /Developer/Executables/Support ;; \
			   *solaris* | *hpux*) echo /Local/Libraries ;; \
			   *) echo /usr/local/lib ;; \
			  esac`")
        
install_bison: build_bison
	$(MKDIRS) $(DSTROOT)/`case "$(TARGET_OS)" in *solaris* | *hpux*)\
			     echo Local ;; \
			  *) echo usr/local ;; \
			 esac`
	(cd $(OBJROOT)/bison_$(ARCH)_obj; \
	$(make) install \
		CFLAGS="$(RC_CFLAGS) $(OTHER_CFLAGS)" \
		LDFLAGS="$(RC_CFLAGS)" \
		prefix="$(DSTROOT)/`case "$(TARGET_OS)" in *solaris* | *hpux*)\
			     echo Local ;; \
			  *) echo usr/local ;; \
			 esac`" \
		bindir="$(DSTROOT)/`case "$(TARGET_OS)" in *win*) \
			     echo Developer/Executables/Utilities ;; \
			  *solaris* | *hpux*) echo Local/Executables ;; \
			  *) echo usr/local/bin ;; \
			 esac`" \
		datadir="$(DSTROOT)/`case "$(TARGET_OS)" in *win*) \
			     echo Developer/Executables/Support ;; \
			  *solaris* | *hpux*) echo Local/Libraries ;; \
			  *) echo usr/local/lib ;; \
			 esac`" \
		exeext="`case "$(TARGET_OS)" in *win*) echo .exe ;; esac`")

clean_bison:
	if [ -d $(OBJROOT)/obj.bison ]; then \
	  cd $(OBJROOT)/obj.bison; \
	  $(MAKE) clean	\
	    exeext="`case "$(TARGET_OS)" in *win*) echo .exe ;; esac`"; \
	fi

lib_ofiles:
	./build_gcc --lib_ofiles \
		--srcroot=$(SRC) \
		--dstroot=$(DSTROOT) \
		--objroot=$(OBJROOT) \
		--symroot=$(SYMROOT) \
		--cflags="$(RC_CFLAGS)" \
		--hosts="$(RC_ARCHS)" \
		--targets="$(TARGETS)"
