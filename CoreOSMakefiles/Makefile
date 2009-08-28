Project=CoreOSMakefiles

Destination = $(MAKEFILEPATH)/CoreOS
BSDMAKEDIR  = /usr/share/mk/CoreOS
BSDSUFFIX   = .mk
BSDDEFINE   = BSDMAKESTYLE
GNUSUFFIX   = .make
INBASENAMES = Commands Variables
INSUFFIX    = .in
STANDARD    = Standard

install:
	@$(MAKE) installsrc SRCROOT=$(DSTROOT)$(Destination)
	rm -f $(DSTROOT)$(Destination)/Makefile
	mv -f $(DSTROOT)$(Destination)/bin $(DSTROOT)$(MAKEFILEPATH)
	install -d $(DSTROOT)$(BSDMAKEDIR)/$(STANDARD)
	@set -x && \
	    for i in $(INBASENAMES); do \
		unifdef -U$(BSDDEFINE) -t $(DSTROOT)$(Destination)/$(STANDARD)/$$i$(INSUFFIX) > $(DSTROOT)$(Destination)/$(STANDARD)/$$i$(GNUSUFFIX); \
		[ $$? -eq 1 ] || exit 1; \
		unifdef -D$(BSDDEFINE) -t $(DSTROOT)$(Destination)/$(STANDARD)/$$i$(INSUFFIX) > $(DSTROOT)$(BSDMAKEDIR)/$(STANDARD)/$$i$(BSDSUFFIX); \
		[ $$? -eq 1 ] || exit 1; \
		$(RM) -f $(DSTROOT)$(Destination)/$(STANDARD)/$$i$(INSUFFIX) || exit 1; \
	    done

installhdrs:
	$(_v) echo No headers to install

installsrc:
	install -d "$(SRCROOT)"
	rsync -a --exclude=.svn ./ "$(SRCROOT)"

clean:
	$(_v) echo Nothing to clean
