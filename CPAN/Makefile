# Makefile orchestrating CPAN

include $(VERS).inc

# These variables cause installation into the Extras directory, adds RC_CFLAGS
# to the compile and linking arguments, and sets DESTDIR to DSTROOT
installarchlib := $(shell perl -MConfig -e 'print $$Config::Config{installextrasarch}')
installbin := $(shell perl -MConfig -e 'print $$Config::Config{installbin}')
installprivlib := $(shell perl -MConfig -e 'print $$Config::Config{installextraslib}')
EXTRASARCH := $(DSTROOT)/$(shell perl -MConfig -e 'print $$Config::Config{extrasarch}')
EXTRASLIB := $(DSTROOT)/$(shell perl -MConfig -e 'print $$Config::Config{extraslib}')
PLARGS := INSTALLDIRS=perl INSTALLARCHLIB='$(installarchlib)' INSTALLPRIVLIB='$(installprivlib)' INSTALLBIN='$(installbin)' INSTALLSCRIPT='$(installbin)'
PLBARGS := --installdirs core --install_path arch='$(installarchlib)' --install_path lib='$(installprivlib)' --install_path bin='$(installbin)' --install_path script='$(installbin)'
make := $(SRCROOT)/make.pl
MAKEARGS := DESTDIR=$(DSTROOT)
BUILDARGS := --destdir $(DSTROOT)
export PERL5LIB := $(EXTRASLIB)
export NO_PERL_PREPENDTOPATH := 1

no_target:
	@set -x && for i in $(PROJECTS); do \
	    echo "===== $$i =====" && \
	    $(make) -C Modules/$$i unpack installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		PLBARGS="$(PLBARGS)" BUILDARGS="$(BUILDARGS)"\
		MAKEARGS="$(MAKEARGS)" && \
	    $(make) -C Modules/$$i installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		PLBARGS="$(PLBARGS)" BUILDARGS="$(BUILDARGS)"\
		MAKEARGS="$(MAKEARGS)" || exit 1; \
	done

install:
	@set -x && for i in $(PROJECTS); do \
	    echo "===== $$i =====" && \
	    $(make) -C Modules/$$i unpack installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		PLBARGS="$(PLBARGS)" BUILDARGS="$(BUILDARGS)"\
		MAKEARGS="$(MAKEARGS)" && \
	    $(make) -C Modules/$$i install installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		PLBARGS="$(PLBARGS)" BUILDARGS="$(BUILDARGS)"\
		MAKEARGS="$(MAKEARGS)" || exit 1; \
	done
	@echo ================ post-install fixups ================
	@set -x && \
	cd $(EXTRASARCH)/auto && \
	for b in `find . -name \*.bundle | sed 's,^\./,,'`; do \
	    rsync -R $$b $(SYMROOT) && \
	    dsymutil $(SYMROOT)/$$b && \
	    strip -x $$b || exit 1; \
	done
	@set -x && \
	cd $(DSTROOT) && \
	for b in usr/bin/*; do \
	    if file $$b | fgrep -q Mach-O; then \
		rsync -R $$b $(SYMROOT) && \
		dsymutil $(SYMROOT)/$$b && \
		strip -x $$b || exit 1; \
	    fi \
	done
	rm -f $(EXTRASARCH)/perllocal.pod
	find $(EXTRASARCH)/auto -name .packlist -print -delete
