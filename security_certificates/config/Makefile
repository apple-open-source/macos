#
#	Makefile to install world-readable directories for trust store files
#
SECURITY_DIR=$(DSTROOT)/System/Library/Security
CRL_CACHE_DIR=$(DSTROOT)/private/var/db/crls
PUBLIC_TRUSTD_DIR=$(DSTROOT)/private/var/protected/trustd
PRIVATE_TRUSTD_DIR=$(DSTROOT)/private/var/protected/trustd/private

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
# Note: PRIVATE_TRUSTD_DIR is intentionally installed world-writeable so any
# user instance of trustd may write to it. This is only permissible because
# access to this directory is restricted by entitlement.
#
install:
	if [ ! -d $(CRL_CACHE_DIR) ]; then \
		mkdir -p $(CRL_CACHE_DIR); \
		chown root:wheel $(CRL_CACHE_DIR); \
		chmod 755 $(CRL_CACHE_DIR); \
	fi
	if [ ! -d $(PUBLIC_TRUSTD_DIR) ]; then \
		mkdir -p $(PUBLIC_TRUSTD_DIR); \
		chown root:wheel $(PUBLIC_TRUSTD_DIR); \
		chmod 755 $(PUBLIC_TRUSTD_DIR); \
	fi
	if [ ! -d $(PRIVATE_TRUSTD_DIR) ]; then \
		mkdir -p $(PRIVATE_TRUSTD_DIR); \
		chown root:wheel $(PRIVATE_TRUSTD_DIR); \
		chmod 777 $(PRIVATE_TRUSTD_DIR); \
	fi
	if [ ! -d $(SECURITY_DIR) ]; then \
		mkdir -p $(SECURITY_DIR); \
		chown root:wheel $(SECURITY_DIR); \
		chmod 755 $(SECURITY_DIR); \
	fi
