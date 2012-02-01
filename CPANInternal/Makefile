# Determine the Perl major version
PERL5_8_OR_GREATER  = $(shell perl -MConfig -e 'print ($$Config{PERL_REVISION} > 5 || ($$Config{PERL_REVISION} == 5 && $$Config{PERL_VERSION} >= 8)  ? "YES" : "NO")')
PERL5_10_OR_GREATER = $(shell perl -MConfig -e 'print ($$Config{PERL_REVISION} > 5 || ($$Config{PERL_REVISION} == 5 && $$Config{PERL_VERSION} >= 10) ? "YES" : "NO")')

# Perl major version must be at least 5.8
ifneq ($(PERL5_8_OR_GREATER),YES)
	$(error You must have perl 5.8 or later)
endif

# Determine our OS major version.
OS_VERSION  = $(shell perl -e 'my $$osVersion = `sw_vers -buildVersion`; chomp($$osVersion); $$osVersion =~ s|^(\d+).*|$$1|; print "$$osVersion"')
SUPPORTED_OS     = NO
LION             = NO
LION_PLUS        = NO
SNOWLEOPARD      = NO
SNOWLEOPARD_PLUS = NO
LEOPARD          = NO
TIGER            = NO
PANTHER          = NO
ifeq ($(OS_VERSION),11)
	LION             = YES
	SUPPORTED_OS     = YES
	LION_PLUS        = YES
	SNOWLEOPARD_PLUS = YES
endif
ifeq ($(OS_VERSION),10)
	SNOWLEOPARD      = YES
	SUPPORTED_OS     = YES
	SNOWLEOPARD_PLUS = YES
endif
ifeq ($(OS_VERSION),9)
	LEOPARD      = YES
	SUPPORTED_OS = YES
endif
ifeq ($(OS_VERSION),8)
	TIGER        = YES
	SUPPORTED_OS = YES
endif

# OS major version must be recognized
ifneq ($(SUPPORTED_OS),YES)
    $(error Unsupported OS version $(OS_VERSION))
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

# Modules that build on Lion or later
ifeq ($(LION_PLUS),YES)

# These modules have fewer dependencies and can build early
TEST_MODULES = 							\
	Test-Tester							\
	Test-use-ok							\
	Test-NoWarnings						\
	Test-Deep							\

FIRST_MODULES =                         \
    Apache-DBI	                        \
    Apache2-SOAP                        \
	Authen-Krb5							\
	Bencode								\
	Class-Accessor-Chained				\
	Class-Data-Accessor					\
	Class-Factory-Util-1.6				\
	Class-Singleton-1.03				\
	Class-Std							\
	Class-Std-Utils						\
	Class-Trigger						\
	Class-WhiteHole						\
	Clone								\
	common-sense                        \
	Config-Std							\
	Context-Preserve					\
	Crypt-OpenSSL-Bignum				\
	Crypt-Rijndael						\
	Crypt-SSLeay						\
	Data-Dump							\
	Data-UUID							\
	Exporter-Easy						\
	File-NFSLock						\
	File-VirtualPath					\
	Heap								\
	IPC-LDT								\
	IPC-Signal							\
	JSON								\
	JSON-Any							\
	JSON-RPC							\
	JSON-XS							    \
	Lingua-EN-Inflect					\
	Lingua-EN-Inflect-Number			\
	Mail-Sender							\
	Module-Find							\
	Net-Daemon							\
	Net-Telnet							\
	Params-Validate						\
	Parse-Yapp							\
	Perl-Tidy							\
	PlRPC								\
	Readonly							\
	Readonly-XS							\
	String-ShellQuote-1.00				\
	Term-ReadLine-Perl					\
	Term-ReadPassword					\
	Text-LevenshteinXS					\
	Text-WordDiff						\
	Time-HiRes-Value					\
	Tree-DAG_Node						\
	Tree-Simple							\
	Tree-Simple-VisitorFactory			\
	UNIVERSAL-moniker					\
	Unix-Getrusage						\
	Variable-Magic						\

# Modules that must build in order
DEPENDENCY_ORDERED_LIST =				\
	Test-Warn							\
	Data-Dumper-Concise					\
	B-Hooks-EndOfScope					\
	Sub-Identify						\

DATETIMESUBDIRS =                       \
    DateTime-Locale                     \
    DateTime-TimeZone                   \
    DateTime                            \
    DateTime-Format-Strptime	        \
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
	Class-Inspector						\
    Class-C3-Componentised              \
    Class-Accessor-Grouped              \
    Data-Page                           \
    Graph                               \
    HTTP-Proxy                          \
    Log-Dispatch                        \
    Log-Log4perl                        \

DBIX_CLASS_MODULES =                    \
	Class-Unload						\
    DBIx-Class                          \
    DBIx-Class-Schema-Loader            \

# Modules that are temporarily disabled
#	PathTools is included as part of the System install (7699376)
DISABLED =                              \
	PathTools                           \

endif

# Modules that build on SnowLeopard
ifeq ($(SNOWLEOPARD),YES)

# These modules have fewer dependencies and can build early
TEST_MODULES = 							\
	Test-Simple							\
	Test-Tester							\
	Test-use-ok							\
	Test-NoWarnings						\
	Test-Deep							\

FIRST_MODULES =                         \
	Algorithm-C3						\
    Apache-DBI	                        \
    Apache2-SOAP                        \
	Authen-Krb5							\
	Bencode								\
	Class-Accessor-Chained				\
	Class-Data-Accessor					\
	Class-Factory-Util-1.6				\
	Class-Singleton-1.03				\
	Class-Std							\
	Class-Std-Utils						\
	Class-Trigger						\
	Class-WhiteHole						\
	Clone								\
	common-sense                        \
	Config-Std							\
	Context-Preserve					\
	Crypt-OpenSSL-Bignum				\
	Crypt-OpenSSL-RSA					\
	Crypt-OpenSSL-Random				\
	Crypt-Rijndael						\
	Crypt-SSLeay						\
	Data-Dump							\
	Data-UUID							\
	Error-0.15							\
	Exporter-Easy						\
	File-NFSLock						\
	File-Temp							\
	File-VirtualPath					\
	Heap								\
	IPC-LDT								\
	IPC-Signal							\
	JSON								\
	JSON-Any							\
	JSON-RPC							\
	JSON-XS							    \
	Lingua-EN-Inflect					\
	Lingua-EN-Inflect-Number			\
	Mail-Sender							\
	Module-Find							\
	Net-Daemon							\
	Net-Telnet							\
	Params-Util							\
	Params-Validate						\
	Parse-Yapp							\
	Path-Class							\
	Perl-Tidy							\
	PlRPC								\
	Readonly							\
	Readonly-XS							\
	Scope-Guard							\
	String-ShellQuote-1.00				\
	Task-Weaken							\
	Term-ReadLine-Perl					\
	Term-ReadPassword					\
	Text-LevenshteinXS					\
	Text-WordDiff						\
	Time-HiRes-Value					\
	Tree-DAG_Node						\
	Tree-Simple							\
	Tree-Simple-VisitorFactory			\
	Try-Tiny							\
	UNIVERSAL-moniker					\
	Unix-Getrusage						\
	Variable-Magic						\

# Modules that must build in order
DEPENDENCY_ORDERED_LIST =				\
	Test-Warn							\
	Data-Dumper-Concise					\
	Sub-Install							\
	Data-OptList						\
	Sub-Exporter						\
	B-Hooks-EndOfScope					\
	Devel-GlobalDestruction				\
	Sub-Identify						\
	Sub-Name							\
	Sub-Uplevel							\
	Test-Exception						\
	Class-MOP							\

DATETIMESUBDIRS =                       \
    DateTime-Locale                     \
    DateTime-TimeZone                   \
    DateTime                            \
    DateTime-Format-Builder             \
    DateTime-Format-Pg                  \
    DateTime-Format-W3CDTF              \
    DateTime-Format-ISO8601             \
    DateTime-Format-Strptime	        \

CLASS_DBI_MODULES =                     \
    DBIx-ContextualFetch                \
    DBI			                        \
    DBD-SQLite	                        \
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
	Class-Inspector						\
	Class-Unload						\
    DBIx-Class                          \
    DBIx-Class-Schema-Loader            \

# Modules that are temporarily disabled
#	PathTools is included as part of the System install (7699376)
DISABLED =                              \
	PathTools                           \

endif


# Modules that build on Leopard
ifeq ($(LEOPARD),YES)

FIRST_MODULES = 					\
	ExtUtils-MakeMaker			\

OTHERCPANSUBDIRS =  			\
	Authen-Krb5 				\
	Readonly					\
	Readonly-XS 				\
	String-ShellQuote-1.00  	\
	Error-0.15  				\
	Log-Log4perl				\
	Log-Dispatch				\
	Mail-Sender 				\
	common-sense                \
	Crypt-Rijndael  			\
	Crypt-OpenSSL-Random		\
	Crypt-OpenSSL-RSA   		\
	Crypt-SSLeay				\
	File-NFSLock				\
	HTTP-Proxy  				\
	Class-Std-Utils 			\
	YAML-Syck   				\
	Heap						\
	Graph   					\
	Test-Exception  			\
	Tree-Simple 				\
	Tree-Simple-VisitorFactory  \
	Class-Std   				\
	Config-Std  				\
	Text-LevenshteinXS  		\
	Text-WordDiff   			\
	Apache2-SOAP				\
	Apache-DBI  				\
	Digest-SHA  				\
	Term-ReadLine-Perl  		\
	Term-ReadPassword   		\
	Exporter-Easy   			\
	Data-UUID   				\
	Time-HiRes-Value			\
	Perl-Tidy   				\
	Bencode 					\
	Net-Telnet  				\
	Crypt-OpenSSL-Bignum		\
	Net-Daemon  				\
	PlRPC   					\
	TimeDate					\
	IPC-LDT 					\
	IPC-Signal  				\
	Parse-Yapp  				\
	File-VirtualPath			\
	FreezeThaw  				\
	JSON-XS  					\
	Unix-Getrusage				\
	Compress-Raw-Bzip2			\
	Compress-Raw-Zlib			\
	IO-Compress					\

DATETIMESUBDIRS =  				\
	Module-Build-0.20 			\
	Params-Validate   			\
	Class-Factory-Util-1.6		\
	Class-Singleton-1.03  		\
	DateTime-TimeZone 			\
	DateTime-Locale   			\
	DateTime  					\
	DateTime-Format-Strptime    \
	DateTime-Format-Builder   	\
	DateTime-Format-Pg			\
	DateTime-Format-W3CDTF		\
	DateTime-Format-ISO8601   	\

SOAP_LITE_MODULES = 			\
	SOAP-Lite 					\
	Pod-WSDL  					\

