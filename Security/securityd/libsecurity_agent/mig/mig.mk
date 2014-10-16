#
#	Makefile to build MIG-generated sources and headers
#
DERIVED_SRC = $(BUILT_PRODUCTS_DIR)/derived_src/security_agent_client

HDRS = $(DERIVED_SRC)/sa_reply.h \
	$(DERIVED_SRC)/sa_request.h
	
SRCS = $(DERIVED_SRC)/sa_reply_server.cpp $(DERIVED_SRC)/sa_reply_user.c \
    $(DERIVED_SRC)/sa_request_server.c $(DERIVED_SRC)/sa_request_user.cpp
SDKROOT := $(shell xcrun --show-sdk-path --sdk macosx.internal)

build: $(HDRS) $(SRCS)

install: build

installhdrs: $(HDRS)

installsrc:

clean:
	rm -f $(HDRS) $(SRCS)

$(DERIVED_SRC)/sa_request_server.c $(DERIVED_SRC)/sa_request_user.cpp $(DERIVED_SRC)/sa_request.h: $(PROJECT_DIR)/mig/sa_request.defs $(PROJECT_DIR)/lib/sa_types.h
	mkdir -p $(DERIVED_SRC)
	ln -sF $(DERIVED_SRC) $(BUILT_PRODUCTS_DIR)/derived_src/security_agent_server
	xcrun mig -isysroot "$(SDKROOT)" \
		-server $(DERIVED_SRC)/sa_request_server.c \
		-user $(DERIVED_SRC)/sa_request_user.cpp \
		-header $(DERIVED_SRC)/sa_request.h $(PROJECT_DIR)/mig/sa_request.defs

$(DERIVED_SRC)/sa_reply_server.cpp $(DERIVED_SRC)/sa_reply_user.c $(DERIVED_SRC)/sa_reply.h: $(PROJECT_DIR)/mig/sa_reply.defs $(PROJECT_DIR)/lib/sa_types.h
	mkdir -p $(DERIVED_SRC)
	xcrun mig  -isysroot "$(SDKROOT)" \
		-server $(DERIVED_SRC)/sa_reply_server.cpp \
		-user $(DERIVED_SRC)/sa_reply_user.c \
		-header $(DERIVED_SRC)/sa_reply.h $(PROJECT_DIR)/mig/sa_reply.defs


