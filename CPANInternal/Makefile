PERL58ORGREATER = $(shell perl -MConfig -e 'print ($$Config{PERL_REVISION} > 5 || ($$Config{PERL_REVISION} == 5 && $$Config{PERL_VERSION} >= 8) ? "YES" : "NO")')


# In order to satisfy problems like 4764835, we need to skip certain modules
# when building on Leopard that we would install when building on, say, Tiger.
LEOPARD_OR_HIGHER = $(shell perl -e 'my $$osVersion = `/usr/bin/uname -r`; chomp($$osVersion); $$osVersion =~ s|^(\d+).*|$$1|; print $$osVersion >= 9 ? "YES" : "NO"')
	
# Test scripts that require interaction with a user typically respect
# this variable, AUTOMATED_TESTING.  When set, they skip the interactive
# tests.  This helps with things like Term::ReadLine waiting for someone
# to type something in, when it will never happen on a build machine.
export AUTOMATED_TESTING := 1

ifneq ($(PERL58ORGREATER),YES)  #check for 5.8.0 or later

$(error You must have perl 5.8.0 or later!)

endif

#
# Some documentation as to how this file works:  (please hold your applause until the end)
#
# This file builds various stock CPAN modules.  Each module is kept in its own subdirectory.
#


#
# You must do 2 things to have your module built:
#
# --1--
# There is a variable, $SUBDIRS, which defines what will get built.  If you're adding a 
# module to be built, you should add it to the $SUBDIRS list, and import it into the CPAN dir.
# Modules will be built in the order which they appear in the $SUBDIRS var.
#
# --2--
# If your module builds via 'perl Makefile.PL; make; make install;' with no modifications, 
# add it to the ExtUtils::MakeMaker section below.
#
# For modules which build with Module::Build, there is a section below for them as well.
#
# If your module requires special treatment, add a special target for them in the SPECIAL TARGETS
# section.
#


ifeq ($(LEOPARD_OR_HIGHER),YES)

# Modules that we need to build when on Leopard (or later).
OTHERCPANSUBDIRS = Readonly Readonly-XS String-ShellQuote-1.00 Error-0.15 Log-Log4perl-0.42 Log-Dispatch-2.10 Mail-Sender Crypt-Rijndael Crypt-OpenSSL-Random Crypt-OpenSSL-RSA File-NFSLock HTTP-Proxy Class-Std-Utils YAML-Syck Heap Graph Test-Exception Tree-Simple Tree-Simple-VisitorFactory Class-Std Config-Std Text-LevenshteinXS Text-WordDiff
DATETIMESUBDIRS = Module-Build-0.20 Params-Validate Class-Factory-Util-1.6 Class-Singleton-1.03  DateTime-TimeZone DateTime-Locale DateTime DateTime-Format-Strptime-1.04 DateTime-Format-Builder DateTime-Format-Pg DateTime-Format-W3CDTF DateTime-Format-ISO8601
SOAPLITESUBDIRS = SOAP-Lite
CLASS_DBI_MODULES = UNIVERSAL-moniker Class-Accessor Class-Data-Inheritable Class-Trigger Class-WhiteHole DBIx-ContextualFetch Ima-DBI Class-DBI SQL-Abstract Class-DBI-AbstractSearch
DBIX_CLASS_MODULES = Sub-Uplevel Test-Simple Class-Accessor-Chained Class-Inspector Data-Page SQL-Abstract-Limit Module-Find Algorithm-C3 Class-C3 Class-Data-Accessor ExtUtils-CBuilder PathTools Lingua-EN-Inflect Lingua-EN-Inflect-Number Data-Dump UNIVERSAL-require DBIx-Class DBIx-Class-Schema-Loader
XML_MODULES = Pod-WSDL

else

# Modules that we need to build when on Tiger (or earlier).
# Leopard installs some of these modules by default.
OTHERCPANSUBDIRS = version Readonly Readonly-XS String-ShellQuote-1.00 Error-0.15 Log-Log4perl-0.42 Log-Dispatch-2.10 Mail-Sender Apache-DBI Crypt-Rijndael Crypt-OpenSSL-Random Crypt-OpenSSL-RSA File-NFSLock HTTP-Proxy Class-Std-Utils YAML-Syck Heap Graph Test-Exception Tree-Simple Tree-Simple-VisitorFactory Class-Std Config-Std Text-LevenshteinXS Text-WordDiff Term-ReadLine-Perl
DATETIMESUBDIRS = Module-Build-0.20 Params-Validate Class-Factory-Util-1.6 Class-Singleton-1.03  DateTime-TimeZone DateTime-Locale DateTime DateTime-Format-Strptime-1.04 DateTime-Format-Builder DateTime-Format-Pg DateTime-Format-W3CDTF DateTime-Format-ISO8601
SOAPLITESUBDIRS = URI libwww-perl SOAP-Lite
CLASS_DBI_MODULES = DBI DBD-SQLite UNIVERSAL-moniker Class-Accessor Class-Data-Inheritable Class-Trigger Class-WhiteHole DBIx-ContextualFetch Ima-DBI Class-DBI SQL-Abstract Class-DBI-AbstractSearch
DBIX_CLASS_MODULES = Sub-Uplevel Test-Simple Class-Accessor-Chained Class-Inspector Data-Page SQL-Abstract-Limit Carp-Clan Module-Find Algorithm-C3 Class-C3 Class-Data-Accessor ExtUtils-CBuilder PathTools Lingua-EN-Inflect Lingua-EN-Inflect-Number Data-Dump UNIVERSAL-require DBIx-Class DBIx-Class-Schema-Loader
XML_MODULES = XML-NamespaceSupport XML-SAX XML-LibXML-Common XML-LibXML XML-Writer XML-Parser XML-XPath Pod-WSDL
# XML-SAX needs this defined to test properly:
export XMLVS_TEST_PARSER=XML::LibXML::SAX

endif



SUBDIRS = $(DATETIMESUBDIRS) $(CLASS_DBI_MODULES) $(XML_MODULES) $(SOAPLITESUBDIRS) $(OTHERCPANSUBDIRS) $(DBIX_CLASS_MODULES)


# These are the places where perl wants to install stuff.  Shouldn't need to be modified.
INSTALLMAN1DIR=/usr/local/share/man/man1   # otherwise they would go in /usr/share
INSTALLMAN3DIR=/usr/local/share/man/man3   # ditto

# Install in /AppleInternal/Library/Perl if perl supports the Extras
# directory (because that indicates support for the @INC modification
# stuff)
INSTALLEXTRAS = $(subst Perl,Perl/Extras,$(shell perl -MConfig -e 'print $$Config{installprivlib}'))
INSTALL_SITE_LIB = $(shell perl -MConfig -e 'print $$Config{installsitelib}')
INSTALL_SITE_ARCH = $(shell perl -MConfig -e 'print $$Config{installsitearch}')
INSTALL_ARCH = /AppleInternal$(INSTALL_SITE_ARCH)
INSTALL_LIB = /AppleInternal$(INSTALL_SITE_LIB)

ifeq "$(shell test -d $(INSTALLEXTRAS) && echo YES )" "" 
    INSTALL_ARCH = $(INSTALL_SITE_ARCH)
    INSTALL_LIB = $(INSTALL_SITE_LIB)
endif


# This is a list of args to perl to add dirs to add to @INC, so that the building 
# modules can see dependent modules
INCARGS= -I$(DSTROOT)/$(INSTALL_ARCH) -I$(DSTROOT)/$(INSTALL_LIB)

INSTALLBIN=$(shell perl -MConfig -e 'print $$Config{installsitebin}')
PREFIX=$(shell perl -MConfig -e 'print $$Config{prefix}')

#
# ExtUtils::MakeMaker section
#
# These modules only require the standard treatment:  perl Makefile.PL; make install;   
# Add your modules which build via that standard here.
#

