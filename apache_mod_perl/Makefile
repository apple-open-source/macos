##
# Makefile for mod_perl
##

# Project info
Project             = mod_perl
UserType            = Administration
ToolType            = Services
ShadowTestFile      = $(BuildDirectory)/Makefile.PL
Extra_CC_Flags      = -Wno-precomp
Extra_Environment   =	   SITELIBEXP="$(PERLLIB)"			\
			  SITEARCHEXP="$(PERLARCHLIB)"			\
		       INSTALLSITELIB="$(DSTROOT)$(PERLLIB)"		\
		      INSTALLSITEARCH="$(DSTROOT)$(PERLARCHLIB)"	\
		       INSTALLPRIVLIB="$(DSTROOT)$(PERLLIB)"		\
		       INSTALLARCHLIB="$(DSTROOT)$(PERLARCHLIB)"	\
		       INSTALLMAN1DIR="$(DSTROOT)$(MANDIR)/man1"	\
		       INSTALLMAN3DIR="$(DSTROOT)$(MANDIR)/man3"
Install_Flags       = APXS='apxs -S LIBEXECDIR="$(DSTROOT)$(APACHE_MODULE_DIR)"	\
				 -S SYSCONFDIR="$(DSTROOT)$(APACHE_CONFIG_DIR)"'
Install_Target      = install

APACHE_MODULE_DIR := $(shell apxs -q LIBEXECDIR)
APACHE_CONFIG_DIR := $(shell apxs -q SYSCONFDIR)

    PERLLIB := $(shell perl -e 'require Config; print "$$Config::Config{privlibexp}\n"')
PERLARCHLIB := $(shell perl -e 'require Config; print "$$Config::Config{archlibexp}\n"')

# It's a generic project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Sources = $(SRCROOT)/$(Project)

lazy_install_source:: shadow_source

ConfigureStamp = $(BuildDirectory)/configure-stamp

$(ConfigureStamp):
	@echo "Configuring $(Project) [$@]..."
	$(_v) cd $(BuildDirectory) &&	\
	  perl Makefile.PL		\
	    USE_APXS=1 WITH_APXS=apxs	\
	    EVERYTHING=1
	$(_v) touch $@

configure:: lazy_install_source $(ConfigureStamp)

build:: configure
	@echo "Building $(Project)..."
	$(_v) make -C $(BuildDirectory) $(Environment)

install:: build
	@echo "Installing $(Project)..."
	$(_v) umask $(Install_Mask) ; make -C $(BuildDirectory)				\
		$(Environment) $(Install_Flags) $(Install_Target)
	$(_v) $(FIND) $(DSTROOT) '(' -name '*.so' -o -name '*.bundle' ')' -print0 |	\
		$(XARGS) -0 strip -S
