# Makefile for generated files.

PERL=/usr/bin/perl

CDSA_HEADERS_DIR = $(SRCROOT)/cdsa/cdsa
SECURITY_HEADERS_DIR = $(SRCROOT)/cdsa/cdsa_utilities
KEYCHAIN_HEADERS_DIR = $(SRCROOT)/Keychain
AUTH_HEADERS_DIR = $(SRCROOT)/SecurityServer/Authorization
CDSA_SOURCES_DIR = $(SRCROOT)/cdsa/cssm
KEYCHAIN_SOURCES_DIR = $(SRCROOT)/Keychain
CSPDL_SOURCES_DIR = $(SRCROOT)/AppleCSPDL
CDSA_UTILITIES_DIR = $(SRCROOT)/cdsa/cdsa_utilities
CDSA_PLUGINLIB_DIR = $(SRCROOT)/cdsa/cdsa_pluginlib
GEN_SOURCE_DIR = $(BUILT_PRODUCTS_DIR)/derived_src
GEN_SOURCE_ENGLISH_DIR = $(GEN_SOURCE_DIR)		#when we localize again: $(GEN_SOURCE_DIR)/English.lproj
GEN_HEADER_DIR = $(BUILT_PRODUCTS_DIR)/include/Security
SECTRANSPORT_HEADERS_DIR = $(SRCROOT)/SecureTransport/SecureTransport

GEN_ERRORCODES = $(CDSA_UTILITIES_DIR)/generator.pl
ERRORCODES_GEN = $(patsubst %,$(GEN_SOURCE_DIR)/%,errorcodes.gen)
ERRORCODES_DEPENDS = $(GEN_ERRORCODES)\
					 $(patsubst %,$(CDSA_HEADERS_DIR)/%,cssmerr.h)\
					 $(patsubst %,$(CDSA_HEADERS_DIR)/%,cssmapple.h)

GEN_ERRORSTRINGS = $(KEYCHAIN_HEADERS_DIR)/generateErrStrings.pl
ERRORSTRINGS_GEN = $(patsubst %,$(GEN_SOURCE_DIR)/%,SecErrorMessages.strings)
ERRORSTRINGS_DEPENDS = $(GEN_ERRORSTRINGS)\
					 $(patsubst %,$(KEYCHAIN_HEADERS_DIR)/%,SecBase.h)\
					 $(patsubst %,$(KEYCHAIN_HEADERS_DIR)/%,SecKeychainAPIPriv.h)\
					 $(patsubst %,$(SECTRANSPORT_HEADERS_DIR)/%,SecureTransport.h)\
					 $(patsubst %,$(AUTH_HEADERS_DIR)/%,Authorization.h)

GEN_APIGLUE = $(CDSA_SOURCES_DIR)/generator.pl
APIGLUE_GEN = $(patsubst %,$(GEN_SOURCE_DIR)/%,transition.gen funcnames.gen generator.rpt)
APIGLUE_DEPENDS = $(patsubst %,$(CDSA_SOURCES_DIR)/%, generator.pl generator.cfg)\
				  $(patsubst %,$(CDSA_HEADERS_DIR)/%,cssmapi.h cssmtype.h cssmconfig.h cssmaci.h cssmcspi.h cssmdli.h cssmcli.h cssmtpi.h)

GEN_SPIGLUE = $(CDSA_PLUGINLIB_DIR)/generator.pl
SPIGLUE_GEN = $(patsubst %,$(GEN_HEADER_DIR)/%,ACabstractsession.h CLabstractsession.h CSPabstractsession.h DLabstractsession.h TPabstractsession.h)\
			  $(patsubst %,$(GEN_SOURCE_DIR)/%,ACabstractsession.cpp CLabstractsession.cpp CSPabstractsession.cpp DLabstractsession.cpp TPabstractsession.cpp)
