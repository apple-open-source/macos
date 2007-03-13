# top-level Makefile exporting B&I interface
# $Id: Makefile,v 1.33 2006/09/06 21:23:37 jharper Exp $
#
# The rules we need to export are as follows:
#
# installsrc: Copy our source code into $(SRCROOT)
#
# install: Build and install files into $(DSTROOT), using $(OBJROOT)
# for temporary files and $(SYMROOT) for files that might be needed
# later.
#
# installhdrs: Install header files into $(DSTROOT)_
#
# clean: Remove all derived and temporary files

# our installed root
PREFIX = /usr/X11R6
BIN_PREFIX = $(PREFIX)/bin
LIB_PREFIX = $(PREFIX)/lib
ETC_PREFIX = /etc/X11
XP_PREFIX = /usr

XP = xp
XC = xc
XC64 = xc-64

ARCH64 = ppc64 x86_64
RC_ARCHS_32 = $(filter-out $(ARCH64),$(RC_ARCHS))
RC_ARCHS_64 = $(filter $(ARCH64),$(RC_ARCHS))

# programs we use
SHELL = /bin/sh
DITTO = /usr/bin/ditto
RM = /bin/rm -f
RMDIR = $(RM) -r
MKDIR = /bin/mkdir -p
CHMOD = /bin/chmod
MV = /bin/mv
INSTALL = /usr/bin/install -c
INSTALL_DATA = /usr/bin/install -c -m 444
LN = /bin/ln
LNDIR = $(SRCROOT)/lndir.sh

# strip stuff (we always build with debugging, then remove all the
# crap after installing into DSTROOT)
STRIP = /usr/bin/strip
STRIP_BIN_FLAGS = -S -x
STRIP_LIB_FLAGS = -S
EXTRA_STRIP_FILES = $(ETC_PREFIX)/rstart/rstartd.real \
	$(ETC_PREFIX)/xdm/chooser $(LIB_PREFIX)/X11/xkb/xkbcomp

# programs we remove suid-root from
NOSUID_FILES = $(BIN_PREFIX)/xterm $(BIN_PREFIX)/xload

ifeq ($(SRCROOT),)
  $(error Only B&I should use the top-level Makefile)
endif

# uninstalled imake location
IMAKE = $(OBJROOT)/xc/config/imake/imake


## exported B&I rules

installsrc ::
	$(DITTO) . $(SRCROOT)

install :: build-src install-xc install-xc-64 \
	   install-misc strip-programs strip-libraries fix-suid-programs \
	   move-etc clean-host-def

installhdrs ::

clean ::


## misc rules

build-src :: build-src-xc build-src-xc-64


## post-install rules to pass verification

strip-programs ::
	cd $(DSTROOT)$(PREFIX)/bin && $(STRIP) $(STRIP_BIN_FLAGS) *
	for f in $(EXTRA_STRIP_FILES); do \
	  $(STRIP) $(STRIP_BIN_FLAGS) $(DSTROOT)$$f; \
	done

strip-libraries ::
	-cd $(DSTROOT)$(PREFIX)/lib \
	   && $(STRIP) $(STRIP_LIB_FLAGS) lib*.*.*.dylib lib*.a

fix-suid-programs ::
	for f in $(NOSUID_FILES); do \
	  $(CHMOD) 755 $(DSTROOT)$$f; \
	done

move-etc ::
	$(MKDIR) $(DSTROOT)/private
	$(MV) $(DSTROOT)/etc $(DSTROOT)/private/etc

# The host.def we compiled with is set up for fat builds. But only the
# thinned versions get installed anywhere. So disable cross compiles

clean-host-def ::
	echo "" >$(DSTROOT)/usr/X11R6/lib/X11/config/host.def


## xc rules

build-src-xc :: $(OBJROOT)/$(XC) 
	cd $^ && $(LNDIR) $(SRCROOT)/xc .

# pbxbuild installs links not files if we give it a symlink tree, so
# actually copy the files it may see..
build-src-xc ::
	$(RMDIR) $(OBJROOT)/$(XC)/programs/Xserver/hw/apple
	$(DITTO) $(SRCROOT)/xc/programs/Xserver/hw/apple \
	  $(OBJROOT)/$(XC)/programs/Xserver/hw/apple

# Replicate X "make World"

IRULESRC = $(OBJROOT)/$(XC)/config/cf/

build-xc ::
	$(RM) $(OBJROOT)/$(XC)/config/cf/host.def
	$(SHELL) $(SRCROOT)/make-host-def $(RC_ARCHS_32) \
	  >$(OBJROOT)/$(XC)/config/cf/host.def
	@if [ ! -f $(IRULESRC)/version.def ]; then \
	  echo "" > $(IRULESRC)/version.def; \
	fi
	@if [ ! -f $(IRULESRC)/date.def ]; then \
	  echo "" > $(IRULESRC)/date.def; \
	fi
	cd $(OBJROOT)/$(XC) && $(MAKE) Makefile.boot
	cd $(OBJROOT)/$(XC) && $(MAKE) -f xmakefile version.def
	cd $(OBJROOT)/$(XC) && $(MAKE) Makefile.boot
	cd $(OBJROOT)/$(XC) && $(MAKE) -f xmakefile VerifyOS
	cd $(OBJROOT)/$(XC) && $(MAKE) -f xmakefile Makefiles
	cd $(OBJROOT)/$(XC) && $(MAKE) -f xmakefile includes
	cd $(OBJROOT)/$(XC) && $(MAKE) -f xmakefile depend
	cd $(OBJROOT)/$(XC) && $(MAKE) -f xmakefile World

install-xc :: build-xc
	cd $(OBJROOT)/$(XC) && $(MAKE) install DESTDIR=$(DSTROOT)
	cd $(OBJROOT)/$(XC) && $(MAKE) install.man DESTDIR=$(DSTROOT)


## xc-64 rules

ifneq ($(RC_ARCHS_64),)

build-src-xc-64 :: $(OBJROOT)/$(XC64)
	cd $^ && $(LNDIR) $(SRCROOT)/xc .

# pbxbuild installs links not files if we give it a symlink tree, so
# actually copy the files it may see..
build-src-xc-64 ::
	$(RMDIR) $(OBJROOT)/$(XC64)/programs/Xserver/hw/apple
	$(DITTO) $(SRCROOT)/xc/programs/Xserver/hw/apple \
	  $(OBJROOT)/$(XC64)/programs/Xserver/hw/apple

build-xc-64 ::
	$(RM) $(OBJROOT)/$(XC64)/config/cf/host.def
	$(SHELL) $(SRCROOT)/make-host-def $(RC_ARCHS_64) \
	  >$(OBJROOT)/$(XC64)/config/cf/host.def
	unset LD_SEG_ADDR_TABLE; unset LD_PREBIND; \
	  cd $(OBJROOT)/$(XC64) && $(MAKE) World

install-xc-64 :: build-xc-64
	cd $(OBJROOT)/$(XC64) && $(SHELL) $(SRCROOT)/merge-libs.sh exports/lib $(DSTROOT)/usr/X11R6/lib

else
build-src-xc-64 ::
install-xc-64 ::
endif

## stuff

install-misc :: $(DSTROOT)/usr/bin $(DSTROOT)/usr/X11R6 \
		$(DSTROOT)/usr/include/ $(DSTROOT)/usr/lib/ \
		$(DSTROOT)/etc/X11/xinit $(DSTROOT)/etc/X11/xserver \
		$(DSTROOT)/usr/share/man/man1
	$(INSTALL) $(SRCROOT)/open-x11 $(DSTROOT)/usr/bin/open-x11
	$(INSTALL_DATA) $(SRCROOT)/RELEASE-NOTES $(DSTROOT)/usr/X11R6/README
	$(INSTALL_DATA) $(SRCROOT)/ACKNOWLEDGEMENTS \
	  $(DSTROOT)/usr/X11R6/ACKNOWLEDGEMENTS
	-$(LN) -s ../X11R6/include/X11 $(DSTROOT)/usr/include/X11
	-$(LN) -s ../X11R6/lib/X11 $(DSTROOT)/usr/lib/X11
	$(INSTALL_DATA) $(SRCROOT)/xinitrc $(DSTROOT)/etc/X11/xinit/xinitrc
	$(INSTALL_DATA) $(SRCROOT)/Xquartz.plist $(DSTROOT)/etc/X11/xserver/Xquartz.plist
	$(INSTALL_DATA) $(SRCROOT)/open-x11.man $(DSTROOT)/usr/share/man/man1/open-x11.1

$(OBJROOT)/% :
	$(MKDIR) $@

$(DSTROOT)/% :
	$(MKDIR) $@
