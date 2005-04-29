#
# Ok this is taken from an automaked file and tweaked out
#
# Portions (c) Copyright 2005 Apple Computer, Inc.
#
include ../platform-settings
ICHATHOME=/private/var/jabber/
ICHATXML=/etc/jabber/jabber.xml
ifeq ($(ISSINGLE),1)
	DEFINES=-DHOME="\"$(ICHATHOME)\"" -DSINGLE -DSTATIC
else
	DEFINES=-DHOME="\"$(ICHATHOME)\"" -DCONFIGXML="\"$(ICHATXML)\""
endif
ifeq ($(ISSTATIC),1)
	DEFINES=-DHOME="\"$(ICHATHOME)\"" -DSTATIC
endif

CFLAGS=$(CCFLAGS) $(DEFINES)

jabberd_HEADERS=jabberd.h single.h 

SUBDIRS=base lib $(PSUBDIR)

LIBS += -lexpat
FRAMEWORKS=-framework CoreFoundation -framework DirectoryService -framework Security 

jabberd_OBJECTS = \
	config.o \
	mio.o \
	mio_raw.o \
	mio_xml.o \
	mio_ssl.o \
	deliver.o \
	heartbeat.o \
	jabberd.o \
	load.o \
	xdb.o \
	mtq.o \
	static.o \
	log.o

jabberd_EXOBJECTS = \
	lib/expat.o \
	lib/base64.o \
	lib/genhash.o \
	lib/jid.o \
	lib/jpacket.o \
	lib/jutil.o \
	lib/karma.o \
	lib/pool.o \
	lib/pproxy.o \
	lib/rate.o \
	lib/sha.o \
	lib/snprintf.o \
	lib/socket.o \
	lib/str.o \
	lib/xmlnode.o \
	lib/xstream.o \
	lib/xhash.o \
	base/base_connect.o \
	base/base_dynamic.o \
	base/base_exec.o \
	base/base_stdout.o \
	base/base_accept.o \
	base/base_file.o \
	base/base_format.o \
	base/base_stderr.o \
	base/base_to.o

jabberd_STATICOBJS=\
	$(JHOME)/jsm/deliver.o \
	$(JHOME)/jsm/jsm.o \
	$(JHOME)/jsm/modules.o \
	$(JHOME)/jsm/offline.o \
	$(JHOME)/jsm/server.o \
	$(JHOME)/jsm/authreg.o \
	$(JHOME)/jsm/sessions.o \
	$(JHOME)/jsm/users.o \
	$(JHOME)/jsm/util.o \
	$(JHOME)/jsm/modules/mod_admin.o \
	$(JHOME)/jsm/modules/mod_agents.o \
	$(JHOME)/jsm/modules/mod_announce.o \
	$(JHOME)/jsm/modules/mod_auth_plain.o \
	$(JHOME)/jsm/modules/mod_auth_digest.o \
	$(JHOME)/jsm/modules/mod_auth_0k.o \
	$(JHOME)/jsm/modules/mod_browse.o \
	$(JHOME)/jsm/modules/mod_echo.o \
	$(JHOME)/jsm/modules/mod_filter.o \
	$(JHOME)/jsm/modules/mod_groups.o \
	$(JHOME)/jsm/modules/mod_last.o \
	$(JHOME)/jsm/modules/mod_presence.o \
	$(JHOME)/jsm/modules/mod_roster.o \
	$(JHOME)/jsm/modules/mod_time.o \
	$(JHOME)/jsm/modules/mod_vcard.o \
	$(JHOME)/jsm/modules/mod_version.o \
	$(JHOME)/jsm/modules/mod_register.o \
	$(JHOME)/jsm/modules/mod_log.o \
	$(JHOME)/jsm/modules/mod_offline.o \
	$(JHOME)/jsm/modules/mod_xml.o \
	$(JHOME)/dnsrv/dnsrv.o \
	$(JHOME)/dialback/dialback.o \
	$(JHOME)/dialback/dialback_out.o \
	$(JHOME)/dialback/dialback_in.o \
	$(JHOME)/pthsock/client.o \
	$(JHOME)/xdb_file/xdb_file.o

ifneq ($(__CYGWIN__),1)
jabberd_STATICOBJS+= $(JHOME)/dnsrv/srv_resolv.o
endif

ifeq ($(__CYGWIN__),1)
PTHLIB=$(shell pth-config --libdir)/libpth.a
EXTRALIBS=$(LIBS) /lib/crt0.o $(shell gcc -print-libgcc-file-name) -lcygwin -lkernel32
endif

single: static

all: all-recursive

clean: clean-recursive

static: static-recursive

$(jabberd_OBJECTS): $(jabberd_HEADERS)

all-local: $(jabberd_OBJECTS)
ifeq ($(__CYGWIN__),1)
# Make .def file:
# deliver__flag, found in jabberd.c, is used in MU-Conference v0.5.2, so
# we need to export this symbol so MU-Conference will build properly.
	echo EXPORTS > jabberd.def
	echo deliver__flag >> jabberd.def
	nm $(jabberd_OBJECTS) $(jabberd_EXOBJECTS) $(PTHLIB) | grep '^........ [T] _' | sed 's/[^_]*_//' | sort >> jabberd.def

# Link DLL.
	ld --base-file jabberd.base -o jabberd.exe $(jabberd_OBJECTS) $(jabberd_EXOBJECTS) $(dllinit_OBJECTS) $(EXTRALIBS) $(LDFLAGS)
	dlltool --as=as --dllname jabberd.exe --def jabberd.def --base-file jabberd.base --output-exp jabberd.exp
	ld --base-file jabberd.base jabberd.exp -o jabberd.exe $(jabberd_OBJECTS) $(jabberd_EXOBJECTS) $(dllinit_OBJECTS) $(EXTRALIBS) $(LDFLAGS)
	dlltool --as=as --dllname jabberd.exe --def jabberd.def --base-file jabberd.base --output-exp jabberd.exp
	ld jabberd.exp -o jabberd.exe $(jabberd_OBJECTS) $(jabberd_EXOBJECTS) $(dllinit_OBJECTS) $(EXTRALIBS) $(LDFLAGS)

# Build the jabberd.a lib to link to:
	dlltool --as=as --dllname jabberd.exe --def jabberd.def --output-lib jabberd.a
else
	$(CC) $(CFLAGS) -o jabberd $(jabberd_OBJECTS) $(jabberd_EXOBJECTS) $(XLDFLAGS) $(LDFLAGS) $(LIBS) $(PLINK) $(FRAMEWORKS)
endif

static-local: $(jabberd_OBJECTS)
	$(CC) $(CFLAGS) -o jabberd $(jabberd_OBJECTS) $(jabberd_EXOBJECTS) $(jabberd_STATICOBJS) $(LDFLAGS) $(SLIBS) $(PLINK) $(FRAMEWORKS)

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
      if test "$$subdir" = "pth-1.3.7"; then \
        local_target="all"; \
      fi; \
	  (cd $$subdir && $(MAKE) $$local_target) \
	   || case "$$amf" in *=*) exit 1;; *k*) fail=yes;; *) exit 1;; esac; \
	done; \
	if test "$$dot_seen" = "no"; then \
	  echo "Making $$target"; \
	  $(MAKE) "$$target-local" || exit 1; \
	fi; test -z "$$fail"

clean-local:
ifeq ($(__CYGWIN__),1)
	rm -f $(jabberd_OBJECTS) jabberd *.exe *.a *.base *.def *.exp
else
	rm -f $(jabberd_OBJECTS) jabberd
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

.PHONY: clean static single