SPIGLUE_DEPENDS = $(patsubst %,$(CDSA_PLUGINLIB_DIR)/%,generator.pl generator.cfg)\
				  $(patsubst %,$(CDSA_HEADERS_DIR)/%,cssmapi.h cssmtype.h cssmconfig.h cssmaci.h cssmcli.h cssmcspi.h cssmdli.h cssmspi.h)

SCHEMA_GEN = $(patsubst %,$(GEN_SOURCE_DIR)/%,Schema.cpp)
SCHEMA_DEPENDS = $(patsubst %,$(KEYCHAIN_SOURCES_DIR)/%,Schema.m4)

KEYSCHEMA_GEN = $(patsubst %,$(GEN_SOURCE_DIR)/%,KeySchema.cpp)
KEYSCHEMA_DEPENDS = $(patsubst %,$(CSPDL_SOURCES_DIR)/%,KeySchema.m4)

build: $(ERRORCODES_GEN) $(ERRORSTRINGS_GEN) $(APIGLUE_GEN) $(SPIGLUE_GEN) $(SCHEMA_GEN) $(KEYSCHEMA_GEN)

debug: build

profile: build

install: build

installhdrs: $(SPIGLUE_GEN)

installsrc:

clean:
	rm -f $(ERRORCODES_GEN) $(ERRORSTRINGS_GEN) $(APIGLUE_GEN) $(SPIGLUE_GEN) $(SCHEMA_GEN) $(KEYSCHEMA_GEN)

.PHONY: build clean debug profile

$(ERRORCODES_GEN): $(ERRORCODES_DEPENDS)
	mkdir -p $(GEN_SOURCE_DIR)
	$(PERL) $(CDSA_UTILITIES_DIR)/generator.pl $(CDSA_HEADERS_DIR) $(GEN_SOURCE_DIR) \
		$(KEYCHAIN_HEADERS_DIR)/SecBase.h $(AUTH_HEADERS_DIR)/Authorization.h

$(ERRORSTRINGS_GEN): $(ERRORSTRINGS_DEPENDS)
	mkdir -p $(GEN_SOURCE_DIR)
	mkdir -p $(GEN_SOURCE_ENGLISH_DIR)
	$(PERL) $(KEYCHAIN_HEADERS_DIR)/generateErrStrings.pl $(CDSA_HEADERS_DIR) $(GEN_SOURCE_ENGLISH_DIR) \
		$(KEYCHAIN_HEADERS_DIR)/SecBase.h $(KEYCHAIN_HEADERS_DIR)/SecKeychainAPIPriv.h \
        $(AUTH_HEADERS_DIR)/Authorization.h $(SECTRANSPORT_HEADERS_DIR)/SecureTransport.h

$(APIGLUE_GEN): $(APIGLUE_DEPENDS)
	mkdir -p $(GEN_SOURCE_DIR)
	  $(PERL) $(CDSA_SOURCES_DIR)/generator.pl $(CDSA_HEADERS_DIR) $(CDSA_SOURCES_DIR)/generator.cfg $(GEN_SOURCE_DIR)

$(SPIGLUE_GEN): $(SPIGLUE_DEPENDS)
	mkdir -p $(GEN_HEADER_DIR)
	mkdir -p $(GEN_SOURCE_DIR)
	  $(PERL) $(CDSA_PLUGINLIB_DIR)/generator.pl $(CDSA_HEADERS_DIR) $(CDSA_PLUGINLIB_DIR)/generator.cfg $(GEN_HEADER_DIR) $(GEN_SOURCE_DIR)

$(SCHEMA_GEN): $(SCHEMA_DEPENDS)
	mkdir -p $(GEN_SOURCE_DIR)
	m4 $(SCHEMA_DEPENDS) > $(SCHEMA_GEN)

$(KEYSCHEMA_GEN): $(KEYSCHEMA_DEPENDS)
	mkdir -p $(GEN_SOURCE_DIR)
	m4 $(KEYSCHEMA_DEPENDS) > $(KEYSCHEMA_GEN)
