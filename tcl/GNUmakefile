# This GNUmakefile is a hack to workaround a B&I submission problem.  Submitting
# symlinks are failing on the submission server.  So instead, we submit the
# unmodified sources and run installsrc at build time.

SRCROOT = /tmp

installsrc:
	mkdir -p $(SRCROOT)
	pax -rw . $(SRCROOT)

installhdrs:
	@echo No headers to install

clean:
	@echo Nothing to clean

SYMROOT = /tmp
NEWSRCROOT = $(SYMROOT)/SRC

.DEFAULT:
	@if [ ! -d "$(NEWSRCROOT)" ]; then \
	    echo mkdir -p "$(NEWSRCROOT)" && \
	    mkdir -p "$(NEWSRCROOT)" && \
	    echo $(MAKE) -f Makefile installsrc SRCROOT="$(NEWSRCROOT)" && \
	    $(MAKE) -f Makefile installsrc SRCROOT="$(NEWSRCROOT)"; \
	fi
	$(MAKE) -C "$(NEWSRCROOT)" -f Makefile $@ SRCROOT="$(NEWSRCROOT)"
