#
# Top-level Makefile for cspxutils. Allows build or clean
# of all directories in one swoop.  
#
SHELL := /bin/zsh

SUBDIRS= utilLib aesVect ascTool asymCompat asymPerform asymTest attachLeak \
	badattr badmac badsig ccSymCompat ccSymTest ccHmacCompat ccHmacClone \
	ccOneShot ccCtxSize ccPerform ccOpensslCompat contextReuse \
	cryptTool dbTool dbVerifyKey dhFulltest dhTest dsaPartial ecdhTest \
	hashClone hashCompat hashTest hashTime hashTimeLibCrypt hashTimeSA keyDate \
	keyHash keyHashAsym keyStore keyStoreLeak macCompat macTest mdsdump mdsLookup \
	miniWrap pbeTest perform pubKeyTool randTest rawRsaSig rawSig rsatool \
	sha2Time sha2Vectors sha2VectorsCdsa sigPerform sigtest \
	sshKey ssl2Padding symCompat symDelta symTest wrap wrapTest

INSTALLDIR=$(DSTROOT)/usr/local/bin

# default SRCROOT for clean target
SRCROOT ?= $(shell pwd)

# just for all target
BUILD_DIR = $(shell echo $(LOCAL_BUILD_DIR))
LOCALBUILD=$BUILD_DIR

first:
	@foreach i in $(SUBDIRS); \
		if [[ -a $$i ]] then; \
			echo "=== Making $$i ==="; \
			cd $$i || exit 1; \
			make -r || exit 1; \
			cd ..; \
		fi; \
	end

build: first

sphinx:
	make "SPHINX=-DSPHINX"

# architecture options

64bit:
	make "DEBUG_CFLAGS=-arch x86_64" "CMDLINE_LDFLAGS=-arch x86_64" 
	
64bitFat:
	make "DEBUG_CFLAGS=-arch x86_64 -arch i386" "CMDLINE_LDFLAGS=-arch x86_64 -arch i386" 

fat:
	make "DEBUG_CFLAGS=-arch i386" "CMDLINE_LDFLAGS=-arch i386" 

install:
	@foreach i in $(SUBDIRS); \
		echo "=== Installing $$i ==="; \
		cd $$i; \
		make install "DSTROOT=$(DSTROOT)" "SYMROOT=$(SYMROOT)" \
			"OBJROOT=$(OBJROOT)" || exit 1; \
		cd ..; \
	end
	install -d -m 0755 $(INSTALLDIR)
	install -p -m 0755 cspdvt $(INSTALLDIR)
	echo "** BUILD SUCCEEDED **"

installhdrs:
	@foreach i in $(SUBDIRS); \
		echo "=== Installing Headers for $$i ==="; \
		cd $$i; \
		make installhdrs "DSTROOT=$(DSTROOT)" "SYMROOT=$(SYMROOT)" \
			"OBJROOT=$(OBJROOT)" "SRCROOT=$(SRCROOT)/$$i" \
			|| exit 1; \
		cd ..; \
	end

installsrc:
	cp -R . $(SRCROOT)

clean:
	@foreach i in $(SUBDIRS); \
		if [[ -a $$i ]] then; \
			echo "=== Cleaning $$i ==="; \
			cd $$i || exit 1; \
			make clean "DSTROOT=$(DSTROOT)" "SYMROOT=$(SYMROOT)" \
				"OBJROOT=$(OBJROOT)" "SRCROOT=$(SRCROOT)/$$i" \
				|| exit 1; \
			cd ..; \
		fi; \
	end

all:
	@if [[ -z "$(BUILD_DIR)" ]] then; \
		echo "=== You must set the LOCAL_BUILD_DIR environment variable to make all. ==="; \
		exit 1 ; \
	fi
	make 64bitFat || exit(1)
	cd ../clxutils; make 64bitFat || exit(1)
	@echo "=== cspxutils and clxutils are ready to run. ==="
