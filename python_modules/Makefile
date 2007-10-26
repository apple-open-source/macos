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

EXTRAS = $(shell python -c "import sys, os;print os.path.join(sys.prefix, 'Extras')")
EXTRASPYTHON = $(EXTRAS)/lib/python
make := $(SRCROOT)/make.pl
export PYTHONPATH := $(DSTROOT)$(EXTRASPYTHON)
ifeq ($(DEBUG),YES)
export DISTUTILS_DEBUG := YES
endif

no_target:
	for i in $(PROJECTS); do \
	    echo ===== Building $$i ===== && \
	    $(make) -C $$i \
		EXTRAS="$(EXTRAS)" EXTRASPYTHON="$(EXTRASPYTHON)" \
		|| exit 1; \
	done

install:
	for i in $(PROJECTS); do \
	    echo ===== Installing $$i ===== && \
	    $(make) -C $$i install \
		EXTRAS="$(EXTRAS)" EXTRASPYTHON="$(EXTRASPYTHON)" \
		|| exit 1; \
	done
	@echo ===== Stripping binaries =====
	find $(DSTROOT)$(EXTRASPYTHON) -name \*.so -print -exec strip -x {} \;
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

installhdrs:
	@echo python_modules has no headers to install

installsrc:
	ditto . $(SRCROOT)

clean:
