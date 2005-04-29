# Makefile for generated files.

SOURCES = $(BUILT_PRODUCTS_DIR)/derived_src
HEADERS = $(SOURCES)

HFILES = $(HEADERS)/ACabstractsession.h
CPPFILES = $(SOURCES)/ACabstractsession.cpp

build: $(HFILES) $(CPPFILES)

install: build

installhdrs: $(HFILES)

installsrc:

clean:
	rm -f $(SPIGLUE_GEN)

debug: build

profile: build

.PHONY: build clean debug profile

# partial dependencies only
$(HFILES) $(CPPFILES) : lib/generator.pl lib/generator.cfg
	mkdir -p $(SOURCES)
	perl lib/generator.pl $(CSSM_HEADERS) lib/generator.cfg $(HEADERS) $(SOURCES)
