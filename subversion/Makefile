RBLIBDIR = $(shell ruby -r mkmf -e 'puts Config.expand($$libdir)')

Project               = subversion
ProjectVersion        = 1.6.5

#-------------------------------------------------------------------------
# build/get-py-info.py appends "-framework Python" to its --link and --libs
# output.  This is wrong, and especially bad when building multi-version
# (e.g., when python 2.6 is the default, and we are building for 2.5,
# "-framework Python" will bring in the 2.6 python dylib, causing crashes).
# So the build_get-py-info.py.diff patch removes the appending of
# "-framework Python".
#-------------------------------------------------------------------------
Patches        = build_get-py-info.py.diff \
                 configure.diff \
                 Makefile.in.diff \
                 spawn.diff \
                 xcode.diff

GnuAfterInstall       = perl-bindings python-bindings ruby-bindings \
                        post-install install-plist
Extra_Configure_Flags = --enable-shared --disable-static \
                        --without-berkeley-db \
                        --with-apxs --disable-mod-activation \
                        --with-apr=/usr --with-apr-util=/usr \
                        --with-neon=/usr/local \
                        --with-swig=/usr \
                        --with-ruby-sitedir=$(RBLIBDIR)/ruby \
                        --without-sasl

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target  = install
Install_Flags   = DESTDIR=$(DSTROOT)

# multi-version support
VERSIONERDIR := /usr/local/versioner

#-------------------------------------------------------------------------
# Patch subversion's Makefile and Makefile.PL just after running configure
#
# In Makefile, we need to change the include path to the correct version
# of perl and python, and we need to force the libsvn_swig_perl and
# libsvn_swig_py dylibs to be installed in a version-specific place, so
# each version of perl and python has its own dylib.  The install name
# of these dylibs are set to the correct version-specific places, by defining
# a new macro, LINKVERS in Makefile.in, which uses the environment variable
# RPATHVERS (defined in "for" loops below) as the value to libtool's
# -rpath argument.
#
# For Makefile.PL, we need to remove -L/usr/lib, which is always first,
# and will cause the bundles to link against the libsvn_swig_perl dylib
# in /usr/lib, instead of the version-specific place (in the DSTROOT).
# This really only matters during the transition to the version-specific
# place, while the BuildRoot still contains libsvn_swig_perl in /usr/lib
# (it will eventually be cleaned out in a world build).
#-------------------------------------------------------------------------
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

PERLVERS := $(shell perl -MConfig -e 'print $$Config::Config{version}')
PYTHONVERS := $(shell python -c 'import sys; print("%d.%d" % sys.version_info[:2])')
MAKEFILEPL := subversion/bindings/swig/perl/native/Makefile.PL
$(ConfigStamp2): $(ConfigStamp)
	mv '$(BuildDirectory)/Makefile' '$(BuildDirectory)/Makefile.bak'
	sed -e '/^SWIG_PL_INCLUDES/s/$(PERLVERS)/$$(VERSIONER_PERL_VERSION)/g' \
	    -e '/^SWIG_PY_INCLUDES/s/$(PYTHONVERS)/$$(VERSIONER_PYTHON_VERSION)/g' \
	    -e '/^swig_py_libdir/s,$${exec_prefix}/lib,$${RPATHVERS},' \
	    -e '/^swig_pl_libdir/s,$${exec_prefix}/lib,$${RPATHVERS},' \
	    '$(BuildDirectory)/Makefile.bak' > '$(BuildDirectory)/Makefile'
	mv '$(BuildDirectory)/$(MAKEFILEPL)' '$(BuildDirectory)/$(MAKEFILEPL).bak'
	sed 's,-L/usr/lib,,' '$(BuildDirectory)/$(MAKEFILEPL).bak' > '$(BuildDirectory)/$(MAKEFILEPL)'
	$(TOUCH) $(ConfigStamp2)

# Perl multi-version support
PERLVERSIONS := $(VERSIONERDIR)/perl/versions
PERLSUBDEFAULT := $(shell sed -n '/^DEFAULT = /s///p' $(PERLVERSIONS))
PERLDEFAULT := $(shell grep '^$(PERLSUBDEFAULT)' $(PERLVERSIONS))
PERLUNORDEREDVERS := $(shell grep -v '^DEFAULT' $(PERLVERSIONS))
# do default version last
PERLORDEREDVERS := $(filter-out $(PERLDEFAULT),$(PERLUNORDEREDVERS)) $(PERLDEFAULT)
PERLTOPDIR := $(BuildDirectory)/subversion/bindings/swig/perl
PERLTOPDIRORIG := $(PERLTOPDIR).orig

perl-bindings:
	mv $(PERLTOPDIR) $(PERLTOPDIRORIG)
	@set -x && \
	cd $(BuildDirectory) && \
	for vers in $(PERLORDEREDVERS); do \
	    export VERSIONER_PERL_VERSION=$${vers} && \
	    installarchlib=`perl -MConfig -e 'print $$Config::Config{installarchlib}' | sed 's,Perl,Perl/Extras,'` && \
	    installprivlib=`perl -MConfig -e 'print $$Config::Config{installprivlib}' | sed 's,Perl,Perl/Extras,'` && \
	    installbin=$${installprivlib}/bin && \
	    PLARGS="INSTALLDIRS=perl INSTALLARCHLIB='$${installarchlib}' INSTALLPRIVLIB='$${installprivlib}' INSTALLBIN='$${installbin}' INSTALLSCRIPT='$${installbin}'" && \
	    MAKEARGS="DESTDIR='$(DSTROOT)'" && \
	    export RPATHVERS="$${installarchlib}/lib" && \
	    ditto $(PERLTOPDIRORIG) $(PERLTOPDIR) && \
	    make swig-pl PLARGS="$${PLARGS}" MAKEARGS="$${MAKEARGS}" ARCHFLAGS="$(RC_CFLAGS)" && \
	    make install-swig-pl PLARGS="$${PLARGS}" MAKEARGS="$${MAKEARGS}" DESTDIR="$(DSTROOT)" && \
	    for module in Client Core Delta Fs Ra Repos Wc; do\
		    $(CP) $(DSTROOT)$${installarchlib}/auto/SVN/_$${module}/_$${module}.bundle $(SYMROOT) && \
		    $(STRIP) -x $(DSTROOT)$${installarchlib}/auto/SVN/_$${module}/_$${module}.bundle && \
		    $(RM) $(DSTROOT)$${installarchlib}/auto/SVN/_$${module}/_$${module}.bs || exit 1; \
	    done && \
	    $(RM) $(DSTROOT)$${installarchlib}/perllocal.pod && \
	    mv $(PERLTOPDIR) $(PERLTOPDIR).$${vers} || exit 1; \
	done

