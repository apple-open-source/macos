export SHELL := /bin/sh

##############################################################################
# Targets
##############################################################################

.PHONY: install
install:
	@if [ $(DSTROOT) ]; then \
	    installPath="$(DSTROOT)/usr/local/cbl/etc"; \
	    files="cbl_log4perl.conf"; \
		mkdir -p -m 0755 $$installPath; \
		install -m 0644 $$files $$installPath; \
	fi
