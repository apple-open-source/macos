##
# Makefile for mod_perl
##

# Project info
Project             = mod_perl
UserType            = Administration
ToolType            = Services
ShadowTestFile      = $(BuildDirectory)/Makefile.PL
Extra_CC_Flags      = 
Extra_Environment   =	   SITELIBEXP="$(PERLLIB)"			\
			  SITEARCHEXP="$(PERLARCHLIB)"			\
		       INSTALLSITELIB="$(DSTROOT)$(PERLLIB)"		\
		      INSTALLSITEARCH="$(DSTROOT)$(PERLARCHLIB)"	\
		       INSTALLPRIVLIB="$(DSTROOT)$(PERLLIB)"		\
		       INSTALLARCHLIB="$(DSTROOT)$(PERLARCHLIB)"	\
		       INSTALLSITEMAN1DIR="$(DSTROOT)$(MANDIR)/man1"	\
		       INSTALLSITEMAN3DIR="$(DSTROOT)$(MANDIR)/man3"
Install_Flags       = APXS='apxs-1.3 -S LIBEXECDIR="$(DSTROOT)$(APACHE_MODULE_DIR)"	\
				 -S SYSCONFDIR="$(DSTROOT)$(APACHE_CONFIG_DIR)"'
Install_Target      = install

# Fix for perl getgrgid($gid) failure
export APACHE_USER=www
export APACHE_GROUP=www

APACHE_MODULE_DIR := $(shell apxs-1.3 -q LIBEXECDIR)
APACHE_CONFIG_DIR := $(shell apxs-1.3 -q SYSCONFDIR)

    PERLLIB := $(subst Perl,Perl/Extras,$(shell perl -e 'require Config; print $$Config::Config{privlibexp}'))
PERLARCHLIB := $(subst Perl,Perl/Extras,$(shell perl -e 'require Config; print $$Config::Config{archlibexp}'))

# It's a generic project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Sources = $(SRCROOT)/$(Project)

lazy_install_source:: shadow_source

ConfigureStamp = $(BuildDirectory)/configure-stamp

$(ConfigureStamp):
	@echo "Configuring $(Project) [$@]..."
	$(_v) cd $(BuildDirectory) &&	\
	  perl Makefile.PL		\
	    USE_APXS=1 WITH_APXS=/usr/sbin/apxs-1.3	\
	    EVERYTHING=1
	$(_v) touch $@

configure:: lazy_install_source $(ConfigureStamp)

build:: configure
	@echo "Building $(Project)..."
	$(_v) make -C $(BuildDirectory) $(Environment)

# Places to clean after install
CLEAN_HERE = $(DSTROOT)/$(APACHE_MODULE_DIR) $(DSTROOT)/System/Library/Perl

install:: build
	@echo "Installing $(Project)..."
	$(_v) umask $(Install_Mask) ; make -C $(BuildDirectory)				\
		$(Environment) $(Install_Flags) $(Install_Target)
	$(_v) $(FIND) $(CLEAN_HERE) '(' -name '*.so' -o -name '*.bundle' ')' -print0 |	\
		$(XARGS) -0 strip -S
	$(_v) $(FIND) $(CLEAN_HERE) -name ".packlist" -delete
	$(_v) $(FIND) $(CLEAN_HERE) -name "perllocal.pod" -delete
	$(_V) $(FIND) $(DSTROOT) -name "ap_config_auto.h" -delete
