# Makefile for generated files.

PERL=/usr/bin/perl

CDSA_HEADERS_DIR = $(SRCROOT)/cdsa/cdsa
SECURITY_HEADERS_DIR = $(SRCROOT)/cdsa/cdsa_utilities
CDSA_SOURCES_DIR = $(SRCROOT)/cdsa/cssm
KEYCHAIN_SOURCES_DIR = $(SRCROOT)/Keychain
CSPDL_SOURCES_DIR = $(SRCROOT)/AppleCSPDL
CDSA_UTILITIES_DIR = $(SRCROOT)/cdsa/cdsa_utilities
CDSA_PLUGINLIB_DIR = $(SRCROOT)/cdsa/cdsa_pluginlib
GEN_SOURCE_DIR = $(SYMROOT)/derived_src
GEN_HEADER_DIR = $(SYMROOT)/include/Security

GEN_ERRORCODES = $(CDSA_UTILITIES_DIR)/generator.pl
ERRORCODES_GEN = $(patsubst %,$(GEN_SOURCE_DIR)/%,errorcodes.gen)
ERRORCODES_DEPENDS = $(GEN_ERRORCODES)\
					 $(patsubst %,$(CDSA_HEADERS_DIR)/%,cssmerr.h)\
					 $(patsubst %,$(CDSA_HEADERS_DIR)/%,cssmapple.h)

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

build: $(ERRORCODES_GEN) $(APIGLUE_GEN) $(SPIGLUE_GEN) $(SCHEMA_GEN) $(KEYSCHEMA_GEN)

debug: build

profile: build

install: build

installhdrs: $(SPIGLUE_GEN)

installsrc:

clean:
	rm -f $(ERRORCODES_GEN) $(APIGLUE_GEN) $(SPIGLUE_GEN) $(SCHEMA_GEN) $(KEYSCHEMA_GEN)

.PHONY: build clean debug profile

$(ERRORCODES_GEN): $(ERRORCODES_DEPENDS)
	mkdir -p $(GEN_SOURCE_DIR)
	(cd $(CDSA_UTILITIES_DIR);\
	  $(PERL) ./generator.pl $(CDSA_HEADERS_DIR) $(GEN_SOURCE_DIR))

$(APIGLUE_GEN): $(APIGLUE_DEPENDS)
	mkdir -p $(GEN_SOURCE_DIR)
	(cd $(CDSA_SOURCES_DIR);\
	  $(PERL) ./generator.pl $(CDSA_HEADERS_DIR) $(GEN_SOURCE_DIR))

$(SPIGLUE_GEN): $(SPIGLUE_DEPENDS)
	mkdir -p $(GEN_HEADER_DIR)
	mkdir -p $(GEN_SOURCE_DIR)
	(cd $(CDSA_PLUGINLIB_DIR);\
	  $(PERL) ./generator.pl $(CDSA_HEADERS_DIR) $(GEN_HEADER_DIR) $(GEN_SOURCE_DIR))

$(SCHEMA_GEN): $(SCHEMA_DEPENDS)
	mkdir -p $(GEN_SOURCE_DIR)
	m4 $(SCHEMA_DEPENDS) > $(SCHEMA_GEN)

$(KEYSCHEMA_GEN): $(KEYSCHEMA_DEPENDS)
	mkdir -p $(GEN_SOURCE_DIR)
	m4 $(KEYSCHEMA_DEPENDS) > $(KEYSCHEMA_GEN)
