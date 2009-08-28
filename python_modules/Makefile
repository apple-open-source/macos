# Makefile orchestrating python_modules

PROJECTS = \
    setuptools \
    altgraph \
    modulegraph \
    macholib \
    bdist_mpkg \
    py2app \
    numpy \
    xattr \
    bonjour-py

EXTRAS = $(shell python -c "import sys, os;print(os.path.join(sys.prefix, 'Extras'))")
EXTRASPYTHON = $(EXTRAS)/lib/python
OSL = OpenSourceLicenses
OSV = OpenSourceVersions

make := $(SRCROOT)/make.pl
export PYTHONPATH := $(DSTROOT)$(EXTRASPYTHON)
ifeq ($(DEBUG),YES)
export DISTUTILS_DEBUG := YES
endif

no_target:
	echo 'specify target install, installsrc, installhdrs, clean'
	false

install:
	mkdir -p "$(OBJROOT)/$(OSL)"
	mkdir -p "$(OBJROOT)/$(OSV)"
	for i in $(PROJECTS); do \
	    echo ===== Installing $$i ===== && \
	    $(make) -C $$i install \
		EXTRAS="$(EXTRAS)" EXTRASPYTHON="$(EXTRASPYTHON)" \
		OSL="$(OBJROOT)/$(OSL)" OSV="$(OBJROOT)/$(OSV)" \
		|| exit 1; \
	done
	@echo ===== Stripping binaries =====
	set -x && cd $(DSTROOT)$(EXTRASPYTHON) && \
	for i in `find . -name \*.so | sed 's,^\./,,'`; do \
	    rsync -R $$i $(SYMROOT) && \
	    strip -x $$i || exit 1; \
	done
	@echo ===== Fixing empty files =====
	@set -x && \
	for i in `find $(DSTROOT)$(EXTRASPYTHON) -name __init__.py -size 0c`; do \
	    echo '#' > $$i && \
	    j=`echo $$i | sed 's,^$(DSTROOT),,'` && \
	    python -c "from py_compile import compile;compile('$$i', dfile='$$j', doraise=True)" && \
	    python -O -c "from py_compile import compile;compile('$$i', dfile='$$j', doraise=True)" || exit 1; \
	done
	for i in `find $(DSTROOT)$(EXTRASPYTHON) -name zip-safe -size 0c`; do \
	    echo > $$i || exit 1; \
	done
	mkdir -p $(DSTROOT)/usr/local/$(OSL)
	@set -x && \
	cd $(OBJROOT)/$(OSL) && \
	for i in *; do \
	    echo '##########' `basename $$i` '##########' && \
	    cat $$i || exit 1; \
	done > $(DSTROOT)/usr/local/$(OSL)/python_modules.txt
	mkdir -p $(DSTROOT)/usr/local/$(OSV)
	(cd $(OBJROOT)/$(OSV) && \
	echo '<plist version="1.0">' && \
	echo '<array>' && \
	cat * && \
	echo '</array>' && \
	echo '</plist>') > $(DSTROOT)/usr/local/$(OSV)/python_modules.plist

installhdrs:
	@echo python_modules has no headers to install

installsrc:
	ditto . $(SRCROOT)

clean:
