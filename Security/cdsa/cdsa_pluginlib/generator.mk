# Makefile for generated files.

PERL=/usr/bin/perl

CDSA_HEADERS_DIR = Headers/cdsa
SECURITY_HEADERS_DIR = Headers/Security
CDSA_PLUGINLIB_DIR = Sources/cdsa_pluginlib

GEN_SPIGLUE = $(CDSA_PLUGINLIB_DIR)/generator.pl
SPIGLUE_GEN = $(patsubst %,$(SECURITY_HEADERS_DIR)/%,ACabstractsession.h CLabstractsession.h CSPabstractsession.h DLabstractsession.h TPabstractsession.h)\
			  $(patsubst %,$(CDSA_PLUGINLIB_DIR)/%,ACabstractsession.cpp CLabstractsession.cpp CSPabstractsession.cpp DLabstractsession.cpp TPabstractsession.cpp)
SPIGLUE_DEPENDS = $(patsubst %,$(CDSA_PLUGINLIB_DIR)/%,generator.pl generator.cfg)\
				  $(patsubst %,$(CDSA_HEADERS_DIR)/%,cssmapi.h cssmtype.h cssmconfig.h cssmaci.h cssmcli.h cssmcspi.h cssmdli.h cssmspi.h)

build: $(SPIGLUE_GEN)

clean:
	rm -f $(SPIGLUE_GEN)

debug: build

profile: build

.PHONY: build clean debug profile

$(SPIGLUE_GEN): $(SPIGLUE_DEPENDS)
	(cd $(CDSA_PLUGINLIB_DIR);\
	  $(PERL) ./generator.pl ../../$(CDSA_HEADERS_DIR) ../../$(SECURITY_HEADERS_DIR) .)
