#
#	Makefile to build MIG-generated sources and headers
#
SRC = $(SYMROOT)/derived_src
HDR = $(SYMROOT)/include

build:	$(SRC)/.mig.ucsp $(SRC)/.mig.secagent

debug: build

profile: build

install: build

installhdrs: build

installsrc:

clean:
	rm -f $(SRC)/.mig.ucsp $(SRC)/.mig.secagent \
		$(SRC)/ucsp*.cpp $(SRC)/secagent*.cpp $(HDR)/ucsp.h $(HDR)/secagent.h

$(SRC)/.mig.ucsp: SecurityServer/ucsp.defs SecurityServer/ucsp_types.h
	mkdir -p $(SRC)
	mkdir -p $(HDR)
	cd /tmp; mig -server $(SRC)/ucspServer.cpp -user $(SRC)/ucspUser.cpp \
		-header $(HDR)/ucsp.h $(SRCROOT)/SecurityServer/ucsp.defs
	touch $(SRC)/.mig.ucsp

$(SRC)/.mig.secagent: SecurityServer/secagent.defs SecurityServer/secagent_types.h
	mkdir -p $(SRC)
	mkdir -p $(HDR)
	cd /tmp; mig -server $(SRC)/secagentServer.cpp -user $(SRC)/secagentUser.cpp \
		-header $(HDR)/secagent.h $(SRCROOT)/SecurityServer/secagent.defs
	touch $(SRC)/.mig.secagent
