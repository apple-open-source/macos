#
# Ok this is taken from an automaked file and tweaked out
#
# Portions (c) Copyright 2005 Apple Computer, Inc.
#
include platform-settings

ifeq ($(__CYGWIN__),1)

ifeq ($(MAKECMDGOALS), single)
SUBDIRS:=jabberd/pth-1.4.0 jabberd/lib pthsock xdb_file dnsrv jsm dialback jabberd jsm_apple
else 
ifeq ($(MAKECMDGOALS), static)
SUBDIRS:=jabberd/pth-1.4.0 jabberd/lib pthsock xdb_file dnsrv jsm dialback jabberd jsm_apple
else
SUBDIRS:=jabberd/pth-1.4.0 jabberd pthsock xdb_file dnsrv jsm dialback jsm_apple
endif
endif

else

SUBDIRS=pthsock xdb_file dnsrv jsm dialback jabberd jabberd/pth-1.4.0 jsm_apple

endif

all: all-recursive

clean: clean-recursive

static: static-recursive

single: single-recursive

install:
	printf "\n\nNo actual make install, you just run it out of the directory!\n"

all-local:

install-local:

static-local:

single-local:

all-recursive install-data-recursive install-exec-recursive \
installdirs-recursive install-recursive uninstall-recursive  \
check-recursive installcheck-recursive single-recursive static-recursive:
	@set fnord $(MAKEFLAGS); amf=$$2; \
	dot_seen=no; \
    if test "$@" = "single-recursive"; then \
      export ISSINGLE=1; \
    fi; \
	if test "$@" = "static-recursive"; then \
      export ISSTATIC=1; \
	fi; \
	target=`echo $@ | sed s/-recursive//`; \
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  echo "Making $$target in $$subdir"; \
	  if test "$$subdir" = "."; then \
	    dot_seen=yes; \
	    local_target="$$target-local"; \
	  else \
	    local_target="$$target"; \
	  fi; \
	  (cd $$subdir && $(MAKE) $$local_target) \
	   || case "$$amf" in *=*) exit 1;; *k*) fail=yes;; *) exit 1;; esac; \
	done; \
	if test "$$dot_seen" = "no"; then \
	  $(MAKE) "$$target-local" || exit 1; \
	fi; test -z "$$fail"

clean-local:

mostlyclean-recursive clean-recursive distclean-recursive \
maintainer-clean-recursive:
	@set fnord $(MAKEFLAGS); amf=$$2; \
	dot_seen=no; \
	rev=''; list='$(SUBDIRS)'; for subdir in $$list; do \
	  rev="$$subdir $$rev"; \
	  test "$$subdir" = "." && dot_seen=yes; \
	done; \
	test "$$dot_seen" = "no" && rev=". $$rev"; \
	target=`echo $@ | sed s/-recursive//`; \
	for subdir in $$rev; do \
	  echo "Making $$target in $$subdir"; \
	  if test "$$subdir" = "."; then \
	    local_target="$$target-local"; \
	  else \
	    local_target="$$target"; \
	  fi; \
	  (cd $$subdir && $(MAKE) $$local_target) \
	   || case "$$amf" in *=*) exit 1;; *k*) fail=yes;; *) exit 1;; esac; \
	done && test -z "$$fail"
