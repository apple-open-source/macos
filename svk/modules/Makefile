# Makefile orchestrating perl extras

PROJECTS = \
	SVN-Dump \
	SVN-Simple \
	SVN-Mirror \
	SVK

# These variables cause installation into the Extras directory, adds RC_CFLAGS
# to the compile and linking arguments, and sets DESTDIR to DSTROOT
installarchlib := $(subst Perl,Perl/Extras,$(shell perl -MConfig -e 'print $$Config::Config{installarchlib}'))
installbin := $(subst $(DSTROOT),,$(EXTRAS)/bin)
installprivlib := $(subst Perl,Perl/Extras,$(shell perl -MConfig -e 'print $$Config::Config{installprivlib}'))
PLARGS := INSTALLDIRS=perl INSTALLARCHLIB='$(installarchlib)' INSTALLPRIVLIB='$(installprivlib)' INSTALLBIN='$(installbin)' INSTALLSCRIPT='$(installbin)'
MAKEARGS := DESTDIR=$(DSTROOT)
export PERL5LIB := $(EXTRASPERL)

no_target:
	@for i in $(PROJECTS); do \
	    echo make -C $$i installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		MAKEARGS="$(MAKEARGS)" ARCHFLAGS="$(RC_CFLAGS)" && \
	    make -C $$i installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		MAKEARGS="$(MAKEARGS)" ARCHFLAGS="$(RC_CFLAGS)"\
		|| exit 1; \
	done

install:
	@for i in $(PROJECTS); do \
	    echo make -C $$i install installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		MAKEARGS="$(MAKEARGS)" ARCHFLAGS="$(RC_CFLAGS)" && \
	    make -C $$i install installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		MAKEARGS="$(MAKEARGS)" ARCHFLAGS="$(RC_CFLAGS)" \
		|| exit 1; \
	done
	rm -f $(EXTRASPERL)/$(ARCHLIB)/perllocal.pod
	find $(EXTRASPERL)/$(ARCHLIB)/auto -name \*.bundle -exec strip -x {} \;
	find $(EXTRASPERL)/$(ARCHLIB)/auto -name .packlist -delete

plist:
	@for i in $(PROJECTS); do \
		make -C $$i plist || exit 1; \
	done
