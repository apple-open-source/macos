#
# Ok this is taken from an automaked file and tweaked out
#
include ../platform-settings

CFLAGS=$(CCFLAGS) $(DEFINES) -I../jabberd/

jsm_HEADERS=jsm.h ../jabberd/jabberd.h

jsm_OBJECTS = \
	deliver.o \
	jsm.o \
	modules.o \
	offline.o \
	server.o \
	authreg.o \
	sessions.o \
	users.o \
	util.o

jsm_EXOBJECTS = \
	modules/mod_admin.o \
	modules/mod_announce.o \
	modules/mod_auth_crypt.o \
	modules/mod_auth_plain.o \
	modules/mod_auth_digest.o \
	modules/mod_auth_0k.o \
	modules/mod_echo.o \
	modules/mod_filter.o \
	modules/mod_groups.o \
	modules/mod_presence.o \
	modules/mod_xml.o \
	modules/mod_roster.o \
	modules/mod_time.o \
	modules/mod_vcard.o \
	modules/mod_version.o \
	modules/mod_register.o \
	modules/mod_log.o \
	modules/mod_last.o \
	modules/mod_offline.o \
	modules/mod_agents.o \
	modules/mod_browse.o \
	modules/mod_disco.o

SUBDIRS=modules

single: static

all: all-recursive

clean: clean-recursive

$(jsm_OBJECTS): $(jsm_HEADERS)

all-local: $(jsm_OBJECTS) $(jsm_HEADERS)
ifeq ($(__CYGWIN__),1)
	$(CC) $(CFLAGS) $(MCFLAGS) -o jsm.dll $(jsm_OBJECTS) $(jsm_EXOBJECTS) ../jabberd/jabberd.a $(LDFLAGS) $(LIBS) -lcrypt
else
	$(CC) $(CFLAGS) $(MCFLAGS) -o jsm.so $(jsm_OBJECTS) $(jsm_EXOBJECTS) $(LDFLAGS) $(LIBS) 
endif

static: static-recursive

static-local: $(jsm_OBJECTS) $(jsm_HEADERS)

install-local:

all-recursive install-data-recursive install-exec-recursive \
installdirs-recursive install-recursive uninstall-recursive  \
check-recursive installcheck-recursive info-recursive static-recursive:
	@set fnord $(MAKEFLAGS); amf=$$2; \
	dot_seen=no; \
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
ifeq ($(__CYGWIN__),1)
	rm -f $(jsm_OBJECTS) jsm.dll
else
	rm -f $(jsm_OBJECTS) jsm.so
endif

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
