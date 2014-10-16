#
# Top-level Makefile for NISCC/TLS_SSL.  Not compatible with Jasper tools or
# PB project files or Makefiles. Doesn't use SYMROOT, DSTROOT, etc., though
# it does require the LOCAL_BUILD_DIR env var to be set.
#
SHELL := /bin/zsh

SUBDIRS= certDecode nisccSimpleClient skipThisNisccCert nisccCertVerify

# default SRCROOT for clean target
SRCROOT ?= $(shell pwd)

first:
	@foreach i in $(SUBDIRS); \
		echo "=== Making $$i ==="; \
		cd $$i || exit 1; \
		make || exit 1; \
		cd ..; \
	end

build: first

sphinx:
	make "SPHINX=-DSPHINX"

install:	first

# this module doesn't need to install any headers
installhdrs:

installsrc:

clean:
	@foreach i in $(SUBDIRS); \
		echo "=== Cleaning $$i ==="; \
		cd $$i || exit 1; \
		make clean || exit 1; \
		cd ..; \
	end