# Python multi-version support
PYTHONVERSIONS := $(VERSIONERDIR)/python/versions
PYTHONDEFAULT := $(shell sed -n '/^DEFAULT = /s///p' $(PYTHONVERSIONS))
# ignore 3.0 for now
PYTHONIGNORE := 3.0
PYTHONUNORDEREDVERS := $(filter-out $(PYTHONIGNORE),$(shell grep -v '^DEFAULT' $(PYTHONVERSIONS)))
# do default version last
PYTHONORDEREDVERS := $(filter-out $(PYTHONDEFAULT),$(PYTHONUNORDEREDVERS)) $(PYTHONDEFAULT)
PYTHONTOPDIR := $(BuildDirectory)/subversion/bindings/swig/python
PYTHONTOPDIRORIG := $(PYTHONTOPDIR).orig

python-bindings:
	mv $(PYTHONTOPDIR) $(PYTHONTOPDIRORIG)
	@set -x && \
	cd $(BuildDirectory) && \
	for vers in $(PYTHONORDEREDVERS); do \
	    export VERSIONER_PYTHON_VERSION=$${vers} && \
	    export RPATHVERS=`python -c 'import sys; print sys.prefix'`/Extras/lib && \
	    PYTHONLIBDIR=$${RPATHVERS}/python && \
	    PYDIR=$${PYTHONLIBDIR}/libsvn && \
	    PYDIR_EXTRA=$${PYTHONLIBDIR}/svn && \
	    ditto $(PYTHONTOPDIRORIG) $(PYTHONTOPDIR) && \
	    make swig-py && \
	    make install-swig-py swig_pydir="$${PYDIR}" swig_pydir_extra="$${PYDIR_EXTRA}" DESTDIR="$(DSTROOT)" && \
	    for module in client core delta diff fs ra repos wc; do \
		    $(CP) $(DSTROOT)$${PYDIR}/_$${module}.so $(SYMROOT) && \
		    $(STRIP) -x $(DSTROOT)$${PYDIR}/_$${module}.so || exit 1; \
	    done && \
	    mv $(PYTHONTOPDIR) $(PYTHONTOPDIR).$${vers} || exit 1; \
	done

# Ruby stuff
RBARCHDIR = $(shell ruby -r mkmf -e 'puts Config.expand($$archdir)')

ruby-bindings:
	cd $(BuildDirectory) && make swig-rb
	cd $(BuildDirectory) && make install-swig-rb DESTDIR=$(DSTROOT)
	@for module in client core delta diff fs ra repos wc; do \
		$(CP) $(DSTROOT)$(RBARCHDIR)/svn/ext/$${module}.bundle $(SYMROOT); \
		$(STRIP) -x $(DSTROOT)$(RBARCHDIR)/svn/ext/$${module}.bundle; \
	done

LIBEXECDIR = $(shell apxs -q LIBEXECDIR)

# Post-install cleanup
post-install:
	@for binary in svn svnadmin svndumpfilter svnlook svnserve svnsync svnversion; do \
		file=$(DSTROOT)/usr/bin/$${binary}; \
		echo $(CP) $${file} $(SYMROOT); \
		$(CP) $${file} $(SYMROOT); \
		echo $(STRIP) -x $${file}; \
		$(STRIP) -x $${file}; \
	done
	@set -x && \
	for library in `find $(DSTROOT) -type f -name \*.dylib`; do \
		$(CP) $${library} $(SYMROOT) && \
		$(STRIP) -x $${library} || exit 1; \
	done
	find $(DSTROOT) -name \*.la -delete
	find $(DSTROOT)/System/Library/Perl -name .packlist -delete
	@for module in authz dav; do \
		$(CP) $(DSTROOT)$(LIBEXECDIR)/mod_$${module}_svn.so $(SYMROOT); \
		$(STRIP) -x $(DSTROOT)$(LIBEXECDIR)/mod_$${module}_svn.so; \
	done

# Extract the source.
install_source::
	$(RMDIR) $(SRCROOT)/$(Project) $(SRCROOT)/$(Project)-$(ProjectVersion)
	$(TAR) -C $(SRCROOT) -xof $(SRCROOT)/$(Project)-$(ProjectVersion).tar.bz2
	$(MV) $(SRCROOT)/$(Project)-$(ProjectVersion) $(SRCROOT)/$(Project)
	@set -x && \
	cd $(SRCROOT)/$(Project) && \
	for patchfile in $(Patches); do \
		patch -p0 -F0 -i $(SRCROOT)/files/$$patchfile || exit 1; \
	done
	ed - $(SRCROOT)/$(Project)/build-outputs.mk < $(SRCROOT)/files/fix-build-outputs.mk.ed

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

# testbots!
testbots:
	$(MAKE) -C $(OBJROOT) check
