# Makefile orchestrating CPAN

include $(VERS).inc

# These variables cause installation into the Extras directory, adds RC_CFLAGS
# to the compile and linking arguments, and sets DESTDIR to DSTROOT
PERL = $(DSTROOT)/System/Library/Perl
VERSION := $(shell perl -e 'printf "%vd", $$^V')
ARCHLIB := $(shell perl -MConfig -e 'print $$Config::Config{archname}')
EXTRAS = $(PERL)/Extras
EXTRASPERL = $(EXTRAS)/$(VERSION)
installarchlib := $(subst Perl,Perl/Extras,$(shell perl -MConfig -e 'print $$Config::Config{installarchlib}'))
installbin := $(shell perl -MConfig -e 'print $$Config::Config{installbin}')
installprivlib := $(subst Perl,Perl/Extras,$(shell perl -MConfig -e 'print $$Config::Config{installprivlib}'))
PLARGS := INSTALLDIRS=perl INSTALLARCHLIB='$(installarchlib)' INSTALLPRIVLIB='$(installprivlib)' INSTALLBIN='$(installbin)' INSTALLSCRIPT='$(installbin)'
make := $(SRCROOT)/make.pl
MAKEARGS := DESTDIR=$(DSTROOT)
SLP = $(PERL)/$(VERSION)
export PERL5LIB := $(EXTRASPERL)
export NO_PERL_PREPENDTOPATH := 1

no_target:
	@set -x && for i in $(PROJECTS); do \
	    $(make) -C Modules/$$i installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		MAKEARGS="$(MAKEARGS)" || exit 1; \
	done

install:
	@set -x && for i in $(PROJECTS); do \
	    $(make) -C Modules/$$i install installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		MAKEARGS="$(MAKEARGS)" || exit 1; \
	done
	@echo ================ post-install fixups ================
	rm -f $(EXTRASPERL)/$(ARCHLIB)/perllocal.pod
	find $(EXTRASPERL)/$(ARCHLIB)/auto -name \*.bundle -print -exec strip -x {} \;
	find $(EXTRASPERL)/$(ARCHLIB)/auto -name .packlist -print -delete
	/Developer/Makefiles/bin/compress-man-pages.pl "$(DSTROOT)/usr/share/man"
