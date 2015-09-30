#
#	Makefile to install world-readable directories for trust store files
#
SECURITY_DIR=$(DSTROOT)/System/Library/Security
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
	if [ ! -d $(SECURITY_DIR) ]; then \
		mkdir -p $(SECURITY_DIR); \
		chown root:wheel $(SECURITY_DIR); \
		chmod 755 $(SECURITY_DIR); \
	fi