Crypt-OpenSSL-Random Crypt-OpenSSL-RSA Crypt-Rijndael UNIVERSAL-moniker Class-Accessor Class-Inspector Class-Data-Inheritable Class-Trigger Class-WhiteHole DBI DBD-SQLite DBIx-ContextualFetch Ima-DBI Class-DBI SQL-Abstract Class-DBI-AbstractSearch XML-NamespaceSupport XML-SAX XML-LibXML-Common XML-LibXML Log-Log4perl-0.42 Error-0.15 MIME-Base64-2.20 URI HTML-Tagset-3.03 Class-Factory-Util-1.6 Class-Singleton-1.03 Params-Validate DateTime-Format-Pg DateTime DateTime-Format-Strptime-1.04 Readonly Readonly-XS String-ShellQuote-1.00 Mail-Sender Apache-DBI DateTime-Format-W3CDTF File-NFSLock HTTP-Proxy XML-Writer XML-Parser XML-XPath Pod-WSDL Class-Std-Utils YAML-Syck Heap Graph Test-Simple Carp-Clan Module-Find Lingua-EN-Inflect Lingua-EN-Inflect-Number Data-Dump UNIVERSAL-require Tree-Simple Tree-Simple-VisitorFactory Text-LevenshteinXS Term-ReadLine-Perl ::
	@echo "=============== Making $@ ==================";				\
	cd $(OBJROOT)/$@;								\
	mv Makefile.PL Makefile.PL.orig;						\
	cat Makefile.PL.orig ../add_rc_constants.pl > Makefile.PL;			\
	if [ -d Expat ]; then					\
		mv Expat/Makefile.PL Expat/Makefile.PL.orig;						\
		cat Expat/Makefile.PL.orig ../add_rc_constants.pl > Expat/Makefile.PL;			\
	fi;	 							\
	PERL_MM_USE_DEFAULT=1 perl $(INCARGS) Makefile.PL 		\
		"PERL=/usr/bin/perl $(INCARGS)"						\
	    INSTALLDIRS=site								\
	    INSTALLSITEARCH=$(DSTROOT)/$(INSTALL_ARCH)				\
        INSTALLSITELIB=$(DSTROOT)/$(INSTALL_LIB)					\
        PREFIX=$(DSTROOT)/$(PREFIX)							\
	    INSTALLSITEBIN=$(DSTROOT)/$(INSTALLBIN)					\
	    INSTALLSCRIPT=$(DSTROOT)/$(INSTALLBIN)					\
	    INSTALLSITEMAN1DIR=$(DSTROOT)/$(INSTALLMAN1DIR)				\
	    INSTALLSITEMAN3DIR=$(DSTROOT)/$(INSTALLMAN3DIR);				\
	make all test pure_install;									
	@echo "";

#
# Module::Build section
#
# These modules use the newer Modules::Build module to build and install
# Add your Module::Build based modules here.
#

version Log-Dispatch-2.10 Module-Build-0.20 DateTime-TimeZone DateTime-Locale DateTime-Format-Builder DateTime-Format-ISO8601 Sub-Uplevel Test-Exception Class-Accessor-Chained Data-Page SQL-Abstract-Limit Algorithm-C3 Class-C3 Class-Data-Accessor ExtUtils-CBuilder PathTools DBIx-Class DBIx-Class-Schema-Loader Class-Std Config-Std Text-WordDiff::
	@echo "=============== Making $@ ==================";				\
	cd $(OBJROOT)/$@;								\
	perl $(INCARGS) Build.PL							\
	    destdir=$(DSTROOT) installdirs=site 					\
	    --install_path arch=$(INSTALL_ARCH)					\
	    --install_path lib=$(INSTALL_LIB)						\
	    --install_path libdoc=$(INSTALLMAN3DIR)					\
	    --install_path bindoc=$(INSTALLMAN1DIR);					\
	perl $(INCARGS) Build;								\
	perl $(INCARGS) Build test;							\
	perl $(INCARGS) Build install;						\
	echo

#
# Custom build section
#
# Below here are modules which need some special treatment.  Add any such modules here.
# Please comment what you add, so that we can understand your reasoning...
#

#
# libwww-perl
# needs to have the -n flag, so that it will be non-interactive
#

libwww-perl::
	@echo "=============== Making $@ ==================";				\
	cd $(OBJROOT)/$@;								\
	mv Makefile.PL Makefile.PL.orig;						\
	cat Makefile.PL.orig ../add_rc_constants.pl > Makefile.PL;			\
	perl $(INCARGS) Makefile.PL -n "PERL=/usr/bin/perl $(INCARGS)"				\
	    INSTALLSITEARCH=$(DSTROOT)/$(INSTALL_ARCH)				\
	    INSTALLSITELIB=$(DSTROOT)/$(INSTALL_LIB)					\
        PREFIX=$(DSTROOT)/$(PREFIX)							\
	    INSTALLSITEBIN=$(DSTROOT)/$(INSTALLBIN)					\
	    INSTALLSITEMAN1DIR=$(DSTROOT)/$(INSTALLMAN1DIR)				\
	    INSTALLSITEMAN3DIR=$(DSTROOT)/$(INSTALLMAN3DIR);				\
	make all test pure_install;									
	echo "";

#
# SOAP-Lite
# Needs all sorts of options for various transport modules.  This was just my best guess 
# at what we might need in the future.
#

