#
#	Makefile to build MIG-generated sources and headers
#
DERIVED_SRC = $(BUILT_PRODUCTS_DIR)/derived_src

HDRS = $(DERIVED_SRC)/sa_reply.h \
	$(DERIVED_SRC)/sa_request.h
	
SRCS = $(DERIVED_SRC)/sa_reply_server.cpp $(DERIVED_SRC)/sa_reply_user.c \
    $(DERIVED_SRC)/sa_request_server.c $(DERIVED_SRC)/sa_request_user.cpp

build: $(HDRS) $(SRCS)

install: build

installhdrs: $(HDRS)

installsrc:

clean:
	rm -f $(HDRS) $(SRCS)

$(DERIVED_SRC)/sa_request_server.c $(DERIVED_SRC)/sa_request_user.cpp $(DERIVED_SRC)/sa_request.h: $(SRCROOT)/mig/sa_request.defs $(SRCROOT)/lib/sa_types.h
	mkdir -p $(DERIVED_SRC)
	mig -server $(DERIVED_SRC)/sa_request_server.c -user $(DERIVED_SRC)/sa_request_user.cpp \
		-header $(DERIVED_SRC)/sa_request.h $(SRCROOT)/mig/sa_request.defs

$(DERIVED_SRC)/sa_reply_server.cpp $(DERIVED_SRC)/sa_reply_user.c $(DERIVED_SRC)/sa_reply.h: $(SRCROOT)/mig/sa_reply.defs $(SRCROOT)/lib/sa_types.h
	mkdir -p $(DERIVED_SRC)
	mig -server $(DERIVED_SRC)/sa_reply_server.cpp -user $(DERIVED_SRC)/sa_reply_user.c \
		-header $(DERIVED_SRC)/sa_reply.h $(SRCROOT)/mig/sa_reply.defs


