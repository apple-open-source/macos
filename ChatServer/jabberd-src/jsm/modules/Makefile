include ../../platform-settings

CFLAGS=$(CCFLAGS) $(DEFINES) -I../../jabberd/

jsm_modules_HEADERS=../jsm.h ../../jabberd/jabberd.h

jsm_modules_OBJECTS=\
	mod_admin.o \
	mod_announce.o \
	mod_auth_crypt.o \
	mod_auth_plain.o \
	mod_auth_digest.o \
	mod_auth_0k.o \
	mod_echo.o \
	mod_filter.o \
	mod_groups.o \
	mod_presence.o \
	mod_xml.o \
	mod_roster.o \
	mod_time.o \
	mod_vcard.o \
	mod_version.o \
	mod_register.o \
	mod_log.o \
	mod_last.o \
	mod_offline.o \
	mod_browse.o \
	mod_agents.o \
	mod_disco.o

$(jsm_modules_OBJECTS): $(jsm_modules_HEADERS)

single: static

all: $(jsm_modules_OBJECTS) $(jsm_modules_HEADERS)

static: $(jsm_modules_OBJECTS) $(jsm_modules_HEADERS)

clean:
	rm -f $(jsm_modules_OBJECTS)
