# Makefile for wxPerl

PROJECT = wxPerl
APP = $(APPDIR)/$(PROJECT).app
APPBUILD = $(PROJECT).app-build
APPDIR = $(DSTROOT)/System/Library/Perl/Extras/Applications
APPTARBALL = $(APPBUILD).tar.gz
installarchlib := $(subst Perl,Perl/Extras,$(shell perl -MConfig -e 'print $$Config::Config{installarchlib}'))
installprivlib := $(subst Perl,Perl/Extras,$(shell perl -MConfig -e 'print $$Config::Config{installprivlib}'))
MAKEARGS := DESTDIR='$(DSTROOT)' PASTHRU_INC='$(RC_CFLAGS)' OTHERLDFLAGS='$(RC_CFLAGS)'
ENV := PATH=$(FAKEBIN):$(PATH)

no_target: build

build: $(PROJECT)/Makefile
	@echo cd $(PROJECT) && \
	cd $(PROJECT) && \
	echo $(ENV) make && \
	$(ENV) make

$(PROJECT)/Makefile:
	@echo cd $(PROJECT) && \
	cd $(PROJECT) && \
	echo $(ENV) perl Makefile.PL INSTALLDIRS=perl \
	    INSTALLARCHLIB-$(installarchlib) \
	    INSTALLPRIVLIB=$(installprivlib) && \
	$(ENV) perl Makefile.PL INSTALLDIRS=perl \
	    INSTALLARCHLIB-$(installarchlib) \
	    INSTALLPRIVLIB=$(installprivlib)

install: $(PROJECT)/Makefile wxPerl-install wxPerl.app-install

wxPerl-install:
	@echo cd $(PROJECT) && \
	cd $(PROJECT) && \
	echo $(ENV) make install $(MAKEARGS) && \
	$(ENV) make install $(MAKEARGS)
	@for i in `find $(DSTROOT)$(installprivlib) -name \*.bundle`; do \
	    echo strip -x $$i && \
	    strip -x $$i; \
	done
	rm -f $(DSTROOT)$(installarchlib)/perllocal.pod
	find $(DSTROOT)$(installprivlib) -name .packlist -delete -print

wxPerl.app-install:
	mkdir -p $(APPDIR)
	gnutar xzf $(APPTARBALL)
	(echo '#include <Carbon.r>' && cat $(DSTROOT)/usr/lib/*.r) > $(APPBUILD)/$(PROJECT).r
	@echo cd $(APPBUILD) && \
	cd $(APPBUILD) && \
	echo xcodebuild install DSTROOT=$(DSTROOT) && \
	xcodebuild install DSTROOT=$(DSTROOT)
	rm -rf $(APP)/Contents/MacOS/wxPerl $(APP)/Contents/Resources/English.lproj/main.nib $(DSTROOT)/usr/bin/$(PROJECT) $(DSTROOT)/usr/bin/._wxPerl
	cp -p /usr/bin/perl $(APP)/Contents/MacOS/wxPerl
	cc $(RC_CFLAGS) -Os -Wl,-x $(PROJECT).c -o $(DSTROOT)/usr/bin/$(PROJECT)
