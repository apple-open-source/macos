# Makefile for generated files.

PERL=/usr/bin/perl

CDSA_HEADERS_DIR = Headers/cdsa
CDSA_UTILITIES_DIR = Sources/cdsa_utilities

GEN_ERRORCODES = $(CDSA_UTILITIES_DIR)/generator.pl
ERRORCODES_GEN = $(patsubst %,$(CDSA_UTILITIES_DIR)/%,errorcodes.gen)
ERRORCODES_DEPENDS = $(GEN_ERRORCODES)\
					 $(patsubst %,$(CDSA_HEADERS_DIR)/%,cssmerr.h)

build: $(ERRORCODES_GEN)

clean:
	rm -f $(ERRORCODES_GEN)

debug: build

profile: build

.PHONY: build clean debug profile

$(ERRORCODES_GEN): $(ERRORCODE_DEPENDS)
	(cd $(CDSA_UTILITIES_DIR);\
	  $(PERL) ./generator.pl ../../$(CDSA_HEADERS_DIR) .)
