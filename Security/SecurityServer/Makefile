#
#	Makefile to build MIG-generated sources and headers
#
XSRCROOT:=$(shell cd $(SRCROOT) >/dev/null; pwd)
TARGET:=$(shell cd $(BUILT_PRODUCTS_DIR) >/dev/null; pwd)
SRC:=$(TARGET)/derived_src
HDR:=$(TARGET)/include

build:	$(SRC)/.mig.ucsp $(SRC)/.mig.secagent

debug: build

profile: build

install: build

installhdrs: build

installsrc:

clean:
	rm -f $(SRC)/.mig.ucsp $(SRC)/.mig.secagent \
		$(SRC)/ucsp*.cpp $(SRC)/secagent*.cpp $(HDR)/ucsp.h $(HDR)/secagent.h

$(SRC)/.mig.ucsp: SecurityServer/ucsp.defs SecurityServer/ucspNotify.defs SecurityServer/ucsp_types.h
	mkdir -p $(SRC)
	mkdir -p $(HDR)
	cd /tmp; mig -server $(SRC)/ucspServer.cpp -user $(SRC)/ucspUser.cpp \
		-header $(HDR)/ucsp.h $(XSRCROOT)/SecurityServer/ucsp.defs
	cd /tmp; mig -server $(SRC)/ucspNotifyReceiver.cpp -user $(SRC)/ucspNotifySender.cpp \
		-header $(HDR)/ucspNotify.h $(XSRCROOT)/SecurityServer/ucspNotify.defs
	touch $(SRC)/.mig.ucsp

$(SRC)/.mig.secagent: SecurityServer/secagent.defs SecurityServer/secagent_types.h
	mkdir -p $(SRC)
	mkdir -p $(HDR)
	cd /tmp; mig -server $(SRC)/secagentServer.cpp -user $(SRC)/secagentUser.cpp \
		-header $(HDR)/secagent.h $(XSRCROOT)/SecurityServer/secagent.defs
	touch $(SRC)/.mig.secagent
