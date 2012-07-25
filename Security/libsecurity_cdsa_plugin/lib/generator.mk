# Makefile for generated files.

SOURCES = $(BUILT_PRODUCTS_DIR)/derived_src/security_cdsa_plugin
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
$(HFILES) $(CPPFILES) : $(PROJECT_DIR)/lib/generator.pl $(PROJECT_DIR)/lib/generator.cfg
	mkdir -p $(SOURCES)
	perl $(PROJECT_DIR)/lib/generator.pl $(CSSM_HEADERS) $(PROJECT_DIR)/lib/generator.cfg $(HEADERS) $(SOURCES)
