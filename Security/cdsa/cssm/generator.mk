# Makefile for generated files.

PERL=/usr/bin/perl

CDSA_HEADERS_DIR = Headers/cdsa
CDSA_SOURCES_DIR = Sources/cdsa

GEN_APIGLUE = $(CDSA_SOURCES_DIR)/generator.pl
APIGLUE_GEN = $(patsubst %,$(CDSA_SOURCES_DIR)/%,transition.gen funcnames.gen generator.rpt)
APIGLUE_DEPENDS = $(patsubst %,$(CDSA_SOURCES_DIR)/%, generator.pl generator.cfg)\
				  $(patsubst %,$(CDSA_HEADERS_DIR)/%,cssmapi.h cssmtype.h cssmconfig.h cssmaci.h cssmcspi.h cssmdli.h cssmcli.h cssmtpi.h)

build: $(APIGLUE_GEN)

clean:
	rm -f $(APIGLUE_GEN)

debug: build

profile: build

.PHONY: build clean debug profile

$(APIGLUE_GEN): $(APIGLUE_DEPENDS)
	(cd $(CDSA_SOURCES_DIR);\
	  $(PERL) ./generator.pl ../../$(CDSA_HEADERS_DIR) .)
