#
#  Copyright (c) 2003-2004,2006-2007,2011,2014 Apple Inc. All Rights Reserved.
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
DERIVED_SRC = $(BUILT_PRODUCTS_DIR)/derived_src/securityd_client

HDRS = $(DERIVED_SRC)/ucsp.h $(DERIVED_SRC)/ucspNotify.h $(DERIVED_SRC)/cshosting.h
SRCS =	$(DERIVED_SRC)/ucspServer.cpp $(DERIVED_SRC)/ucspClient.cpp \
		$(DERIVED_SRC)/ucspClientC.c \
		$(DERIVED_SRC)/ucspNotifyReceiver.cpp $(DERIVED_SRC)/ucspNotifySender.cpp \
		$(DERIVED_SRC)/cshostingServer.cpp $(DERIVED_SRC)/cshostingClient.cpp
INCLUDES = $(PROJECT_DIR)/mig/ss_types.defs
SDKROOT := $(shell xcrun --show-sdk-path --sdk macosx.internal)
build: $(HDRS) $(SRCS)

install: build

installhdrs: $(HDRS)

installsrc:

clean:
	rm -f $(HDRS) $(SRCS)

$(DERIVED_SRC)/ucsp.h $(DERIVED_SRC)/ucspServer.cpp $(DERIVED_SRC)/ucspClient.cpp: $(PROJECT_DIR)/mig/ucsp.defs $(INCLUDES)
	mkdir -p $(DERIVED_SRC)
	xcrun mig -isysroot "$(SDKROOT)" \
        -server $(DERIVED_SRC)/ucspServer.cpp \
        -user $(DERIVED_SRC)/ucspClient.cpp \
		-header $(DERIVED_SRC)/ucsp.h $(PROJECT_DIR)/mig/ucsp.defs
		
$(DERIVED_SRC)/ucspClientC.c: $(DERIVED_SRC)/ucspClient.cpp
	cp $(DERIVED_SRC)/ucspClient.cpp $(DERIVED_SRC)/ucspClientC.c

$(DERIVED_SRC)/ucspNotify.h $(DERIVED_SRC)/ucspNotifyReceiver.cpp $(DERIVED_SRC)/ucspNotifySender.cpp: $(PROJECT_DIR)/mig/ucspNotify.defs $(INCLUDES)
	mkdir -p $(DERIVED_SRC)
	xcrun mig -isysroot "$(SDKROOT)" \
        -server $(DERIVED_SRC)/ucspNotifyReceiver.cpp \
        -user $(DERIVED_SRC)/ucspNotifySender.cpp \
		-header $(DERIVED_SRC)/ucspNotify.h $(PROJECT_DIR)/mig/ucspNotify.defs

$(DERIVED_SRC)/cshosting.h $(DERIVED_SRC)/cshostingServer.cpp $(DERIVED_SRC)/cshostingClient.cpp: $(PROJECT_DIR)/mig/cshosting.defs $(INCLUDES)
	mkdir -p $(DERIVED_SRC)
	xcrun mig -isysroot "$(SDKROOT)" \
        -server $(DERIVED_SRC)/cshostingServer.cpp \
        -user $(DERIVED_SRC)/cshostingClient.cpp \
		-header $(DERIVED_SRC)/cshosting.h $(PROJECT_DIR)/mig/cshosting.defs
