#
#	Makefile to install world-readable directories for trust store files
#
KEYCHAINS_DIR=$(DSTROOT)/private/var/Keychains
CRL_CACHE_DIR=$(DSTROOT)/private/var/db/crls

SYSTEM_LIBRARY_DIR=$(DSTROOT)/System/Library

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
	if [ ! -d $(KEYCHAINS_DIR) ]; then \
		mkdir -p $(KEYCHAINS_DIR); \
		chown root:wheel $(KEYCHAINS_DIR); \
		chmod 755 $(KEYCHAINS_DIR); \
	fi
