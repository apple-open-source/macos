##---------------------------------------------------------------------
# GNUmakefile for tcl (to build partially parallel)
##---------------------------------------------------------------------

DSTROOT ?= /tmp/tcl/Release
OBJROOT ?= /tmp/tcl/Objects
SYMROOT ?= /tmp/tcl/Symbols
export DSTROOT OBJROOT SYMROOT
PARTS = 1 2 3
TESTOK := -f $(shell echo $(foreach p,$(PARTS),"$(OBJROOT)/.ok$(p)") | sed 's/ / -a -f /g')

install::
	mkdir -p "$(DSTROOT)" "$(SYMROOT)"
	@set -x && \
	for p in $(PARTS); do \
	    mkdir -p "$(OBJROOT)/DSTROOT$$p" && \
	    mkdir -p "$(OBJROOT)/OBJROOT$$p" || exit 1; \
	    (echo "######## Building part $$p:" `date` '########' > "$(SYMROOT)/LOG$$p" 2>&1 && \
		$(MAKE) -f Makefile install$$p \
		DSTROOT="$(OBJROOT)/DSTROOT$$p" \
		OBJROOT="$(OBJROOT)/OBJROOT$$p" \
		>> "$(SYMROOT)/LOG$$p" 2>&1 && \
		touch "$(OBJROOT)/.ok$$p" && \
		echo "######## Finished part $$p:" `date` '########' >> "$(SYMROOT)/LOG$$p" 2>&1 \
	    ) & \
	done && \
	wait && \
	for p in $(PARTS); do \
	    cat "$(SYMROOT)/LOG$$p" && \
	    rm -f "$(SYMROOT)/LOG$$p" || exit 1; \
	done && \
	if [ $(TESTOK) ]; then \
	    $(MAKE) merge; \
	else \
	    echo '#### error detected, not merging'; \
	    exit 1; \
	fi

merge:
	@set -x && \
	for p in $(PARTS); do \
	    ditto "$(OBJROOT)/DSTROOT$$p" "$(DSTROOT)" || exit 1; \
	done

.DEFAULT:
	@$(MAKE) -f Makefile $@
