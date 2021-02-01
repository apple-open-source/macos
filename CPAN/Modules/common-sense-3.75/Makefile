NAME = common-sense
VERSION = 3.75

include ../Makefile.inc

emptyfiles = /usr/share/man/man3/common::sense.3pm

postinstall:
	@set -x && \
	for e in $(emptyfiles); do \
	    if [ ! -s $(DSTROOT)$$e ]; then \
		rm -f $(DSTROOT)$$e || exit 1; \
	    fi \
	done