SOAP-Lite::
	@echo "=============== Making $@ ==================";				\
	cd $(OBJROOT)/$@;								\
	mv Makefile.PL Makefile.PL.orig;						\
	cat Makefile.PL.orig ../add_rc_constants.pl > Makefile.PL;			\
	perl Makefile.PL --noprompt							\
	    INSTALLDIRS=site								\
	    INSTALLSITEARCH=$(DSTROOT)/$(INSTALL_ARCH)				\
	    INSTALLSITELIB=$(DSTROOT)/$(INSTALL_LIB)					\
        PREFIX=$(DSTROOT)/$(PREFIX)							\
	    INSTALLSITEBIN=$(DSTROOT)/$(INSTALLBIN)					\
	    INSTALLSCRIPT=$(DSTROOT)/$(INSTALLBIN)					\
	    INSTALLSITEMAN1DIR=$(DSTROOT)/$(INSTALLMAN1DIR)				\
	    INSTALLSITEMAN3DIR=$(DSTROOT)/$(INSTALLMAN3DIR)				\
	    --HTTP-Client								\
	    --noHTTPS-Client								\
	    --noMAILTO-Client								\
	    --noFTP-Client								\
	    --noHTTP-Daemon								\
	    --noHTTP-Apache								\
	    --noHTTP-FCGI								\
	    --noPOP3-Server								\
	    --noIO-Server								\
	    --noMQ									\
	    --noJABBER									\
	    --noMIMEParser								\
	    --noTCP									\
	    --noHTTP;									\
	make all pure_install;									
	@echo "";


#
# ConfigurationFiles target
#     This target will install custom configuration files for the modules
#     It uses the ConfigurationFiles dir to hold them, and to make them
#

ConfigurationFiles::
	@echo "=============== Making $@ ==================";				\
	cd $(OBJROOT)/$@;								\
	make install;									

#
# install, installhdrs, clean and installsrc are standard.
# These are the targets which XBS calls with make
#

install:: echo-config-info install-ditto-phase $(SUBDIRS) ConfigurationFiles
	@if [ $(DSTROOT) ]; then \
	    echo Stripping symbols from bundles ... ; \
	    echo find $(DSTROOT) -xdev -name '*.bundle' -print -exec strip -S {} \; ; \
	    find $(DSTROOT) -xdev -name '*.bundle' -print -exec strip -S {} \; ;	\
	    echo "" ; \
	    echo Stripping packlists ... ; \
	    find $(DSTROOT) -xdev -name '.packlist' -print -exec rm -f {} \; ; \
	fi
	@if [ -d $(INSTALLEXTRAS) ]; then \
	    echo Creating PrependToPath ... ; \
	    mkdir -p $(DSTROOT)/$(INSTALL_SITE_LIB) ; \
	    echo '/AppleInternal$(INSTALL_SITE_LIB)' > $(DSTROOT)/$(INSTALL_SITE_LIB)/PrependToPath ; \
	fi
	rm -f $(DSTROOT)"$$INSTALLSITEARCH"/perllocal.pod


installhdrs::

clean::
	@for i in $(SUBDIRS); do						\
	    (									\
		echo "=============== Cleaning $$i ==================";		\
		cd $$i;								\
		if [ -e Makefile ]; then					\
		    make realclean;						\
		fi;	 							\
		rm -f Makefile.old;						\
		echo "";							\
	    )									\
	done									\

installsrc::
	ditto . $(SRCROOT)

install-ditto-phase::
	@if [ "$(OBJROOT)" != "." ]; then						\
	    ditto . $(OBJROOT);								\
	fi;
	(cd $(OBJROOT) && ./applyPatches)

echo-config-info::
	@echo "=============== Configuration Information ==============="
	@echo "Perl 5.8.0 or greater: $(PERL58ORGREATER)"
	@echo "Building on Leopard or higher (if so we'll skip certain modules that Leopard installs by default): $(LEOPARD_OR_HIGHER)"
	@echo Building subdirs: $(SUBDIRS)
	@echo INSTALLSITEARCH=$(DSTROOT)/$(INSTALL_ARCH)
	@echo INSTALLSITELIB=$(DSTROOT)/$(INSTALL_LIB)	
	@echo PREFIX=$(DSTROOT)/$(PREFIX)
	@echo INSTALLSITEBIN=$(DSTROOT)/$(INSTALLBIN)
	@echo INSTALLSITEMAN1DIR=$(DSTROOT)/$(INSTALLMAN1DIR)
	@echo INSTALLSITEMAN3DIR=$(DSTROOT)/$(INSTALLMAN3DIR)
	@echo 

