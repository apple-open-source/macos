#
#	Makefile to install world-readable directories for trust store files
#
SECURITY_DIR=$(DSTROOT)/System/Library/Security

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
	if [ ! -d $(SECURITY_DIR) ]; then \
		mkdir -p $(SECURITY_DIR); \
		chown root:wheel $(SECURITY_DIR); \
		chmod 755 $(SECURITY_DIR); \
	fi
