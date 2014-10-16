# Makefile orchestrating python_modules

include $(VERSIONER_PYTHON_VERSION).inc

EXTRAS = $(shell python -c "import sys, os;print(os.path.join(sys.prefix, 'Extras'))")
EXTRASPYTHON = $(EXTRAS)/lib/python

make := $(SRCROOT)/make.pl
export PYTHONPATH := $(DSTROOT)$(EXTRASPYTHON)
ifeq ($(DEBUG),YES)
export DISTUTILS_DEBUG := YES
endif

no_target:
	echo 'specify target install, installsrc, installhdrs, clean'
	false

install:
	@set -x && for i in $(MODULES); do \
	    echo ===== Installing $$i ===== && \
	    $(make) -C Modules/$$i install \
		EXTRAS="$(EXTRAS)" EXTRASPYTHON="$(EXTRASPYTHON)" \
		OSL='$(OSL)' OSV='$(OSV)' \
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

installhdrs:
	@echo $(Project) has no headers to install

installsrc:
	ditto . $(SRCROOT)

clean:
