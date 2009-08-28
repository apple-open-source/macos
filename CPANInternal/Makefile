# Determine the Perl major version
PERL5_8_OR_GREATER  = $(shell perl -MConfig -e 'print ($$Config{PERL_REVISION} > 5 || ($$Config{PERL_REVISION} == 5 && $$Config{PERL_VERSION} >= 8)  ? "YES" : "NO")')
PERL5_10_OR_GREATER = $(shell perl -MConfig -e 'print ($$Config{PERL_REVISION} > 5 || ($$Config{PERL_REVISION} == 5 && $$Config{PERL_VERSION} >= 10) ? "YES" : "NO")')

# Perl major version must be at least 5.8
ifneq ($(PERL5_8_OR_GREATER),YES)
$(error You must have perl 5.8 or later)
endif

# Determine our OS major version.
SNOWLEOPARD = NO
LEOPARD     = NO
TIGER       = NO
PANTHER     = NO
UNKNOWN_OS  = YES
OS_VERSION  = $(shell perl -e 'my $$osVersion = `/usr/bin/uname -r`; chomp($$osVersion); $$osVersion =~ s|^(\d+).*|$$1|; print "$$osVersion"')
ifeq ($(OS_VERSION),10)
	SNOWLEOPARD = YES
	UNKNOWN_OS  = NO
endif
ifeq ($(OS_VERSION),9)
	LEOPARD    = YES
	UNKNOWN_OS = NO
endif
ifeq ($(OS_VERSION),8)
	TIGER      = YES
	UNKNOWN_OS = NO
endif
ifeq ($(OS_VERSION),7)
	PANTHER    = YES
	UNKNOWN_OS = NO
endif

# OS major version must be recognized
ifeq ($(UNKNOWN_OS),YES)
$(error Unknown OS version)
endif

# OS major version must be Tiger or newer
ifeq ($(PANTHER),YES)
$(error You must build on Tiger or later)
endif

# This project builds various CPAN modules.  Each module is kept in its own subdirectory.
#
# You must do 2 things to add a new CPAN module:
#
# --1--
# The SUBDIRS variable defines the modules that will get built for a given OS major version.
# You should add the new module name to one of the sublists that are concatenated to produce
# the SUBDIRS list. Modules are built in the order they appear in the SUBDIRS list.
#
# --2--
# If the new module builds via 'perl Makefile.PL; make; make install;' with no modifications, 
# add it to the ExtUtils::MakeMaker section below.
#
# There is a separate section for modules that build with Module::Build.
#
# If your module requires special treatment, add a custom target in the SPECIAL TARGETS section.
#


# Modules that build on SnowLeopard or later
ifeq ($(SNOWLEOPARD),YES)

# These modules have fewer dependencies and can build early
FIRST_MODULES =                         \
    Algorithm-C3                        \
    Authen-Krb5                         \
    Bencode                             \
    Class-Accessor-Chained              \
    Class-Data-Accessor                 \
    Class-Data-Inheritable              \
    Class-Factory-Util-1.6              \
    Class-Inspector                     \
    Class-Singleton-1.03                \
    Class-Std                           \
    Class-Std-Utils                     \
    Class-Trigger                       \
    Class-WhiteHole                     \
    Config-Std                          \
    Crypt-OpenSSL-Random                \
    Crypt-OpenSSL-RSA                   \
    Crypt-Rijndael                      \
    Crypt-SSLeay                        \
    Data-Dump                           \
    Data-UUID                           \
    Error-0.15                          \
    Exporter-Easy                       \
    File-NFSLock                        \
    Heap                                \
    JSON-Any                            \
    Lingua-EN-Inflect                   \
    Lingua-EN-Inflect-Number            \
    Mail-Sender                         \
    Module-Find                         \
    Params-Validate                     \
    Perl-Tidy                           \
    Readonly                            \
    Readonly-XS                         \
    Scope-Guard                         \
    String-ShellQuote-1.00              \
    Sub-Uplevel                         \
    Term-ReadLine-Perl                  \
    Test-Exception                      \
    Text-LevenshteinXS                  \
    Text-WordDiff                       \
    Time-HiRes-Value                    \
    Tree-Simple                         \
    Tree-Simple-VisitorFactory          \
    UNIVERSAL-moniker                   \
    UNIVERSAL-require                   \
    YAML-Syck                           \

DATETIMESUBDIRS =                       \
    DateTime-Locale                     \
    DateTime-TimeZone                   \
    DateTime                            \
    DateTime-Format-Strptime-1.04       \
    DateTime-Format-Builder             \
    DateTime-Format-Pg                  \
    DateTime-Format-W3CDTF              \
    DateTime-Format-ISO8601             \

CLASS_DBI_MODULES =                     \
    DBIx-ContextualFetch                \
    SQL-Abstract                        \
    SQL-Abstract-Limit                  \
    Ima-DBI                             \
    Class-DBI                           \
    Class-DBI-AbstractSearch            \

SOAP_LITE_MODULES =                     \
    Pod-WSDL                            \
    SOAP-Lite                           \

# Modules that have dependencies on earlier modules
OTHERCPANSUBDIRS =                      \
    Class-C3-XS                         \
    Class-C3                            \
    Class-C3-Componentised              \
    MRO-Compat                          \
    Class-Accessor-Grouped              \
    Data-Page                           \
    Graph                               \
    HTTP-Proxy                          \
    Log-Dispatch                        \
    Log-Log4perl                        \

DBIX_CLASS_MODULES =                    \
    DBIx-Class                          \
    DBIx-Class-Schema-Loader            \
    PathTools                           \

# Modules that are tempoarily disabled
DISABLED =                              \
    Apache2-SOAP                        \

endif


# Modules that build on Leopard
ifeq ($(LEOPARD),YES)

DATETIMESUBDIRS = Module-Build-0.20 Params-Validate Class-Factory-Util-1.6 Class-Singleton-1.03  DateTime-TimeZone DateTime-Locale DateTime DateTime-Format-Strptime-1.04 DateTime-Format-Builder DateTime-Format-Pg DateTime-Format-W3CDTF DateTime-Format-ISO8601
OTHERCPANSUBDIRS = Authen-Krb5 Readonly Readonly-XS String-ShellQuote-1.00 Error-0.15 Log-Log4perl Log-Dispatch Mail-Sender Crypt-Rijndael Crypt-OpenSSL-Random Crypt-OpenSSL-RSA Crypt-SSLeay File-NFSLock HTTP-Proxy Class-Std-Utils YAML-Syck Heap Graph Test-Exception Tree-Simple Tree-Simple-VisitorFactory Class-Std Config-Std Text-LevenshteinXS Text-WordDiff Apache2-SOAP Apache-DBI Digest-SHA Term-ReadLine-Perl Exporter-Easy Data-UUID Time-HiRes-Value Perl-Tidy Bencode
DATETIMESUBDIRS = Module-Build-0.20 Params-Validate Class-Factory-Util-1.6 Class-Singleton-1.03  DateTime-TimeZone DateTime-Locale DateTime DateTime-Format-Strptime-1.04 DateTime-Format-Builder DateTime-Format-Pg DateTime-Format-W3CDTF DateTime-Format-ISO8601 
SOAP_LITE_MODULES = SOAP-Lite Pod-WSDL
CLASS_DBI_MODULES = DBI UNIVERSAL-moniker Class-Accessor Class-Data-Inheritable Class-Trigger Class-WhiteHole DBIx-ContextualFetch Ima-DBI Class-DBI SQL-Abstract Class-DBI-AbstractSearch
DBIX_CLASS_MODULES = Sub-Uplevel Test-Simple Class-Accessor-Chained Class-Inspector Data-Page SQL-Abstract-Limit Module-Find Algorithm-C3 Class-C3-XS Class-C3 Class-C3-Componentised Class-Data-Accessor MRO-Compat Class-Accessor-Grouped ExtUtils-CBuilder PathTools Lingua-EN-Inflect Lingua-EN-Inflect-Number Data-Dump UNIVERSAL-require Scope-Guard JSON-Any DBIx-Class DBIx-Class-Schema-Loader

endif


# Modules that build on Tiger
ifeq ($(TIGER),YES)

OTHERCPANSUBDIRS = Authen-Krb5 version Readonly Readonly-XS String-ShellQuote-1.00 Error-0.15 Log-Log4perl Log-Dispatch Mail-Sender Apache-DBI Crypt-Rijndael Crypt-OpenSSL-Random Crypt-OpenSSL-RSA Crypt-SSLeay File-NFSLock HTTP-Proxy Class-Std-Utils YAML-Syck Heap Graph Test-Exception Tree-Simple Tree-Simple-VisitorFactory Class-Std Config-Std Text-LevenshteinXS Text-WordDiff Term-ReadLine-Perl Digest-SHA Exporter-Easy Data-UUID Time-HiRes-Value Net-IP Digest-HMAC Net-DNS Perl-Tidy Bencode TermReadKey
DATETIMESUBDIRS = Module-Build-0.20 Params-Validate Class-Factory-Util-1.6 Class-Singleton-1.03  DateTime-TimeZone DateTime-Locale DateTime DateTime-Format-Strptime-1.04 DateTime-Format-Builder DateTime-Format-Pg DateTime-Format-W3CDTF DateTime-Format-ISO8601
SOAP_LITE_MODULES = URI libwww-perl SOAP-Lite Pod-WSDL
CLASS_DBI_MODULES = DBI DBD-SQLite UNIVERSAL-moniker Class-Accessor Class-Data-Inheritable Class-Trigger Class-WhiteHole DBIx-ContextualFetch Ima-DBI Class-DBI SQL-Abstract Class-DBI-AbstractSearch
DBIX_CLASS_MODULES = Sub-Uplevel Test-Simple Class-Accessor-Chained Class-Inspector Data-Page SQL-Abstract-Limit Carp-Clan Module-Find Algorithm-C3 Class-C3-XS Class-C3 Class-C3-Componentised Class-Data-Accessor MRO-Compat Class-Accessor-Grouped ExtUtils-CBuilder PathTools Lingua-EN-Inflect Lingua-EN-Inflect-Number Data-Dump UNIVERSAL-require Scope-Guard JSON-Any Digest-MD5 DBIx-Class DBIx-Class-Schema-Loader
LDAP_MODULES = Convert-ASN1 Perl-Ldap
XML_MODULES = XML-NamespaceSupport XML-SAX XML-LibXML-Common XML-LibXML XML-Writer XML-Parser XML-XPath
# XML-SAX needs this defined to test properly:
export XMLVS_TEST_PARSER=XML::LibXML::SAX

endif


# Define SUBDIRS as the complete set of modules
# Leave them in this order since we don't want to go back to Tiger and Leopard and sort out dependencies
SUBDIRS =                   \
    $(FIRST_MODULES)        \
    $(DATETIMESUBDIRS)      \
    $(CLASS_DBI_MODULES)    \
    $(LDAP_MODULES)         \
    $(XML_MODULES)          \
    $(SOAP_LITE_MODULES)    \
    $(OTHERCPANSUBDIRS)     \
    $(DBIX_CLASS_MODULES)   \


# Set INSTALLDIRS to "site" to enable all of the INSTALLSITE* variables
INSTALLDIRS = site

# Grab some configuration values from Perl
CONFIG_INSTALLSITEARCH    = $(shell perl -MConfig -e 'print $$Config{installsitearch}')
CONFIG_INSTALLSITEBIN     = $(shell perl -MConfig -e 'print $$Config{installsitebin}')
CONFIG_INSTALLSITELIB     = $(shell perl -MConfig -e 'print $$Config{installsitelib}')
CONFIG_INSTALLSITEMAN1DIR = $(shell perl -MConfig -e 'print $$Config{installsiteman1dir}')
CONFIG_INSTALLSITEMAN3DIR = $(shell perl -MConfig -e 'print $$Config{installsiteman3dir}')
CONFIG_INSTALLSITESCRIPT  = $(shell perl -MConfig -e 'print $$Config{installsitescript}')
CONFIG_INSTALLPRIVLIB     = $(shell perl -MConfig -e 'print $$Config{installprivlib}')

# Install in /AppleInternal/Library/Perl if the /S/L/Perl/Extras directory exists (because the
# existence of that directory indicates support for the {Append,Prepend}ToPath @INC modification stuff)
INSTALLEXTRAS = $(subst Perl,Perl/Extras,$(CONFIG_INSTALLPRIVLIB))
ifeq "$(shell test -d $(INSTALLEXTRAS) && echo YES )" "YES" 
    APPLE_INSTALLSITEARCH = /AppleInternal$(CONFIG_INSTALLSITEARCH)
    APPLE_INSTALLSITELIB  = /AppleInternal$(CONFIG_INSTALLSITELIB)
else
	APPLE_INSTALLSITEARCH  = $(CONFIG_INSTALLSITEARCH)
    APPLE_INSTALLSITELIB   = $(CONFIG_INSTALLSITELIB)
endif

# prepend DSTROOT for use with ExtUtils::MakeMaker (Modules::Build does not want DSTROOT)
INSTALLSITEARCH    = $(DSTROOT)$(APPLE_INSTALLSITEARCH)
INSTALLSITEBIN     = $(DSTROOT)$(CONFIG_INSTALLSITEBIN)
INSTALLSITELIB     = $(DSTROOT)$(APPLE_INSTALLSITELIB)
INSTALLSITEMAN1DIR = $(DSTROOT)$(CONFIG_INSTALLSITEMAN1DIR)
INSTALLSITEMAN3DIR = $(DSTROOT)$(CONFIG_INSTALLSITEMAN3DIR)
INSTALLSITESCRIPT  = $(DSTROOT)$(CONFIG_INSTALLSITESCRIPT)

# Older versions of ExtUtils::MakeMaker don't support INSTALLSITESCRIPT
INSTALLSCRIPT = $(DSTROOT)$(CONFIG_INSTALLSITESCRIPT)

# This is a list of dirs to add to @INC, so that the building  modules can see other built modules
INCARGS= -I$(INSTALLSITEARCH) -I$(INSTALLSITELIB)


# set up ARCHFLAGS as per rdar://problem/5402242
ifeq ($(SNOWLEOPARD),YES)
    DEFAULT_ARCHFLAGS = -arch i386 -arch x86_64
endif
ifeq ($(LEOPARD),YES)
    DEFAULT_ARCHFLAGS = -arch ppc -arch ppc64 -arch i386 -arch x86_64
endif
ifeq ($(TIGER),YES)
    DEFAULT_ARCHFLAGS = -arch ppc -arch i386
endif

# let RC_CFLAGS override the default ARCHFLAGS
ifneq "$(RC_CFLAGS)" ""
    ARCHFLAGS = $(RC_CFLAGS)
else
    ARCHFLAGS = $(DEFAULT_ARCHFLAGS)
endif


# Test scripts that require interaction with a user typically respect
# this variable, AUTOMATED_TESTING.  When set, they skip the interactive
# tests.  This helps with things like Term::ReadLine waiting for someone
# to type something in, when it will never happen on a build machine.
export AUTOMATED_TESTING   := 1
export PERL_MM_USE_DEFAULT := 1
export PERL_AUTOINSTALL    := --skipdeps

#
# ExtUtils::MakeMaker section
#
# These modules only require the standard treatment:  perl Makefile.PL; make install;   
# Add your modules which build via that approach here.
#

Apache-DBI \
Authen-Krb5 \
Bencode \
Carp-Clan \
Class-Accessor \
Class-Accessor-Grouped \
Class-Data-Inheritable \
Class-C3 \
Class-C3-Componentised \
Class-C3-XS \
Class-DBI \
Class-DBI-AbstractSearch \
Class-Factory-Util-1.6 \
Class-Inspector \
Class-Singleton-1.03 \
Class-Std-Utils \
Class-Trigger \
Class-WhiteHole \
Convert-ASN1 \
Crypt-OpenSSL-Random \
Crypt-OpenSSL-RSA \
Crypt-Rijndael \
Crypt-SSLeay \
Data-Dump \
Data-UUID \
DateTime \
DateTime-Format-Pg \
DateTime-Format-Strptime-1.04 \
DateTime-Format-W3CDTF \
DBD-SQLite \
DBI \
DBIx-Class \
DBIx-Class-Schema-Loader \
DBIx-ContextualFetch \
Digest-HMAC \
Digest-MD5 \
Digest-SHA \
Error-0.15 \
Exporter-Easy \
File-NFSLock \
Graph \
Heap \
HTML-Tagset-3.03 \
HTTP-Proxy \
Ima-DBI \
JSON-Any \
Lingua-EN-Inflect \
Lingua-EN-Inflect-Number \
Log-Log4perl \
Mail-Sender \
MIME-Base64-2.20 \
Module-Find \
MRO-Compat \
Net-IP \
Params-Validate \
Perl-Ldap \
Perl-Tidy \
Pod-WSDL \
Readonly \
Readonly-XS \
Scope-Guard \
SQL-Abstract \
String-ShellQuote-1.00 \
Term-ReadLine-Perl \
Test-Simple \
TermReadKey \
Text-LevenshteinXS \
Time-HiRes-Value \
Tree-Simple \
Tree-Simple-VisitorFactory \
UNIVERSAL-moniker \
UNIVERSAL-require \
URI \
XML-LibXML-Common \
XML-LibXML \
XML-NamespaceSupport \
XML-Parser \
XML-SAX \
XML-Writer \
XML-XPath \
YAML-Syck \
::
	@echo "=============== Making $@ ==================";								\
	cd $(OBJROOT)/$@;																	\
	mv Makefile.PL Makefile.PL.orig;													\
	cat Makefile.PL.orig ../add_rc_constants.pl > Makefile.PL;							\
	if [ -d Expat ]; then																\
		mv Expat/Makefile.PL Expat/Makefile.PL.orig;									\
		cat Expat/Makefile.PL.orig ../add_rc_constants.pl > Expat/Makefile.PL;			\
	fi;	 																				\
	perl $(INCARGS) Makefile.PL 														\
		"PERL=/usr/bin/perl $(INCARGS)"													\
	    INSTALLDIRS=$(INSTALLDIRS)														\
	    INSTALLSITEARCH=$(INSTALLSITEARCH)												\
	    INSTALLSITEBIN=$(INSTALLSITEBIN)												\
	    INSTALLSITELIB=$(INSTALLSITELIB)												\
	    INSTALLSITEMAN1DIR=$(INSTALLSITEMAN1DIR)										\
	    INSTALLSITEMAN3DIR=$(INSTALLSITEMAN3DIR)										\
		INSTALLSITESCRIPT=$(INSTALLSITESCRIPT)											\
		INSTALLSCRIPT=$(INSTALLSCRIPT);													\
	make all test pure_install;									
	@echo "";

# Net-DNS tries to determine if it can/should create XS code.
# this XS code generation fails with a cryptic message about 
# "no table of contents, run ranlib" ... not sure how to update
# the Makefile.PL to do this for us so for now just pass -noxs 
Net-DNS::
	@echo "=============== Making $@ ==================";								\
	cd $(OBJROOT)/$@;																	\
	mv Makefile.PL Makefile.PL.orig;													\
	cat Makefile.PL.orig ../add_rc_constants.pl > Makefile.PL;							\
	perl $(INCARGS) Makefile.PL -noxs -noonline-tests 									\
		"PERL=/usr/bin/perl $(INCARGS)"													\
	    INSTALLDIRS=$(INSTALLDIRS)														\
	    INSTALLSITEARCH=$(INSTALLSITEARCH)												\
	    INSTALLSITEBIN=$(INSTALLSITEBIN)												\
	    INSTALLSITELIB=$(INSTALLSITELIB)												\
	    INSTALLSITEMAN1DIR=$(INSTALLSITEMAN1DIR)										\
	    INSTALLSITEMAN3DIR=$(INSTALLSITEMAN3DIR)										\
		INSTALLSITESCRIPT=$(INSTALLSITESCRIPT)											\
		INSTALLSCRIPT=$(INSTALLSCRIPT);													\
	make all test pure_install;									
	@echo "";

#
# Apache2-SOAP will skip the test suite if there is no stdin.  We do this because Apache2-SOAP's
# tests will fail since we build as root and it doesn't like that. We force stdin to never exist
# for Apache2-SOAP so others running buildit don't have to remember this small tid-bit....
#
Apache2-SOAP::
	@echo "=============== Making $@ ==================";								\
	cd $(OBJROOT)/$@;																	\
	mv Makefile.PL Makefile.PL.orig;													\
	cat Makefile.PL.orig ../add_rc_constants.pl > Makefile.PL;							\
	perl $(INCARGS) Makefile.PL 														\
		"PERL=/usr/bin/perl $(INCARGS)"													\
		INSTALLDIRS=$(INSTALLDIRS)														\
		INSTALLSITEARCH=$(INSTALLSITEARCH)												\
		INSTALLSITEBIN=$(INSTALLSITEBIN)												\
		INSTALLSITELIB=$(INSTALLSITELIB)												\
		INSTALLSITEMAN1DIR=$(INSTALLSITEMAN1DIR)										\
		INSTALLSITEMAN3DIR=$(INSTALLSITEMAN3DIR)										\
		INSTALLSITESCRIPT=$(INSTALLSITESCRIPT)											\
		INSTALLSCRIPT=$(INSTALLSCRIPT);													\
	make all test pure_install < /dev/null;									
	@echo "";


#
# libwww-perl
# needs to have the -n flag, so that it will be non-interactive
#
libwww-perl::
	@echo "=============== Making $@ ==================";								\
	cd $(OBJROOT)/$@;																	\
	mv Makefile.PL Makefile.PL.orig;													\
	cat Makefile.PL.orig ../add_rc_constants.pl > Makefile.PL;							\
	perl $(INCARGS) Makefile.PL -n														\
		"PERL=/usr/bin/perl $(INCARGS)"													\
		INSTALLDIRS=$(INSTALLDIRS)														\
		INSTALLSITEARCH=$(INSTALLSITEARCH)												\
		INSTALLSITEBIN=$(INSTALLSITEBIN)												\
		INSTALLSITELIB=$(INSTALLSITELIB)												\
		INSTALLSITEMAN1DIR=$(INSTALLSITEMAN1DIR)										\
		INSTALLSITEMAN3DIR=$(INSTALLSITEMAN3DIR)										\
		INSTALLSITESCRIPT=$(INSTALLSITESCRIPT)											\
		INSTALLSCRIPT=$(INSTALLSCRIPT);													\
	make all test pure_install;									
	echo "";

#
# SOAP-Lite
# Needs all sorts of options for various transport modules.  This was just my best guess 
# at what we might need in the future.
#

SOAP-Lite::
	@echo "=============== Making $@ ==================";								\
	cd $(OBJROOT)/$@;																	\
	mv Makefile.PL Makefile.PL.orig;													\
	cat Makefile.PL.orig ../add_rc_constants.pl > Makefile.PL;							\
	perl Makefile.PL --noprompt															\
		"PERL=/usr/bin/perl $(INCARGS)"													\
		INSTALLDIRS=$(INSTALLDIRS)														\
		INSTALLSITEARCH=$(INSTALLSITEARCH)												\
		INSTALLSITEBIN=$(INSTALLSITEBIN)												\
		INSTALLSITELIB=$(INSTALLSITELIB)												\
		INSTALLSITEMAN1DIR=$(INSTALLSITEMAN1DIR)										\
		INSTALLSITEMAN3DIR=$(INSTALLSITEMAN3DIR)										\
		INSTALLSITESCRIPT=$(INSTALLSITESCRIPT)											\
		INSTALLSCRIPT=$(INSTALLSCRIPT)													\
	    --HTTP-Client																	\
	    --noHTTPS-Client																\
	    --noMAILTO-Client																\
	    --noFTP-Client																	\
	    --noHTTP-Daemon																	\
	    --noHTTP-Apache																	\
	    --noHTTP-FCGI																	\
	    --noPOP3-Server																	\
	    --noIO-Server																	\
	    --noMQ																			\
	    --noJABBER																		\
	    --noMIMEParser																	\
	    --noTCP																			\
	    --noHTTP;																		\
	make all pure_install;									
	@echo "";


#
# Module::Build section
#
# These modules use the newer Module::Build module to build and install
# Add your Module::Build based modules here.
#

Algorithm-C3 				\
Class-Accessor-Chained 		\
Class-Data-Accessor 		\
Class-Std 					\
Config-Std 					\
Data-Page 					\
DateTime-Locale 			\
DateTime-Format-Builder 	\
DateTime-Format-ISO8601 	\
DateTime-TimeZone 			\
ExtUtils-CBuilder 			\
Log-Dispatch 				\
Module-Build-0.20 			\
PathTools 					\
SQL-Abstract-Limit 			\
Sub-Uplevel 				\
Test-Exception 				\
Text-WordDiff 				\
version 					\
::
	@echo "=============== Making $@ ==================";				\
	cd $(OBJROOT)/$@;													\
	perl $(INCARGS) Build.PL											\
	    destdir=$(DSTROOT)												\
	    installdirs=$(INSTALLDIRS) 										\
	    --install_path arch=$(APPLE_INSTALLSITEARCH)					\
	    --install_path bin=$(CONFIG_INSTALLSITEBIN)						\
	    --install_path lib=$(APPLE_INSTALLSITELIB)						\
	    --install_path bindoc=$(CONFIG_INSTALLSITEMAN1DIR)				\
	    --install_path libdoc=$(CONFIG_INSTALLSITEMAN3DIR)				\
	    --install_path script=$(CONFIG_INSTALLSITESCRIPT);				\
	perl $(INCARGS) Build;												\
	perl $(INCARGS) Build test;											\
	perl $(INCARGS) Build install;										\
	echo

#
# ConfigurationFiles target
#     This target will install custom configuration files for the modules
#     It uses the ConfigurationFiles dir to hold them, and to make them
#

ConfigurationFiles::
	@echo "=============== Making $@ ==================";				\
	cd $(OBJROOT)/$@;													\
	make install;									

#
# install, installhdrs, clean and installsrc are standard.
# These are the targets which XBS calls with make
#

install:: echo-config-info install-ditto-phase $(SUBDIRS) ConfigurationFiles
	@if [ $(DSTROOT) ]; then                                                                        \
	    echo Stripping symbols from bundles ... ;                                                   \
	    echo find $(DSTROOT) -xdev -name '*.bundle' -print -exec strip -S {} \; ;                   \
	    find $(DSTROOT) -xdev -name '*.bundle' -print -exec strip -S {} \; ;	                    \
	    echo "" ;                                                                                   \
	    echo Stripping packlists ... ;                                                              \
	    find $(DSTROOT) -xdev -name '.packlist' -print -exec rm -f {} \; ;                          \
	fi
	@if [ -d $(INSTALLEXTRAS) ]; then                                                               \
	    echo Creating PrependToPath ... ;                                                           \
	    mkdir -p $(DSTROOT)$(CONFIG_INSTALLSITELIB);                                                \
	    echo $(APPLE_INSTALLSITELIB) > $(DSTROOT)$(CONFIG_INSTALLSITELIB)/PrependToPath;   			\
	fi
	rm -f $(DSTROOT)"$$INSTALLSITEARCH"/perllocal.pod


installhdrs::

clean::
	@for i in $(SUBDIRS); do						                	\
	    (									                        	\
			echo "=============== Cleaning $$i ==================";		\
			cd $$i;								                        \
			if [ -e Makefile ]; then					                \
			    make realclean;					                    	\
			fi;	 							                            \
			rm -f Makefile.old;						                    \
			echo "";							                        \
	    )									                        	\
	done									                        	\

installsrc::
	ditto . $(SRCROOT)

install-ditto-phase::
	@if [ "$(OBJROOT)" != "." ]; then			\
	    ditto . $(OBJROOT);						\
	fi;
	(cd $(OBJROOT) && ./applyPatches)

echo-config-info::
	@echo "=============== Configuration Information ==============="
	@echo "Perl 5.8 or greater:     $(PERL5_8_OR_GREATER)"
	@echo "Perl 5.10 or greater:    $(PERL5_10_OR_GREATER)"
	@echo ""
	@echo "Building on Tiger:       $(TIGER)"
	@echo "Building on Leopard:     $(LEOPARD)"
	@echo "Building on SnowLeopard: $(SNOWLEOPARD)"
	@echo ""
	@echo "ARCHFLAGS:               $(ARCHFLAGS)"
	@echo "INCARGS:                 $(INCARGS)"
	@echo ""
	@printf "%-20s%s\n" "INSTALLSITEARCH:"    $(INSTALLSITEARCH)
	@printf "%-20s%s\n" "INSTALLSITEBIN:"     $(INSTALLSITEBIN)
	@printf "%-20s%s\n" "INSTALLSITELIB:"     $(INSTALLSITELIB)
	@printf "%-20s%s\n" "INSTALLSITEMAN1DIR:" $(INSTALLSITEMAN1DIR)
	@printf "%-20s%s\n" "INSTALLSITEMAN3DIR:" $(INSTALLSITEMAN3DIR)
	@printf "%-20s%s\n" "INSTALLSITESCRIPT:"  $(INSTALLSITESCRIPT)
	@printf "%-20s%s\n" "INSTALLSCRIPT:"      $(INSTALLSCRIPT)
	@echo ""
	@echo Building subdirs: $(SUBDIRS)
	@echo ""
