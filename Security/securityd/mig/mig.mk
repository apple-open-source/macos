#
#  Copyright (c) 2003-2004,2014 Apple Inc. All Rights Reserved.
#
#  @APPLE_LICENSE_HEADER_START@
#  
#  This file contains Original Code and/or Modifications of Original Code
#  as defined in and that are subject to the Apple Public Source License
#  Version 2.0 (the 'License'). You may not use this file except in
#  compliance with the License. Please obtain a copy of the License at
#  http://www.opensource.apple.com/apsl/ and read it before using this
#  file.
#  
#  The Original Code and all software distributed under the License are
#  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
#  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
#  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
#  Please see the License for the specific language governing rights and
#  limitations under the License.
#  
#  @APPLE_LICENSE_HEADER_END@
#
#  Makefile to build MIG-generated sources and headers
#
DERIVED_SRC = $(BUILT_PRODUCTS_DIR)/derived_src

HDRS = $(DERIVED_SRC)/self.h
SRCS = $(DERIVED_SRC)/selfServer.cpp $(DERIVED_SRC)/selfUser.cpp
SDKROOT := $(shell xcrun --show-sdk-path --sdk macosx.internal)

build: $(HDRS) $(SRCS)

install: build

installhdrs: $(HDRS)

installsrc:

clean:
	rm -f $(HDRS) $(SRCS)

$(DERIVED_SRC)/self.h $(DERIVED_SRC)/selfServer.cpp $(DERIVED_SRC)/selfUser.cpp: $(SRCROOT)/mig/self.defs
	mkdir -p $(DERIVED_SRC)
	xcrun mig -isysroot "$(SDKROOT)" \
		-server $(DERIVED_SRC)/selfServer.cpp \
		-user $(DERIVED_SRC)/selfUser.cpp \
		-header $(DERIVED_SRC)/self.h $(SRCROOT)/mig/self.defs
