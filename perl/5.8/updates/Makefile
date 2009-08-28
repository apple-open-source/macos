# Makefile updating perl
# Install into the Extras directory (just like CPAN), and after post-install
# cleanups, merge into the regular perl directories.

PROJECTS = \
    Pod-Escapes \
    Pod-Simple \
    podlators

EXTRAS := $(ENV_PERL)/Extras
EXTRASPERL := $(EXTRAS)/$(ENV_PERL_VERSION)
make := $(SRCROOT)/updates/make.pl
export NO_PERL_PREPENDTOPATH := 1
export ARCHFLAGS := $(RC_CFLAGS)

no_target:
	@set -x && for i in $(PROJECTS); do \
	    $(make) -C $$i || exit 1; \
	done

install:
ifeq ($(words $(PROJECTS)),0)
	@echo No updates to install
else
	@set -x && for i in $(PROJECTS); do \
	    $(make) -C $$i install || exit 1; \
	done
	@echo ================ post-install fixups ================
	rm -f $(DSTROOT)$(EXTRASPERL)/$(ENV_PERL_ARCHNAME)/perllocal.pod
	find $(DSTROOT)$(EXTRASPERL)/$(ENV_PERL_ARCHNAME)/auto -name .packlist -print -delete
	ditto $(DSTROOT)$(EXTRASPERL) $(DSTROOT)$(ENV_PERL)/$(ENV_PERL_VERSION)
	rm -rf $(DSTROOT)$(EXTRAS)
endif

clean:
