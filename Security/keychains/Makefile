#
#	Makefile to install the system-startup code for SecurityServer
#
KEYCHAINS_SRC=$(SRCROOT)/keychains

SYSTEM_LIBRARY_DIR=$(DSTROOT)/System/Library
KEYCHAINS_DIR=$(SYSTEM_LIBRARY_DIR)/Keychains
X509ANCHORS=$(KEYCHAINS_DIR)/X509Anchors


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
	cp $(KEYCHAINS_SRC)/X509Anchors $(X509ANCHORS)
	chown root.admin $(X509ANCHORS)
	chmod 664 $(X509ANCHORS)
	ls -l $(X509ANCHORS)
