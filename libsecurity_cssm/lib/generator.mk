#
# Copyright (c) 2001,2004 Apple Computer, Inc. All Rights Reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
# 
# @APPLE_LICENSE_HEADER_END@
#
# generator.mk -- Makefile for generated files.

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
