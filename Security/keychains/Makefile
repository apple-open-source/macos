#
#	Makefile to install built-in roots and certificates
#
KEYCHAINS_SRC=$(SRCROOT)/keychains

SYSTEM_LIBRARY_DIR=$(DSTROOT)/System/Library
KEYCHAINS_DIR=$(SYSTEM_LIBRARY_DIR)/Keychains
X509KEYCHAINS=X509Anchors X509Certificates

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
	if [ ! -d $(KEYCHAINS_DIR) ]; then \
		mkdir -p $(KEYCHAINS_DIR); \
		chown root.admin $(KEYCHAINS_DIR); \
		chmod 755 $(KEYCHAINS_DIR); \
	fi
	cd $(KEYCHAINS_SRC); cp $(X509KEYCHAINS) $(KEYCHAINS_DIR)
	cd $(KEYCHAINS_DIR); \
	chown root.admin $(X509KEYCHAINS); \
	chmod 664 $(X509KEYCHAINS); \
	ls -l $(X509KEYCHAINS)
	if [ ! -d $(CRL_CACHE_DIR) ]; then \
		mkdir -p $(CRL_CACHE_DIR); \
		chown root.wheel $(CRL_CACHE_DIR); \
		chmod 777 $(CRL_CACHE_DIR); \
	fi
