## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = gnits

TESTS = cdemo-static.test cdemo-make.test cdemo-exec.test \
	demo-static.test demo-make.test demo-exec.test \
	demo-inst.test demo-unst.test \
	depdemo-static.test depdemo-make.test \
	depdemo-exec.test depdemo-inst.test depdemo-unst.test \
	mdemo-static.test mdemo-make.test mdemo-exec.test \
	mdemo-inst.test mdemo-unst.test \
	cdemo-conf.test cdemo-make.test cdemo-exec.test \
	demo-conf.test demo-make.test demo-exec.test \
	demo-inst.test demo-unst.test deplibs.test \
	depdemo-conf.test depdemo-make.test \
	depdemo-exec.test depdemo-inst.test depdemo-unst.test \
	mdemo-conf.test mdemo-make.test mdemo-exec.test \
	mdemo-inst.test mdemo-unst.test dryrun.test \
	demo-nofast.test demo-make.test demo-exec.test \
	demo-inst.test demo-unst.test \
	demo-pic.test demo-make.test demo-exec.test \
	demo-nopic.test demo-make.test demo-exec.test \
	depdemo-nofast.test depdemo-make.test \
	depdemo-exec.test depdemo-inst.test depdemo-unst.test \
	cdemo-shared.test cdemo-make.test cdemo-exec.test \
	demo-shared.test demo-make.test demo-exec.test demo-inst.test \
	hardcode.test build-relink.test noinst-link.test demo-unst.test \
	depdemo-shared.test depdemo-make.test \
	depdemo-exec.test depdemo-inst.test build-relink2.test \
	depdemo-unst.test \
	mdemo-shared.test mdemo-make.test mdemo-exec.test \
	mdemo-inst.test mdemo-unst.test \
	assign.test link.test link-2.test nomode.test \
	quote.test sh.test suffix.test

demo-conf.test: $(top_builddir)/libtool

$(top_builddir)/libtool:
	cd $(top_builddir) && $(MAKE) all

# We should export these variables when we build the libtool script.
CFLAGS = @CFLAGS@
CPPFLAGS = @CPPFLAGS@
LDFLAGS = @LDFLAGS@
LIBS = @LIBS@

# Be sure to reexport important environment variables.
TESTS_ENVIRONMENT = MAKE="$(MAKE)" CC="$(CC)" CFLAGS="$(CFLAGS)" \
	CPPFLAGS="$(CPPFLAGS)" LD="$(LD)" LDFLAGS="$(LDFLAGS)" \
	LIBS="$(LIBS)" LN_S="$(LN_S)" NM="$(NM)" RANLIB="$(RANLIB)" \
	OBJEXT="$(OBJEXT)" EXEEXT="$(EXEEXT)"

EXTRA_DIST = defs $(TESTS)

# We need to remove any files that the above tests created.
clean-local:
	-test -f ../cdemo/Makefile && cd ../cdemo && $(MAKE) distclean
	-test -f ../demo/Makefile && cd ../demo && $(MAKE) distclean
	-test -f ../depdemo/Makefile && cd ../depdemo && $(MAKE) distclean
	-test -f ../mdemo/Makefile && cd ../mdemo && $(MAKE) distclean
	rm -rf _inst