CLASS_DBI_MODULES =  			\
	DBI 						\
	UNIVERSAL-moniker   		\
	Class-Accessor  			\
	Class-Data-Inheritable  	\
	Class-Trigger   			\
	Class-WhiteHole 			\
	DBIx-ContextualFetch		\
	Ima-DBI 					\
	Class-DBI   				\
	Clone						\
	Tree-DAG_Node				\
	Test-Tester					\
	Test-NoWarnings				\
	Test-Deep					\
	Test-Warn					\
	SQL-Abstract				\
	Class-DBI-AbstractSearch	\
	Sub-Uplevel 				\
	Test-Simple 				\
	Class-Accessor-Chained  	\
	Class-Inspector 			\
	Data-Page   				\
	SQL-Abstract-Limit  		\
	Module-Find 				\
	Algorithm-C3				\
	Class-C3-XS 				\
	Class-C3					\
	MRO-Compat  				\
	Class-C3-Componentised  	\
	Sub-Identify				\
	Sub-Name					\
	Class-Data-Accessor 		\
	Class-Accessor-Grouped  	\
	ExtUtils-CBuilder   		\
	PathTools   				\
	Lingua-EN-Inflect   		\
	Lingua-EN-Inflect-Number	\
	Data-Dump   				\
	UNIVERSAL-require   		\
	Scope-Guard 				\
	JSON-Any					\
	JSON						\
	JSON-RPC					\
    DBD-SQLite	                \
	File-Temp					\
	Data-Dumper-Concise			\
	Path-Class					\
	DBIx-Class  				\

DBIX_CLASS_MODULES = 			\
	File-Path					\
	Class-Unload				\
	DBIx-Class-Schema-Loader	\


endif


# Modules that build on Tiger
ifeq ($(TIGER),YES)

FIRST_MODULES = 					\
		IO-Tty						\
		ExtUtils-MakeMaker			\

OTHERCPANSUBDIRS = 					\
		Authen-Krb5   				\
		version   					\
		Readonly  					\
		Readonly-XS   				\
		String-ShellQuote-1.00		\
		Error-0.15					\
		Log-Log4perl  				\
		Log-Dispatch  				\
		Mail-Sender   				\
		Apache-DBI					\
		Crypt-Rijndael				\
		Crypt-OpenSSL-Random  		\
		Crypt-OpenSSL-RSA 			\
		Crypt-SSLeay  				\
		File-NFSLock  				\
		HTTP-Proxy					\
		Class-Std-Utils   			\
		YAML-Syck 					\
		Heap  						\
		Graph 						\
		Test-Exception				\
		Tree-Simple   				\
		Tree-Simple-VisitorFactory	\
		Class-Std 					\
		Config-Std					\
		Text-LevenshteinXS			\
		Term-ReadLine-Perl			\
		Term-ReadPassword 			\
		Digest-SHA					\
		Exporter-Easy 				\
		Data-UUID 					\
		Time-HiRes-Value  			\
		Net-IP						\
		Net-Telnet					\
		Digest-HMAC   				\
		Net-DNS   					\
		Perl-Tidy 					\
		Bencode   					\
		TermReadKey   				\
		Crypt-OpenSSL-Bignum  		\
		Net-Daemon					\
		PlRPC 						\
		TimeDate  					\
		Expect						\
		IPC-LDT   					\
		IPC-Signal					\
		Parse-Yapp					\
		File-VirtualPath  			\
		FreezeThaw					\
        common-sense                \
        JSON-XS                     \
		Unix-Getrusage				\

DATETIMESUBDIRS = 					\
		Module-Build-0.20 			\
		Params-Validate   			\
		Class-Factory-Util-1.6		\
		Class-Singleton-1.03  		\
		List-MoreUtils				\
		DateTime-TimeZone 			\
		DateTime-Locale   			\
		DateTime  					\
		DateTime-Format-Strptime-1.04 \
		DateTime-Format-Builder   	\
		DateTime-Format-Pg			\
		DateTime-Format-W3CDTF		\
		DateTime-Format-ISO8601   	\

SOAP_LITE_MODULES = 				\
		URI   						\
		libwww-perl   				\
		SOAP-Lite 					\
		Pod-WSDL  					\

CLASS_DBI_MODULES = 				\
		DBI   						\
		DBD-SQLite					\
		UNIVERSAL-moniker 			\
		Class-Accessor				\
		Class-Data-Inheritable		\
		Class-Trigger 				\
		Class-WhiteHole   			\
		DBIx-ContextualFetch  		\
		Ima-DBI   					\
		Class-DBI 					\
		Clone						\
		Tree-DAG_Node				\
		Test-Tester					\
		Test-NoWarnings				\
		Test-Deep					\
		Test-Warn					\
		SQL-Abstract  				\
		Class-DBI-AbstractSearch  	\

DBIX_CLASS_MODULES = 				\
		Sub-Uplevel   				\
		Test-Simple   				\
		Class-Accessor-Chained		\
		Class-Inspector   			\
		Data-Page 					\
		SQL-Abstract-Limit			\
		Carp-Clan 					\
		Module-Find   				\
		Algorithm-C3  				\
		Class-C3-XS   				\
		Class-C3  					\
		MRO-Compat					\
		Class-C3-Componentised		\
		Sub-Identify				\
		Sub-Name					\
		Class-Data-Accessor   		\
		Class-Accessor-Grouped		\
		ExtUtils-CBuilder 			\
		PathTools 					\
		Lingua-EN-Inflect 			\
		Lingua-EN-Inflect-Number  	\
		Data-Dump 					\
		UNIVERSAL-require 			\
		Scope-Guard   				\
		JSON-Any  					\
		JSON  						\
		JSON-RPC					\
		Digest-MD5					\
	    DBD-SQLite	                \
		File-Temp					\
		Data-Dumper-Concise			\
		Path-Class					\
		DBIx-Class					\
		File-Path					\
		Class-Unload				\
		File-Slurp					\
		DBIx-Class-Schema-Loader  	\

LDAP_MODULES = 						\
		Convert-ASN1  				\
		Perl-Ldap 					\

XML_MODULES = 						\
		XML-NamespaceSupport  		\
		XML-SAX   					\
		XML-LibXML-Common 			\
		XML-LibXML					\
		XML-Writer					\
		XML-Parser					\
		XML-XPath 					\

# XML-SAX needs this defined to test properly:
export XMLVS_TEST_PARSER=XML::LibXML::SAX

endif


# Define SUBDIRS as the complete set of modules
# Leave them in this order since we don't want to go back to Tiger and Leopard and sort out dependencies
SUBDIRS =                   \
	$(TEST_MODULES)			\
    $(FIRST_MODULES)        \
	$(DEPENDENCY_ORDERED_LIST)	\
    $(DATETIMESUBDIRS)      \
    $(CLASS_DBI_MODULES)    \
    $(LDAP_MODULES)         \
    $(XML_MODULES)          \
    $(SOAP_LITE_MODULES)    \
    $(OTHERCPANSUBDIRS)     \
	$(DBIX_CLASS_MODULES)   \


# On Lion and later, produce modules for all supported versions of Perl
# Otherwise, just build for the default version
ifeq ($(LION_PLUS),YES)
    # Build for all supported versions
    VERSIONS_FILE = /usr/local/versioner/perl/versions
    # The value set as default might not exactly match the corresponding version entry
    # Map it to the actual version entry (e.g. 5.10 -> 5.10.0)
    DEFAULT_VALUE      = $(shell sed -n '/^DEFAULT = /s///p'   $(VERSIONS_FILE))
    DEFAULT_VERSION    = $(shell grep '^$(DEFAULT_VALUE)'      $(VERSIONS_FILE))
    VERSIONS_UNORDERED = $(shell grep -v '^DEFAULT = '         $(VERSIONS_FILE))
    # Make sure $VERSIONS is ordered s.t. the default is last
    VERSIONS = $(filter-out $(DEFAULT_VERSION),$(VERSIONS_UNORDERED)) $(DEFAULT_VERSION)
    ifeq ($(strip $(VERSIONS)),)
        $(error Error parsing $(VERSIONS_FILE))
    endif
else
    VERSIONS = $(shell perl -MConfig -e 'print $$Config{version}')
endif


# Set INSTALLDIRS to "site" to enable all of the INSTALLSITE* variables
INSTALLDIRS = site

# Grab some configuration values from Perl
CONFIG_INSTALLSITEBIN     = $(shell perl -MConfig -e 'print $$Config{installsitebin}')
CONFIG_INSTALLSITEMAN1DIR = $(shell perl -MConfig -e 'print $$Config{installsiteman1dir}')
CONFIG_INSTALLSITEMAN3DIR = $(shell perl -MConfig -e 'print $$Config{installsiteman3dir}')
CONFIG_INSTALLSITESCRIPT  = $(shell perl -MConfig -e 'print $$Config{installsitescript}')
CONFIG_INSTALLPRIVLIB     = $(shell perl -MConfig -e 'print $$Config{installprivlib}')
CONFIG_INSTALLSITELIB_DEFAULT = $(shell perl -MConfig -e 'print $$Config{installsitelib}')

# Install in /AppleInternal/Library/Perl if the /S/L/Perl/Extras directory exists (because the
# existence of that directory indicates support for the {Append,Prepend}ToPath @INC modification stuff)
INSTALLEXTRAS = $(subst Perl,Perl/Extras,$(CONFIG_INSTALLPRIVLIB))
ifeq "$(shell test -d $(INSTALLEXTRAS) && echo YES )" "YES" 
    INSTALL_LIB_PREFIX = /AppleInternal
else
    INSTALL_LIB_PREFIX = ""
endif

# prepend DSTROOT for use with ExtUtils::MakeMaker (Modules::Build does not want DSTROOT)
INSTALLSITEBIN     = $(DSTROOT)$(CONFIG_INSTALLSITEBIN)
INSTALLSITEMAN1DIR = $(DSTROOT)$(CONFIG_INSTALLSITEMAN1DIR)
INSTALLSITEMAN3DIR = $(DSTROOT)$(CONFIG_INSTALLSITEMAN3DIR)
INSTALLSITESCRIPT  = $(DSTROOT)$(CONFIG_INSTALLSITESCRIPT)

# Older versions of ExtUtils::MakeMaker don't support INSTALLSITESCRIPT
INSTALLSCRIPT = $(DSTROOT)$(CONFIG_INSTALLSITESCRIPT)

# set up ARCHFLAGS as per rdar://problem/5402242
ifeq ($(SNOWLEOPARD_PLUS),YES)
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
B-Hooks-EndOfScope \
Bencode \
Carp-Clan \
Class-Accessor \
Class-Accessor-Grouped \
Class-C3 \
Class-C3-Componentised \
Class-C3-XS \
Class-DBI \
Class-DBI-AbstractSearch \
Class-Data-Inheritable \
Class-Factory-Util-1.6 \
Class-Inspector \
Class-MOP \
Class-Singleton-1.03 \
Class-Std-Utils \
Class-Trigger \
Class-Unload \
Class-WhiteHole \
Clone \
common-sense \
Compress-Raw-Bzip2 \
Compress-Raw-Zlib \
Context-Preserve \
Convert-ASN1 \
Crypt-OpenSSL-Bignum \
Crypt-OpenSSL-RSA \
Crypt-OpenSSL-Random \
Crypt-Rijndael \
Crypt-SSLeay \
DBD-SQLite \
DBI \
DBIx-Class \
DBIx-Class-Schema-Loader \
DBIx-ContextualFetch \
Data-Dump \
Data-Dumper-Concise \
Data-OptList \
Data-UUID \
DateTime-Format-Pg \
DateTime-Format-Strptime \
DateTime-Format-W3CDTF \
Devel-GlobalDestruction \
Digest-HMAC \
Digest-MD5 \
Digest-SHA \
Error-0.15 \
Expect \
Exporter-Easy \
ExtUtils-MakeMaker \
File-NFSLock \
File-Path \
File-Slurp \
File-Temp \
File-VirtualPath \
FreezeThaw \
Graph \
HTML-Tagset-3.03 \
HTTP-Proxy \
Heap \
IO-Compress \
IO-Tty \
IPC-LDT \
IPC-Signal \
Ima-DBI \
JSON \
JSON-Any \
JSON-RPC \
JSON-XS \
Lingua-EN-Inflect \
Lingua-EN-Inflect-Number \
List-MoreUtils \
Log-Log4perl \
MIME-Base64-2.20 \
MRO-Compat \
Mail-Sender \
Module-Find \
Net-Daemon \
Net-IP \
Net-Telnet \
Params-Util \
Params-Validate \
Parse-Yapp \
Perl-Ldap \
Perl-Tidy \
PlRPC \
Pod-WSDL \
Readonly \
Readonly-XS \
SQL-Abstract \
Scope-Guard \
String-ShellQuote-1.00 \
Sub-Exporter \
Sub-Identify \
Sub-Install \
Sub-Name \
Task-Weaken \
Term-ReadLine-Perl \
Term-ReadPassword \
TermReadKey \
Test-Deep \
Test-NoWarnings \
Test-Simple \
Test-Tester \
Test-Warn \
Test-use-ok \
Text-LevenshteinXS \
Time-HiRes-Value \
TimeDate \
Tree-DAG_Node \
Tree-Simple \
Tree-Simple-VisitorFactory \
Try-Tiny \
UNIVERSAL-moniker \
UNIVERSAL-require \
URI \
Unix-Getrusage \
Variable-Magic \
XML-LibXML \
XML-LibXML-Common \
XML-NamespaceSupport \
XML-Parser \
XML-SAX \
XML-Writer \
XML-XPath \
YAML-Syck \
::
	@echo "=============== Making $@ =================="; \
	set -x; \
	cd $(OBJROOT)/$@ && \
	mv Makefile.PL Makefile.PL.orig && \
	cat Makefile.PL.orig ../add_rc_constants.pl > Makefile.PL && \
	if [ -d Expat ]; then \
		mv Expat/Makefile.PL Expat/Makefile.PL.orig && \
		cat Expat/Makefile.PL.orig ../add_rc_constants.pl > Expat/Makefile.PL || exit 1; \
	fi && \
	for vers in $(VERSIONS); do \
		export VERSIONER_PERL_VERSION=$$vers; \
		INSTALL_SITE_LIB=`perl -MConfig -e 'print $$Config{installsitelib}'`; \
		INSTALL_SITE_ARCH=`perl -MConfig -e 'print $$Config{installsitearch}'`; \
		INSTALL_LIB=$(INSTALL_LIB_PREFIX)$$INSTALL_SITE_LIB; \
		INSTALL_ARCH=$(INSTALL_LIB_PREFIX)$$INSTALL_SITE_ARCH; \
		INCARGS="-I$(DSTROOT)/$$INSTALL_ARCH -I$(DSTROOT)/$$INSTALL_LIB"; \
		perl $$INCARGS Makefile.PL \
			"PERL=/usr/bin/perl $$INCARGS" \
			INSTALLSITELIB=$(DSTROOT)/$$INSTALL_LIB \
			INSTALLSITEARCH=$(DSTROOT)/$$INSTALL_ARCH \
			INSTALLDIRS=$(INSTALLDIRS) \
			INSTALLSITEBIN=$(INSTALLSITEBIN) \
			INSTALLSITEMAN1DIR=$(INSTALLSITEMAN1DIR) \
			INSTALLSITEMAN3DIR=$(INSTALLSITEMAN3DIR) \
			INSTALLSITESCRIPT=$(INSTALLSITESCRIPT) \
			INSTALLSCRIPT=$(INSTALLSCRIPT) && \
		make all test pure_install clean || exit 1; \
	done;
	@echo "";

# Net-DNS tries to determine if it can/should create XS code.
# this XS code generation fails with a cryptic message about 
# "no table of contents, run ranlib" ... not sure how to update
# the Makefile.PL to do this for us so for now just pass -noxs 
Net-DNS::
	@echo "=============== Making $@ =================="; \
	set -x; \
	cd $(OBJROOT)/$@ && \
	mv Makefile.PL Makefile.PL.orig && \
	cat Makefile.PL.orig ../add_rc_constants.pl > Makefile.PL && \
	for vers in $(VERSIONS); do \
		export VERSIONER_PERL_VERSION=$$vers; \
		INSTALL_SITE_LIB=`perl -MConfig -e 'print $$Config{installsitelib}'`; \
		INSTALL_SITE_ARCH=`perl -MConfig -e 'print $$Config{installsitearch}'`; \
		INSTALL_LIB=$(INSTALL_LIB_PREFIX)$$INSTALL_SITE_LIB; \
		INSTALL_ARCH=$(INSTALL_LIB_PREFIX)$$INSTALL_SITE_ARCH; \
		INCARGS="-I$(DSTROOT)/$$INSTALL_ARCH -I$(DSTROOT)/$$INSTALL_LIB"; \
		perl $$INCARGS Makefile.PL -noxs -noonline-tests \
			"PERL=/usr/bin/perl $$INCARGS" \
			INSTALLSITELIB=$(DSTROOT)/$$INSTALL_LIB \
			INSTALLSITEARCH=$(DSTROOT)/$$INSTALL_ARCH \
			INSTALLDIRS=$(INSTALLDIRS) \
			INSTALLSITEBIN=$(INSTALLSITEBIN) \
			INSTALLSITEMAN1DIR=$(INSTALLSITEMAN1DIR) \
			INSTALLSITEMAN3DIR=$(INSTALLSITEMAN3DIR) \
			INSTALLSITESCRIPT=$(INSTALLSITESCRIPT) \
			INSTALLSCRIPT=$(INSTALLSCRIPT) && \
		make all test pure_install clean || exit 1; \
	done;
	@echo "";

#
# Apache2-SOAP will skip the test suite if there is no stdin.  We do this because Apache2-SOAP's
# tests will fail since we build as root and it doesn't like that. We force stdin to never exist
# for Apache2-SOAP so others running buildit don't have to remember this small tid-bit....
#
Apache2-SOAP::
	@echo "=============== Making $@ =================="; \
	set -x; \
	cd $(OBJROOT)/$@ && \
	mv Makefile.PL Makefile.PL.orig && \
	cat Makefile.PL.orig ../add_rc_constants.pl > Makefile.PL && \
	for vers in $(VERSIONS); do \
		export VERSIONER_PERL_VERSION=$$vers; \
		INSTALL_SITE_LIB=`perl -MConfig -e 'print $$Config{installsitelib}'`; \
		INSTALL_SITE_ARCH=`perl -MConfig -e 'print $$Config{installsitearch}'`; \
		INSTALL_LIB=$(INSTALL_LIB_PREFIX)$$INSTALL_SITE_LIB; \
		INSTALL_ARCH=$(INSTALL_LIB_PREFIX)$$INSTALL_SITE_ARCH; \
		INCARGS="-I$(DSTROOT)/$$INSTALL_ARCH -I$(DSTROOT)/$$INSTALL_LIB"; \
		perl $$INCARGS Makefile.PL \
			"PERL=/usr/bin/perl $$INCARGS" \
			INSTALLSITELIB=$(DSTROOT)/$$INSTALL_LIB \
			INSTALLSITEARCH=$(DSTROOT)/$$INSTALL_ARCH \
			INSTALLDIRS=$(INSTALLDIRS) \
			INSTALLSITEBIN=$(INSTALLSITEBIN) \
			INSTALLSITEMAN1DIR=$(INSTALLSITEMAN1DIR) \
			INSTALLSITEMAN3DIR=$(INSTALLSITEMAN3DIR) \
			INSTALLSITESCRIPT=$(INSTALLSITESCRIPT) \
			INSTALLSCRIPT=$(INSTALLSCRIPT) && \
		make all test pure_install clean < /dev/null || exit 1; \
	done;
	@echo "";


#
# libwww-perl
# needs to have the -n flag, so that it will be non-interactive
#
libwww-perl::
	@echo "=============== Making $@ =================="; \
	set -x; \
	cd $(OBJROOT)/$@ && \
	mv Makefile.PL Makefile.PL.orig && \
	cat Makefile.PL.orig ../add_rc_constants.pl > Makefile.PL && \
	for vers in $(VERSIONS); do \
		export VERSIONER_PERL_VERSION=$$vers; \
		INSTALL_SITE_LIB=`perl -MConfig -e 'print $$Config{installsitelib}'`; \
		INSTALL_SITE_ARCH=`perl -MConfig -e 'print $$Config{installsitearch}'`; \
		INSTALL_LIB=$(INSTALL_LIB_PREFIX)$$INSTALL_SITE_LIB; \
		INSTALL_ARCH=$(INSTALL_LIB_PREFIX)$$INSTALL_SITE_ARCH; \
		INCARGS="-I$(DSTROOT)/$$INSTALL_ARCH -I$(DSTROOT)/$$INSTALL_LIB"; \
		perl $$INCARGS Makefile.PL -n \
			"PERL=/usr/bin/perl $$INCARGS" \
			INSTALLSITELIB=$(DSTROOT)/$$INSTALL_LIB \
			INSTALLSITEARCH=$(DSTROOT)/$$INSTALL_ARCH \
			INSTALLDIRS=$(INSTALLDIRS) \
			INSTALLSITEBIN=$(INSTALLSITEBIN) \
			INSTALLSITEMAN1DIR=$(INSTALLSITEMAN1DIR) \
			INSTALLSITEMAN3DIR=$(INSTALLSITEMAN3DIR) \
			INSTALLSITESCRIPT=$(INSTALLSITESCRIPT) \
			INSTALLSCRIPT=$(INSTALLSCRIPT) && \
		make all test pure_install clean || exit 1; \
	done;
	@echo "";

#
# SOAP-Lite
# Needs all sorts of options for various transport modules.  This was just my best guess 
# at what we might need in the future.
#

SOAP-Lite::
	@echo "=============== Making $@ =================="; \
	set -x; \
	cd $(OBJROOT)/$@ && \
	mv Makefile.PL Makefile.PL.orig && \
	cat Makefile.PL.orig ../add_rc_constants.pl > Makefile.PL && \
	for vers in $(VERSIONS); do \
		export VERSIONER_PERL_VERSION=$$vers; \
		INSTALL_SITE_LIB=`perl -MConfig -e 'print $$Config{installsitelib}'`; \
		INSTALL_SITE_ARCH=`perl -MConfig -e 'print $$Config{installsitearch}'`; \
		INSTALL_LIB=$(INSTALL_LIB_PREFIX)$$INSTALL_SITE_LIB; \
		INSTALL_ARCH=$(INSTALL_LIB_PREFIX)$$INSTALL_SITE_ARCH; \
		INCARGS="-I$(DSTROOT)/$$INSTALL_ARCH -I$(DSTROOT)/$$INSTALL_LIB"; \
		perl Makefile.PL --noprompt \
			"PERL=/usr/bin/perl $$INCARGS" \
			INSTALLSITELIB=$(DSTROOT)/$$INSTALL_LIB \
			INSTALLSITEARCH=$(DSTROOT)/$$INSTALL_ARCH \
			INSTALLDIRS=$(INSTALLDIRS) \
			INSTALLSITEBIN=$(INSTALLSITEBIN) \
			INSTALLSITEMAN1DIR=$(INSTALLSITEMAN1DIR) \
			INSTALLSITEMAN3DIR=$(INSTALLSITEMAN3DIR) \
			INSTALLSITESCRIPT=$(INSTALLSITESCRIPT) \
			INSTALLSCRIPT=$(INSTALLSCRIPT) \
			--HTTP-Client \
			--noHTTPS-Client \
			--noMAILTO-Client \
			--noFTP-Client \
			--noHTTP-Daemon \
			--noHTTP-Apache \
			--noHTTP-FCGI \
			--noPOP3-Server \
			--noIO-Server \
			--noMQ \
			--noJABBER \
			--noMIMEParser \
			--noTCP \
			--noHTTP; \
		make all pure_install clean || exit 1; \
	done;
	@echo "";


#
# Module::Build section
#
# These modules use the newer Module::Build module to build and install
# Add your Module::Build based modules here.
#

Algorithm-C3 \
Class-Accessor-Chained \
Class-Data-Accessor \
Class-Std \
Config-Std \
Data-Page \
DateTime \
DateTime-Locale \
DateTime-Format-Builder \
DateTime-Format-ISO8601 \
DateTime-TimeZone \
ExtUtils-CBuilder \
Log-Dispatch \
Module-Build-0.20 \
Path-Class \
PathTools \
SQL-Abstract-Limit \
Sub-Uplevel \
Test-Exception \
Text-WordDiff \
version \
::
	@echo "=============== Making $@ =================="; \
	set -x; \
	for vers in $(VERSIONS); do \
		export VERSIONER_PERL_VERSION=$$vers; \
		INSTALL_SITE_LIB=`perl -MConfig -e 'print $$Config{installsitelib}'`; \
		INSTALL_SITE_ARCH=`perl -MConfig -e 'print $$Config{installsitearch}'`; \
		INSTALL_LIB=$(INSTALL_LIB_PREFIX)$$INSTALL_SITE_LIB; \
		INSTALL_ARCH=$(INSTALL_LIB_PREFIX)$$INSTALL_SITE_ARCH; \
		INCARGS="-I$(DSTROOT)/$$INSTALL_ARCH -I$(DSTROOT)/$$INSTALL_LIB"; \
		cd $(OBJROOT)/$@ && \
		perl $$INCARGS Build.PL \
			destdir=$(DSTROOT) \
			installdirs=$(INSTALLDIRS) \
			--install_path lib=$$INSTALL_LIB \
			--install_path arch=$$INSTALL_ARCH \
			--install_path bin=$(CONFIG_INSTALLSITEBIN) \
			--install_path bindoc=$(CONFIG_INSTALLSITEMAN1DIR) \
			--install_path libdoc=$(CONFIG_INSTALLSITEMAN3DIR) \
			--install_path script=$(CONFIG_INSTALLSITESCRIPT) && \
		perl $$INCARGS Build && \
		perl $$INCARGS Build test && \
		perl $$INCARGS Build install || exit 1; \
	done;
	@echo "";

#
# ConfigurationFiles target
#     This target will install custom configuration files for the modules
#     It uses the ConfigurationFiles dir to hold them, and to make them
#

ConfigurationFiles::
	@echo "=============== Making $@ =================="; \
	cd $(OBJROOT)/$@; \
	make install;									

#
# install, installhdrs, clean and installsrc are standard.
# These are the targets which XBS calls with make
#

install:: echo-config-info install-ditto-phase $(SUBDIRS) ConfigurationFiles
	@if [ $(DSTROOT) ]; then \
	    echo Stripping symbols from bundles ... ; \
	    echo find $(DSTROOT) -xdev -name '*.bundle' -print -exec strip -S {} \; ;                   \
	    find $(DSTROOT) -xdev -name '*.bundle' -print -exec strip -S {} \; ;                        \
	    echo "" ; \
	    echo Stripping packlists ... ; \
	    find $(DSTROOT) -xdev -name '.packlist' -print -exec rm -f {} \; ;                          \
	fi

install_config::
	@if [ -d $(INSTALLEXTRAS) ]; then \
	    echo Creating PrependToPath ... ; \
		if [ $(LION_PLUS) == "YES" ]; then \
			for vers in $(VERSIONS); do \
				export VERSIONER_PERL_VERSION=$$vers; \
				INSTALL_SITE_LIB=`perl -MConfig -e 'print $$Config{installsitelib}'`; \
				mkdir -p $(DSTROOT)$$INSTALL_SITE_LIB; \
				if echo $$vers | egrep -q '5\.10(\..*)?'; then \
					echo "/AppleInternal/Library/Perl/5.10/darwin-thread-multi-2level"          >  $(DSTROOT)$$INSTALL_SITE_LIB/PrependToPath; \
					echo "/AppleInternal/Library/Perl/5.10"                                     >> $(DSTROOT)$$INSTALL_SITE_LIB/PrependToPath; \
					echo "/AppleInternal/Library/Perl/Extras/5.10/darwin-thread-multi-2level"   >> $(DSTROOT)$$INSTALL_SITE_LIB/PrependToPath; \
					echo "/AppleInternal/Library/Perl/Extras/5.10"                              >> $(DSTROOT)$$INSTALL_SITE_LIB/PrependToPath; \
					echo "/AppleInternal/Library/Perl/5.10.0/darwin-thread-multi-2level"        >> $(DSTROOT)$$INSTALL_SITE_LIB/PrependToPath; \
					echo "/AppleInternal/Library/Perl/5.10.0"                                   >> $(DSTROOT)$$INSTALL_SITE_LIB/PrependToPath; \
					echo "/AppleInternal/Library/Perl/Extras/5.10.0/darwin-thread-multi-2level" >> $(DSTROOT)$$INSTALL_SITE_LIB/PrependToPath; \
					echo "/AppleInternal/Library/Perl/Extras/5.10.0"                            >> $(DSTROOT)$$INSTALL_SITE_LIB/PrependToPath; \
					echo "/AppleInternal/Library/Perl/5.10.1/darwin-thread-multi-2level"        >> $(DSTROOT)$$INSTALL_SITE_LIB/PrependToPath; \
					echo "/AppleInternal/Library/Perl/5.10.1"                                   >> $(DSTROOT)$$INSTALL_SITE_LIB/PrependToPath; \
					echo "/AppleInternal/Library/Perl/Extras/5.10.1/darwin-thread-multi-2level" >> $(DSTROOT)$$INSTALL_SITE_LIB/PrependToPath; \
					echo "/AppleInternal/Library/Perl/Extras/5.10.1"                            >> $(DSTROOT)$$INSTALL_SITE_LIB/PrependToPath; \
				else \
					echo $(INSTALL_LIB_PREFIX)$$INSTALL_SITE_LIB								>  $(DSTROOT)$$INSTALL_SITE_LIB/PrependToPath; \
				fi; \
			done; \
		else \
			mkdir -p $(DSTROOT)$(CONFIG_INSTALLSITELIB_DEFAULT); \
			echo $(INSTALL_LIB_PREFIX)$(CONFIG_INSTALLSITELIB_DEFAULT)                   		>  $(DSTROOT)$(CONFIG_INSTALLSITELIB_DEFAULT)/PrependToPath; \
		fi; \
	else \
		echo "ERROR: INSTALLEXTRAS path '$(INSTALLEXTRAS)' does not exist"; \
		exit 1; \
	fi

installhdrs::

clean::
	@for i in $(SUBDIRS); do \
	    ( \
			echo "=============== Cleaning $$i =================="; \
			cd $$i; \
			if [ -e Makefile ]; then \
			    make realclean; \
			fi; \
			rm -f Makefile.old; \
			echo ""; \
	    ) \
	done \

installsrc::
	ditto . $(SRCROOT)
	find $(SRCROOT) -type d -name CVS  -exec rm -rf {} \; -prune
	find $(SRCROOT) -type d -name .svn -exec rm -rf {} \; -prune

install-ditto-phase::
	@if [ "$(OBJROOT)" != "." ]; then \
	    ditto . $(OBJROOT); \
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
	@echo "Building on Lion:        $(LION)"
	@echo ""
	@echo "ARCHFLAGS:               $(ARCHFLAGS)"
	@echo ""
	@printf "%-20s%s\n" "INSTALLSITEBIN:"     $(INSTALLSITEBIN)
	@printf "%-20s%s\n" "INSTALLSITEMAN1DIR:" $(INSTALLSITEMAN1DIR)
	@printf "%-20s%s\n" "INSTALLSITEMAN3DIR:" $(INSTALLSITEMAN3DIR)
	@printf "%-20s%s\n" "INSTALLSITESCRIPT:"  $(INSTALLSITESCRIPT)
	@printf "%-20s%s\n" "INSTALLSCRIPT:"      $(INSTALLSCRIPT)
	@echo ""
	@echo Building subdirs: $(SUBDIRS)
	@echo ""
