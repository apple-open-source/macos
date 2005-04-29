#
#	Makefile to install built-in roots and certificates
#
KEYCHAINS_SRC=$(SRCROOT)/keychains

SYSTEM_LIBRARY_DIR=$(DSTROOT)/System/Library

#
# world-writable directory we need to create for CRL cache
#
CRL_CACHE_DIR=$(DSTROOT)/private/var/db/crls

.PHONY: build installhdrs installsrc clean install

build:
	@echo null build.

installhdrs:
	@echo null installhdrs.

installsrc:
	@echo null installsrc.

clean:
	@echo null clean.

#
# Install
#
install:
	if [ ! -d $(CRL_CACHE_DIR) ]; then \
		mkdir -p $(CRL_CACHE_DIR); \
		chown root:wheel $(CRL_CACHE_DIR); \
		chmod 755 $(CRL_CACHE_DIR); \
	fi
