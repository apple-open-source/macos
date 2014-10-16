#
#  Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
DEFS = $(SRCROOT)/mig/tokend.defs
DERIVED_SRC = $(BUILT_PRODUCTS_DIR)/derived_src
HEADER = $(DERIVED_SRC)/tokend.h
HDRS = $(HEADER)
SERVER = $(DERIVED_SRC)/tokendServer.cpp
CLIENT = $(DERIVED_SRC)/tokendClient.cpp
SRCS = $(SERVER) $(CLIENT)
SDKROOT := $(shell xcrun --show-sdk-path --sdk macosx.internal)

build: $(HDRS) $(SRCS)

install: build

installhdrs: $(HDRS)

installsrc:

clean:
	rm -f $(HDRS) $(SRCS)

$(SRCS) $(HDRS): $(DEFS)
	mkdir -p $(DERIVED_SRC)
	xcrun mig -isysroot "$(SDKROOT)" $(MIGFLAGS) \
		-server $(SERVER) \
		-user $(CLIENT) \
		-header $(HEADER) $(DEFS)
