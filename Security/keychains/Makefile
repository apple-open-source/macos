#
#	Makefile to install built-in roots and certificates
#
KEYCHAINS_SRC=$(SRCROOT)/keychains

SYSTEM_LIBRARY_DIR=$(DSTROOT)/System/Library
KEYCHAINS_DIR=$(SYSTEM_LIBRARY_DIR)/Keychains
ANCHORS_DIR=$(KEYCHAINS_DIR)/Anchors
CERTIFICATES_DIR=$(KEYCHAINS_DIR)/Certificates

#
# world-writable directory we need to create for CRL cache
#
CRL_CACHE_DIR=$(DSTROOT)/private/var/db/crls

#
# The other phases do nothing
#
build:	
	@echo null build.

debug:
	@echo null debug.

profile:
	@echo null profile.

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
	for d in "$(KEYCHAINS_DIR)" "$(ANCHORS_DIR)" "$(CERTIFICATES_DIR)"; do \
	    if [ ! -d "$${d}" ]; then \
		    mkdir -p "$${d}"; \
		    chown root:admin "$${d}"; \
		    chmod 755 "$${d}"; \
	    fi; \
	done; \
	find "$(KEYCHAINS_SRC)/roots" -maxdepth 1 -a -type f -exec cp {} "$(ANCHORS_DIR)" \; ; \
	find "$(KEYCHAINS_SRC)/certs" -maxdepth 1 -a -type f -exec cp {} "$(CERTIFICATES_DIR)" \; ; \
	chown root:admin "$(ANCHORS_DIR)/"*; \
	chmod 664 "$(ANCHORS_DIR)/"*; \
	chown root:admin "$(CERTIFICATES_DIR)/"*; \
	chmod 664 "$(CERTIFICATES_DIR)/"*; \
	if [ ! -d $(CRL_CACHE_DIR) ]; then \
		mkdir -p $(CRL_CACHE_DIR); \
		chown root:wheel $(CRL_CACHE_DIR); \
		chmod 777 $(CRL_CACHE_DIR); \
	fi
