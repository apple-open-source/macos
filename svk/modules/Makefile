# Makefile orchestrating perl extras

PROJECTS = \
	Algorithm-Diff \
	Algorithm-Annotate \
	YAML-Syck \
	Data-Hierarchy \
	PerlIO-via-dynamic \
	PerlIO-via-symlink \
	IO-Digest \
	SVN-Simple \
	PerlIO-eol \
	Class-Autouse \
	Locale-Maketext-Simple \
	App-CLI \
	List-MoreUtils \
	Class-Data-Inheritable \
	Path-Class \
	UNIVERSAL-require \
	TermReadKey \
	File-Temp \
	PathTools \
	Locale-Maketext-Lexicon \
	IO-Pager \
	TimeDate \
	File-chdir \
	SVN-Mirror \
	FreezeThaw \
	SVK

# These variables cause installation into the Extras directory, adds RC_CFLAGS
# to the compile and linking arguments, and sets DESTDIR to DSTROOT
installarchlib := $(subst Perl,Perl/Extras,$(shell perl -MConfig -e 'print $$Config::Config{installarchlib}'))
installbin := $(subst $(DSTROOT),,$(EXTRAS)/bin)
installprivlib := $(subst Perl,Perl/Extras,$(shell perl -MConfig -e 'print $$Config::Config{installprivlib}'))
PLARGS := INSTALLDIRS=perl INSTALLARCHLIB='$(installarchlib)' INSTALLPRIVLIB='$(installprivlib)' INSTALLBIN='$(installbin)' INSTALLSCRIPT='$(installbin)'
MAKEARGS := PASTHRU_INC='$(RC_CFLAGS)' OTHERLDFLAGS='$(RC_CFLAGS)' DESTDIR=$(DSTROOT)
export PERL5LIB := $(EXTRASPERL)

no_target:
	@for i in $(PROJECTS); do \
	    echo make -C $$i installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		MAKEARGS="$(MAKEARGS)" && \
	    make -C $$i installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		MAKEARGS="$(MAKEARGS)"; \
	done

install:
	@for i in $(PROJECTS); do \
	    echo make -C $$i install installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		MAKEARGS="$(MAKEARGS)" && \
	    make -C $$i install installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		MAKEARGS="$(MAKEARGS)"; \
	done
	rm -f $(EXTRASPERL)/$(ARCHLIB)/perllocal.pod
	find $(EXTRASPERL)/$(ARCHLIB)/auto -name \*.bundle -exec strip -x {} \;
	find $(EXTRASPERL)/$(ARCHLIB)/auto -name .packlist -delete

plist:
	@for i in $(PROJECTS); do \
		make -C $$i plist; \
	done
